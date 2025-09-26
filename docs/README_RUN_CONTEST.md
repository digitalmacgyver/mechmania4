# MechMania IV - Contest/Match Execution Guide

## Quick Start - Single Match

For a single match with visual observation:

```bash
# Terminal 1: Start the observer (with sprites)
cd build
./mm4obs -G -p2323 -hlocalhost

# Terminal 2: Start the server
cd build
./mm4serv -p2323 -hlocalhost

# Terminal 3: Team 1
cd build
./mm4team -p2323 -hlocalhost

# Terminal 4: Team 2
cd build
./mm4team_vortex -p2323 -hlocalhost
```

## Tournament Mode - Multiple Matches

For running multiple consecutive matches (tournament/contest mode):

### 1. Start Observer in Restart Mode

```bash
cd build

# Restart mode with full graphics
./mm4obs -R -G -p2323 -hlocalhost

# Options explained:
#   -R: Restart/reconnect mode - auto-reconnects between matches
#   -G: Full graphics mode with sprites
#   -p: Port number (default 2323)
#   -h: Hostname (default localhost)
```

**Observer Features in Restart Mode:**
- Automatically reconnects when server restarts
- Press **Space** to show MechMania logo between matches
- Maintains display continuity throughout tournament
- Never freezes - always responsive to input

### 2. Run Matches

For each match in the tournament:

```bash
# Start server with custom settings
cd build
./mm4serv -p2323 -hlocalhost -n4 -c60

# Options:
#   -n: Number of ships per team (default 4, max 10)
#   -c: Starting credits for ship configuration (default 60)
```

### 3. Connect Teams

Teams can connect from anywhere on the network:

```bash
# Team 1 - Chrome Funkadelic (default)
cd build
./mm4team -p2323 -hlocalhost -n"Chrome Funkadelic"

# Team 2 - Groogroo
cd build
./mm4team_groogroo -p2323 -hlocalhost -n"Groogroo"

# Team 3 - Vortex
cd build
./mm4team_vortex -p2323 -hlocalhost -n"Team Vortex"

# Custom team with specific name
./mm4team -p2323 -hlocalhost -n"Your Team Name"
```

## Observer Display Modes

### Basic Geometry Mode
```bash
# Simple geometric shapes (better performance)
./mm4obs -p2323 -hlocalhost
```

### Full Sprite Mode
```bash
# Detailed sprites with animations
./mm4obs -G -p2323 -hlocalhost
```

### With Custom Graphics Registry (if not using default location)
```bash
# Specify graphics registry location
./mm4obs -G -ggraphics.reg -p2323 -hlocalhost
```

## Observer Keyboard Controls

| Key | Function | Useful For |
|-----|----------|------------|
| **G** | Toggle sprite/geometry mode | Switch between performance and quality |
| **N** | Toggle object names | Identify ships and stations |
| **V** | Toggle velocity vectors | See ship movements |
| **Space** | Cycle logo display (3 modes) | Show logo between matches |
| **ESC/Q** | Quit observer | Exit gracefully |

### Logo Display Modes (Space key):
- **Mode 0**: No logo (normal game view)
- **Mode 1**: Semi-transparent logo overlay
- **Mode 2**: Opaque logo (perfect for intermissions)

## Running a Full Tournament

### Setup Script Example

Create a `run_tournament.sh`:

```bash
#!/bin/bash

PORT=2323
HOST=localhost

echo "Starting MechMania IV Tournament"
echo "================================"

# Start observer in background with restart mode
echo "Starting Observer in restart mode..."
cd build
./mm4obs -R -G -p$PORT -h$HOST &
OBSERVER_PID=$!

echo "Observer started (PID: $OBSERVER_PID)"
echo "Press Space in observer window to show logo between matches"
echo ""

# Function to run a match
run_match() {
    TEAM1=$1
    TEAM2=$2
    echo "----------------------------------------"
    echo "Starting match: $TEAM1 vs $TEAM2"
    echo "----------------------------------------"

    # Start server
    ./mm4serv -p$PORT -h$HOST &
    SERVER_PID=$!
    sleep 2

    # Start teams
    ./$TEAM1 -p$PORT -h$HOST &
    TEAM1_PID=$!

    ./$TEAM2 -p$PORT -h$HOST &
    TEAM2_PID=$!

    # Run for match duration (e.g., 5 minutes)
    echo "Match running for 5 minutes..."
    sleep 300

    # Clean up
    kill $TEAM1_PID $TEAM2_PID $SERVER_PID 2>/dev/null

    echo "Match complete. Waiting 10 seconds before next match..."
    sleep 10
}

# Run tournament matches
run_match "mm4team" "mm4team_groogroo"
run_match "mm4team_vortex" "mm4team"
run_match "mm4team_groogroo" "mm4team_vortex"

echo ""
echo "Tournament complete!"
echo "Press Ctrl+C to close observer"

# Keep script running so observer stays open
wait $OBSERVER_PID
```

