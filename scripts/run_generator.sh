#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

plot() {
  exec "$ROOT/.venv/bin/python3" "$ROOT/scripts/plot_book.py" "$@"
}

run() {
  BUILD=$(cmake --build "$ROOT/build" --target order_generator -j "$(nproc 2>/dev/null || echo 4)" 2>&1) || {
    echo "$BUILD" >&2
    exit 1
  }
  "$ROOT/build/cxx/engine/tools/order_generator" "$@"
}

case "${1:-}" in
  plot)
    shift
    plot "$@"
    ;;
  run)
    shift
    run "$@"
    ;;
  *)
    HAS_SNAPSHOT=0
    HAS_PLOT=0
    ARGS=()
    for arg in "$@"; do
      if [ "$arg" = "--plot" ]; then
        HAS_PLOT=1
      else
        ARGS+=("$arg")
        if [ "$arg" = "--snapshot" ]; then HAS_SNAPSHOT=1; fi
      fi
    done
    run ${ARGS[@]+"${ARGS[@]}"}
    if [ "$HAS_PLOT" -eq 1 ] || [ "$HAS_SNAPSHOT" -eq 1 ]; then
      SNAPSHOT_FILE="book_snapshots.csv"
      for i in "${!ARGS[@]}"; do
        if [ "${ARGS[$i]}" = "--snapshot-output" ] && [ $((i+1)) -lt "${#ARGS[@]}" ]; then
          SNAPSHOT_FILE="${ARGS[$((i+1))]}"
        fi
      done
      if [ -f "$SNAPSHOT_FILE" ]; then
        plot "$SNAPSHOT_FILE"
      fi
    fi
    ;;
esac
