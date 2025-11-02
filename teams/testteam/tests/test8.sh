#!/bin/bash
# Test 8: Legacy mode crash reproduction harness

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

cd "$PROJECT_ROOT"

source scripts/venv/bin/activate

CMD=(python3 scripts/test_collision_modes.py
     --team1 noop
     --team2 noop
     --legacy-mode
     --server-arg=--legacy-mode
     --max-turns 5)

echo "Running Test 8: Legacy mode regression (noop vs noop, 5 turns)"
echo "Command: ${CMD[*]}"
echo ""

TEMP_LOG=$(mktemp)
if ! "${CMD[@]}" >"$TEMP_LOG" 2>&1; then
  echo "Test driver returned non-zero status. Output:"
  cat "$TEMP_LOG"
  rm -f "$TEMP_LOG"
  exit 1
fi

LEGACY_LOG=$(grep "Server log:" "$TEMP_LOG" | head -n1 | sed 's/.*Server log: //')

echo "=========================================="
echo "Test 8: Legacy mode regression"
echo "=========================================="

if [ -n "$LEGACY_LOG" ] && [ -f "$LEGACY_LOG" ]; then
  echo "Legacy server log: $LEGACY_LOG"
else
  echo "Legacy server log path not found in driver output."
fi

echo ""
echo "Driver output:"
cat "$TEMP_LOG"

rm -f "$TEMP_LOG"
