#!/bin/bash
# Test script for NoOp team - runs a quick match with two NoOp teams

echo "Starting test match with two NoOp teams..."

# Kill any existing processes
pkill -f mm4serv 2>/dev/null
pkill -f mm4team_noop 2>/dev/null
pkill -f mm4obs 2>/dev/null
sleep 1

# Start server
./mm4serv &
SERVER_PID=$!
echo "Server started (PID: $SERVER_PID)"
sleep 1

# Start two NoOp teams
./mm4team_noop -p2323 &
TEAM1_PID=$!
echo "NoOp Team 1 started (PID: $TEAM1_PID)"
sleep 0.5

./mm4team_noop -p2323 &
TEAM2_PID=$!
echo "NoOp Team 2 started (PID: $TEAM2_PID)"
sleep 0.5

# Start observer in headless mode for 5 seconds
echo "Running observer for 5 seconds..."
SDL_VIDEODRIVER=dummy timeout 5 ./mm4obs -p2323 --verbose

# Clean up
echo "Cleaning up..."
kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null

echo "Test complete!"