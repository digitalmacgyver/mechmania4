# Quick Start: Running Collision Tests

## Automated Test Suite (Recommended)

From project root:
```bash
./teams/testteam/tests/run_all_tests.sh
```

This will:
1. Run all 5 collision tests sequentially
2. Save output to `test_output_*.log` files
3. Display collision events for each test
4. Show summary statistics

## Manual Test Execution

### Step 1: Build the Project
```bash
cd build
cmake ..
make -j4
```

### Step 2: Run a Single Test

From project root:
```bash
# Start server with verbose logging
build/mm4serv -p2323 --verbose > test1_output.log 2>&1 &
SERVER_PID=$!

# Start team 1 (noop - does nothing)
build/mm4team_noop -hlocalhost -p2323 &
NOOP_PID=$!

# Start team 2 (testteam with test file)
# Option 1: Using --test-file flag (recommended)
build/mm4team_testteam -hlocalhost -p2323 \
  --test-file teams/testteam/tests/test1_ship_station_collision.txt &
TEST_PID=$!

# Option 2: Using stdin (alternative)
# cat teams/testteam/tests/test1_ship_station_collision.txt | \
#   build/mm4team_testteam -hlocalhost -p2323 --test-file - &

# Wait for game to complete, then clean up
wait $SERVER_PID
kill $NOOP_PID $TEST_PID 2>/dev/null || true
```

### Step 3: View Results
```bash
# See all collision events
grep "COLLISION_DETECTED\|LASER_COLLISION" test1_output.log

# Count collisions
grep -c "COLLISION_DETECTED\|LASER_COLLISION" test1_output.log
```

## Test File Summary

| Test | Purpose | Expected Collision |
|------|---------|-------------------|
| test1 | Ship-Station | `Ship[SHIP] <-> Station[STATION]` |
| test2 | Laser-Station | `LASER_COLLISION: ... -> Station` |
| test3 | Ship-Ship | `Ship[SHIP] <-> Ship[SHIP]` |
| test4 | Ship-Asteroid | `Ship[SHIP] <-> Asteroid[ASTEROID]` (probabilistic) |
| test5 | Laser-Ship | `LASER_COLLISION: ... -> Ship[SHIP]` |

## Log Format

### Collision Detection
```
COLLISION_DETECTED: Ship #0[SHIP] pos=(100.5,50.2) vel=(10.2@45.0°) rad=20.0 <-> Station #1[STATION] pos=(0.0,0.0) vel=(0.0@0.0°) rad=35.0 | dist=112.3 overlap=2.7
```

### Laser Collision
```
LASER_COLLISION: Ship #0[Team1] fires from pos=(100.0,50.0) power=512.0 range=150.5 -> Station #1[STATION][Team2] at pos=(0.0,0.0) laser_vel=(30.0@45.0°) mass=1.234
```

## Troubleshooting

**No collisions detected?**
- Check that verbose logging is enabled (`--verbose` flag)
- Verify ships are actually moving (check test file orders)
- For test4, asteroid collisions are probabilistic - try multiple runs

**Server doesn't start?**
- Make sure port 2323 is not already in use
- Try a different port: `./mm4serv -p2324 --verbose`
- Update client commands to match: `-p2324`

**Tests hang?**
- Kill stuck processes: `pkill mm4serv; pkill mm4team`
- Clean up: `killall mm4serv mm4team_noop mm4team_testteam`

## Next Steps

After running tests:
1. Review collision events in log files
2. Compare legacy vs new collision handling behavior
3. Verify physics calculations are correct
4. Check for any unexpected edge cases
