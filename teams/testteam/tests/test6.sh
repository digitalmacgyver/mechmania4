#!/bin/bash
# Test 6: Client Messaging Interface Testing
# Tests SetMessage, AppendMessage, and ClearMessage with various scenarios

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

cd "$PROJECT_ROOT"

# Activate virtual environment
source scripts/venv/bin/activate

echo "Running Test 6: Client Messaging Interface (15 turns)..."
echo "Command: python3 scripts/test_collision_modes.py --team1 noop --team2 testteam --test-file teams/testteam/tests/test6_test_client_messages.txt --max-turns 15"
echo ""

# Run test and capture output
TEMP_LOG=$(mktemp)
python3 scripts/test_collision_modes.py \
    --team1 noop \
    --team2 testteam \
    --test-file teams/testteam/tests/test6_test_client_messages.txt \
    --max-turns 15 \
    > "$TEMP_LOG" 2>&1

# Extract log file paths from Python output
TESTTEAM_LOG=$(grep "TestTeam log:.*New" "$TEMP_LOG" | sed 's/.*TestTeam log: //')

echo "=========================================="
echo "Test 6: Client Messaging Interface"
echo "=========================================="

# Print log file path
echo ""
echo "TestTeam Log File:"
echo "  $TESTTEAM_LOG"

# Check for test results
echo ""
echo "Checking test results..."

if [ ! -f "$TESTTEAM_LOG" ]; then
    echo "  ✗ Log file not found: $TESTTEAM_LOG"
    rm -f "$TEMP_LOG"
    exit 1
fi

# Count successful tests
TESTS_RUN=$(grep "\[MSG-TEST\] Testing" "$TESTTEAM_LOG" | wc -l)
TESTS_COMPLETE=$(grep "\[MSG-TEST\] Test.*complete" "$TESTTEAM_LOG" | wc -l)

echo "  Tests executed: $TESTS_RUN"
echo "  Tests completed: $TESTS_COMPLETE"

# Check for specific test results
echo ""
echo "Specific Test Results:"

# Test 1: Basic SET
if grep -q "Testing BASIC_SET" "$TESTTEAM_LOG" && grep -q "MSG_SUCCESS" "$TESTTEAM_LOG"; then
    echo "  ✓ BASIC_SET passed"
else
    echo "  ✗ BASIC_SET failed"
fi

# Test 10: Long message truncation
if grep -q "Testing LONG_MESSAGE_TRUNCATE" "$TESTTEAM_LOG" && grep -q "MSG_TRUNCATED" "$TESTTEAM_LOG"; then
    echo "  ✓ LONG_MESSAGE_TRUNCATE passed (correctly detected truncation)"
else
    echo "  ✗ LONG_MESSAGE_TRUNCATE failed"
fi

# Test 11: Append to full buffer
if grep -q "Testing APPEND_TO_FULL_BUFFER" "$TESTTEAM_LOG" && grep -q "MSG_NO_SPACE" "$TESTTEAM_LOG"; then
    echo "  ✓ APPEND_TO_FULL_BUFFER passed (correctly detected no space)"
else
    echo "  ✗ APPEND_TO_FULL_BUFFER failed"
fi

# Test 6: Clear message
if grep -q "Testing CLEAR_TEST" "$TESTTEAM_LOG" && grep -q "Buffer cleared successfully" "$TESTTEAM_LOG"; then
    echo "  ✓ CLEAR_TEST passed"
else
    echo "  ✗ CLEAR_TEST failed"
fi

echo ""
echo "=========================================="

# Check if all tests completed
if [ "$TESTS_RUN" -eq "$TESTS_COMPLETE" ] && [ "$TESTS_RUN" -gt 0 ]; then
    echo "Result: PASS - All $TESTS_COMPLETE message tests completed successfully"
    echo ""
    echo "Full test output available in: $TESTTEAM_LOG"
    rm -f "$TEMP_LOG"
    exit 0
else
    echo "Result: FAIL - Some tests did not complete ($TESTS_COMPLETE/$TESTS_RUN)"
    echo ""
    echo "Full test output available in: $TESTTEAM_LOG"
    rm -f "$TEMP_LOG"
    exit 1
fi
