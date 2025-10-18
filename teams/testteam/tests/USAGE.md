# TestTeam Usage Guide

## Command-Line Options

TestTeam now supports multiple ways to provide test moves:

### Option 1: Command-Line Flag (Recommended)
```bash
./mm4team_testteam -hlocalhost -p2323 --test-file path/to/test.txt
```

### Option 2: Standard Input
```bash
cat path/to/test.txt | ./mm4team_testteam -hlocalhost -p2323 --test-file -
```

### Option 3: Default File (Backward Compatible)
```bash
./mm4team_testteam -hlocalhost -p2323
# Looks for test_moves.txt in current directory
```

## Examples

### Running Test 1 (Ship-Station Collision)
```bash
# Start server
./mm4serv -p2323 --verbose &

# Start noop team
./mm4team_noop -hlocalhost -p2323 &

# Start testteam with test file
./mm4team_testteam -hlocalhost -p2323 \
  --test-file ../teams/testteam/tests/test1_ship_station_collision.txt
```

### Using stdin with pipe
```bash
cat ../teams/testteam/tests/test2_station_laser_collision.txt | \
  ./mm4team_testteam -hlocalhost -p2323 --test-file -
```

### Using here-document (inline test)
```bash
./mm4team_testteam -hlocalhost -p2323 --test-file - <<EOF
# Inline test: ship rotates and thrusts
0,1,TURN,1.57
0,2,THRUST,30
EOF
```

## Test File Format

```
# Comments start with #
# Format: shipnum,turn,ORDER_KIND,magnitude

# Ship 0, Turn 1: Rotate 90 degrees (π/2 radians)
0,1,TURN,1.57

# Ship 0, Turn 2: Thrust forward at 30 units/sec
0,2,THRUST,30

# Ship 1, Turn 5: Fire laser with power 100
1,5,LASER,100
```

### Order Types

| Name | Code | Description |
|------|------|-------------|
| THRUST | 0 | Linear thrust (units/sec) |
| TURN | 1 | Rotation (radians) |
| JETTISON | 2 | Drop cargo |
| LASER | 3 | Fire laser beam |

You can use either the name or the numeric code.

## Error Handling

TestTeam will:
- Skip empty lines and comments
- Skip malformed lines with warnings
- Skip orders for invalid ship numbers
- Continue execution even if some orders fail

Example warning output:
```
[TestTeam] Warning: Line 5 has 3 fields (expected 4), skipping: 0,1,TURN
[TestTeam] Warning: Invalid ship number 5 (skipping)
[TestTeam] Loaded 10 test moves from test1.txt
```

## Debugging

Enable verbose output to see order execution:
```bash
./mm4serv -p2323 --verbose
```

Output will show:
```
[TestTeam] Turn 1: Ship 0 (Test-1) executing TURN 1.57
[TestTeam] Turn 2: Ship 0 (Test-1) executing THRUST 30.00
COLLISION_DETECTED: Test-1[SHIP] pos=(100,50) ... <-> Station[STATION] ...
```

## Integration with Test Suite

The automated test runner uses `--test-file`:
```bash
./teams/testteam/tests/run_all_tests.sh
```

This runs all 5 collision tests and saves output to `test_output_*.log` files.