## Network Play

### Server on Public IP
```bash
# Server machine (public IP: 192.168.1.100)
./mm4serv -p2323 -h0.0.0.0

# Observer (can be anywhere)
./mm4obs -R -G -p2323 -h192.168.1.100

# Team machines
./mm4team -p2323 -h192.168.1.100
```

### Multiple Concurrent Matches
```bash
# Match 1 on port 2323
./mm4serv -p2323 -hlocalhost

# Match 2 on port 2324
./mm4serv -p2324 -hlocalhost

# Match 3 on port 2325
./mm4serv -p2325 -hlocalhost
```

## Tips for Contest Organization

### Before the Contest
1. Test all team executables work
2. Verify network connectivity if using multiple machines
3. Start observer in restart mode (-R) early
4. Have the logo displayed (Space key) while waiting

### During the Contest
1. Keep observer running continuously in -R mode
2. Show logo between matches for professional appearance
3. Use sprite mode (-G) for audience viewing
4. Toggle names (N) to help spectators identify teams

### Recording Matches
```bash
# Use screen recording software to capture observer window
# Or redirect server output to log file
./mm4serv -p2323 -hlocalhost > match_log.txt 2>&1
```

### Handling Disconnections
- Observer with -R flag auto-reconnects
- Teams may need manual restart if they crash
- Server must be restarted for each new match

## Troubleshooting

### Observer Issues

**"Failed to load sprites"**
```bash
# Ensure graphics files are accessible
cd build
ln -sf ../team/src/graphics.reg .
ln -sf ../team/src/gfx .
```

**Observer not updating**
- Make sure you built the latest version
- Observer now always updates even when disconnected

### Connection Issues

**"Connection refused"**
- Start server before teams
- Check firewall settings
- Verify correct port and hostname

**Teams not appearing**
- Maximum 2 teams per match
- Teams must connect within timeout period
- Check team executables are built correctly

### Performance

**Lag during matches**
- Reduce ships: `./mm4serv -n3` (3 ships per team)
- Disable sprites in observer (press G)
- Run without observer for testing

## Competition Rules Summary

- Ships collect vinyl from asteroids
- Deliver vinyl to home station for points
- Combat with lasers (requires uranium fuel)
- Match ends when time limit reached
- Highest score wins

See `CONTEST_RULES.md` for complete rules.

## Quick Reference Card

```
OBSERVER LAUNCH
===============
Basic:        ./mm4obs -p2323 -hlocalhost
Sprites:      ./mm4obs -G -p2323 -hlocalhost
Restart:      ./mm4obs -R -G -p2323 -hlocalhost
Custom GFX:   ./mm4obs -G -ggraphics.reg -p2323 -hlocalhost

SERVER LAUNCH
=============
Default:      ./mm4serv -p2323 -hlocalhost
Custom:       ./mm4serv -p2323 -hlocalhost -n5 -c80
Public:       ./mm4serv -p2323 -h0.0.0.0

TEAM LAUNCH
===========
Default AI:   ./mm4team -p2323 -hlocalhost
Groogroo:     ./mm4team_groogroo -p2323 -hlocalhost
Vortex:       ./mm4team_vortex -p2323 -hlocalhost
Named:        ./mm4team -p2323 -hlocalhost -n"Team Name"

KEY BINDINGS
============
G     - Graphics mode toggle
N     - Names toggle
V     - Velocity vectors
Space - Logo display (3 modes)
Q/ESC - Quit
```