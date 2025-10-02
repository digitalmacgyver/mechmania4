# Collision Experiment Branch Summary

## Overview
This branch (`asteroid-race-experiment`) implements a comprehensive solution to fix the asteroid double-claiming bug while preserving exact legacy behavior for backward compatibility.

## The Problem
The original MechMania IV engine had a critical bug where multiple ships could claim the same asteroid's resources, creating "phantom vinyl" out of nothing. This occurred because:
1. The `pThEat` pointer in asteroids would be overwritten by each collision
2. Each ship would still receive resources when processing their collision
3. Result: 120-120 scores were possible from only 200 tons of initial vinyl

Additionally, the collision processing had systematic biases:
- Team 0 always processed first, giving them priority
- Deterministic order created unfair advantages

## Our Solution

### Two-Track Approach
1. **Fair Mode (Default)**: Implements proper collision resolution with randomization
2. **Legacy Mode**: Preserves original bugs for historical accuracy

### Key Implementation Details

#### Files Modified

1. **team/src/Asteroid.h**
   - Added `double dClosestClaimDist` for tracking closest ship distance

2. **team/src/Asteroid.C**
   - `HandleCollision()`: Added two defense layers in fair mode:
     - Layer 1: Dead asteroids don't process collisions
     - Layer 2: Closest ship wins ownership (prevents overwriting)

3. **team/src/World.h**
   - Added `CollisionEvent` structure for two-phase processing
   - Added legacy bookkeeping: `legacyAsteroidClaims` vector
   - Added priority enum for collision types (docking > claiming > damage)

4. **team/src/World.C**
   - `CollisionEvaluationFair()`: New two-phase collision system
     - Phase 1: Detect all collisions
     - Phase 2: Randomize order to eliminate bias
     - Phase 3: Process with validation
   - `CollisionEvaluationLegacy()`: Preserved original behavior
   - `RecordLegacyAsteroidClaim()`: Records claims for batch processing
   - `ProcessLegacyAsteroidClaims()`: Allows multiple ships to claim same asteroid
   - `DetectCollisions()`: Builds collision event list
   - `ProcessCollisionEvent()`: Handles individual collision
   - `ValidateCollision()`: Ensures objects still exist/valid

5. **team/src/Ship.C**
   - `HandleCollision()`: Modified to record claims in legacy mode instead of immediately adding resources

### Functions Created/Modified

#### New Functions in CWorld:
- `CollisionEvaluationFair()` - Main fair collision processor
- `RecordLegacyAsteroidClaim(CAsteroid*, CShip*)` - Records claim for later processing
- `ProcessLegacyAsteroidClaims()` - Batch processes all claims (allows double-claiming)
- `DetectCollisions(std::vector<CollisionEvent>&)` - Builds event list
- `ProcessCollisionEvent(const CollisionEvent&)` - Processes single event
- `ValidateCollision(const CollisionEvent&)` - Validates collision is still valid

#### Modified Functions:
- `CAsteroid::HandleCollision()` - Added defense layers and closest-ship logic
- `CShip::HandleCollision()` - Records claims in legacy mode
- `CWorld::CollisionEvaluation()` - Dispatches to fair or legacy version

## Design Principles

1. **No Class Pollution**: Legacy behavior flags stay in World class only
2. **Defense in Depth**: Multiple layers prevent bugs in fair mode
3. **Exact Legacy Preservation**: Original bugs reproducible when needed
4. **Fair Randomization**: Eliminates systematic Team 0 advantage
5. **Clean Separation**: Fair and legacy modes clearly separated

## Testing Results

- Fair mode: Properly prevents double-claiming, 100-100 scores from 200 tons vinyl
- Legacy mode: Reproduces original bug, 120-120 scores possible
- Cargo collection: Works correctly in both modes
- Child asteroid physics: Separation forces properly applied
- Station docking: Takes priority over damage as intended

## Why This Matters

1. **Competitive Integrity**: Fair mode ensures no team has systematic advantages
2. **Historical Accuracy**: Legacy mode allows studying original contest behavior
3. **Clean Architecture**: Solution doesn't pollute core classes with mode flags
4. **Comprehensive Fix**: Addresses all collision race conditions, not just asteroids

## Branch Status
This experimental branch successfully demonstrates a complete solution to the collision problems while maintaining backward compatibility. The implementation is ready for integration into the main codebase.