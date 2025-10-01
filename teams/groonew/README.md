# Team Groonew Strategy Guide

## Team Overview
**Name:** "Groonew!"
**Team Number:** 14
**Authors:** Matt (9/2025)
**Philosophy:** "Groogroo don't eat Groogroo; Groogroo do."

In this team we build on the original Team Groogroo's excellent navigation and resource collection, and then perhaps also add other strategies as well.

## Ship Configuration
Each ship is configured with:
- **Fuel Capacity:** 20 tons (reduced from default 30)
- **Cargo Capacity:** 40 tons (increased from default 30)
- **Brain:** GetVinyl AI module

This configuration prioritizes cargo hauling over fuel efficiency, suggesting an aggressive collection strategy.

## Core Strategy: The Magic Bag System

### What is the Magic Bag?
The Magic Bag is a centralized planning data structure that pre-calculates optimal paths to all objects in space for each ship. It's a 2D array where:
- First dimension: Ship index (0-3)
- Second dimension: Target entries (up to 100 per ship)

### Information Stored in Each Entry
Each Magic Bag entry contains:
```
- thing: Pointer to target object (asteroid/station)
- fueltraj: Fuel trajectory with:
  - fuel_used: Fuel cost for this maneuver
  - traj: Trajectory vector to target
  - order_kind: Type of order (THRUST or TURN)
  - order_mag: Magnitude of order
- total_fuel: Estimated total fuel cost (hardcoded to 5.0)
- turns_total: Number of turns to reach target (1-28)
- collision: Collision detection info (placeholder)
- claimed_by_mech: Ship claiming this target (unused)
```

### Magic Bag Population Process (Each Turn)
1. **Create new Magic Bag** with 4 ships × 100 entries capacity
2. **For each living ship:**
   - **For each thing in world:**
     - Skip dead objects and generic things
     - Track total vinyl/uranium remaining
     - **For turns 1-28 into future:**
       - Calculate "optimal" maneuver to intercept
       - Determine fuel cost
       - Check for collisions (stub implementation)
       - If reachable, add entry and break
3. **Delete old Magic Bag** after all ships decide

Our maneuver is "optimal" in the sense of greedily arriving at a target fast
with a planning horizon of ~2 actions. There are more optimal planning methods
available over longer horizons, and the existence of competing resources
(tradeoffs of speed vs. fuel consumption, etc.) mean there only optimality with
regard to a chosen goal - in our case time to intercept of desired target.

## Ship Decision Tree (GetVinyl Brain)

### Phase 1: Collision Avoidance (Highest Priority)
```
IF collision detected in next 2 turns:
  IF collision < 2 turns away:
    IF enemy station AND carrying cargo:
      → Jettison all cargo (prevent enemy scoring)
    ELSE IF uranium asteroid:
      IF fits in fuel tank:
        → Shield up for absorption
      ELSE:
        → (TODO: Shoot asteroid)
    ELSE IF vinyl asteroid:
      IF fits in cargo hold:
        → Do nothing (ram it)
      ELSE:
        → (TODO: Shoot asteroid)
    ELSE IF enemy ship:
      → (TODO: Attack)
    ELSE IF friendly ship:
      → Do nothing

  ELSE IF collision in 2-3 turns:
    IF enemy station AND carrying cargo:
      → Turn 180° for later jettison
    ELSE:
      → Similar logic with more time to react
```

### Phase 2: Resource Collection
```
IF not avoiding collision:
  Determine preferred asteroid type:
    IF fuel > 5 AND vinyl exists:
      → Prefer VINYL
    ELSE:
      → Prefer URANIUM

  IF cargo > 13 tons OR (no vinyl left AND has cargo):
    → Return to base
    Calculate path to home station (up to 50 turns ahead)
  ELSE:
    → Find best target from Magic Bag
    Search entries for current ship:
      - Skip non-asteroids
      - Skip wrong material type
      - Select target with minimum turns_total
    → Execute best maneuver (THRUST or TURN)
```

### Phase 3: Shield Management
```
After all orders set:
  IF shields < 11 units:
    Reserve 5 fuel for emergency
    → Add shields up to 11 or available fuel
```

## Navigation Algorithm
The team uses a sophisticated two-phase navigation:

1. **Immediate Thrust:** If target is within 0.1 radians of current heading AND thrust ≤ 30
2. **Turn Then Thrust:** Otherwise, calculate angle to face target after 1 turn of drift

This accounts for ship momentum and plans one move ahead.

## Key Strategic Elements

### Strengths
- **Centralized Planning:** Magic Bag provides global optimization
- **Anti-Enemy Scoring:** Jettisons cargo near enemy stations
- **Resource Prioritization:** Dynamically switches between fuel/vinyl
- **Shield Maintenance:** Keeps 11-unit shield buffer
- **Look-Ahead Navigation:** Plans up to 28 turns in advance

### Weaknesses
- **Incomplete Implementation:** Combat and asteroid shooting not implemented
- **Fixed Fuel Estimates:** Doesn't calculate actual fuel costs
- **No Collision Path Planning:** Collision detection is stubbed
- **No Multi-Ship Coordination:** Ships don't claim targets
- **Regenerates Magic Bag:** Recalculates everything each turn (inefficient)

## Unique Behaviors
1. **Cargo Dumping:** Prevents enemies from scoring by jettisoning vinyl near their base
2. **Large Cargo Hold:** 40-ton capacity for aggressive collection
3. **Emergency Fuel Reserve:** Always keeps 5 tons for escape

## Trigonometry Reference

We use a lot of trigonometry to calculate thrust orders in our navigation - most of it is straightforward. 

However, in the case of finding a thrust amount that will put our ship on a
desired trajectory given an arbitrary current orientation, it is complicated
enough that we present here the formulas and some discussion of our approach in
both polar and cartesian coordinates.

