# Team Chrome Funkadelic Strategy Guide

## Team Overview
**Name:** "Chrome Funkadelic"
**Author:** Misha Voloshin (9/26/1998)
**Team Number:** Random (1-16)
**Station:** "HeartLand"
**Theme:** 1970s Disco/Funk Culture

## Ship Configuration
The team fields 4 ships with disco-themed names:
- **Ship 0:** "SS TurnTable"
- **Ship 1:** "Bell Bottoms"
- **Ship 2:** "DiscoInferno"
- **Ship 3:** "PurpleVelvet"

Each ship is configured with:
- **Fuel Capacity:** 45 tons (50% increase from default)
- **Cargo Capacity:** 15 tons (50% decrease from default)
- **Brain:** Gatherer AI module

This configuration prioritizes mobility and range over cargo capacity, suggesting a hit-and-run collection strategy.

## Core Strategy: Object-Oriented AI Hierarchy

### AI Brain Architecture
Chrome Funkadelic uses an innovative object-oriented inheritance system where each AI behavior extends the previous one:

```
CBrain (Base)
  └── Voyager (Station Departure)
  └── Stalker (Navigation/Interception)
      └── Shooter (Combat)
          └── Gatherer (Resource Collection + All Above)
```

### Key Design Features
- **Dynamic Brain Switching:** Ships can temporarily change their AI (e.g., Voyager takes over when docked)
- **Behavior Inheritance:** Complex behaviors build on simpler ones
- **Self-Managing:** AI objects can delete themselves when no longer needed

## Ship AI Modules

### 1. Voyager (Station Departure)
**Purpose:** Safely depart from station without collisions
**Lifetime:** Temporary (self-deletes after undocking)
**Behavior:**
```
Calculate departure angle = ShipNumber × 90°
WHILE docked:
  Turn to departure angle
  IF angle difference < 0.2 radians:
    Thrust at max speed
  When undocked → Delete self, restore previous brain
```
This ensures ships leave the station in different directions (0°, 90°, 180°, 270°) to avoid collisions.

### 2. Stalker (Interception)
**Purpose:** Navigate to and intercept any target
**Behavior:**
```
IF collision already on course:
  Cancel thrust (coast to impact)
ELSE:
  Estimate interception time (dt)
  Calculate angle to future target position

  IF target ahead (angle < tolerance):
    Thrust forward (10.0)
  ELSE IF target behind (angle > π - 0.15):
    Thrust backward (-10.0)  // More fuel-efficient
  ELSE:
    Turn toward target (1.2 × angle for sharper turns)
```

### 3. Shooter (Combat)
**Purpose:** Attack targets with laser
**Inherits:** Stalker behavior for approach
**Behavior:**
```
IF distance > 350 miles:
  Use Stalker behavior (approach target)
ELSE:
  Stop thrusting (stabilize for accurate shot)
  Calculate target's position next turn
  Turn to face future position
  Fire laser (distance + 100 miles buffer)
```

### 4. Gatherer (Main AI)
**Purpose:** Complete resource collection loop
**Inherits:** Stalker navigation + Shooter combat

## Gatherer Decision Tree (Main Ship Logic)

### Phase 1: Docked State
```
IF docked at station:
  Create Voyager brain → Exit
```

### Phase 2: Target Selection
```
IF carrying cargo:
  → Target = Home station
  → Log message: "Getting X tons of vinyl and going home"
ELSE:
  Search all world objects:
    IF enemy ship found:
      → Target = Enemy ship (prioritize combat!)
    ELSE find nearest asteroid:
      IF fuel < 20 tons:
        → Only consider uranium asteroids
      ELSE:
        → Consider all asteroids
      → Target = Nearest valid asteroid
```

### Phase 3: Navigation
```
Use Stalker behavior to approach target
```

### Phase 4: Special Handling
```
IF target is asteroid AND won't fit in hold:
  Use Shooter behavior (blast it smaller)

IF fuel < 5 tons AND cargo > 5 tons:
  Jettison 5 tons vinyl (emergency fuel conservation)

IF shields < 30:
  Add 3 units to shields
```

### Phase 5: Collision Avoidance
```
IF not heading to station:
  FOR each object in world:
    IF collision in < 15 seconds:
      IF already thrusting/jettisoning:
        → Continue (will move anyway)
      ELSE IF time > 15 seconds:
        → Thrust backward (-15.0)
        → Log: "Ship brakes for Object"
      ELSE:
        → No time to dodge!
        → Target = Collision object
        → Use Shooter (blast it)
```

## Strategic Analysis

### Strengths
- **Fuel Efficiency:** 45-ton tanks allow long-range operations
- **Smart Navigation:** Backward thrust when more efficient
- **Combat Ready:** Actively hunts enemy ships
- **Emergency Systems:** Auto-jettison cargo when fuel critical
- **Collision Prevention:** Multi-layered avoidance system
- **Organized Departure:** Ships exit station in 4 directions

### Weaknesses
- **Small Cargo Hold:** Only 15 tons limits vinyl collection
- **Aggressive Targeting:** May waste time chasing enemy ships
- **Simple Selection:** Chooses nearest target, not optimal
- **Poor Time Estimation:** Interception calculations are "arbitrary"
- **Shield Management:** Only maintains 30 units (vs Groogroo's 11)
- **No Coordination:** Ships operate independently

### Unique Features
1. **Object-Oriented Design:** Clean, extensible AI architecture
2. **Dynamic Brain Swapping:** Voyager temporarily takes control
3. **Backward Thrust:** Recognizes when reversing is cheaper
4. **Enemy Prioritization:** Always targets enemy ships first
5. **Message Logging:** Ships announce their actions

## Comparison with Groogroo

| Feature | Chrome Funkadelic | Groogroo |
|---------|------------------|----------|
| **Fuel Capacity** | 45 tons | 20 tons |
| **Cargo Capacity** | 15 tons | 40 tons |
| **Planning** | Reactive (nearest) | Predictive (28 turns) |
| **Coordination** | None | Magic Bag (attempted) |
| **Combat** | Functional | Not implemented |
| **Shield Buffer** | 30 units | 11 units |
| **Enemy Ships** | Hunt actively | Ignore |
| **Architecture** | OOP/Inheritance | Procedural/Centralized |

## Strategy Summary
Chrome Funkadelic implements a **"lone wolf"** strategy where each ship independently:
1. Departs safely from station
2. Hunts enemy ships when spotted
3. Collects nearest resources
4. Returns when cargo full
5. Maintains shields and avoids collisions

The team favors **mobility over capacity**, using large fuel tanks to chase enemies and travel far for resources. While less sophisticated than Groogroo's predictive planning, Chrome Funkadelic's implementation is complete and functional, making it a reliable baseline competitor.

## Code Quality Notes
- Well-documented with clear comments
- Good software engineering (inheritance, encapsulation)
- Complete implementation (no TODOs)
- Includes debug messages for monitoring
- Written as example/tutorial code for other teams