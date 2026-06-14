#!/usr/bin/env python3
"""Plot order book snapshots from the matching engine generator.

Usage:
    python3 plot_book.py book_snapshots.csv
    python3 plot_book.py book_snapshots.csv --save plot.png
    python3 plot_book.py book_snapshots.csv --window 200
"""

import argparse
import sys

try:
    import matplotlib.pyplot as plt
    import pandas as pd
except ImportError as e:
    print(f"Missing dependency: {e.name}")
    print("Install with: pip install pandas matplotlib")
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Plot order book snapshots")
    parser.add_argument("file", help="Path to snapshot CSV")
    parser.add_argument("--save", "-s", help="Save plot to file instead of showing")
    parser.add_argument(
        "--window",
        "-w",
        type=int,
        default=0,
        help="Rolling window for smoothing (default: no smoothing)",
    )
    args = parser.parse_args()

    df = pd.read_csv(args.file)

    if df.empty:
        print("Empty snapshot file — nothing to plot")
        sys.exit(1)

    # Forward-fill missing values so lines stay continuous
    df.ffill(inplace=True)

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 8), sharex=True)

    x = df["order_num"]
    n = range(len(x))

    # Price chart
    ax1.plot(n, df["best_bid"], label="Best bid", color="green", alpha=0.7)
    ax1.plot(n, df["best_ask"], label="Best ask", color="red", alpha=0.7)
    if args.window > 0:
        ax1.plot(
            n,
            df["best_bid"].rolling(args.window).mean(),
            label=f"Bid ({args.window}-MA)",
            color="green",
            linestyle="--",
        )
        ax1.plot(
            n,
            df["best_ask"].rolling(args.window).mean(),
            label=f"Ask ({args.window}-MA)",
            color="red",
            linestyle="--",
        )
    ax1.set_ylabel("Price")
    ax1.legend()
    ax1.grid(True, axis="y", alpha=0.3)

    # Quantity chart
    ax2.bar(n, df["bid_qty"], label="Bid qty", color="green", alpha=0.5)
    ax2.bar(n, df["ask_qty"], label="Ask qty", color="red", alpha=0.5)
    ax2.set_ylabel("Quantity")
    ax2.legend()
    ax2.grid(True, axis="y", alpha=0.3)

    # Spread chart
    ax3.plot(n, df["spread"], label="Spread", color="purple", alpha=0.7)
    if args.window > 0:
        ax3.plot(
            n,
            df["spread"].rolling(args.window).mean(),
            label=f"Spread ({args.window}-MA)",
            color="purple",
            linestyle="--",
        )
    ax3.set_xlabel("Order number")
    ax3.set_ylabel("Spread")
    ax3.legend()
    ax3.grid(True, axis="y", alpha=0.3)

    step = max(1, len(x) // 9)
    tick_positions = n[::step]
    tick_labels = [f"{v:,}" for v in x.values[::step]]
    ax3.set_xticks(tick_positions)
    ax3.set_xticklabels(tick_labels, rotation=45, ha="right")

    plt.tight_layout()

    if args.save:
        plt.savefig(args.save, dpi=150)
        print(f"Saved to {args.save}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
