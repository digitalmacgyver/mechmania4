# Quick Testing Guide

This directory contains test harnesses for quickly verifying engine functionality after making code changes.

## Setup

### 1. Create Python Virtual Environment

```bash
cd scripts/
python3 -m venv venv
```

### 2. Activate Virtual Environment

**Linux/macOS:**
```bash
source venv/bin/activate
```

**Windows:**
```bash
venv\Scripts\activate
```

### 3. Install Dependencies (if any)

```bash
pip install -r requirements.txt
```

Currently no external dependencies are required - all test scripts use Python standard library only.

## Running Tests

### Collision Handling Mode Tests

Tests both legacy and new collision handling implementations:

```bash
cd /home/viblio/coding_projects/mm_claude
source scripts/venv/bin/activate
python3 scripts/test_collision_modes.py
```

**Options:**
- `--team1 TEAM` - Choose team for player 1 (choices: groogroo, testteam, noop)
- `--team2 TEAM` - Choose team for player 2 (choices: groogroo, testteam, noop)

**Examples:**
```bash
# Default: groogroo vs groogroo
python3 scripts/test_collision_modes.py

# Test with noop teams (no orders issued)
python3 scripts/test_collision_modes.py --team1 noop --team2 noop

# Test with scripted testteam (reads from test_moves.txt)
python3 scripts/test_collision_modes.py --team1 testteam --team2 noop
```

**What it tests:**
- Legacy collision handling mode (`--legacy-collision-handling` flag)
- New collision handling mode (default behavior)
- Verifies both modes complete games without crashes

**Expected output:**
```
MechMania IV - Collision Handling Test Harness
============================================================

============================================================
Testing Legacy Collision Handling Mode
============================================================
...
✓ Legacy mode test PASSED

============================================================
Testing New Collision Handling Mode
============================================================
...
✓ New mode test PASSED

============================================================
Test Summary
============================================================
Legacy mode: ✓ PASSED
New mode:    ✓ PASSED
============================================================

✓ All tests passed!
```

## Test Teams

### groogroo
Full-featured AI team. Good for realistic game testing.
- **Use case**: General testing, performance testing
- **Behavior**: Actively collects vinyl, avoids collisions, returns to base

### noop
Minimal team that issues no orders. Ships remain docked or drift if launched.
- **Use case**: Isolation testing (test only one team's behavior)
- **Behavior**: No orders issued, ships idle at station

### testteam
Scripted team that reads moves from `test_moves.txt` file.
- **Use case**: Controlled scenario testing, specific collision setups
- **Behavior**: Executes pre-programmed orders on schedule
- **Configuration**: Edit `test_moves.txt` in project root

#### Using testteam

1. Create a `test_moves.txt` file in the project root:

```
# Format: shipnum,turn,ORDER_KIND,magnitude
# shipnum: 0-3 (which ship)
# turn: 1-based turn number
# ORDER_KIND: THRUST, TURN, JETTISON, or LASER
# magnitude: order magnitude

# Example: Ship 0 thrusts forward 100 units/sec on turn 1
0,1,THRUST,100.0

# Ship 1 turns 90 degrees (1.57 radians) on turn 2
1,2,TURN,1.57

# Ship 2 jettisons 5 tons on turn 3
2,3,JETTISON,5.0

# Ship 3 fires a 50-unit laser on turn 4
3,4,LASER,50.0
```

2. Run tests with testteam:

```bash
python3 scripts/test_collision_modes.py --team1 testteam --team2 noop
```

See `test_moves_example.txt` for a complete example.

## Test Configuration

Test scripts are configured to work with:
- **Build directory:** `build/` (relative to project root)
- **Game timeout:** 240 seconds (4 minutes) per test game
- **Executables:**
  - `build/mm4serv` - Server
  - `build/mm4obs` - Observer
  - `build/mm4team_groogroo` - Groogroo team AI
  - `build/mm4team_testteam` - Scripted test team
  - `build/mm4team_noop` - No-op team
  - `team/src/graphics.reg` - Graphics registry

## Troubleshooting

### "Missing required files" error
Run `cmake --build build` from the project root to compile executables.

### Tests timeout
- Default timeout is 240 seconds (4 minutes)
- Ensure teams are properly configured
- Check server/client logs for errors
- For noop teams: games may timeout if no vinyl is collected (expected behavior)

### Port conflicts
Tests use `find_free_port()` to avoid conflicts, but if you see "Address already in use" errors:
```bash
# Linux: Find and kill processes using port range
netstat -tlnp | grep -E '230[0-9]{2}'
kill <pid>
```

### testteam not executing moves
- Verify `test_moves.txt` exists in project root
- Check file format (shipnum,turn,ORDER_KIND,magnitude)
- Look for parse warnings in testteam output
- Ensure turn numbers are correct (1-based)

## CI/CD Integration

These tests can be integrated into CI pipelines:

```yaml
# Example GitHub Actions
- name: Run collision mode tests
  run: |
    python3 -m venv scripts/venv
    source scripts/venv/bin/activate
    python3 scripts/test_collision_modes.py
```

## Deactivating Virtual Environment

When done testing:
```bash
deactivate
```