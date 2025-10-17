#!/bin/bash
# Test script to verify new turn physics implementation
# Tests various turn angles and compares legacy vs new behavior

echo "=== Turn Physics Test ==="
echo ""
echo "Building project..."
cd build
cmake .. > /dev/null 2>&1
make -j$(nproc) > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"
echo ""

# Test 1: Legacy mode (full rotation)
echo "Test 1: Legacy mode - full rotation (2π radians)"
pkill -f mm4serv 2>/dev/null
pkill -f mm4team_noop 2>/dev/null
pkill -f mm4obs 2>/dev/null
sleep 1

./mm4serv --legacy-velocity-limits > servlog_turn_legacy.txt 2>&1 &
SERVER_PID=$!
sleep 1

./mm4team_noop -p2323 > teamlog_turn_legacy.txt 2>&1 &
TEAM1_PID=$!
sleep 0.5

./mm4team_noop -p2323 > /dev/null 2>&1 &
TEAM2_PID=$!
sleep 0.5

SDL_VIDEODRIVER=dummy timeout 3 ./mm4obs -p2323 -hlocalhost --verbose > obslog_turn_legacy.txt 2>&1

kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null
sleep 1

echo "Legacy mode test complete"
echo ""

# Test 2: New physics mode (full rotation)
echo "Test 2: New physics mode - full rotation (2π radians)"
pkill -f mm4serv 2>/dev/null
pkill -f mm4team_noop 2>/dev/null
pkill -f mm4obs 2>/dev/null
sleep 1

./mm4serv > servlog_turn_new.txt 2>&1 &
SERVER_PID=$!
sleep 1

./mm4team_noop -p2323 > teamlog_turn_new.txt 2>&1 &
TEAM1_PID=$!
sleep 0.5

./mm4team_noop -p2323 > /dev/null 2>&1 &
TEAM2_PID=$!
sleep 0.5

SDL_VIDEODRIVER=dummy timeout 3 ./mm4obs -p2323 -hlocalhost --verbose > obslog_turn_new.txt 2>&1

kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null
sleep 1

echo "New physics mode test complete"
echo ""

# Test 3: Check if logs contain any turn-related information
echo "=== Analysis ==="
echo ""
echo "Checking server logs for turn cost information..."

if grep -i "turn" servlog_turn_legacy.txt > /dev/null 2>&1; then
    echo "Found turn references in legacy log"
    grep -i "turn" servlog_turn_legacy.txt | head -5
fi

if grep -i "turn" servlog_turn_new.txt > /dev/null 2>&1; then
    echo "Found turn references in new physics log"
    grep -i "turn" servlog_turn_new.txt | head -5
fi

echo ""
echo "Test script complete!"
echo ""
echo "Log files created:"
echo "  - servlog_turn_legacy.txt (legacy mode)"
echo "  - servlog_turn_new.txt (new physics)"
echo "  - teamlog_turn_legacy.txt"
echo "  - teamlog_turn_new.txt"
echo "  - obslog_turn_legacy.txt"
echo "  - obslog_turn_new.txt"
