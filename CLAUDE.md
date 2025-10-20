# MechMania IV Engine Development Guidelines

## Debug Logging Best Practices

### How to Add Debug Logging to Engine Code

When debugging the engine or adding verbose logging, follow these patterns to access the global parser and verbose flag:

#### 1. Include the Parser Header

In your `.C` implementation file, add:
```cpp
#include "ParserModern.h"  // For CParser class definition
```

#### 2. Declare the External Parser Instance

At the top of your `.C` file (after includes, before functions), add:
```cpp
extern CParser* g_pParser;
```

**Important**: Do NOT declare this inside functions. It must be at file scope.

#### 3. Use Verbose Logging Guards

Always guard verbose logging with null checks and verbose flag checks:
```cpp
if (g_pParser && g_pParser->verbose) {
    printf("[DEBUG-TAG] Your debug message: value=%.2f\n", someValue);
}
```

### Common Mistakes to Avoid

❌ **Wrong - Declaring extern inside a function**:
```cpp
void MyFunction() {
    extern CParser* g_pParser;  // DON'T DO THIS - doesn't work reliably
    if (g_pParser && g_pParser->verbose) {
        printf("Debug message\n");
    }
}
```

✅ **Correct - Declaring extern at file scope**:
```cpp
#include "ParserModern.h"

extern CParser* g_pParser;  // At file scope, outside any function

void MyFunction() {
    if (g_pParser && g_pParser->verbose) {
        printf("Debug message\n");
    }
}
```

❌ **Wrong - Not checking for null pointer**:
```cpp
if (g_pParser->verbose) {  // DON'T DO THIS - can crash if g_pParser is NULL
    printf("Debug message\n");
}
```

✅ **Correct - Always check for null first**:
```cpp
if (g_pParser && g_pParser->verbose) {  // Safe: short-circuit evaluation
    printf("Debug message\n");
}
```

### Debugging Tip: When Logging Doesn't Show Up

If you add logging but don't see output, check:

1. **Is `extern CParser* g_pParser;` at file scope?** (Not inside a function)
2. **Did you run with `--verbose` flag?** The verbose flag must be set
3. **Is the code path actually executing?** Add unconditional `printf()` to verify
4. **Did you rebuild after changes?** Run `cmake --build build`

### Example: Adding Debug Logging to World.C

```cpp
/* World.C */
#include "World.h"
#include "ParserModern.h"  // For CParser

extern CParser* g_pParser;  // At file scope

void CWorld::LaserModel() {
    // Debug logging to trace laser processing
    if (g_pParser && g_pParser->verbose) {
        printf("[LASER-DEBUG] Processing lasers, ship count=%d\n", GetShipCount());
    }

    // ... rest of function
}
```

### When to Remove Debug Logging

Debug logging added during investigation should be **removed before committing** unless:
- It provides valuable diagnostic information for future debugging
- It's guarded by the verbose flag (won't show up in normal operation)
- It's documented as intentional diagnostic output

Use git to check for debug logging before commits:
```bash
git diff | grep -i "printf.*DEBUG"
git diff | grep -i "printf.*VERBOSE"
```

## Team Messaging System

### Overview

The messaging system allows teams to send text messages to the observer for display. This is useful for debugging AI strategies, logging decisions, and providing commentary during matches.

### Messaging API (New - Recommended)

Teams have access to a safe messaging interface via the `CTeam` class:

```cpp
// Replace entire message buffer (512 bytes max)
MessageResult result = pmyTeam->SetMessage("Starting turn 5");

// Append to message buffer
result = pmyTeam->AppendMessage(" - found asteroid");
result = pmyTeam->AppendMessage(" - going home");

// Check return value
if (result == MSG_TRUNCATED) {
    // Message was too long and was truncated
}
if (result == MSG_NO_SPACE) {
    // No space available (append only)
}

// Clear the message buffer
pmyTeam->ClearMessage();
```

**MessageResult Return Codes:**
- `MSG_SUCCESS` (0): Message fully written
- `MSG_TRUNCATED` (1): Message written but truncated to fit available space
- `MSG_NO_SPACE` (2): No space available (append only - buffer is full)

**Buffer Details:**
- Maximum size: 512 bytes (defined as `maxTextLen` in Team.h)
- Buffer is cleared automatically at the start of each turn
- Messages are displayed by the observer and then cleared after each physics sub-step
- All operations are null-terminated and buffer-overflow safe

### Legacy Messaging Interface

The old interface using direct buffer manipulation is still supported but not recommended:

