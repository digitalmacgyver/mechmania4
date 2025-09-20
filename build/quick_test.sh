#!/bin/bash

echo "Running quick headless test: Vortex vs ChromeFunk"

# Start server
./mm4serv -p2626 -T2 > test_server.log 2>&1 &
SERVER_PID=$!
echo "Server started (PID: $SERVER_PID)"
sleep 3

# Start Vortex team
./mm4team_vortex -p2626 -hlocalhost > test_vortex.log 2>&1 &
VORTEX_PID=$!
echo "Vortex started (PID: $VORTEX_PID)"
sleep 2

# Start ChromeFunk team
./mm4team -p2626 -hlocalhost > test_chrome.log 2>&1 &
CHROME_PID=$!
echo "ChromeFunk started (PID: $CHROME_PID)"

# Run for 45 seconds
echo "Running match for 45 seconds..."
sleep 45

# Stop everything
echo "Stopping match..."
kill -TERM $SERVER_PID 2>/dev/null
sleep 1
kill $VORTEX_PID $CHROME_PID 2>/dev/null

# Check results
echo ""
echo "Match Results:"
echo "--------------"
grep "Team 1" test_server.log | tail -1
grep "Team 2" test_server.log | tail -1
echo ""
grep "delivered" test_server.log | tail -5