# MechMania IV Engine Development Guidelines

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

   **5b. Ship-Ship Collision Physics (Ship.C HandleCollision() method)**
   - Location: Inline in `HandleCollision()` method, ship collision handling section
   - Private method: `HandleElasticShipCollision()` implements new physics
   - Legacy: Non-physical separation impulse that creates momentum from nothing (both ships gain velocity)
   - New: Proper 2D elastic collision using standard formulas that conserve momentum and kinetic energy

   **5c. Ship-Asteroid Collision Physics (Asteroid.C CreateFragmentsNew() method)**
   - Location: Lines 233-276 in `CreateFragmentsNew()`, OthKind == SHIP section
   - Legacy: Uses simple relative velocity (CreateFragmentsOld)
   - New: Perfectly elastic collision with proper momentum and energy conservation

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