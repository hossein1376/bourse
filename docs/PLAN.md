# Order Matching Engine — Project Reference

## Table of Contents

1. [Project Overview](#project-overview)
2. [Domain Principles](#domain-principles)
3. [System Architecture](#system-architecture)
4. [Service Properties](#service-properties)
5. [Infrastructure & Technologies](#infrastructure--technologies)
6. [Hard Engineering Problems](#hard-engineering-problems)
7. [Learning Resources](#learning-resources)
8. [Implementation Roadmap](#implementation-roadmap)

---

## Project Overview

A simplified financial exchange backend built as a polyglot microservice system. The goal is not to build a production trading platform — it is to use a domain with genuine real-time, low-latency, and correctness requirements as the forcing function for learning Rust, C++, and Go in a systems context.

Each language earns its role based on the actual demands of the component it owns:

| Language | Service              | Primary Justification                                                  |
| -------- | -------------------- | ---------------------------------------------------------------------- |
| C++      | Matching Engine      | Microsecond latency, cache-sensitive data structures, no GC            |
| Rust     | Market Data Service  | High-concurrency I/O, memory safety on untrusted input, no GC          |
| Go       | Gateway + Settlement | Concurrent I/O-bound work, broad ecosystem, correctness over raw speed |

---

## Domain Principles

### The Order Book

The order book is the central data structure of any exchange. It is a live, two-sided ledger of all outstanding unfilled orders for a given instrument (e.g. AAPL stock, BTC/USD pair).

- **Bids** — all outstanding buy orders, sorted by price descending (highest willingness to pay at the top)
- **Asks** — all outstanding sell orders, sorted by price ascending (lowest willingness to accept at the top)
- **Best bid** — the highest price any buyer is currently willing to pay
- **Best ask** — the lowest price any seller is currently willing to accept
- **Spread** — the difference between best ask and best bid; a narrow spread indicates a liquid market
- **Depth** — the total quantity available at or near the best prices on each side

A trade occurs when the best bid meets or exceeds the best ask — the spread collapses to zero or below.

### Price-Time Priority

The fairness rule of the order book. When multiple orders sit at the same price level, the one that arrived **earliest** gets filled first. This is strict FIFO per price level.

This rule exists to reward passive liquidity providers — participants who post limit orders and wait. It prevents a faster participant from repeatedly queue-jumping at the same price.

### Order Types

**Market Order**
Execute immediately at the best available price on the opposite side. No price is specified. Always fills (assuming any liquidity exists), but the execution price is uncertain. Aggressive by nature.

**Limit Order**
Execute at the specified price or better (lower for buys, higher for sells). If no matching counterparty exists at that price right now, the order rests in the book and waits. Passive by nature unless it crosses the spread immediately.

**IOC — Immediate or Cancel**
A limit order with a time constraint: fill whatever quantity you can right now, cancel whatever remains unfilled. Never rests in the book. Useful when a participant wants a price guarantee but accepts a partial fill.

**FOK — Fill or Kill**
All or nothing. Either the entire quantity fills immediately, or the entire order is cancelled. No partial fills, no resting. Used when partial fills are operationally problematic.

**Stop Order** _(v2 scope)_
Dormant until a trigger price is reached in the market. Upon triggering, it becomes either a market or limit order. Introduces state and conditional logic into the engine.

**Iceberg Order** _(v2 scope)_
Only a portion of the total quantity is visible in the book at any time. When the visible slice is filled, the next slice becomes visible. Used by large participants to conceal full order size.

### Fills and Trade Events

A **fill** occurs when two orders are matched. It can be:

- **Full fill** — the entire quantity of one or both orders is consumed
- **Partial fill** — only part of an order's quantity is matched; the remainder either rests in the book or is cancelled depending on order type

A **trade event** is the output emitted when a fill occurs. It contains:

- Instrument identifier
- Execution price
- Executed quantity
- Aggressor side (which order crossed the spread)
- Buyer order ID and seller order ID
- Sequence number
- Timestamp (nanosecond precision)

### Sequence Numbers

Every order receives a monotonically increasing sequence number at the moment it enters the system. This number is the source of truth for time priority. It also enables deterministic replay — given the same sequence of orders in the same order, the engine always produces the same output.

### Key Terminology Reference

| Term            | Meaning                                                        |
| --------------- | -------------------------------------------------------------- |
| Instrument      | The tradable asset (e.g. AAPL, BTC/USD)                        |
| Aggressor       | The order that crossed the spread and triggered a match        |
| Passive / Maker | The resting order that was matched against                     |
| Taker           | Synonym for aggressor                                          |
| NBBO            | National Best Bid/Offer — the best prices across all venues    |
| L1 Data         | Best bid, best ask, and their quantities only                  |
| L2 Data         | Full price level depth (quantity at each price level)          |
| L3 Data         | Individual order-level detail (who posted what at which level) |
| Circuit Breaker | A halt mechanism triggered when price moves exceed a threshold |

---

## System Architecture

### Data Flow

```
Client
  │
  ▼
┌─────────────────────────-┐
│   Order Gateway (Go)     │  ← WebSocket / REST inbound
│   - Risk checks          │
│   - Sequence numbering   │
│   - Rate limiting        │
└────────────┬────────────-┘
             │ Kafka: orders-{symbol}
             ▼
┌────────────────────────-─┐
│  Matching Engine (C++)   │  ← One instance per partition set
│  - Order book per symbol │
│  - Matching loop         │
│  - Fill generation       │
└──────┬──────────┬───────-┘
       │          │
       │ Kafka    │ Kafka
       │ trades   │ book-updates
       ▼          ▼
┌──────────┐  ┌──────────────────────┐
│Settlement│  │ Market Data (Rust)   │
│  (Go)    │  │ - Book mirror        │
│ positions│  │ - Snapshot + delta   │
│ balances │  │ - WebSocket broadcast│
└──────────┘  └──────────────────────┘
```

### Kafka Topic Design

| Topic                   | Partition Key | Consumers                      |
| ----------------------- | ------------- | ------------------------------ |
| `orders-{symbol}`       | symbol        | Matching Engine                |
| `trades-{symbol}`       | symbol        | Settlement, Market Data, Audit |
| `book-updates-{symbol}` | symbol        | Market Data                    |
| `confirmations`         | client ID     | Gateway (for client ACKs)      |

Partitioning by symbol on the orders topic is mandatory — Kafka only guarantees ordering within a partition, and order sequence integrity per instrument is a correctness requirement, not a preference.

---

## Service Properties

### Order Gateway (Go)

**Responsibilities**

- Accept inbound orders over WebSocket and/or a simplified REST interface
- Perform pre-trade risk checks: position limits, credit checks, duplicate order detection, per-client rate limiting
- Assign a monotonically increasing sequence number to each accepted order
- Publish accepted orders to the correct Kafka partition
- Route confirmations and trade reports back to the originating client

**Key Design Considerations**

- The gateway is the only component that assigns sequence numbers. This must be serialized — a single atomic counter or a Kafka offset used as the sequence.
- Risk checks must be fast. They run on every order in the hot path. Keep them in memory; avoid synchronous database calls here.
- Sequence number assignment and Kafka produce should be treated as a single atomic operation conceptually. A failure between the two could lose an order or introduce a gap in the sequence.
- The gateway is stateless in terms of order book knowledge — it does not know what is in the book.

**Metrics to expose**

- Orders received per second (by symbol, by client)
- Orders rejected per second (by rejection reason)
- Kafka produce latency histogram
- Active client connections
- Risk check latency histogram

### Matching Engine (C++)

**Responsibilities**

- Consume orders from Kafka in strict partition order
- Maintain an in-memory order book per instrument
- Execute the matching loop on each incoming order
- Emit trade events and order book update events to Kafka
- Support order cancellation and amendment

**Order Book Data Structure**
The naive implementation is `std::map<Price, std::queue<Order>>`. This is correct but has cache locality issues under load. A more considered approach:

- A doubly-linked list of price level nodes, with a hash map for O(1) price level lookup by price
- Each price level contains a doubly-linked list of orders (for O(1) insertion at the tail and O(1) cancellation with a direct pointer)
- Separate structures for the bid side and ask side

**Key Design Considerations**

- The engine is deterministic by design. It is a pure function: orders in → trade events out. No external calls during matching.
- Kafka consumer offset = the engine's checkpoint. On restart, replay from the last committed offset to rebuild book state, or from a snapshot offset.
- Cancellation requires an order ID to order-node pointer index for O(1) lookup. Without this, cancellation is O(n) per price level.
- The matching loop should emit partial fill events as they happen, not after the full order is processed.

**Metrics to expose**

- Orders processed per second (per symbol)
- Matches per second (per symbol)
- Matching loop latency histogram (target: microsecond buckets)
- Order book depth (number of price levels, total resting orders)
- Cancellation rate

### Market Data Service (Rust)

**Responsibilities**

- Consume trade events and book update events from Kafka
- Maintain a mirror of the current order book state per instrument
- Serve L1 and L2 market data to WebSocket subscribers
- Handle snapshot requests: new subscribers need a full book snapshot before receiving incremental deltas
- Publish NBBO updates

**Key Design Considerations**

- The snapshot + delta problem is the core challenge. A subscriber connecting mid-stream cannot receive incremental updates from an arbitrary offset — they will have an inconsistent view. The service must atomically capture a snapshot and begin streaming deltas from exactly the point the snapshot was taken.
- Memory safety under adversarial input matters here — WebSocket clients are external and untrusted.
- The Kafka consumer and the WebSocket broadcaster operate on the same book state concurrently. Synchronization strategy (e.g. lock-per-symbol vs. a channel-based single-writer model) has a measurable impact on throughput.
- Subscriber connection churn (connects and disconnects) should not affect the book mirror's update path.

**Metrics to expose**

- Active subscribers (by symbol)
- Book update events consumed per second
- Snapshot delivery latency
- WebSocket message broadcast latency
- Kafka consumer lag (critical — if this grows, subscribers are receiving stale data)

### Settlement Service (Go)

**Responsibilities**

- Consume trade events from Kafka
- Update account positions and balances
- Emit trade confirmations back toward the client (via the `confirmations` topic or direct store)
- Maintain an audit trail of all executed trades

**Key Design Considerations**

- Exactly-once semantics matter here. A double-processed trade event would incorrectly update a balance twice. Use Kafka consumer group offsets committed transactionally with the database write, or an idempotency key on each trade event.
- Updating two accounts in one trade (buyer's cash decreases, buyer's position increases; seller's position decreases, seller's cash increases) must be atomic at the database level.
- This service is not on the latency-critical path but is on the correctness-critical path.

**Metrics to expose**

- Trades settled per second
- Settlement latency (time from trade event to database commit)
- Failed settlements (with reason)
- Consumer lag on the trades topic

---

## Infrastructure & Technologies

### Kafka

**Why Kafka over other brokers**
Kafka's log-based storage with configurable retention makes it suitable as an event store, not just a transport. The matching engine can reconstruct its state by replaying the orders topic from a known offset. This property is what makes crash recovery tractable.

**Retention Strategy**

- `orders-{symbol}`: long retention (days/weeks) to enable full replay
- `trades-{symbol}`: long retention for audit purposes
- `book-updates-{symbol}`: short retention (hours) — these are transient state diffs, not authoritative records

**Consumer Group Design**

- Matching engine: static partition assignment (not automatic rebalancing). Each engine instance owns specific partitions. Use `assign()` not `subscribe()`.
- Settlement and market data: standard consumer groups, safe for automatic rebalancing since they are stateless processors.

### Kubernetes

**Matching Engine as a StatefulSet**
The matching engine is not truly stateless — it owns specific Kafka partitions and its in-memory book state is rebuilt from them. Model it as a StatefulSet with stable pod identities. Each pod's partition assignment should be derivable from its ordinal index.

**KEDA for Downstream Scaling**
Settlement and market data services can scale horizontally based on Kafka consumer group lag. KEDA's Kafka scaler watches the lag metric and adjusts replica count. This is a realistic autoscaling pattern that avoids over-provisioning during quiet periods.

**Resource Profiles**

| Service         | Type              | Notes                                                      |
| --------------- | ----------------- | ---------------------------------------------------------- |
| Matching Engine | CPU-bound         | Set explicit CPU requests and limits; avoid CPU throttling |
| Market Data     | Memory + network  | Scale on subscriber count or consumer lag                  |
| Gateway         | Network I/O bound | Scale on active connections or request rate                |
| Settlement      | I/O bound (DB)    | Scale on consumer lag                                      |

### Prometheus

**Suggested Histogram Buckets**
For latency-sensitive services (matching engine, gateway), use microsecond-scale buckets:
`50µs, 100µs, 250µs, 500µs, 1ms, 5ms, 10ms, 50ms, 100ms`

Standard millisecond buckets will mask the latency profile of the hot path entirely.

**Key Cross-Service Metrics**

- End-to-end order latency: timestamp at gateway ingress vs. trade event timestamp. Requires clock synchronization between hosts (NTP or PTP).
- Kafka consumer lag per service: the single most important health indicator for each downstream service.

### OpenTelemetry

**Trace Propagation Through Kafka**
This is the most interesting OTEL exercise in the system. Trace context must travel through Kafka message headers so a single trace can span gateway → matching engine → settlement/market data.

The standard approach is the W3C Trace Context propagation format injected into Kafka record headers by the producer and extracted by the consumer. Without this, you see three disconnected traces and cannot reconstruct a full order lifecycle.

**Key Spans per Trace**

| Span               | Service         | What to measure                           |
| ------------------ | --------------- | ----------------------------------------- |
| `order.receive`    | Gateway         | Client → gateway boundary                 |
| `order.risk_check` | Gateway         | Duration of risk validation               |
| `order.produce`    | Gateway         | Kafka produce latency                     |
| `order.consume`    | Matching Engine | Queue wait time (produce → consume delta) |
| `order.match`      | Matching Engine | Matching loop duration                    |
| `trade.produce`    | Matching Engine | Trade event Kafka produce                 |
| `trade.settle`     | Settlement      | DB write duration                         |
| `trade.broadcast`  | Market Data     | Subscriber fan-out duration               |

---

## Hard Engineering Problems

**Sequence number integrity across restarts**
The gateway assigns sequence numbers. If the gateway restarts, it must resume from the last issued sequence number — not reset to zero. Options: persist the counter in Redis/a database, or derive it from the Kafka partition offset (using the offset as the sequence number directly).

**Matching engine crash recovery**
On restart, the engine must rebuild the order book before it can process new orders. Pure Kafka replay is elegant but slow for instruments with high historical order volume. A snapshot-at-offset pattern (periodically serialize the order book state and record the Kafka offset it was taken at, then replay only the delta) reduces recovery time significantly.

**Exactly-once settlement**
Kafka's at-least-once delivery means the settlement service may receive the same trade event more than once (after a consumer restart). Without protection, this double-settles a trade. Solutions: transactional outbox pattern, idempotency keys on trade events checked against a seen-events store, or Kafka transactions with transactional producers and consumers.

**Clock synchronization for end-to-end latency**
Measuring order lifecycle latency across services running on different hosts requires synchronized clocks. NTP provides ~1ms accuracy, which is insufficient for microsecond-level analysis. PTP (IEEE 1588) provides sub-microsecond accuracy. In a Kubernetes environment, this is a host-level concern that affects the reliability of cross-service latency metrics.

**Snapshot delivery under concurrent book updates**
The market data service must deliver a consistent snapshot to new subscribers without pausing book updates for existing subscribers. A naïve mutex around the whole book serializes delivery. A copy-on-write approach or a version counter with readers waiting for a stable version are more scalable alternatives.

**Kafka partition rebalancing during engine upgrade**
Rolling a new version of the matching engine means briefly reassigning partitions. During reassignment, orders could be processed by the old instance or the new one. Without a handoff protocol, this risks processing orders out of sequence or missing orders during the transition window. A blue/green deployment per partition set is safer than a rolling update for this service specifically.

---

## Learning Resources

### Domain Knowledge

**Books**

- _Trading and Exchanges: Market Microstructure for Practitioners_ — Larry Harris
  The essential starting point. Explains order types, the limit order book, matching, and market structure in plain language without heavy mathematics. Read Chapters 1–8 before writing any code.

- _Inside the Black Box_ — Rishi Narang
  A readable overview of algorithmic trading systems. Useful for understanding why latency matters and how participants interact with exchanges.

**Exchange Technical Specifications** _(free, public)_

- [NASDAQ OUCH Protocol Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/TradingProducts/OUCH5.0.pdf) — NASDAQ's order entry protocol. A concrete, engineering-focused document on what an order submission looks like and what acknowledgements come back.
- [Coinbase Advanced Trade API Docs](https://docs.cdp.coinbase.com/advanced-trade/docs/ws-overview) — Crypto exchanges publish detailed documentation on order types, matching rules, and market data feeds. More accessible and modern than traditional exchange specs.
- [Kraken WebSocket API](https://docs.kraken.com/api/docs/websocket-v2/book) — Good reference for understanding L2 order book data format and snapshot + delta mechanics.

### Systems & Performance

**Articles & Papers**

- [LMAX Disruptor](https://lmax-exchange.github.io/disruptor/disruptor.html) — The design document behind LMAX's high-performance inter-thread messaging ring buffer. Directly relevant to the C++ matching engine's internal architecture.
- [Mechanical Sympathy Blog](https://mechanical-sympathy.blogspot.com/) — Martin Thompson's writing on low-latency systems, cache-aware data structures, and memory layout. Essential background for the C++ component.
- [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/) — A short, practical post specifically about order book data structure design.

### Infrastructure

**Kafka**

- _Kafka: The Definitive Guide_ (O'Reilly, free online) — Covers consumer groups, partitioning strategy, exactly-once semantics, and log compaction. Chapters 4, 6, and 8 are most relevant to this project.
- [Kafka Transactions Deep Dive](https://www.confluent.io/blog/transactions-apache-kafka/) — Confluent's detailed post on exactly-once semantics. Required reading before implementing settlement.

**OpenTelemetry**

- [OTel Specification: Context Propagation](https://opentelemetry.io/docs/concepts/context-propagation/) — Understand how trace context crosses process boundaries.
- [OTel Kafka Instrumentation](https://opentelemetry.io/docs/languages/go/instrumentation/) — Go-specific instrumentation guide; patterns apply to manual instrumentation in C++ and Rust as well.

**Kubernetes**

- [KEDA Documentation](https://keda.sh/docs/latest/scalers/apache-kafka/) — Specifically the Kafka scaler. Straightforward to configure once you understand consumer group lag as a scaling metric.
- [Agones](https://agones.dev/) — Not directly applicable, but an interesting reference for how stateful workloads are managed on Kubernetes.

### Open Source References

These are not templates to copy from — they are reference points for understanding real design decisions.

- [Coinbase matching engine blog post](https://www.coinbase.com/blog/how-we-scaled-data-access-with-visor) — Engineering blog on scaling exchange infrastructure.
- [Jasper](https://github.com/fmstephe/matching_engine) — A simple Go matching engine. Useful to read early on to see the matching loop pattern, before implementing your own in C++.
- [Rithmic / Trading Technologies API docs](https://api.trading.rithmic.com/) — More advanced, but shows what a production-grade order entry protocol actually looks like.

---

## Implementation Roadmap

A suggested order that builds confidence incrementally and defers integration complexity until the core logic is solid.

### Phase 1 — Core Matching Engine in Isolation (C++)

Build the order book and matching loop as a standalone library with no Kafka, no network, no other services. Write it against a harness that feeds orders in and asserts on the trade events that come out.

Milestones:

- Limit order insertion and cancellation
- Market order matching
- IOC and FOK semantics
- Partial fill handling
- An order generator that produces a realistic stream (random walk price, Poisson order arrival)

This phase is done when the engine is deterministic and well-tested. All subsequent phases build on this foundation.

### Phase 2 — Kafka Integration for the Engine (C++)

Wrap the matching engine with a Kafka consumer (orders in) and producer (trades and book updates out). No gateway yet — publish test orders directly to Kafka with a CLI tool or script.

Milestones:

- Consume from `orders-{symbol}` in partition order
- Produce trade events to `trades-{symbol}`
- Produce book update events to `book-updates-{symbol}`
- Verify deterministic output by replaying the same input sequence twice

### Phase 3 — Order Gateway (Go)

Build the inbound service: accept orders over WebSocket, assign sequence numbers, perform basic risk checks, publish to Kafka.

Milestones:

- WebSocket server accepting orders in a simple JSON schema
- Sequence number assignment (using Kafka offset as sequence number is the simplest correct approach)
- Per-client rate limiting
- End-to-end test: order in via WebSocket → trade event visible in Kafka

### Phase 4 — Market Data Service (Rust)

Build the book mirror and subscriber broadcast system.

Milestones:

- Consume from `book-updates-{symbol}` and maintain book mirror
- Serve L1 (best bid/ask) over WebSocket
- Implement snapshot + delta delivery for new subscribers
- Serve L2 (full depth) on demand

### Phase 5 — Settlement Service (Go)

Build trade processing and position tracking.

Milestones:

- Consume from `trades-{symbol}`
- Persist trade records and position updates transactionally
- Idempotent processing (handle duplicate delivery safely)

### Phase 6 — Observability

Add Prometheus metrics, OTEL tracing, and structured logging across all services. This phase should not be deferred entirely — instrument as you go, but dedicate a phase to auditing coverage and configuring dashboards.

Milestones:

- Prometheus endpoints on all services with the metrics described above
- Microsecond-scale histogram buckets on the matching engine
- OTEL trace context propagated through Kafka headers
- A full order lifecycle visible as a single trace in Jaeger or Tempo
- KEDA configured for settlement and market data based on consumer lag

### Phase 7 — Kubernetes Deployment

Containerize all services and write Kubernetes manifests.

Milestones:

- Dockerfiles for each service (multi-stage builds for C++ and Rust)
- StatefulSet for the matching engine with stable pod identity
- Deployments for gateway, market data, and settlement
- KEDA ScaledObjects for the consumer-lag-based autoscaling
- Helm chart or Kustomize overlays for environment configuration
- Local development via kind or minikube
