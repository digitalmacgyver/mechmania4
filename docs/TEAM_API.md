# MechMania IV Team API Reference

This document provides the programming interface for implementing team strategies in MechMania IV. For a narrative rules overview see `CONTEST_RULES.md`. For detailed mechanics consult:
- `CONTEST_NAVIGATION_FOR_CONTESTANTS.md` / `CONTEST_NAVIGATION_FOR_DEVS.md`
- `CONTEST_PHYSICS_FOR_CONTESTANTS.md` / `CONTEST_PHYSICS_FOR_DEVS.md`
- `CONTEST_DAMAGE_FOR_CONTESTANTS.md` / `CONTEST_DAMAGE_FOR_DEVS.md`

## Table of Contents
1. [Team Setup](#team-setup)
2. [Ship Configuration](#ship-configuration)
3. [Ship Orders](#ship-orders)
4. [World Information](#world-information)
5. [Navigation and Physics](#navigation-and-physics)
6. [Combat and Defense](#combat-and-defense)
7. [Resource Collection](#resource-collection)
8. [Utility Functions](#utility-functions)
9. [Hello World Example](#hello-world-example)
10. [Global Constants](#global-constants)

## Team Setup

### Creating Your Team Class

Your team must inherit from `CTeam` and implement required methods:

```cpp
class MyTeam : public CTeam {
public:
    MyTeam();
    ~MyTeam();

    void Init();  // Called once at game start
    void Turn();  // Called every simulation step
};

// Required factory function
CTeam* CTeam::CreateTeam() {
    return new MyTeam();
}
```

### Team Initialization

```cpp
void MyTeam::Init() {
    // Set team identity (1-16)
    SetTeamNumber(1);

    // Name your team and station
    SetName("Team Name");
    GetStation()->SetName("Base Name");

    // Name your ships
    GetShip(0)->SetName("Ship Alpha");
    GetShip(1)->SetName("Ship Beta");
    GetShip(2)->SetName("Ship Gamma");
    GetShip(3)->SetName("Ship Delta");

    // Configure ships (see Ship Configuration)
}
```

## Ship Configuration

### Capacity Allocation

Ships have 60 tons total capacity split between fuel and cargo:

```cpp
// Default: 30 fuel, 30 cargo
// Custom example: High fuel configuration
GetShip(0)->SetCapacity(S_FUEL, 45.0);   // 45 tons fuel
GetShip(0)->SetCapacity(S_CARGO, 15.0);  // 15 tons cargo (automatic)

// High cargo configuration
GetShip(1)->SetCapacity(S_CARGO, 40.0);  // 40 tons cargo
GetShip(1)->SetCapacity(S_FUEL, 20.0);   // 20 tons fuel (automatic)
```

> **CRITICAL WARNING - SetCapacity() Restrictions:**
>
> `SetCapacity()` should **ONLY** be called during the `Init()` method. This is when capacity settings are serialized and sent to the server to configure your ships for the entire game.
>
> **What happens if you call SetCapacity() outside Init():**
> - The call will modify your team's **local copy** of the ship data
> - The server will **NOT** receive or apply this change
> - Every turn, the server sends your team a fresh world state that **overwrites** your local changes
> - **Result:** Your team's view will briefly show the wrong capacities, then revert to the actual values, causing confusing behavior
>
> **Why this matters:**
> - There are no O_ orders (O_THRUST, O_LASER, O_TURN, etc.) that modify ship capacities
> - After `Init()`, the ONLY way teams influence the game is through O_ orders
> - Ship capacities are fixed for the entire game after initialization
>
> **Correct usage:** Configure capacities once in `Init()`, then use `GetCapacity()` to read them during gameplay.

### AI Brain Assignment

Ships need a brain (AI controller) to make decisions:

```cpp
// Assign AI to ship
GetShip(0)->SetBrain(new MyShipAI());

// Create custom AI by inheriting CBrain
class MyShipAI : public CBrain {
public:
    void Decide();  // Called each turn for this ship
};

void MyShipAI::Decide() {
    // Access ship via pShip member variable
    pShip->SetOrder(O_THRUST, 10.0);
}
```

## Ship Orders

### Order Types

Ships can issue these orders each turn:

```cpp
// Movement orders (MUTUALLY EXCLUSIVE - choose at most ONE per turn)
pShip->SetOrder(O_THRUST, value);   // -60 to +60 units/sec (Δv along orientation)
pShip->SetOrder(O_TURN, radians);   // Rotation in radians (normalized to [-π, π])
pShip->SetJettison(VINYL, tons);    // Eject cargo
pShip->SetJettison(URANIUM, tons);  // Eject fuel

// Combat/defense orders (COMBINABLE - can use both per turn, with any movement order)
pShip->SetOrder(O_LASER, distance); // Fire laser beam
pShip->SetOrder(O_SHIELD, amount);  // Add to shields
```

> **Order Restrictions:**
> - **Movement orders are mutually exclusive:** Issuing `O_THRUST`, `O_TURN`, or `O_JETTISON` automatically cancels the other two. Only ONE movement order executes per turn.
> - **Combat/defense orders are combinable:** Both `O_LASER` and `O_SHIELD` can be issued every turn and can be combined with any movement order.

### Order Examples

```cpp
// Accelerate forward
pShip->SetOrder(O_THRUST, 10.0);

// Reverse thrust
pShip->SetOrder(O_THRUST, -10.0);

// Turn 90 degrees counter-clockwise
pShip->SetOrder(O_TURN, PI/2);

// Turn 270 degrees clockwise is normalized to -90 degrees (shortest path)
pShip->SetOrder(O_TURN, 3*PI/2);  // Normalized to -PI/2, charged for PI/2 rotation

// Fire 200-mile laser
pShip->SetOrder(O_LASER, 200.0);

// Add 5 shield units
pShip->SetOrder(O_SHIELD, 5.0);

// Dump 3 tons of cargo
pShip->SetJettison(VINYL, 3.0);
```

> **Turn Normalization:** Turn orders are automatically normalized to the range `[-π, π]` (shortest path). If you request a turn of `+3π/2` radians (270° clockwise), it will be normalized to `-π/2` radians (90° counter-clockwise) and you'll be charged fuel for the shorter `-π/2` rotation. You cannot choose to turn "the long way around."

### Turn Processing Pipeline
Orders you submit during `Turn()` are batched for the upcoming second and then resolved in the following sequence **for each physics sub-step** (default 5 sub-steps of 0.2 s):
1. **Jettison** – queued cargo/fuel ejections convert into asteroid spawns and immediately reduce ship mass.
2. **Shield charge** – `O_SHIELD` consumes fuel and raises shields (clamped to capacity).
3. **Turn / thrust integration** – the portion of `O_TURN` and `O_THRUST` scheduled for this slice is applied with velocity capping.
4. **Drift** – positions and orientations advance by the slice duration.
5. **Collisions** – the snapshot-based collision engine resolves overlaps and applies damage/resource transfers.
6. **Add / remove** – newly spawned objects are added; destroyed ones are culled.

After all sub-steps finish, **lasers fire** once, applying damage and momentum changes to their targets. Shield orders therefore always precede thrust, and lasers always evaluate after movement.

### Launch from Station (Undocking)

When a ship issues `O_THRUST` while docked at a station, it will **automatically undock** and be **teleported** to a safe launch position before physics begins:

**Launch Distance:**
- **Distance from station**: `Station_Size + Ship_Size + (Ship_Size / 2)`
- With default sizes (Station=30, Ship=12): **48 units from station center**
- This ensures the ship is beyond collision range (30+12=42 units)

**Launch Direction:**
- **Forward thrust (`O_THRUST > 0`)**: Ship placed **in front** (along ship's orientation)
- **Reverse thrust (`O_THRUST < 0`)**: Ship placed **behind** (opposite to orientation)

**Launch Fuel Cost:**
- **While docked**: `O_THRUST` orders are NOT limited by current fuel or fuel capacity
- **Entire launch turn**: The full `O_THRUST` order from a launching ship (including all thrust applied after undocking) consumes NO fuel. Other orders (`O_SHIELD`, `O_LASER`) still consume fuel normally.
- **Subsequent turns**: Normal fuel costs apply after the launch turn completes

**Example:**
```cpp
// Ship docked at station (256, 256) with orientation 0° (pointing east)

// Forward launch - ship placed to the east
pShip->SetOrder(O_THRUST, 10.0);
// Result: Ship teleports to (256+48, 256) = (304, 256)
// Fuel cost: 0.0 (entire launch turn is free)

// Reverse launch - ship placed to the west
pShip->SetOrder(O_THRUST, -10.0);
// Result: Ship teleports to (256-48, 256) = (208, 256)
// Fuel cost: 0.0 (entire launch turn is free)

// Can launch with ANY thrust value, even with zero fuel
pShip->SetOrder(O_THRUST, 60.0);  // Maximum thrust allowed
// Fuel cost: 0.0 (no fuel required for launch)
```

> **Important:** The launch position is calculated along the line defined by the ship's **current orientation** at the moment of undocking. Ships always face a specific direction (orient angle in radians), and the launch teleport moves them 48 units along (or opposite to) that direction vector from the station center.
>
> **Fuel Note:** Because launch turns are completely free, you can issue thrust orders of any magnitude (up to ±60) when docked, regardless of your current fuel level or fuel capacity. This allows ships to launch even with empty fuel tanks, though they will need fuel for subsequent turns.

### Fuel Costs

Each order returns a **fuel estimate**:

```cpp
double fuel_est = pShip->SetOrder(O_THRUST, 10.0);
// This is an estimate under current conditions (velocity, mass, fuel).
// The actual fuel charged is computed during physics (5 sub-steps) and
// can differ slightly if the world state changes (e.g., collisions or
// other orders also consume fuel that turn).
```

**How thrust fuel is computed (summary):**

- A per-second thrust order is applied as **five equal instantaneous impulses** at 0.0, 0.2, 0.4, 0.6, and 0.8 seconds.
- After each impulse the engine evaluates $v_{\text{des}} = v_{\text{old}} + \Delta v$.
- If $\lVert v_{\text{des}} \rVert \le 30$ (the speed circle) the impulse is accepted as-is.
- Otherwise the engine projects $v_{\text{des}}$ back to the circle along the same ray and the trimmed portion is treated as **overshoot**.

The fuel charged for each impulse uses the same slope for both the applied delta-v and any overshoot:

$$
\text{fuel}_{\text{base}} = \lVert \Delta v \rVert \cdot \frac{\text{ship\_mass}}{6 \cdot \text{max\_speed} \cdot \text{empty\_mass}}
$$

$$
\text{fuel}_{\text{gov}} = \max(0, \lVert v_{\text{des}} \rVert - 30) \cdot \frac{\text{ship\_mass}}{6 \cdot \text{max\_speed} \cdot \text{empty\_mass}}
$$

Totals for the order are the sums of the five impulses. While **docked**, thrust and turn orders remain free (no base or governor fuel), though velocity is still clamped to the 30-speed circle.

## World Information

### Accessing World Objects

```cpp
CWorld* world = GetWorld();

// Iterate through all objects
for (UINT i = world->UFirstIndex;
     i != BAD_INDEX;
     i = world->GetNextIndex(i)) {

    CThing* thing = world->GetThing(i);
    if (!thing || !thing->IsAlive()) continue;

    // Check object type
    ThingKind kind = thing->GetKind();
    switch (kind) {
        case SHIP:     // Enemy or friendly ship
        case STATION:  // Station
        case ASTEROID: // Resource asteroid
        case GENTHING: // Generic object (laser beam)
    }
}
```

### Ship Information

```cpp
// Your ship's resources
double fuel = pShip->GetAmount(S_FUEL);
double cargo = pShip->GetAmount(S_CARGO);
double shields = pShip->GetAmount(S_SHIELD);

// Capacities
double max_fuel = pShip->GetCapacity(S_FUEL);
double max_cargo = pShip->GetCapacity(S_CARGO);

// Status
bool docked = pShip->IsDocked();
UINT ship_num = pShip->GetShipNumber();  // 0-3

// Physics
CCoord position = pShip->GetPos();       // x,y coordinates
CTraj velocity = pShip->GetVelocity();   // rho,theta polar
double orientation = pShip->GetOrient(); // radians
double mass = pShip->GetMass();         // includes cargo/fuel
```

**Initial Ship Orientation:**

All ships spawn docked at their team's station, facing toward the map center for balanced gameplay:

```cpp
// At game start (during Init()):
CStation* station = team->GetStation();
CCoord station_pos = station->GetPos();

// Check which team you are to predict initial orientation
if (station_pos.fX < 0.0) {
    // Team at (-256, -256): ships face EAST
    // GetOrient() returns 0.0 radians at spawn
} else {
    // Team at (256, 256): ships face WEST
    // GetOrient() returns PI (≈3.14159) radians at spawn
}

// All ships on your team have same initial orientation
double initial_orient = pShip->GetOrient();  // 0 or π depending on team
```

This orientation fix ensures both teams have symmetric starting conditions (both face toward center), eliminating the legacy asymmetry where one team faced away from center.

### Team Information

```cpp
// Your team
CTeam* team = pShip->GetTeam();
double score = team->GetScore();        // Vinyl at station
CStation* station = team->GetStation();
CShip* teammate = team->GetShip(2);     // Get ship by index

// Check if object belongs to your team
if (thing->GetTeam() == team) {
    // Friendly object
}
```

### Asteroid Information

```cpp
CAsteroid* asteroid = (CAsteroid*)thing;

// Material type
AsteroidKind material = asteroid->GetMaterial();
if (material == VINYL) {
    // Purple - goes in cargo hold
} else if (material == URANIUM) {
    // Green - goes in fuel tank
}

// Size
double mass = asteroid->GetMass();

// Check if it fits
if (pShip->AsteroidFits(asteroid)) {
    // Will be collected on collision
}
```

## Navigation and Physics

### Coordinate System

**Important:** The game uses a standard mathematical coordinate system where:
- The world ranges from (-512, -512) to (512, 512)
- **+Y points downward on the screen display**
- +X points to the right
- Angles are in radians, with 0 pointing right (+X direction)
- Heading angles: 0 = right, PI/2 = down (+Y), PI = left, -PI/2 = up (-Y)

**Clarifying note:** In the underlying game world, PI/2 corresponds to the direction of increasing Y. The observer UI follows standard screen coordinates, so increasing Y is rendered downward; the angles displayed therefore appear inverted relative to the math convention.

### Position and Distance

```cpp
// Get positions
CCoord my_pos = pShip->GetPos();
CCoord target_pos = thing->GetPos();

// Calculate distance
double distance = my_pos.DistTo(target_pos);

// Get vector to target
CTraj vector_to_target = my_pos.VectTo(target_pos);
// vector_to_target.rho = distance
// vector_to_target.theta = angle to target
```

**Toroidal Topology (World Wrapping):**

The game world has toroidal topology - when you leave one edge, you appear on the opposite edge. **All coordinate methods automatically handle this wrapping:**

- `DistTo()` - Returns the **shortest distance** accounting for world wrapping
- `AngleTo()` - Returns the angle toward the **shortest path** (may wrap through edges)
- `VectTo()` - Returns the vector along the **shortest path**

**Example:** A ship at (500, 0) measuring distance to (-500, 0):
- Direct distance (left): 1000 units
- Wrapped distance (right): 24 units ← **DistTo() returns this**
- Angle: 0° (pointing right/east) ← **AngleTo() returns this**

The shortest path automatically wraps through the right edge because 24 units < 1000 units.

**Important implications:**
- Navigation algorithms work naturally - just use `DistTo()` and `AngleTo()`
- No need to manually check for wrapping or calculate alternate paths
- Maximum distance between any two points is ~724 units (corner to opposite corner)

**Testing toroidal calculations:** Run `./build/test_toroidal_coordinates` to see comprehensive examples of toroidal distance and angle calculations, including:
- Simple non-wrapping cases
- Single-edge wrapping (through top/bottom/left/right)
- Corner wrapping (through two adjacent edges)
- Explanation of why 3-edge wrapping is geometrically impossible in 2D

### Collision Detection

```cpp
#include "GameConstants.h"  // exposes g_no_collide_sentinel and other globals

// Check if on collision course
double time_to_impact = pShip->DetectCollisionCourse(*thing);
if (time_to_impact == g_no_collide_sentinel) {
    // No collision predicted
} else if (time_to_impact < 5.0) {
    // Collision in less than 5 seconds!
}
```

### Interception Calculation

```cpp
// Calculate angle to intercept moving target
double intercept_time = 10.0;  // Estimated time
double angle = pShip->AngleToIntercept(*target, intercept_time);

// Turn toward intercept point
pShip->SetOrder(O_TURN, angle);
```

### Predictive Movement

```cpp
// Predict future positions
CCoord future_pos = thing->PredictPosition(5.0);  // 5 seconds ahead

// Get relative velocity
CTraj rel_vel = pShip->RelativeVelocity(*thing);
```

## Combat and Defense

### Firing Lasers

> Damage behaviour and worked examples are consolidated in `CONTEST_DAMAGE_FOR_DEVS.md`.

```cpp
// Find what laser will hit
CThing* target = pShip->LaserTarget();
if (target && target->GetTeam() != team) {
    // Calculate required power
    double range = pShip->GetPos().DistTo(target->GetPos());

    // Fire with extra beam length to deal shield damage of: 30*(extra_beam_length=100)/1000.0 = 3 units
    pShip->SetOrder(O_LASER, range + 100.0);
}
```

### Shield Management

```cpp
// Check shields
double shields = pShip->GetAmount(S_SHIELD);

// Maintain minimum shields
if (shields < g_ship_default_shield_amount) {
    double needed = g_ship_default_shield_amount - shields;
    double fuel = pShip->GetAmount(S_FUEL);

    // Add shields (limited by available fuel)
    pShip->SetOrder(O_SHIELD, min(needed, fuel));
}
```

### Damage System Overview

- **Ships** lose shields when they collide with asteroids or other ships, or when they are hit by lasers. If incoming damage exceeds remaining shields the ship is destroyed. Ships dock by “colliding” with a friendly station and take no damage (and cannot be damaged) while docked.
- **Asteroids** take damage from ship collisions and lasers. Any collision shatters an asteroid into three equal fragments unless doing so would produce chunks lighter than `g_thing_minmass` (3 tons), in which case the asteroid is destroyed. Asteroids bounce off stations and do not collide with one another.
- **Stations** only take damage from lasers. Laser damage removes vinyl from their storage until it reaches zero.

#### Order Sequencing
Orders are processed in a specific sequence each turn. See [Turn Processing Pipeline](#turn-processing-pipeline) for the complete execution order, including sub-step breakdown and laser timing.

#### Collision Damage (Ships)
Ship collisions use **momentum change** to calculate damage. Both objects in a collision take damage equal to:
```
damage = |Δp| / g_laser_damage_mass_divisor
```
where `|Δp|` is the magnitude of momentum change experienced by each object, and `g_laser_damage_mass_divisor` defaults to 1000.0.

**Damage to shield conversion:** 1000 damage points = 1 shield unit depleted.

**Key properties:**
- **Both objects take equal damage** (Newton's 3rd Law: equal and opposite momentum changes)
- **Elastic collisions** (ship-ship, ship-large asteroid): `|Δp| = (2 × m₁ × m₂ / (m₁ + m₂)) × v_rel_normal`
- **Inelastic collisions** (ship-small asteroid): `|Δp| = (m₁ × m₂ / (m₁ + m₂)) × v_rel`
- `v_rel_normal` is the relative velocity component along the collision line (depends on impact angle)

**Mass ranges:**
- Ships: 40–100 tons (40 ton hull + 0–60 tons fuel/cargo)
- Asteroids: 3–40 tons (typically)

**Maximum damage scenarios** (head-on collision at max relative velocity of 60 u/s):
- Two heavy ships (100 tons each): **~6000 damage** (~6.0 shield units depleted) to each
- Two light ships (40 tons each): **~2400 damage** (~2.4 shield units depleted) to each
- Heavy vs light ship: **~3400 damage** (~3.4 shield units depleted) to both
- Ship (any mass) vs 40-ton asteroid (elastic): **~2400–3400 damage** (~2.4–3.4 shield units depleted) depending on ship mass

**Impact angle matters:** Glancing collisions have lower `v_rel_normal`, resulting in less damage than head-on collisions.

#### Laser Damage (Ships)
- A ship chooses a beam length up to `min(fWXMax, fWYMax)` (default 512). Firing costs `beam_length / g_laser_range_per_fuel_unit` fuel (`g_laser_range_per_fuel_unit` defaults to 50).
- When the beam hits, the engine treats it as a collision with a virtual object of mass
  `g_laser_mass_scale_per_remaining_unit × (beam_length − distance_to_target)` (default scale: 30.0).
- **Damage formula:** `damage = virtual_mass = 30.0 × (beam_length − distance_to_target)`
- **Shield depletion:** 1000 damage = 1 shield unit depleted, so `shields_lost = damage / 1000`
- **Example:** A beam with 100 units of remaining length deals 3000 damage, depleting 3.0 shield units.
- At point blank range (ships are at least 24 units apart because of their radii) each unit of fuel spent on a laser costs the shooter 1 fuel but forces the target to spend roughly 1.5 fuel to rebuild shields. The trade flips once the target is farther than one third of the chosen beam length; at two thirds the beam costs twice as much fuel to shoot as the target needs to raise shields.

#### Laser Damage (Asteroids)
- **Damage threshold:** A laser must deal **≥1000 damage** to shatter an asteroid into three equal chunks (subject to the minimum-mass rule of 3 tons per fragment).
- **Required beam length:** Since damage = 30 × (beam_length − distance), you need at least 33.33 units of remaining beam to reach the 1000 damage threshold.
- **Example:** To shatter an asteroid at distance 100, fire a beam of length ≥133.33 (damage = 30 × 33.33 = 1000).
- Shots that deal <1000 damage have no effect—damage does not accumulate between laser hits.

#### Laser Damage (Stations)
- **Vinyl depletion:** Stations lose vinyl based on damage: `vinyl_lost = damage / 1000`
- **Example:** A laser dealing 5000 damage removes 5.0 tons of vinyl from the station.
- Vinyl is clamped at zero (cannot go negative).

#### Default Combat Constants
The numeric examples above use the default configuration defined in `team/src/GameConstants.C` and `team/src/Coord.h`:

| Constant | Default | Purpose |
| --- | --- | --- |
| `g_laser_range_per_fuel_unit` | `50.0` | Fuel cost (tons) per unit of beam length. |
| `g_laser_mass_scale_per_remaining_unit` | `30.0` | Converts remaining beam length to an equivalent impact mass. |
| `g_laser_damage_mass_divisor` | `1000.0` | Scales impact mass into shield damage. |
| `g_ship_spawn_mass` | `40.0` | Base hull mass before adding fuel or cargo. |
| `g_ship_total_stat_capacity` | `60.0` | Total capacity shared by fuel and cargo tanks. |
| `g_game_max_speed` | `30.0` | Per-object speed clamp; sets the relative-speed ceiling. |
| `g_thing_minmass` | `3.0` | Minimum mass for asteroid fragments after shattering. |
| `fWXMax`, `fWYMax` | `512.0` | Half the world width/height; sets the absolute laser range cap. |

Adjust these constants (or command-line parameters that override them) to explore alternate combat balance.

## Resource Collection

### Finding Resources

```cpp
// Find nearest vinyl asteroid
CThing* best_target = nullptr;
double best_dist = 999999;

for (UINT i = world->UFirstIndex; i != BAD_INDEX; i = world->GetNextIndex(i)) {
    CThing* thing = world->GetThing(i);
    if (!thing || thing->GetKind() != ASTEROID) continue;

    CAsteroid* ast = (CAsteroid*)thing;

    // Check material type
    if (ast->GetMaterial() != VINYL) continue;

    // Check if it fits
    if (!pShip->AsteroidFits(ast)) continue;

    double dist = pShip->GetPos().DistTo(ast->GetPos());
    if (dist < best_dist) {
        best_dist = dist;
        best_target = ast;
    }
}
```

### Station Docking

```cpp
// Check if carrying cargo
if (pShip->GetAmount(S_CARGO) > 0) {
    // Head to station
    CStation* station = team->GetStation();

    // Navigate to station
    // Collision with station automatically:
    // - Docks ship
    // - Deposits all cargo
    // - Enables free thrust/turn (no fuel cost while docked)
}

// Departing station
if (pShip->IsDocked()) {
    // Choose departure angle
    double angle = ship_number * PI/2;  // Spread out
    pShip->SetOrder(O_TURN, angle - pShip->GetOrient());

    // Thrust away (costs no fuel while docked)
    pShip->SetOrder(O_THRUST, 20.0);
}
```

## Utility Functions

### Message Logging

```cpp
// Print team messages (visible to observer)
char msg[256];
sprintf(msg, "Ship %s collecting asteroid at (%.0f,%.0f)\n",
        pShip->GetName(), target->GetPos().fX, target->GetPos().fY);
strcat(team->MsgText, msg);  // Append to team message buffer
```

### Coordinate Wrapping

World coordinates wrap at edges (-512 to 512):

```cpp
// Positions automatically wrap
CCoord pos(600, 300);  // Automatically wraps to (88, 300)

// Check for wrapped distance
double dist1 = pos1.DistTo(pos2);  // Direct distance
// DistTo automatically handles wrapping for shortest path
```

### Game Constants

All game constants are defined in `team/src/GameConstants.h` and initialized in `GameConstants.C`. Include the header to access these values:

```cpp
#include "GameConstants.h"
```

**Physics and Movement:**
| Constant | Default | Description |
|----------|---------|-------------|
| `g_game_max_speed` | 30.0 | Maximum velocity (units/sec) for ships and asteroids |
| `g_game_max_thrust_order_mag` | 60.0 | Maximum thrust order magnitude (±60 units/sec) |
| `g_game_turn_duration` | 1.0 | Duration of each game turn (seconds) |
| `g_physics_simulation_dt` | 0.2 | Time step for physics sub-steps (seconds) |

**Object Sizes (Collision Radii):**
| Constant | Default | Description |
|----------|---------|-------------|
| `g_ship_spawn_size` | 12.0 | Ship collision radius |
| `g_station_spawn_size` | 30.0 | Station collision radius |
| `g_asteroid_size_base` | 3.0 | Base asteroid radius (before mass scaling) |
| `g_thing_minsize` | 1.0 | Minimum object collision radius |

**Object Masses:**
| Constant | Default | Description |
|----------|---------|-------------|
| `g_ship_spawn_mass` | 40.0 | Ship hull mass (empty, before fuel/cargo) |
| `g_station_spawn_mass` | 99999.9 | Station mass (effectively infinite) |
| `g_initial_asteroid_mass` | 40.0 | Initial asteroid mass at world spawn |
| `g_thing_minmass` | 3.0 | Minimum object mass (fragments smaller than this vaporize) |

**World Setup:**
| Constant | Default | Description |
|----------|---------|-------------|
| `g_initial_team_ship_count` | 4 | Ships per team at game start |
| `g_initial_vinyl_asteroid_count` | 5 | Vinyl asteroids spawned at start |
| `g_initial_uranium_asteroid_count` | 5 | Uranium asteroids spawned at start |
| `g_asteroid_split_child_count` | 3 | Fragments created when asteroid shatters |

**Ship Configuration:**
| Constant | Default | Description |
|----------|---------|-------------|
| `g_ship_total_stat_capacity` | 60.0 | Total fuel + cargo capacity (tons) |
| `g_ship_default_fuel_capacity` | 30.0 | Default fuel capacity (tons) |
| `g_ship_default_cargo_capacity` | 30.0 | Default cargo capacity (tons) |
| `g_ship_default_shield_capacity` | 8000.0 | Maximum shield capacity |
| `g_ship_default_shield_amount` | 30.0 | Starting shield amount |
| `g_ship_default_docking_distance` | 30.0 | Max distance to dock with station |

**Combat and Damage:**
| Constant | Default | Description |
|----------|---------|-------------|
| `g_laser_range_per_fuel_unit` | 50.0 | Beam length per ton of fuel |
| `g_laser_mass_scale_per_remaining_unit` | 30.0 | Converts remaining beam length to impact mass |
| `g_laser_damage_mass_divisor` | 1000.0 | Converts momentum change to shield damage |

**Sentinel Values (Special Return Codes):**
| Constant | Default | Description |
|----------|---------|-------------|
| `g_no_collide_sentinel` | -1.0 | Returned by `DetectCollisionCourse()` when no collision predicted |
| `g_no_damage_sentinel` | -123.45 | Indicates no damage direction recorded |
| `BAD_INDEX` | (UINT)-1 | Invalid world object index |

**Example usage:**
```cpp
#include "GameConstants.h"

// Check collision prediction
double impact_time = pShip->DetectCollisionCourse(*target);
if (impact_time == g_no_collide_sentinel) {
    // No collision predicted
}

// Check if asteroid will vaporize when shattered
if (asteroid->GetMass() / 3.0 < g_thing_minmass) {
    // Asteroid too small to fragment - will vaporize instead
}

// Calculate maximum possible velocity after thrust
double max_v_after_thrust = pShip->GetVelocity().rho + g_game_max_thrust_order_mag;
if (max_v_after_thrust > g_game_max_speed) {
    // Velocity will be clamped, causing governor fuel penalty
}
```

## Hello World Example

Here's a complete, minimal team implementation with clear documentation:

```cpp
/* HelloWorld.h - Minimal team implementation */
#ifndef HELLO_WORLD_H
#define HELLO_WORLD_H

#include "Team.h"
#include "Brain.h"

// Main team class
class HelloWorld : public CTeam {
public:
    HelloWorld() {}
    ~HelloWorld() {}

    void Init();  // Setup ships
    void Turn();  // Run each ship's AI
};

// Simple collector AI
class SimpleCollector : public CBrain {
public:
    CThing* target;  // Current target

    SimpleCollector() : target(nullptr) {}
    void Decide();   // Make decisions

private:
    void FindTarget();      // Select asteroid or station
    void NavigateToTarget(); // Move toward target
    void AvoidCollision();   // Emergency collision avoidance
    void MaintainShields();  // Keep shields charged
};

#endif

/* HelloWorld.C - Implementation */
#include "HelloWorld.h"
#include "GameConstants.h"
#include <cmath>

// Factory function - required by game
CTeam* CTeam::CreateTeam() {
    return new HelloWorld();
}

void HelloWorld::Init() {
    // Set team identity
    SetTeamNumber(1);
    SetName("Hello World");

    // Configure each ship with balanced fuel/cargo
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);

        // 35 fuel, 25 cargo - balanced configuration
        ship->SetCapacity(S_FUEL, 35.0);
        ship->SetCapacity(S_CARGO, 25.0);

        // Assign simple AI
        ship->SetBrain(new SimpleCollector());
    }
}

void HelloWorld::Turn() {
    // Each ship's brain decides independently
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (ship && ship->GetBrain()) {
            ship->GetBrain()->Decide();
        }
    }
}

void SimpleCollector::Decide() {
    // Safety checks
    if (!pShip) return;

    // Clear previous orders
    pShip->ResetOrders();

    // Priority system:
    // 1. Maintain shields
    MaintainShields();

    // 2. Avoid imminent collisions
    AvoidCollision();

    // 3. Find and navigate to target
    FindTarget();
    NavigateToTarget();
}

void SimpleCollector::FindTarget() {
    CWorld* world = pShip->GetWorld();
    CTeam* team = pShip->GetTeam();

    // If carrying cargo, go home
    if (pShip->GetAmount(S_CARGO) > 5.0) {
        target = team->GetStation();
        return;
    }

    // Find nearest collectible asteroid
    CThing* best = nullptr;
    double best_dist = 99999;

    // Scan all objects
    for (unsigned int i = world->UFirstIndex;
         i != BAD_INDEX;
         i = world->GetNextIndex(i)) {

        CThing* thing = world->GetThing(i);
        if (!thing || !thing->IsAlive()) continue;

        // Only want asteroids
        if (thing->GetKind() != ASTEROID) continue;

        CAsteroid* ast = (CAsteroid*)thing;

        // Prefer fuel if low
        bool need_fuel = (pShip->GetAmount(S_FUEL) < 15.0);
        if (need_fuel && ast->GetMaterial() != URANIUM) continue;

        // Check if it fits in our hold
        if (!pShip->AsteroidFits(ast)) continue;

        // Find closest
        double dist = pShip->GetPos().DistTo(thing->GetPos());
        if (dist < best_dist) {
            best_dist = dist;
            best = thing;
        }
    }

    target = best;
}

void SimpleCollector::NavigateToTarget() {
    if (!target) return;

    // Check if we'll collide anyway
    double impact_time = pShip->DetectCollisionCourse(*target);
    if (impact_time != g_no_collide_sentinel && impact_time < 10.0) {
        // Already on collision course - just drift
        return;
    }

    // Calculate intercept angle
    double time_estimate = 5.0;  // Simple estimate
    double angle = pShip->AngleToIntercept(*target, time_estimate);

    // Turn toward target
    if (fabs(angle) > 0.1) {
        pShip->SetOrder(O_TURN, angle);
    }

    // Thrust if facing target
    if (fabs(angle) < 0.5) {
        pShip->SetOrder(O_THRUST, 10.0);
    }
}

void SimpleCollector::AvoidCollision() {
    CWorld* world = pShip->GetWorld();

    // Check all objects for collisions
    for (unsigned int i = world->UFirstIndex;
         i != BAD_INDEX;
         i = world->GetNextIndex(i)) {

        CThing* thing = world->GetThing(i);
        if (!thing || thing == target) continue;

        // Check collision time
        double impact = pShip->DetectCollisionCourse(*thing);
        if (impact == NO_COLLIDE || impact > 3.0) continue;

        // Emergency evasion!
        // If it's an enemy ship or large asteroid, dodge
        if (thing->GetKind() == SHIP ||
            (thing->GetKind() == ASTEROID &&
             !pShip->AsteroidFits((CAsteroid*)thing))) {

            // Reverse thrust
            pShip->SetOrder(O_THRUST, -15.0);
            return;  // Handle only one emergency at a time
        }
    }
}

void SimpleCollector::MaintainShields() {
    double shields = pShip->GetAmount(S_SHIELD);
    double fuel = pShip->GetAmount(S_FUEL);

    // Keep 20 units of shields if possible
    if (shields < 20.0 && fuel > 10.0) {
        double needed = 20.0 - shields;
        double available = fuel - 10.0;  // Keep reserve

        pShip->SetOrder(O_SHIELD, min(needed, available));
    }
}
```

This Hello World example demonstrates:
- Clean, modular design with separate concerns
- Basic resource collection strategy
- Collision avoidance
- Shield management
- Clear documentation
- Minimal but functional implementation

Teams can extend this foundation by adding:
- Combat capabilities (laser targeting)
- Team coordination
- Advanced pathfinding
- Strategic decision making
- Enemy tracking and avoidance
## Global Constants

The engine centralises every tuning value in `team/src/GameConstants.h`
(`GameConstants.C` contains their default initialisation). Include that header
from your team code whenever you need these knobs instead of hard-coding
literals. Highlights:

* Timing/physics: `g_game_turn_duration`, `g_physics_simulation_dt`,
  `g_game_max_speed`, `g_game_max_thrust_order_mag`.
* Ship configuration: `g_ship_total_stat_capacity`,
  `g_ship_default_fuel_capacity`, `g_ship_default_cargo_capacity`,
  `g_ship_default_shield_capacity`, `g_ship_default_shield_amount`,
  `g_ship_spawn_size`, `g_ship_spawn_mass`, `g_ship_default_docking_distance`.
* Combat tuning: `g_laser_range_per_fuel_unit`,
  `g_laser_mass_scale_per_remaining_unit`, `g_laser_damage_mass_divisor`.
* Collision helpers: `g_no_collide_sentinel` signals “no impact”, while
  `g_no_damage_sentinel` indicates no incoming damage direction.
* Object constraints: `g_thing_minmass`, `g_thing_minsize`, plus the asteroid
  thresholds such as `g_asteroid_large_mass_threshold`.

Consult `GameConstants.h` for the full catalogue; every value is documented in
place so you can adjust behaviour confidently.
