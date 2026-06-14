# Matching Engine Benchmark

Measured on Apple Silicon M4 Pro, 24GB RAM, macOS. Compiler: Apple Clang 16.

Engine built with `-O2 -Wall -Wextra -Wpedantic`. Generator runs 100,000 orders unless noted. No CSV output unless specified.

## Results

| Workload                  | std::map | Linked list | Delta       |
| ------------------------- | -------- | ----------- | ----------- |
| Default mix (60/20/10/10) | 79.7 ms  | **75.1 ms** | **-6%**     |
| 100% Limit                | 83.4 ms  | **77.4 ms** | **-7%**     |
| 100% Market               | 67.8 ms  | 70.2 ms     | +4%         |
| 100% FOK                  | 70.0 ms  | **67.5 ms** | **-4%**     |
| Wide spread (100)         | 111.7 ms | **89.9 ms** | **-19%** 🎉 |
| Tight spread (2)          | 78.2 ms  | 78.8 ms     | +1% (noise) |
| Deep book (2000 spread)   | 86.6 ms  | —           | N/A         |
| With CSV output           | 102.0 ms | —           | N/A         |

### Orders/sec (linked list)

| Workload            | Wall       | Orders/sec |
| ------------------- | ---------- | ---------- |
| Default mix         | 75.1 ms    | **1.33M**  |
| 100% Limit          | 77.4 ms    | 1.29M      |
| 100% Market         | 70.2 ms    | **1.42M**  |
| 100% FOK            | 67.5 ms    | 1.48M      |
| Wide spread (100)   | 89.9 ms    | 1.11M      |
| Tight spread (2)    | 78.8 ms    | 1.27M      |
| 1M orders (default) | **0.75 s** | **1.33M**  |

## Analysis

### CPU-bound, no I/O wait

User time ≈ wall time in all runs. The engine is purely compute-bound.

### std::map vs linked list

The custom `DoublyLinkedList<PriceLevel>` replaced `std::map<Price, PriceLevel>` with sorted pointer walk. Results matched predictions:

- **Insertion-heavy (wide spread):** Linked list wins by **19%** — O(n) walk beats O(log n) tree rebalancing when there are many unique price levels.
- **Matching-heavy (market):** Slight regression of **+4%** — pointer chasing in the linked list is marginally slower than compact map nodes.
- **Default mix:** **6% faster** overall.

The linked list was implemented as a learning exercise (PLAN §192) and performs as expected: measurable improvement for some workloads, negligible for others.

### Spread effect

- **Wide spread (100):** Fewer orders cross → more unique price levels inserted → linked list's O(n) walk sees less variance than map's O(log n) tree. **19% improvement over std::map**.
- **Tight spread (2):** Orders cluster at same prices → both implementations hit existing levels equally fast. +1% noise.

### CSV I/O

Writing 80k trades to file adds ~28% overhead over console output. If throughput matters, batch writes or use binary format.

### Comparison to PLAN.md estimates

The PLAN targets microsecond-level matching latency. Current single-order latency:

```
Default mix: 75.1 ms / 100k orders = ~750 ns/order
Market only: 70.2 ms / 100k orders = ~700 ns/order
```

Well within microsecond territory.

## Reproduce

```sh
# Build
cmake -S . -B build && cmake --build build --target order_generator -j

# Run a benchmark
hyperfine './build/cxx/engine/tools/order_generator --orders 100000' --warmup 1

# With custom parameters
./build/cxx/engine/tools/order_generator --orders 1000000 --seed 42 --output trades.csv
```

## Baseline

These numbers use the custom `DoublyLinkedList` for price levels. Previous baseline used `std::map`. Save this file and re-run after any engine changes to detect regressions.
