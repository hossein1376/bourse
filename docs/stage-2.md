# Phase 2 — Kafka Integration for the Engine

## Goal

Wrap the matching engine with a Kafka consumer (orders in) and producer (trades and book updates out). No gateway yet — publish test orders directly to Kafka with a CLI tool or script.

## Architecture

```
                   ┌──────────────┐
                   │  Generator / │
                   │   CLI tool   │
                   └──────┬───────┘
                          │  orders-eq (FlatBuffer)
                          ▼
              ┌─────────────────────┐
              │                     │
              │    Consumer         │
              │  (rdkafka::Consumer)│
              └──────────┬──────────┘
                         │ Order deserialized
                         ▼
              ┌─────────────────────┐
              │                     │
              │   MatchingEngine    │
              │  process_order()    │
              │                     │
              └────┬──────────┬─────┘
                   │          │
        on_book_   │          │  on_trade_
                   │          │
                   ▼          ▼
        ┌──────────────┐ ┌──────────────┐
        │  Producer    │ │  Producer    │
        │ book-updates │ │ trades-eq    │
        │ (L2 depth)   │ │ (FlatBuffer) │
        └──────────────┘ └──────────────┘
```

## Infrastructure

### RedPanda

RedPanda replaces Kafka + Zookeeper with a single binary. It's Kafka API-compatible — the same `librdkafka` client connects without changes.

**`docker-compose.yml`:**

```yaml
version: "3"
services:
  redpanda:
    image: redpandadata/redpanda:latest
    ports:
      - "9092:9092"
    command:
      - redpanda
      - start
      - --smp 1
      - --memory 512M
      - --reserve-memory 0M
      - --node-id 0
      - --kafka-addr PLAINTEXT://0.0.0.0:29092,OUTSIDE://0.0.0.0:9092
      - --advertise-kafka-addr PLAINTEXT://redpanda:29092,OUTSIDE://localhost:9092
```

Start with: `docker compose up -d`

### Topics (auto-created on first use)

| Topic             | Partition key      | Produced by     | Consumed by             | Schema                                                  |
| ----------------- | ------------------ | --------------- | ----------------------- | ------------------------------------------------------- |
| `orders-eq`       | (single partition) | Generator / CLI | `kafka_engine`          | `Order.fbs`                                             |
| `trades-eq`       | (single partition) | `kafka_engine`  | Settlement, Market Data | `TradeEvent.fbs`                                        |
| `book-updates-eq` | (single partition) | `kafka_engine`  | Market Data             | `BookUpdate.fbs` (L2 depth, default top 10 levels/side) |

Single partition is sufficient for one engine instance. Multi-partition (by symbol) is deferred until multiple symbols/instances are needed.

### Consumer group

The engine uses **static partition assignment** (`assign()` not `subscribe()`), matching PLAN.md §278. No automatic rebalancing — the engine owns its partition and must handle restarts via offset commits.

## FlatBuffers Schemas

`cxx/engine/tools/kafka_engine/schema/`:

```fbs
// order.fbs
namespace engine;

table Order {
  order_id: uint64;
  side: uint8;       // 0=Buy, 1=Sell
  type: uint8;        // 0=Limit, 1=Market, 2=IOC, 3=FOK
  price: uint64;
  quantity: uint64;
}

// trade_event.fbs
namespace engine;

table TradeEvent {
  taker_order_id: uint64;
  maker_order_id: uint64;
  taker_side: uint8;
  price: uint64;
  quantity: uint64;
  sequence: uint64;
}

// book_update.fbs
namespace engine;

table Level {
  price: uint64;
  quantity: uint64;
}

table BookUpdate {
  bid_levels: [Level];
  ask_levels: [Level];
}
```

The `flatc` compiler generates C++ headers during the CMake build.

## Dependencies

| Library       | Integration                | Purpose                                      |
| ------------- | -------------------------- | -------------------------------------------- |
| `librdkafka`  | `FetchContent` from GitHub | Kafka protocol client (C, with C++ bindings) |
| `flatbuffers` | `FetchContent` from GitHub | Serialization + `flatc` schema compiler      |

