#!/bin/bash
# Test 7: Launch Audio Trigger
# Expected: Observer logs manual audio diagnostics, launch events,
#           and launch effect playback when ships undock.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$PROJECT_ROOT"

source scripts/venv/bin/activate

echo "Running Test 7: Launch Audio Trigger (20 turns)..."
CMD=(python3 scripts/test_collision_modes.py \
    --team1 noop \
    --team2 testteam \
    --test-file teams/testteam/tests/test7_ship_launches.txt \
    --max-turns 20 \
    --observer-verbose \
    --observer-assets-root ..)

printf 'Command: %q ' "${CMD[@]}"
printf '\n\n'

TEMP_LOG=$(mktemp)
"${CMD[@]}" >"$TEMP_LOG" 2>&1 || true

LEGACY_LOG=$(grep "Server log:.*Legacy" "$TEMP_LOG" | sed 's/.*Server log: //')
NEW_LOG=$(grep "Server log:.*New" "$TEMP_LOG" | sed 's/.*Server log: //')
OBS_LOG=$(grep "Observer log:" "$TEMP_LOG" | tail -n1 | sed 's/.*Observer log: //')

echo "=========================================="
echo "Test 7: Launch Audio Trigger"
echo "=========================================="

if [[ -f "$OBS_LOG" ]]; then
    echo "Observer log: $OBS_LOG"
else
    echo "Observer log not found in output"
    cat "$TEMP_LOG"
    rm -f "$TEMP_LOG"
    exit 1
fi

PASS=1

LAUNCH_EVENTS=$(grep -c "launch event emitted" "$OBS_LOG" || true)
if [[ "$LAUNCH_EVENTS" -lt 2 ]]; then
    echo "✗ Expected launch events not found (count=$LAUNCH_EVENTS)"
    PASS=0
else
    echo "✓ Launch events logged: $LAUNCH_EVENTS"
fi

EFFECT_EVENTS=$(grep -c "effect playing event=.*launch.default" "$OBS_LOG" || true)
if [[ "$EFFECT_EVENTS" -lt 2 ]]; then
    echo "✗ Expected launch audio playback not logged (count=$EFFECT_EVENTS)"
    PASS=0
else
    echo "✓ Launch audio playback logged: $EFFECT_EVENTS"
fi

MANUAL_EVENTS=$(grep -c "effect playing event=manual.audio.ping" "$OBS_LOG" || true)
if [[ "$MANUAL_EVENTS" -lt 1 ]]; then
    echo "✗ Manual audio trigger did not log playback"
    PASS=0
else
    echo "✓ Manual audio trigger logged"
fi

echo "=========================================="
if [[ "$PASS" -eq 1 ]]; then
    echo "Result: PASS"
else
    echo "Result: FAIL"
fi

rm -f "$TEMP_LOG"
exit $((PASS==1 ? 0 : 1))
