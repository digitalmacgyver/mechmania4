# MechMania IV Engine Development Guidelines

## Strategy Pattern for Legacy/New Feature Implementations

### Overview
When adding switchable legacy/new behaviors to the engine, we use the Strategy pattern to:
- Avoid exposing implementation details in headers (prevents client recompilation)
- Provide clean access to private members without friend declarations
- Maintain a consistent pattern across the codebase
- Enable runtime behavior selection based on configuration

### Pattern Structure

```cpp
// Header file (Thing.h or Ship.h)
class MyClass {
private:
    // Private members that strategies need to access
    double privateData;

    // Nested strategy interface - can access private members!
    class MyStrategy {
    public:
        virtual ~MyStrategy() = default;
        virtual double execute(MyClass* obj, /* params */) = 0;
    };

    // Strategy instance
    std::unique_ptr<MyStrategy> strategy;

public:
    // Public interface remains unchanged
    double PublicMethod(/* params */);

    // Initialize strategy based on configuration
    void InitializeStrategy();
};

// Implementation file (Thing.C or Ship.C)

// Anonymous namespace for implementation hiding
namespace {
    // Legacy implementation
    class LegacyStrategy : public MyClass::MyStrategy {
    public:
        double execute(MyClass* obj, /* params */) override {
            // Can access obj->privateData directly!
            // Implementation of legacy behavior
            return obj->privateData * 2.0;
        }
    };

    // New/improved implementation
    class ImprovedStrategy : public MyClass::MyStrategy {
    public:
        double execute(MyClass* obj, /* params */) override {
            // Implementation of new behavior
            return obj->privateData * 3.0;
        }
    };
}

void MyClass::InitializeStrategy() {
    extern CParser* g_pParser;
    if (g_pParser && !g_pParser->UseNewFeature("feature-name")) {
        strategy = std::make_unique<LegacyStrategy>();
    } else {
        strategy = std::make_unique<ImprovedStrategy>();
    }
}

double MyClass::PublicMethod(/* params */) {
    if (!strategy) {
        InitializeStrategy();
    }
    return strategy->execute(this, /* params */);
}
```

### Current Implementations Using This Pattern

1. **Collision Detection (Thing.h/Thing.C)**
   - Feature: `collision-detection`
   - Legacy: Approximation-based collision detection
   - New: Quadratic formula approach
   - Method: `DetectCollisionCourse()`

2. **Thrust Order Processing (Ship.h/Ship.C)**
   - Feature: `velocity-limits`
   - Legacy: Current velocity/acceleration limits
   - New: Improved limits (to be implemented)
   - Method: `SetOrder(O_THRUST, value)`

3. **Thrust Drift Processing (Ship.h/Ship.C)**
   - Feature: `velocity-limits`
   - Legacy: Current drift behavior
   - New: Improved drift (to be implemented)
   - Method: `Drift()` thrusting block

### When to Use This Pattern

Use the Strategy pattern when:
- You have multiple implementations of the same behavior
- Implementations need access to private class members
- Behavior selection is determined by configuration/runtime flags
- You want to avoid exposing implementation details in headers
- Future implementations might be added

### Benefits

1. **No Client Recompilation**: Adding new strategies only changes .C files
2. **Encapsulation**: Implementation details stay in .C files
3. **Clean Access**: Nested classes can access private members naturally
4. **Testability**: Each strategy can be tested independently
5. **Consistency**: Same pattern across all switchable behaviors

### Implementation Checklist

When adding a new legacy/new behavior:

- [ ] Define nested strategy interface in class header
- [ ] Add strategy member pointer to class
- [ ] Create InitializeStrategy() method
- [ ] Implement strategy classes in anonymous namespace in .C file
- [ ] Update public method to dispatch through strategy
- [ ] Add feature flag to ArgumentParser
- [ ] Update bundle configurations if needed
- [ ] Test both legacy and new behaviors
- [ ] Document the feature in this file

### Notes and Considerations

- **Initialization**: Strategies are typically initialized in constructors or on first use
- **Const Methods**: For const methods, strategies need const-correct interfaces
- **Multiple Objects**: When methods involve multiple objects (like collision detection), pass both as parameters
- **State Modification**: Strategies can modify object state through the passed pointer
- **Performance**: Virtual dispatch overhead is negligible compared to the computations involved

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

3. Follow the pattern structure above for the implementation

4. Update this documentation with the new feature details