#!/bin/bash
# Run all collision tests in sequence

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -d "teams/testteam/tests" ]; then
  echo -e "${RED}Error: Must run from project root directory${NC}"
  exit 1
fi

# Check if build directory exists
if [ ! -d "build" ]; then
  echo -e "${RED}Error: Build directory not found. Run 'cmake .' first.${NC}"
  exit 1
fi

# Check if executables exist
if [ ! -f "build/mm4serv" ] || [ ! -f "build/mm4team_noop" ] || [ ! -f "build/mm4team_testteam" ]; then
  echo -e "${RED}Error: Executables not found. Run 'make' first.${NC}"
  exit 1
fi

echo -e "${GREEN}=== MechMania IV Collision Test Suite ===${NC}"
echo ""

# List of test files
TESTS=(
  "teams/testteam/tests/test1_ship_station_collision.txt"
  "teams/testteam/tests/test2_station_laser_collision.txt"
  "teams/testteam/tests/test3_ship_ship_collision.txt"
  "teams/testteam/tests/test4_ship_asteroid_collision.txt"
  "teams/testteam/tests/test5_ship_laser_collision.txt"
)

# Run each test
for TEST_FILE in "${TESTS[@]}"; do
  if [ ! -f "$TEST_FILE" ]; then
    echo -e "${YELLOW}Skipping $TEST_FILE (not found)${NC}"
    continue
  fi

  TEST_NAME=$(basename "$TEST_FILE" .txt)
  echo -e "${GREEN}Running: $TEST_NAME${NC}"
  echo "Test file: $TEST_FILE"
  echo ""

  # Start server in background
  cd build
  ./mm4serv -p2323 --verbose > "../test_output_${TEST_NAME}.log" 2>&1 &
  SERVER_PID=$!
  cd ..

  # Give server time to start
  sleep 0.5

  # Start team 1 (noop)
  cd build
  ./mm4team_noop -hlocalhost -p2323 > /dev/null 2>&1 &
  TEAM1_PID=$!
  cd ..

  # Give team 1 time to connect
  sleep 0.5

  # Start team 2 (testteam) with test file
  cd build
  ./mm4team_testteam -hlocalhost -p2323 --test-file "../$TEST_FILE" > /dev/null 2>&1 &
  TEAM2_PID=$!
  cd ..

  # Wait for game to complete (timeout after 60 seconds)
  TIMEOUT=60
  ELAPSED=0
  while kill -0 $SERVER_PID 2>/dev/null && [ $ELAPSED -lt $TIMEOUT ]; do
    sleep 1
    ELAPSED=$((ELAPSED + 1))
  done

  # Kill any remaining processes
  kill $SERVER_PID 2>/dev/null || true
  kill $TEAM1_PID 2>/dev/null || true
  kill $TEAM2_PID 2>/dev/null || true
  wait $SERVER_PID 2>/dev/null || true
  wait $TEAM1_PID 2>/dev/null || true
  wait $TEAM2_PID 2>/dev/null || true

  # Extract and display collision events
  echo -e "${YELLOW}Collision events:${NC}"
  grep "COLLISION_DETECTED\|LASER_COLLISION" "test_output_${TEST_NAME}.log" || echo "  (no collisions detected)"
  echo ""

  # Count collisions
  COLLISION_COUNT=$(grep -c "COLLISION_DETECTED\|LASER_COLLISION" "test_output_${TEST_NAME}.log" || echo "0")
  echo "Total collision events: $COLLISION_COUNT"
  echo ""
  echo "Full log saved to: test_output_${TEST_NAME}.log"
  echo -e "${GREEN}====================================${NC}"
  echo ""
done

echo -e "${GREEN}All tests complete!${NC}"
echo ""
echo "To view collision events from all tests:"
echo "  grep \"COLLISION_DETECTED\\|LASER_COLLISION\" test_output_*.log"
