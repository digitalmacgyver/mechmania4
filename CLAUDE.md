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

4. **Turn Order Processing (Ship.C Drift() method)**
   - Feature: `velocity-limits`
   - Location: Inline in `Drift()` method, O_TURN handling section
   - Legacy: Calls `SetOrder(O_TURN, turnamt)` for full turn, causing premature fuel clamping on low fuel
   - New: Calls `SetOrder(O_TURN, turnamt * dt)` for dt-sized turn, fixes premature clamping bug
   - Note: Also affects orient update - legacy uses `orient += omega * dt`, new uses `orient += omega`

5. **Jettison Physics (Ship.C HandleJettison() method)**
   - Feature: `physics`
   - Location: Inline in `HandleJettison()` method, momentum calculation section
   - Legacy: Multiplies asteroid momentum by 2.0, causing excessive recoil (violates conservation of momentum)
   - New: Correct Newtonian conservation of momentum (1.0x multiplier)

6. **Ship-Ship Collision Physics (Ship.C HandleCollision() method)**
   - Feature: `physics`
   - Location: Inline in `HandleCollision()` method, ship collision handling section
   - Private method: `HandleElasticShipCollision()` implements new physics
   - Legacy: Non-physical separation impulse that creates momentum from nothing (both ships gain velocity)
   - New: Proper 2D elastic collision using standard formulas that conserve momentum and kinetic energy

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