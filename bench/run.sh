#!/usr/bin/env bash
# Run a benchmark under conditions the harness can't fix itself: pin to one core (TSC
# and cache coherence), disable ASLR (layout reproducibility). Governor/turbo need root
# and are left to the machine — benchmark.h warns to stderr when they aren't ideal.
#
#   bench/run.sh build-gcc-trunk/src/matrix4/lu_decomposition_bench [args...]
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <benchmark-binary> [args...]" >&2
  exit 2
fi

bin=$1
shift

run=(taskset -c 0)            # pin to core 0
if command -v setarch >/dev/null 2>&1; then
  run=(setarch -R "${run[@]}")  # -R disables ASLR for reproducible layout
fi

exec "${run[@]}" "$bin" "$@"
