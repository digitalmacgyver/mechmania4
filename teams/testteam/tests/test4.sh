#!/bin/bash
# Test 4: Ship-Asteroid Collision
# Expected: Ship traversing field hits an asteroid

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

cd "$PROJECT_ROOT"

# Activate virtual environment
source scripts/venv/bin/activate

echo "Running Test 4: Ship-Asteroid Collision (300 turns - full game for random asteroids)..."
echo "Command: python3 scripts/test_collision_modes.py --team1 noop --team2 testteam --test-file teams/testteam/tests/test4_ship_asteroid_collision.txt --max-turns 300"
echo ""

# Run test and capture output
TEMP_LOG=$(mktemp)
python3 scripts/test_collision_modes.py \
    --team1 noop \
    --team2 testteam \
    --test-file teams/testteam/tests/test4_ship_asteroid_collision.txt \
    --max-turns 300 \
    > "$TEMP_LOG" 2>&1

# Extract log file paths from Python output
LEGACY_LOG=$(grep "Server log:.*Legacy" "$TEMP_LOG" | sed 's/.*Server log: //')
NEW_LOG=$(grep "Server log:.*New" "$TEMP_LOG" | sed 's/.*Server log: //')
LEGACY_TESTTEAM_LOG=$(grep "TestTeam log:.*Legacy" "$TEMP_LOG" | sed 's/.*TestTeam log: //')
NEW_TESTTEAM_LOG=$(grep "TestTeam log:.*New" "$TEMP_LOG" | sed 's/.*TestTeam log: //')

echo "=========================================="
echo "Test 4: Ship-Asteroid Collision"
echo "=========================================="

# Print log file paths
echo ""
echo "Server Log Files:"
echo "  Legacy: $LEGACY_LOG"
echo "  New:    $NEW_LOG"
echo ""
echo "TestTeam Log Files:"
echo "  Legacy: $LEGACY_TESTTEAM_LOG"
echo "  New:    $NEW_TESTTEAM_LOG"

# Check legacy mode
echo ""
echo "Legacy Mode:"
if [ -f "$LEGACY_LOG" ]; then
    # Look for Test-1 colliding with any asteroid
    COLLISION_COUNT=$(grep "COLLISION_DETECTED.*Test-1.*ASTEROID" "$LEGACY_LOG" | wc -l)
    if [ "$COLLISION_COUNT" -gt 0 ]; then
        echo "  ✓ Ship-asteroid collision detected: $COLLISION_COUNT collisions"
        LEGACY_PASS=1
    else
        echo "  ✗ No ship-asteroid collision detected"
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
    COLLISION_COUNT=$(grep "COLLISION_DETECTED.*Test-1.*ASTEROID" "$NEW_LOG" | wc -l)
    if [ "$COLLISION_COUNT" -gt 0 ]; then
        echo "  ✓ Ship-asteroid collision detected: $COLLISION_COUNT collisions"
        NEW_PASS=1
    else
        echo "  ✗ No ship-asteroid collision detected"
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