Symbols:
  * vs​​ - The ship's starting velocity vector. Cartesian coordinates: (vsx, vsy)
  * vt​​ - The target velocity vector (we only care about its direction).
  * θ - The ship's orientation angle (the direction of thrust).
  * ϕ - The angle of the target velocity, vt​​.
  * vr​​ - The required delta-v from the thruster.
  * k - The scalar magnitude of the thrust vr​​.
  * vf​​ - The ship's final velocity vector.
  * c - The scalar magnitude (speed) of the final velocity vf​​.


The Complete Solution

In most cases, the required thrust magnitude k is given by the formula:

k = (vsx​ * tan ϕ − vsy​) / (sin θ − cos θ tan ϕ)

Mathematical Edge Cases & Physical Meaning

This formula can fail for two mathematical reasons, which represent different physical situations.

1. Failure by Division by Zero (Denominator = 0)

Mathematical Condition: The denominator, sin θ − cos θ tan ϕ, becomes zero. This happens when the thruster direction θ is collinear with the target direction ϕ.

Physical Meaning: This represents a physically impossible maneuver in most cases. The thruster is unable to cancel any velocity that is perpendicular to the target line.

The Exception: A solution is possible if, and only if, the initial velocity vs​​ has no perpendicular component to begin with (i.e., vs​​ is also collinear with ϕ).

2. Failure by Undefined Tangent (Vertical Target)

Mathematical Condition: The formula is invalid if the target direction ϕ is vertical (±π/2), because tan ϕ is undefined.

Physical Meaning: This is not a physical failure. A solution can still be found by using an alternate formula derived directly from the component equations:

k = −vsx ​/ cos θ

The Exception: This alternate formula only fails if its denominator, cosθ, is zero. This happens when the thruster θ is also vertical (±π/2). In that situation, the thruster and target are collinear, which is just a specific instance of Failure Case #1.

1. Defining the Final Velocity Vector (vf​​)

We started by defining the final velocity vector, vf​​, in terms of its ideal final speed, c, and the target direction, ϕ. Using trigonometry, we expressed the vector's components as:

vf ​= (c⋅cos ϕ,c⋅sin ϕ)

This ensures that, by definition, the final velocity vector always points along the desired target line. The sign of c determines whether it points in the same direction as ϕ (if c is positive) or the opposite direction (if c is negative).

2. Solving for the Final Speed (c)

To find the value of the ideal final speed, c, we used our system of two component equations:

vsx ​+ k⋅cos θ = c⋅cos ϕ

vsy ​+ k⋅sin θ = c⋅sin ϕ

By rearranging the first equation, we can isolate c. This gives us a general formula to find the final speed once we have already calculated the required thrust, k:

c = (vsx ​+ k⋅cos θ)/cos ϕ

You can use either of the two original equations to solve for c; they will both yield the same result.

This process determines the realistic outcome of a maneuver, given the ship's physical limits.

Step 1: Calculate the Ideal Solution

First, calculate the ideal, unbounded thrust magnitude (k) and final speed (c) using the formulas we derived:

k = ( vsx * ​tan ϕ − vsy​) / (sin θ − cos θ tan ϕ)

c = ( vsx ​+ k⋅cos θ) / cos ϕ

Step 2: Check the Ideal Solution Against Physical Limits

Next, check if this ideal solution violates the ship's constraints. There are two "dead zones" where the ideal maneuver is not fully achievable as calculated.

Dead Zone 1: Final Speed Exceeds Limit

Condition: The calculated ideal speed c is greater than 30.

Outcome: The maneuver is successful. The ship applies the calculated thrust k (assuming it's within its own limit). The resulting velocity vector points in the correct target direction ϕ, but its magnitude is "clamped" down to the maximum speed of 30 by the game engine.

Dead Zone 2: Required Thrust Exceeds Limit

Condition: The calculated ideal thrust magnitude, ∣k∣, is greater than 60. This happens when the thruster direction θ gets very close to the target direction ϕ.

Outcome: The maneuver is unsuccessful. The ship cannot generate the required thrust. In a real system, the ship would apply its maximum possible thrust (60) in the direction of θ. However, this would be insufficient to alter the ship's course all the way to the target direction ϕ. The final velocity would end up pointing somewhere between the original direction and the target direction.

There's a much more concise and geometrically elegant formula for the thrust when you use polar coordinates for the inputs.

Let's define our known values in polar terms:

The initial velocity, vs​​, has a magnitude s and an angle α.

The target direction is the angle ϕ.

The thruster is oriented at the angle θ.

Polar Coordinate Solution

The required thrust, vr​​, can be described by its magnitude and angle.

Magnitude (∣vr​​∣)

The magnitude of the required thrust is:

∣vr​​∣ = ∣s⋅sin(ϕ−α) / sin(θ−ϕ)∣

Angle (angle of vr​​)

The direction of the thrust depends on the sign of the calculation inside the absolute value bars (let's call that value k).

If k is positive, the thrust is applied in the direction of the ship's orientation: θ.

If k is negative, it's a reverse thrust, applied in the opposite direction: θ+π.

This polar formula neatly captures the physics: the required thrust is proportional to how far your current trajectory is from the target (sin(ϕ−α)), and inversely proportional to how well your thruster is aligned to make the correction (sin(θ−ϕ)).

The final, more complete formula for the signed thrust magnitude, k, is:

k = s⋅sin(ϕ−α) / sin(θ−ϕ)

## Future Improvements (TODOs in Code)
- Implement laser combat for large asteroids
- Add ship-to-ship combat
- Implement collision path verification
- Add target claiming to prevent multiple ships pursuing same object
- Calculate actual fuel costs instead of hardcoded value
- Optimize Magic Bag to update incrementally