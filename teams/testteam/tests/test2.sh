#!/bin/bash
# Test 2: Laser-Station Collision
# Expected: Ship fires laser that hits station

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

cd "$PROJECT_ROOT"

# Activate virtual environment
source scripts/venv/bin/activate

echo "Running Test 2: Laser-Station Collision (20 turns)..."
echo "Command: python3 scripts/test_collision_modes.py --team1 noop --team2 testteam --test-file teams/testteam/tests/test2_station_laser_collision.txt --max-turns 20"
echo ""

# Run test and capture output
TEMP_LOG=$(mktemp)
python3 scripts/test_collision_modes.py \
    --team1 noop \
    --team2 testteam \
    --test-file teams/testteam/tests/test2_station_laser_collision.txt \
    --max-turns 20 \
    > "$TEMP_LOG" 2>&1

# Extract log file paths from Python output
LEGACY_LOG=$(grep "Server log:.*Legacy" "$TEMP_LOG" | sed 's/.*Server log: //')
NEW_LOG=$(grep "Server log:.*New" "$TEMP_LOG" | sed 's/.*Server log: //')

echo "=========================================="
echo "Test 2: Laser-Station Collision"
echo "=========================================="

# Print log file paths
echo ""
echo "Log Files:"
echo "  Legacy: $LEGACY_LOG"
echo "  New:    $NEW_LOG"

# Check legacy mode
echo ""
echo "Legacy Mode:"
if [ -f "$LEGACY_LOG" ]; then
    # Look for laser collision with Test Station on turn 6
    LASER_COUNT=$(grep "LASER_COLLISION:" "$LEGACY_LOG" | grep "Test Station" | wc -l)
    if [ "$LASER_COUNT" -gt 0 ]; then
        echo "  ✓ Laser-station collision detected: $LASER_COUNT hits"
        LEGACY_PASS=1
    else
        echo "  ✗ No laser-station collision detected"
        LEGACY_PASS=0
    fi
else
    echo "  ✗ Log file not found: $LEGACY_LOG"
    LEGACY_PASS=0
fi

# Check new mode
echo ""
echo "New Mode:"
if [ -f "$NEW_LOG" ]; then
    LASER_COUNT=$(grep "LASER_COLLISION:" "$NEW_LOG" | grep "Test Station" | wc -l)
    if [ "$LASER_COUNT" -gt 0 ]; then
        echo "  ✓ Laser-station collision detected: $LASER_COUNT hits"
        NEW_PASS=1
    else
        echo "  ✗ No laser-station collision detected"
        NEW_PASS=0
    fi
else
    echo "  ✗ Log file not found: $NEW_LOG"
    NEW_PASS=0
fi

echo ""
echo "=========================================="
if [ "$LEGACY_PASS" -eq 1 ] && [ "$NEW_PASS" -eq 1 ]; then
    echo "Result: PASS"
    rm -f "$TEMP_LOG"
    exit 0
else
    echo "Result: FAIL"
    rm -f "$TEMP_LOG"
    exit 1
fi