```cpp
char msg[256];
sprintf(msg, "Ship %s collecting asteroid\n", pShip->GetName());
strncat(pmyTeam->MsgText, msg, maxTextLen - strlen(pmyTeam->MsgText) - 1);
```

### Testing the Messaging System

A comprehensive test suite is available at `teams/testteam/tests/test6_test_client_messages.txt`.

Run the test:
```bash
bash teams/testteam/tests/test6.sh
```

The test covers:
- Basic SetMessage, AppendMessage, ClearMessage operations
- Message overwrites and concatenation
- Zero-length messages
- Buffer overflow scenarios (511+ byte messages)
- Appending to full buffers (MSG_NO_SPACE behavior)
- Multiple sequential operations

**TestTeam Message Command Format:**

TestTeam can execute message commands from test files using this format:
```
turn,MSG_OP,test_name,message_text
```

Where `MSG_OP` is one of:
- `MSG_SET`: Call SetMessage()
- `MSG_APPEND`: Call AppendMessage()
- `MSG_CLEAR`: Call ClearMessage()

Example test file:
```
# Turn 1: Set initial message
1,MSG_SET,TEST_BASIC_SET,Hello World

# Turn 2: Append to message
2,MSG_APPEND,TEST_APPEND, - additional text

# Turn 3: Clear message
3,MSG_CLEAR,TEST_CLEAR,

# Turn 4: Test overflow (600 char message, buffer is 512)
4,MSG_SET,TEST_OVERFLOW,AAAA...(600 A's)...AAAA
```

TestTeam will log detailed output showing buffer state before/after operations and return codes.

## Simple Method Dispatch for Legacy/New Feature Implementations

### Overview
When adding switchable legacy/new behaviors to the engine, we use a simple method dispatch pattern to:
- Enable runtime behavior selection based on configuration
- Maintain a stable public API
- Keep implementation details private
- Allow direct access to protected members within class methods
- Minimize code complexity and dependencies

### Pattern Structure

```cpp
// Header file (Thing.h or Ship.h)
class MyClass {
public:
    // Public interface remains unchanged
    double PublicMethod(/* params */);

protected:
    // Protected members accessible by private methods
    double protectedData;

private:
    // Private implementation methods for different behaviors
    double PublicMethodOld(/* params */);  // Legacy implementation
    double PublicMethodNew(/* params */);  // New/improved implementation
};

// Implementation file (Thing.C or Ship.C)

// Global parser instance
extern CParser* g_pParser;

double MyClass::PublicMethod(/* params */) {
    // Simple dispatch based on feature flag
    if (g_pParser && !g_pParser->UseNewFeature("feature-name")) {
        return PublicMethodOld(/* params */);
    } else {
        return PublicMethodNew(/* params */);
    }
}

double MyClass::PublicMethodOld(/* params */) {
    // Legacy implementation
    // Can directly access protected/private members
    return protectedData * 2.0;
}

double MyClass::PublicMethodNew(/* params */) {
    // New/improved implementation
    // Can directly access protected/private members
    protectedData *= 1.5;
    return protectedData * 2.0;
```

### Current Implementations Using This Pattern

1. **Collision Detection (Thing.h/Thing.C)**
   - Feature: `collision-detection`
   - Public method: `DetectCollisionCourse()`
   - Private methods: `DetectCollisionCourseOld()`, `DetectCollisionCourseNew()`
   - Legacy: Approximation-based collision detection
   - New: Quadratic formula approach

2. **Thrust Order Processing (Ship.h/Ship.C)**
   - Feature: `velocity-limits`
   - Public method: `SetOrder(O_THRUST, value)`
   - Private methods: `ProcessThrustOrderOld()`, `ProcessThrustOrderNew()`
   - Legacy: Buggy fuel calculation with double-spend on each dt tick
   - New: Correct fuel calculation using dt-sized thrust amounts

3. **Thrust Drift Processing (Ship.h/Ship.C)**
   - Feature: `velocity-limits`
   - Called from: `Drift()` method
   - Private methods: `ProcessThrustDriftOld()`, `ProcessThrustDriftNew()`
   - Legacy: Buggy drift behavior with fuel double-spend
   - New: Correct drift using CalcThrustCost with dt-sized amounts

