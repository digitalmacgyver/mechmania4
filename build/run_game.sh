#!/bin/bash

echo "Starting MechMania IV: The Vinyl Frontier"
echo "========================================="
echo ""

# Check for groogroo option
TEAM2_EXEC="./mm4team"
TEAM2_NAME="ChromeFunk"
if [ "$1" = "groogroo" ]; then
    echo "Using Groogroo team logic for Team 2"
    TEAM2_EXEC="./mm4team_groogroo"
    TEAM2_NAME="Groogroo"
fi

# Start server
echo "Starting server on port 2323..."
./mm4serv -p2323 -T2 &
SERVER_PID=$!
sleep 2

# Start team 1
echo "Connecting Team 1 (ChromeFunk)..."
./mm4team -p2323 -hlocalhost &
TEAM1_PID=$!
sleep 1

# Start team 2
echo "Connecting Team 2 ($TEAM2_NAME)..."
$TEAM2_EXEC -p2323 -hlocalhost &
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
echo "Team 1: ChromeFunk (Red)"
echo "Team 2: $TEAM2_NAME (Blue)"
echo ""

./mm4obs -p2323 -hlocalhost -G

# When observer exits, clean up
echo ""
echo "Observer closed. Shutting down game..."
kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null
wait
echo "Game ended."