#!/bin/bash

# Change to the build directory where executables are located
cd "$(dirname "$0")/../build" || exit 1

echo "Starting MechMania IV: Groogroo vs ChromeFunk"
echo "==========================================="
echo ""

# Start server
echo "Starting server on port 2323..."
./mm4serv -p2323 -T2 &
SERVER_PID=$!
sleep 2

# Start team 1 - Groogroo
echo "Connecting Team 1 (Groogroo - Red)..."
./mm4team_groogroo -p2323 -hlocalhost &
TEAM1_PID=$!
sleep 1

# Start team 2 - ChromeFunk
echo "Connecting Team 2 (Chrome Funkadelic - Blue)..."
./mm4team -p2323 -hlocalhost &
TEAM2_PID=$!
sleep 1

# Start observer
echo "Starting Observer with graphics..."
echo ""
echo "Controls:"
echo "  N - Toggle names"
echo "  V - Toggle velocity vectors"
echo "  SPACE - Cycle attractor"
echo "  Q or ESC - Quit"
echo ""
echo "Team 1: Groogroo (Red)"
echo "Team 2: Chrome Funkadelic (Blue)"
echo ""

./mm4obs -p2323 -hlocalhost -G

# When observer exits, clean up
echo ""
echo "Observer closed. Shutting down game..."
kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null
wait
echo "Game ended."