#!/bin/bash
# Test script for groonew team with verbose output

echo "Starting test match with groonew team (verbose)..."

# Kill any existing processes
pkill -f mm4serv 2>/dev/null
pkill -f mm4team 2>/dev/null
pkill -f mm4obs 2>/dev/null
sleep 1

# Start server
./mm4serv &
SERVER_PID=$!
echo "Server started (PID: $SERVER_PID)"
sleep 1

# Start groonew team with verbose flag
./mm4team_groonew -p2323 --verbose &
TEAM1_PID=$!
echo "Groonew Team started with verbose logging (PID: $TEAM1_PID)"
sleep 0.5

# Start NoOp team as opponent
./mm4team_noop -p2323 &
TEAM2_PID=$!
echo "NoOp Team started (PID: $TEAM2_PID)"
sleep 0.5

# Start observer in headless mode for 10 seconds to capture team decisions
echo "Running observer for 10 seconds to capture verbose output..."
SDL_VIDEODRIVER=dummy timeout 10 ./mm4obs -p2323 --verbose

# Clean up
echo "Cleaning up..."
kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null

echo "Test complete!"