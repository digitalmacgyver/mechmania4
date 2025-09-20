#!/bin/bash

echo "Starting MechMania IV: Vortex vs ChromeFunk"
echo "==========================================="
echo ""

# Start server (using port 2324 - in safe range 2323-2400)
echo "Starting server on port 2324..."
./mm4serv -p2324 -T2 &
SERVER_PID=$!
sleep 2

# Start team 1 - Vortex
echo "Connecting Team 1 (Vortex Squadron - Red)..."
./mm4team_vortex -p2324 -hlocalhost &
TEAM1_PID=$!
sleep 1

# Start team 2 - ChromeFunk
echo "Connecting Team 2 (Chrome Funkadelic - Blue)..."
./mm4team -p2324 -hlocalhost &
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
echo "Team 1: Vortex Squadron (Red)"
echo "Team 2: Chrome Funkadelic (Blue)"
echo ""
echo "Strategy: Zone control with 35/25 fuel/cargo balance"
echo ""

./mm4obs -p2324 -hlocalhost -G

# When observer exits, clean up
echo ""
echo "Observer closed. Shutting down game..."
kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null
wait
echo "Game ended."