4. **Turn Order Processing with Triangular Velocity Profile (Ship.C Drift() method)**
   - Feature: `velocity-limits`
   - Location: Inline in `Drift()` method, O_TURN handling section
   - Helper functions: `GetTriangularOmega()`, `CalcTurnCostPhysical()`
   - Physics model: Ship modeled as uniform disk with triangular angular velocity profile
   - Legacy: Linear cost model, each dt tick processes `turnamt * dt`, accelerates/decelerates 5 times
   - New: Physically accurate model with single triangular acceleration/deceleration over full 1-second turn

   **Triangular Velocity Profile Physics:**
   - Ship accelerates linearly to peak angular velocity ω_max at t=0.5s
   - Ship decelerates linearly from ω_max back to zero at t=1.0s
   - Peak angular velocity: ω_max = 2θ/T where θ is total angle, T=1 second
   - Each sub-tick (dt=0.2s) uses angular velocities from the triangular profile
   - Fuel cost per sub-tick = rotational kinetic energy change: ΔKE = 0.5×I×(ω_end² - ω_start²)
   - Special handling for tick 2 (phase 0.4→0.6) which crosses peak: separates accel and decel energy
   - Total fuel across all ticks matches SetOrder cost (no 5x discrepancy)

   **Key Implementation Details:**
   - Server.C passes `turn_phase` ∈ [0,1] to PhysicsModel → World.C → Thing/Ship Drift()
   - `turn_phase = tick / num_ticks` gives progress through turn (0.0, 0.2, 0.4, 0.6, 0.8 for 5 ticks)
   - Tick crossing peak (0.4→0.6): fuel = accel_energy(ω_start→ω_max) + decel_energy(ω_max→ω_end)
   - Other ticks: fuel = 0.5×I×|ω_end² - ω_start²| / energy_per_fuel_ton
   - Orient update: new mode uses `orient += omega` where omega is average of start/end angular velocities

   **Cost Comparison (70-ton ship, full 360° rotation):**
   - Legacy linear: 0.1667 tons (6 rotations per fuel ton)
   - New physical: 0.3513 tons (based on rotational kinetics, 2× peak KE)
   - SetOrder and Drift costs now match perfectly (no 5x error)

5. **Collision Physics and Momentum Conservation (Multiple files)**
   - Feature: `physics`
   - This flag controls ALL collision physics and momentum transfer in the game

   **5a. Jettison Physics (Ship.C HandleJettison() method)**
   - Location: Inline in `HandleJettison()` method, momentum calculation section
   - Legacy: Multiplies asteroid momentum by 2.0, causing excessive recoil (violates conservation of momentum)
   - New: Correct Newtonian conservation of momentum (1.0x multiplier)

   **5b. Ship-Ship Collision Physics and Damage (Ship.C HandleCollision() method)**
   - Location: Inline in `HandleCollision()` method, ship collision handling section
   - Private method: `HandleElasticShipCollision()` implements new collision physics
   - Private method: `CalculateCollisionMomentumChange()` calculates |Δp| for damage
   - **Collision Physics:**
     - Legacy: Non-physical separation impulse that creates momentum from nothing (both ships gain velocity)
     - New: Proper 2D elastic collision using standard formulas that conserve momentum and kinetic energy
   - **Collision Damage:**
     - Legacy: `damage = m_other × v_rel / 1000` (asymmetric - lighter ships take more damage)
     - New: `damage = |Δp| / 1000` (symmetric - both ships take equal damage based on momentum change)

   **5c. Ship-Asteroid Collision Physics and Damage (Asteroid.C CreateFragmentsNew(), Ship.C HandleCollision())**
   - Location: Lines 233-276 in `CreateFragmentsNew()`, OthKind == SHIP section for physics
   - Location: Ship.C HandleCollision() for damage calculation (same as ship-ship)
   - **Collision Physics:**
     - Legacy: Uses simple relative velocity (CreateFragmentsOld)
     - New: Perfectly elastic collision with proper momentum and energy conservation
   - **Collision Damage:**
     - Same formula as ship-ship collisions (see 5b above)
     - Asteroids always fragment into smaller pieces regardless of damage (unlike lasers which require threshold)

   **5d. Laser-Asteroid Collision Physics (Asteroid.C CreateFragmentsNew() method)**
   - Location: Lines 277-294 in `CreateFragmentsNew()`, OthKind == GENTHING section
   - Legacy: Uses relative velocity for fragments (CreateFragmentsOld)
   - New: Perfectly inelastic collision - laser mass and momentum absorbed by asteroid
   - Formula: `v_final = (m_ast × v_ast + m_laser × v_laser) / (m_ast + m_laser)`
   - Fragments inherit center-of-mass velocity plus symmetric spread pattern

   **5e. Laser-Ship Collision Physics (Ship.C HandleCollision() method)**
   - Location: Lines 838-878, OthKind == GENTHING section
   - Legacy: No momentum transfer (ships unaffected by laser impacts beyond damage)
   - New: Perfectly inelastic collision - photon momentum absorbed by ship
   - Formula: `v_final = (m_ship × v_ship + m_laser × v_laser) / (m_ship + m_laser)`
   - Effect: Ship velocity changes by ~1-7 u/s depending on beam strength and ship mass

   **5f. Laser Velocity Model (World.C LaserModel() method)**
   - Location: Lines 277-294, laser velocity initialization
   - Legacy: `laser_vel = target_velocity + 1.0` (non-physical, target-tracking)
   - New: `laser_vel = 30 u/s along beam direction` (photon physics: light travels at c)
   - This affects ALL laser collisions (ships and asteroids) by changing laser momentum vector

