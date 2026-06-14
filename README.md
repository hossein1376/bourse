# Order Matching Engine

A simplified financial exchange backend built as a polyglot microservice system.
Each language earns its role based on the component's demands:

| Language | Service              | Responsibility                                                  |
| -------- | -------------------- | --------------------------------------------------------------- |
| C++      | Matching Engine      | Microsecond-latency order book, cache-sensitive data structures |
| Go       | Gateway + Settlement | Inbound orders, risk checks, Kafka produce, settlement          |
| Rust     | Market Data Service  | High-concurrency WebSocket broadcast, book mirror               |

## Architecture

```
Client → Gateway (Go) → Kafka → Matching Engine (C++) ─┬→ Kafka → Settlement (Go)
                                                       └→ Kafka → Market Data (Rust)
```

See [docs/PLAN.md](docs/PLAN.md) for the full data flow and Kafka topic design.

## Quick Start

**Prerequisites:** Go 1.26+, C++20 compiler, CMake 3.20+, Rust, Redpanda or Kafka.

```bash
# Start the message broker
docker compose up -d

# Build C++ matching engine and its dependencies (includes FlatBuffers, librdkafka)
cmake -B build && cmake --build build

# Build and run the Go gateway
make -C go generate  # requires flatc on PATH
go -C go build ./cmd/gateway
go -C go test -race ./internal/...

# Run C++ tests
cmake --build build --target engine_test && ctest --test-dir build
```

## Project Structure

| Path              | Contents                                                         |
| ----------------- | ---------------------------------------------------------------- |
| `cxx/engine/`     | C++ matching engine library, tools, and tests                    |
| `cxx/util/`       | Doubly-linked list used by the order book                        |
| `go/cmd/gateway/` | Go order gateway entry point                                     |
| `go/internal/`    | Gateway packages: order types, Kafka producer, WebSocket handler |
| `docs/PLAN.md`    | Full architecture, domain model, and 7-phase roadmap             |
| `scripts/`        | Python plotting scripts for order generator output               |
| `compose.yml`     | Redpanda (Kafka-compatible broker) for local development         |

## Development

- **C++ tests:** `ctest --test-dir build`
- **Go unit tests:** `go -C go test -race ./internal/...`
- **Go integration tests:** `go -C go test --tags=integration ./tests/...` (requires Redpanda)
- **FlatBuffers regeneration:** `make -C go generate`
- **Order generator:** `./scripts/run_generator.sh run` (synthetic market data)
