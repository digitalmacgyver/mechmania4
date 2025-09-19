#!/bin/bash

echo "=== MechMania IV System Test ==="
echo ""

# Function to check if process is running
check_process() {
    local name=$1
    if pgrep -x "$name" > /dev/null; then
        echo "✓ $name is running"
        return 0
    else
        echo "✗ $name is not running"
        return 1
    fi
}

# Check all components
echo "Checking components:"
check_process "mm4serv"
SERVER_OK=$?

check_process "mm4team"
TEAM_OK=$?

echo ""

# Check network connections
echo "Checking network connections:"
netstat -an | grep 2323 | grep ESTABLISHED > /dev/null
if [ $? -eq 0 ]; then
    echo "✓ Clients connected to server on port 2323"
    CONN_COUNT=$(netstat -an | grep 2323 | grep ESTABLISHED | wc -l)
    echo "  Active connections: $CONN_COUNT"
else
    echo "✗ No active connections on port 2323"
fi

echo ""

# Test observer connectivity (without graphics)
echo "Testing observer connectivity:"
echo "Note: Observer requires X11 display or SDL_VIDEODRIVER=dummy for headless mode"

# Check if X display is available
if [ -n "$DISPLAY" ]; then
    echo "✓ X11 display available: $DISPLAY"
    echo "  Observer should work with graphics"
else
    echo "! No X11 display available"
    echo "  To run observer in headless mode:"
    echo "  SDL_VIDEODRIVER=dummy ./mm4obs -p2323 -hlocalhost"
fi

echo ""
echo "=== System Status Summary ==="

if [ $SERVER_OK -eq 0 ] && [ $TEAM_OK -eq 0 ]; then
    echo "✓ Core system is operational"
    echo "  Server and team clients are running"
else
    echo "✗ System issues detected"
fi

echo ""
echo "To start the observer with graphics (requires X11):"
echo "  ./mm4obs -p2323 -hlocalhost -G"
echo ""
echo "To monitor without graphics:"
echo "  watch -n 1 'ps aux | grep mm4 | grep -v grep'"