6. **Ship Launch Docking Distance (Ship.C ProcessThrustDriftNew/Old() methods)**
   - Feature: `docking`
   - Location: Inline in `ProcessThrustDriftNew()` and `ProcessThrustDriftOld()` methods, undocking section
   - Legacy: Uses `dDockDist + 5.0` for launch distance, causing re-docking bug with low thrust (<35 units/sec)
   - New: Fixed safe distance = `station_radius + ship_radius + (ship_radius / 2.0)` = 48 units
   - Bug Details: Legacy mode ships with thrust < 35 u/s launch to 35 units (< collision threshold of 42), re-dock, incrementing dDockDist each turn until escape
   - Fix: New mode always launches to 48 units (> 42 threshold), guaranteeing immediate escape regardless of thrust

7. **Free Launch Thrust (Ship.h/Ship.C)**
   - Feature: `velocity-limits` (controlled by same flag as thrust processing)
   - Member: `bool bLaunchedThisTurn` - tracks if ship undocked during current turn
   - Reset: In `ResetOrders()` at start of each turn
   - Set: In `ProcessThrustDriftNew()` when `bDockFlag` changes from true to false
   - Used: Passed to `CalcThrustCost()` as 7th parameter for free thrust calculation
   - Legacy: Only first dt tick (while docked) is free, remaining ticks charge fuel (~0.0097 per tick)
   - New: Entire turn (all dt ticks) when ship launches is free, no fuel cost
   - Implementation: `CalcThrustCost()` checks `is_free_thrust = (is_docked || launched_this_turn)` for unlimited budget
   - Note: Flag is server-side only, not serialized to clients (clients see free thrust via SetOrder simulation)

### When to Use This Pattern

Use the simple method dispatch pattern when:
- You have multiple implementations of the same behavior
- Implementations need access to protected/private class members
- Behavior selection is determined by configuration/runtime flags
- You want to keep implementation details private
- You want to minimize code complexity

### Benefits

1. **Simplicity**: Straightforward if/else dispatch without complex patterns
2. **Direct Member Access**: Private methods can access all class members directly
3. **No Dependencies**: No need for std::unique_ptr or virtual functions
4. **Clean Headers**: No strategy classes or helper methods in headers
5. **Easy to Understand**: Clear code flow without indirection
6. **Consistent Pattern**: Same approach across all switchable behaviors

### Implementation Checklist

When adding a new legacy/new behavior:

- [ ] Add private methods for Old and New implementations in class header
- [ ] Implement the private methods in the .C file
- [ ] Update public method to use simple if/else dispatch
- [ ] Add feature flag to ArgumentParser
- [ ] Update bundle configurations if needed
- [ ] Test both legacy and new behaviors
- [ ] Document the feature in this file

### Notes and Considerations

- **Member Access**: Private methods can directly access protected and private members
- **Const Correctness**: Const methods should have const implementations
- **Multiple Objects**: For methods involving other objects (like collision detection), access their public members or pass as parameters
- **State Modification**: Private methods can directly modify object state
- **Performance**: No virtual dispatch overhead, just a simple branch
- **Code Organization**: Keep Old and New implementations close together for easy comparison

### Example: Adding a New Feature

To add a new switchable behavior:

1. Add feature flag in ArgumentParser.C:
   ```cpp
   features["my-feature"] = true;  // Default to new behavior
   ```

2. Add command-line option:
   ```cpp
   options.add_options()("legacy-my-feature", "Use legacy my feature");
   ```

3. Follow the simple method dispatch pattern above for the implementation

4. Update this documentation with the new feature details

