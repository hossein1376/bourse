# Matching Engine Benchmark

Measured on Apple Silicon M4 Pro, 24GB RAM, macOS. Compiler: Apple Clang 16.

Engine built with `-O2 -Wall -Wextra -Wpedantic`. Generator runs 100,000 orders unless noted. No CSV output unless specified.

## Results

| Workload                  | Wall       | Orders/sec | Notes                                |
| ------------------------- | ---------- | ---------- | ------------------------------------ |
| Default mix (60/20/10/10) | 79.7 ms    | **1.25M**  | Realistic order type distribution    |
| 100% Limit                | 83.4 ms    | 1.20M      | Mostly no-cross, orders rest in book |
| 100% Market               | 67.8 ms    | **1.47M**  | All orders hit book, fast matching   |
| 100% FOK                  | 70.0 ms    | 1.43M      | All rejected, `can_fully_fill` path  |
| Wide spread (100)         | 111.7 ms   | 0.90M      | Fewer crosses, more map insertions   |
| Tight spread (2)          | 78.2 ms    | 1.28M      | More crossing, less resting          |
| Deep book (2000 spread)   | 86.6 ms    | 1.15M      | 500+ price levels, O(log n) scales   |
| With CSV output           | 102.0 ms   | 0.98M      | fstream formatting + I/O overhead    |
| **1M orders** (default)   | **0.79 s** | **1.28M**  | Linear scaling confirmed             |

## Analysis

### CPU-bound, no I/O wait

User time ≈ wall time in all runs. The engine is purely compute-bound.

### Orders/sec breakdown

```
Default:   ████████████████████████░░░░░░  1.25M
Market:    ██████████████████████████░░░░  1.47M  (fastest)
FOK:       █████████████████████████░░░░░  1.43M
Wide:      ██████████████████░░░░░░░░░░░░  0.90M  (slowest)
```

### Spread effect

- **Wide spread (100):** 40% slower than default. Fewer orders cross → more rest in the book. Unique prices create new map entries → O(log n) insertions.
- **Tight spread (2):** Orders cluster at same prices → `try_emplace` hits existing keys → just appends to the list.

### Book depth scales

`std::map` with 10 vs 500+ price levels adds negligible overhead (<8%). O(log n) with a small constant.

### CSV I/O

Writing 80k trades to file adds ~28% overhead. If throughput matters, batch writes or use binary format.

### Comparison to PLAN.md estimates

The PLAN targets microsecond-level matching latency. Current single-order latency:

```
Default mix: 79.7 ms / 100k orders = ~800 ns/order
Market only: 67.8 ms / 100k orders = ~680 ns/order
```

Well within microsecond territory. The linked-list optimization (PLAN §192) would improve cache behavior but the current `std::map` is already fast enough for realistic throughput.

## Reproduce

```sh
# Build
cmake -S . -B build && cmake --build build --target order_generator -j

# Run a benchmark
hyperfine './build/cxx/engine/tools/order_generator --orders 100000' --warmup 1

# With custom parameters
./run_generator.sh --orders 1000000 --seed 42 --output trades.csv
```

## Baseline

These numbers serve as a baseline for future optimization. Save this file and re-run after any engine changes to detect regressions.
