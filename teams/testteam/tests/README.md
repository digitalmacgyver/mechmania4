# Collision Test Suite

This directory contains test scenarios for the MechMania IV collision system.

## Test Files

### Test 1: Ship-Station Collision (`test1_ship_station_collision.txt`)
**Purpose**: Test ship colliding back with its own station after launching

**Scenario**:
- Turn 1: Ship rotates to face away from station
- Turns 2-5: Ship "walks out" of station with small reverse thrust (-5)
  - In legacy mode, may see repeated docking events as `dDockDist` increments
- Turn 6: Ship thrusts forward (30) toward station
  - Should collide with station

**Expected Log Pattern**:
```
COLLISION_DETECTED: Ship*[SHIP] <-> Station*[STATION]
```

### Test 2: Station-Laser Collision (`test2_station_laser_collision.txt`)
**Purpose**: Test laser hitting station

**Scenario**:
- Turns 1-5: Same as Test 1 - ship moves away from station
- Turn 6: Ship fires laser at maximum range toward station

**Expected Log Pattern**:
```
LASER_COLLISION: Ship* -> Station*[STATION]
```

### Test 3: Ship-Ship Collision (`test3_ship_ship_collision.txt`)
**Purpose**: Test two ships colliding with each other

**Scenario**:
- Ship 0: Moves away from station, then returns (like Test 1)
- Ship 1: Rotates opposite direction and launches toward Ship 0
- Ships should collide around turns 6-10

**Expected Log Pattern**:
```
COLLISION_DETECTED: Ship*[SHIP] <-> Ship*[SHIP]
```

### Test 4: Ship-Asteroid Collision (`test4_ship_asteroid_collision.txt`)
**Purpose**: Test ship traversing the field to hit asteroids

**Scenario**:
- Ship slowly traverses back and forth between stations across 300 turns
- Should eventually hit an asteroid (probabilistic)
- Cycle repeats 3 times to increase probability

**Expected Log Pattern**:
```
COLLISION_DETECTED: Ship*[SHIP] <-> Asteroid*[ASTEROID]
```

**Note**: This test may fail by chance if no asteroids are in the path.

### Test 5: Ship-Laser Collision (`test5_ship_laser_collision.txt`)
**Purpose**: Test laser hitting a ship (Extra Credit)

**Scenario**:
- Ship 0: Positions itself away from station as a target
- Ship 1: Aims and fires laser at Ship 0
- Laser should hit Ship 0, causing damage

**Expected Log Pattern**:
```
LASER_COLLISION: Ship* -> Ship*[SHIP]
```

## Running Tests

### Using the Python Test Harness

From project root:
```bash
# Activate virtual environment
source scripts/venv/bin/activate

# Run a specific test
python3 scripts/test_collision_modes.py \
  --team1 noop \
  --team2 testteam \
  --test-file teams/testteam/tests/test1_ship_station_collision.txt \
  --verbose

# Run all tests in sequence (example script)
for test in teams/testteam/tests/test*.txt; do
  echo "Running $test..."
  python3 scripts/test_collision_modes.py \
    --team1 noop \
    --team2 testteam \
    --test-file "$test" \
    --verbose
done
```

### Manual Testing

From build directory:
```bash
# Build the executables
cmake .. && make

# Run server with verbose logging
./mm4serv -p2323 --verbose &

# Run test team
./mm4team_noop -hlocalhost -p2323 &
./mm4team_testteam -hlocalhost -p2323 \
  < ../teams/testteam/tests/test1_ship_station_collision.txt

# Watch for collision events
# Server output will show lines starting with:
#   COLLISION_DETECTED: ...
#   LASER_COLLISION: ...
```

## Filtering Output

To see only collision events:
```bash
./mm4serv -p2323 --verbose 2>&1 | grep "COLLISION_DETECTED\|LASER_COLLISION"
```

## Test Configuration

All tests assume:
- **Team 1**: noop (does nothing)
- **Team 2**: testteam (executes scripted orders)
- Initial ship orientation: 0.0 radians (facing east/right)
- Station at origin for Team 2

## Order Format

Test files use the format:
```
shipnum,turn,ORDER_KIND,magnitude
```

Where ORDER_KIND:
- `0` = THRUST
- `1` = TURN
- `2` = JETTISON
- `3` = LASER
- `4` = SHIELD

## Physics Notes

- Ship initial orientation: 0.0 (facing right/east)
- π radians = 180° (facing left/west)
- 3π/4 ≈ 2.356 radians ≈ 135° (facing northwest)
- -π/4 ≈ -0.785 radians ≈ -45° (facing southeast)