### 8. **Collision Processing (World.C CollisionEvaluation() method)**
   - Feature: `collision-handling`
   - Location: World.C CollisionEvaluation() method
   - Private methods: `CollisionEvaluationOld()`, `CollisionEvaluationNew()`
   - **Legacy**: Original collision processing with known bugs:
     - Asteroids can be hit multiple times in same frame (multi-fragmentation bug)
     - Ship-ship collisions processed multiple times (double damage bug)
     - Dead objects continue processing collisions within same frame
   - **New**: Currently identical to legacy (prepared for future fixes)
     - Will address multi-hit bugs in future work
     - Will fix collision ordering issues
     - Will prevent dead object collision processing
   - Note: This refactoring preserves exact legacy behavior while preparing infrastructure for collision system improvements

### 9. **Laser Range Check (World.C LaserModel() method)**
   - Feature: `rangecheck-bug`
   - Location: World.C LaserModel() method, lines 304-353
   - **Legacy**: Buggy floating-point range check (original 1998 code)
     - Compares `dLasRng` (distance from laser endpoint back to ship) with `dLasPwr` (laser beam length)
     - Since `LasPos = ship_position + (dLasPwr * direction_vector)`, these values should theoretically be equal
     - Floating-point errors in trigonometric calculations cause `dLasRng > dLasPwr` to trigger incorrectly
     - Bug particularly manifests when laser power = 512.0 (exactly half world size)
     - When triggered, incorrectly nullifies `pTarget`, preventing lasers from hitting valid targets
   - **New**: Correct range validation
     - Checks actual distance from ship to target against laser beam length
     - `if (target_distance > dLasPwr) { pTarget = NULL; }`
     - Rarely triggers in practice (LaserTarget() already filters by facing direction)
     - Included for correctness and future maintainability

## Testing Engine Changes

After making code changes to the engine, use the Python test harness to verify both legacy and new modes work correctly.

### Quick Test (Recommended)

```bash
# From project root
source scripts/venv/bin/activate
python3 scripts/test_collision_modes.py
```

This runs a full game in both legacy and new collision handling modes with groogroo teams and verifies completion.

### Test Options

```bash
# Test with different teams
python3 scripts/test_collision_modes.py --team1 noop --team2 noop
python3 scripts/test_collision_modes.py --team1 testteam --team2 noop

# Test with specific collision scenario
python3 scripts/test_collision_modes.py \
  --team1 noop \
  --team2 testteam \
  --test-file teams/testteam/tests/test1_ship_station_collision.txt

# View all options
python3 scripts/test_collision_modes.py --help
```

### Available Test Teams

- **groogroo**: Full AI team for realistic testing
- **noop**: Does nothing, useful for isolation testing
- **testteam**: Reads scripted moves from a file for controlled scenarios

### Testing Collision Scenarios

Pre-built collision test scenarios are in `teams/testteam/tests/`:

```bash
# Test ship-station collision
python3 scripts/test_collision_modes.py --team1 noop --team2 testteam \
  --test-file teams/testteam/tests/test1_ship_station_collision.txt

# Test laser-station collision
python3 scripts/test_collision_modes.py --team1 noop --team2 testteam \
  --test-file teams/testteam/tests/test2_station_laser_collision.txt

# Test ship-ship collision
python3 scripts/test_collision_modes.py --team1 noop --team2 testteam \
  --test-file teams/testteam/tests/test3_ship_ship_collision.txt

# Test ship-asteroid collision
python3 scripts/test_collision_modes.py --team1 noop --team2 testteam \
  --test-file teams/testteam/tests/test4_ship_asteroid_collision.txt

# Test laser-ship collision
python3 scripts/test_collision_modes.py --team1 noop --team2 testteam \
  --test-file teams/testteam/tests/test5_ship_laser_collision.txt
```

See `teams/testteam/tests/README.md` for detailed descriptions of each test scenario.

### Creating Custom Test Scenarios

Create a test file with format:
```
# shipnum,turn,ORDER_KIND,magnitude
0,1,THRUST,100.0
1,2,TURN,1.57
2,3,JETTISON,5.0
3,4,LASER,50.0
```

See `teams/testteam/tests/` for examples and `teams/testteam/tests/USAGE.md` for detailed documentation.

### First-Time Setup

If the virtual environment doesn't exist:

```bash
cd scripts/
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt  # Currently no dependencies needed
```

For complete testing documentation, see [scripts/QUICK_TESTING.md](scripts/QUICK_TESTING.md).