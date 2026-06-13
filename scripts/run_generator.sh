#!/usr/bin/env bash
set -euo pipefail
if ! cmake --build build --target order_generator -j "$(nproc 2>/dev/null || echo 4)" >/dev/null 2>&1; then
  cmake --build build --target order_generator
  exit 1
fi
exec ./build/cxx/engine/tools/order_generator "$@"
