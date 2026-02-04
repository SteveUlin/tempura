#!/usr/bin/env bash
#
# Compilation timeout wrapper — prepended via CMAKE_CXX_COMPILER_LAUNCHER.
#
# Usage: timeout_compile.sh <timeout_secs> <compiler> [compiler_args...]
#
# Wraps the compiler invocation with GNU `timeout`. Files exceeding 50% of the
# timeout are logged as slow. Files killed by timeout get a clear diagnostic.
#
# Environment:
#   TEMPURA_SLOW_THRESHOLD  Override the slow-file warning threshold (seconds).
#                           Default: half of the timeout value.

set -euo pipefail

TIMEOUT_SECS="$1"
shift
COMPILER="$1"
shift

# Find the source file in the argument list (last .cpp/.cc/.c argument)
SOURCE_FILE=""
for arg in "$@"; do
  case "$arg" in
    *.cpp|*.cc|*.c|*.cxx) SOURCE_FILE="$arg" ;;
  esac
done

# Preprocessor-only or link-only invocations: skip timeout
if [[ -z "$SOURCE_FILE" ]]; then
  exec "$COMPILER" "$@"
fi

SLOW_THRESHOLD="${TEMPURA_SLOW_THRESHOLD:-$(( TIMEOUT_SECS / 2 ))}"

START=$(date +%s%N)

# Run compiler under timeout. --kill-after=10 sends SIGKILL if SIGTERM is ignored.
set +e
timeout --kill-after=10 "${TIMEOUT_SECS}" "$COMPILER" "$@"
EXIT_CODE=$?
set -e

END=$(date +%s%N)
ELAPSED_NS=$(( END - START ))
ELAPSED_S=$(( ELAPSED_NS / 1000000000 ))

BASENAME=$(basename "$SOURCE_FILE")

# timeout(1) returns 124 on SIGTERM kill, 137 on SIGKILL
if [[ $EXIT_CODE -eq 124 || $EXIT_CODE -eq 137 ]]; then
  echo "" >&2
  echo "=====================================================================" >&2
  echo "  COMPILE TIMEOUT: ${BASENAME}" >&2
  echo "  Killed after ${TIMEOUT_SECS}s — this file is too expensive." >&2
  echo "" >&2
  echo "  Suggestions:" >&2
  echo "    - Move template instantiations to a .cpp file" >&2
  echo "    - Reduce -ftemplate-depth if possible" >&2
  echo "    - Profile with -ftime-report to find the slow phase" >&2
  echo "    - Increase TEMPURA_COMPILE_TIMEOUT if this is expected" >&2
  echo "=====================================================================" >&2
  exit 1
fi

# Warn about slow (but successful) compilations
if [[ $ELAPSED_S -ge $SLOW_THRESHOLD && $SLOW_THRESHOLD -gt 0 ]]; then
  echo "⚠️  SLOW COMPILE: ${BASENAME} took ${ELAPSED_S}s (threshold: ${SLOW_THRESHOLD}s)" >&2
fi

exit $EXIT_CODE
