#!/usr/bin/env bash
# measure_reflection_compile.sh — Reproducible compile-time measurements
#
# Usage:
#   ./tools/measure_reflection_compile.sh [target...]
#
# Measures clean rebuild time for each target using build-reflection.
# Outputs a markdown table with wall time and -ftime-trace breakdown.
#
# Prerequisites:
#   - nix develop .#reflection
#   - cmake --preset clang-p2996  (first-time configure)

set -euo pipefail

BUILD_DIR="build-reflection"
TARGETS=(
  "symbolic4_core_test"
  "symbolic4_compressed_test"
  "symbolic4_eval_test"
  "symbolic4_wrappers_test"
  "symbolic4_model_test"
)

# Override targets if provided on command line
if [[ $# -gt 0 ]]; then
  TARGETS=("$@")
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Error: $BUILD_DIR not found. Run: cmake --preset clang-p2996"
  exit 1
fi

echo "| Target | Wall Time (s) | Frontend (ms) | Instantiation (ms) | Codegen (ms) |"
echo "|--------|---------------|---------------|---------------------|--------------|"

for target in "${TARGETS[@]}"; do
  # Clean object files for this target
  obj_dir="$BUILD_DIR/src/symbolic4/CMakeFiles/${target}.dir"
  if [[ -d "$obj_dir" ]]; then
    rm -f "$obj_dir"/*.o "$obj_dir"/*.json
  fi

  # Build and measure wall time
  start_time=$(date +%s%N)
  cmake --build "$BUILD_DIR" --target "$target" 2>&1 | tail -1
  end_time=$(date +%s%N)
  wall_ms=$(( (end_time - start_time) / 1000000 ))
  wall_s=$(echo "scale=1; $wall_ms / 1000" | bc)

  # Parse -ftime-trace JSON if available
  trace_file=$(find "$obj_dir" -name "*.json" 2>/dev/null | head -1)
  if [[ -n "$trace_file" && -f "$trace_file" ]]; then
    # Extract phase durations from Chrome trace format
    frontend_ms=$(jq '[.traceEvents[] | select(.name == "Frontend") | .dur // 0] | add // 0 | . / 1000 | floor' "$trace_file" 2>/dev/null || echo "—")
    instantiation_ms=$(jq '[.traceEvents[] | select(.name == "InstantiateFunction" or .name == "InstantiateClass") | .dur // 0] | add // 0 | . / 1000 | floor' "$trace_file" 2>/dev/null || echo "—")
    codegen_ms=$(jq '[.traceEvents[] | select(.name == "CodeGen Function" or .name == "OptFunction") | .dur // 0] | add // 0 | . / 1000 | floor' "$trace_file" 2>/dev/null || echo "—")
  else
    frontend_ms="—"
    instantiation_ms="—"
    codegen_ms="—"
  fi

  echo "| $target | $wall_s | $frontend_ms | $instantiation_ms | $codegen_ms |"
done