Both are fetched at configure time. No system packages required.

## New Executable: `kafka_engine`

`cxx/engine/tools/kafka_engine/`:

| File                     | Responsibility                                                              |
| ------------------------ | --------------------------------------------------------------------------- |
| `schema/order.fbs`       | Order FlatBuffer schema                                                     |
| `schema/trade_event.fbs` | Trade event FlatBuffer schema                                               |
| `schema/book_update.fbs` | Book update (L1) FlatBuffer schema                                          |
| `main.cpp`               | Parse CLI args, wire consumer → engine → producer, run loop                 |
| `consumer.cpp`/`.hpp`    | Poll RedPanda, deserialize FlatBuffer → `Order`, forward to `process_order` |
| `producer.cpp`/`.hpp`    | Serialize `TradeEvent`/`BookUpdate` → FlatBuffer, produce to RedPanda       |
| `CMakeLists.txt`         | Fetch FlatBuffers + librdkafka, compile schemas, link target                |

**Consumer loop:**

```
for each message in partition:
    order = flatbuffers::GetRoot<Order>(msg->payload())
    engine.process_order(order)

    // callbacks automatically produce:
    //   - TradeEvent → trades-eq (via on_trade_)
    //   - BookUpdate → book-updates-eq (via on_book_)

    consumer.commit(msg)
```

## Engine Changes

Minimal — add a book update callback:

`matching_engine.hpp`:

```cpp
using BookCallback = std::function<void(const OrderBook &)>;

explicit MatchingEngine(TradeCallback on_trade,
                        BookCallback on_book = nullptr);
```

`matching_engine.cpp`:

- Store `BookCallback on_book_` alongside `on_trade_`
- At the end of `process_order` (after `match_against_book`), call `on_book_(book_)` if set

This gives the `kafka_engine` permission to inspect the book and produce L2 updates after every processed order. The callback receives the full `OrderBook` — it walks price levels via `for_each_level` and serializes both sides.

The L2 depth is configurable (default: top 10 levels per side) to keep message sizes bounded.

## Replay Verification

`replay_test.cpp` in the engine test suite:

1. Process a sequence of N orders, recording all `TradeEvent`s
2. Reset the engine
3. Process the same sequence again
4. Assert both event sequences are identical (same events, same order, same sequence numbers)

This verifies determinism — the PLAN's Phase 2 milestone. It doesn't need Kafka; it tests that the engine produces identical output for identical input.

A secondary integration test runs against RedPanda:

1. Publish orders to RedPanda via the generator
2. Run `kafka_engine` to consume and produce
3. Consume trades from RedPanda and verify they match expected output

## Implementation Order

| Step | What                                     | Files                                         |
| ---- | ---------------------------------------- | --------------------------------------------- |
| 1    | `docker-compose.yml` with RedPanda       | `.`                                           |
| 2    | FlatBuffers schemas + CMake fetch        | `schema/*.fbs`, `kafka_engine/CMakeLists.txt` |
| 3    | Engine: add `BookCallback`               | `matching_engine.hpp`, `matching_engine.cpp`  |
| 4    | `consumer.cpp` — poll + deserialize      | `kafka_engine/`                               |
| 5    | `producer.cpp` — serialize + produce     | `kafka_engine/`                               |
| 6    | `main.cpp` — wire everything             | `kafka_engine/`                               |
| 7    | `replay_test.cpp` — deterministic replay | `engine/tests/`                               |
| 8    | Integration test against RedPanda        | `engine/tests/`                               |

## Out of Scope (Phase 2)

- Multiple symbols — single symbol `eq` only. Multi-symbol requires partition assignment strategy.
- WebSocket gateway — that's Phase 3 (Go service).
- Crash recovery / snapshotting — Kafka offset commits are the checkpoint. Full recovery design is a "Hard Engineering Problem" from PLAN.md.
