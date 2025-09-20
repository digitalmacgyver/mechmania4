# Team Groogroo Strategy Guide

## Team Overview
**Name:** "GrooGroo eat Groogroo!"
**Team Number:** 13
**Authors:** Zach, Arun, Matt (10/3/1998)
**Philosophy:** "Groogroo don't eat Groogroo; Groogroo do."

## Ship Configuration
The team consists of 4 ships with biological-themed names:
- **Ship 0:** "Larvae"
- **Ship 1:** "Tree"
- **Ship 2:** "Host"
- **Ship 3:** "Symbiant"

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
       - Calculate optimal maneuver to intercept
       - Determine fuel cost
       - Check for collisions (stub implementation)
       - If reachable, add entry and break
3. **Delete old Magic Bag** after all ships decide

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
4. **Biological Naming:** Suggests hive-mind or symbiotic theme

## Future Improvements (TODOs in Code)
- Implement laser combat for large asteroids
- Add ship-to-ship combat
- Implement collision path verification
- Add target claiming to prevent multiple ships pursuing same object
- Calculate actual fuel costs instead of hardcoded value
- Optimize Magic Bag to update incrementally