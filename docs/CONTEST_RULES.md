# MechMania IV: The Vinyl Frontier - Contest Rules

## Game Overview
MechMania IV is a 2D space resource collection and combat game where two teams compete to collect vinyl (the primary resource) while managing fuel and avoiding or engaging in combat. The contest runs for 300 seconds of simulated time.

## Playing Field

### World Dimensions
- **Size:** 1024 × 1024 units (from [-512, 512) on both X and Y axes)
- **Topology:** Toroidal (edges wrap around) - objects leaving one edge appear on the opposite edge
- **Coordinate System:** Continuous floating-point coordinates

### Initial World Setup
- **2 Teams:** Each with 1 station and 4 ships
- **Station Placement:**
  - Team 0: (-256, -256) - bottom-left quadrant
  - Team 1: (256, 256) - top-right quadrant
- **Asteroids:** Randomly distributed vinyl and uranium asteroids of varying sizes

> **Developer note:** All of the numerical values above (team counts, asteroid
> mass, ship defaults, etc.) originate from `team/src/GameConstants.h`. The
> engine initialises them in `GameConstants.C`, so refer there if you need exact
> numbers or plan to tweak the simulation locally.

## Ships

### Configuration
At initialization, each team can allocate their ships' capacity between fuel and cargo. Defaults come from `g_ship_total_stat_capacity`, `g_ship_default_fuel_capacity`, and `g_ship_default_cargo_capacity` in `GameConstants.h`:
- **Total Capacity:** 60 tons per ship
- **Default Split:** 30 tons fuel, 30 tons cargo
- **Customizable:** Teams can adjust the fuel/cargo ratio at game start
- **Shield Capacity:** 8000 units (effectively unlimited)
- **Mass:** 40 tons (empty hull) + current fuel + current vinyl cargo
- **Maximum Speed:** 30 units/second
- **Size:** A radius 12 circle

### Ship Capabilities
Ships can perform the following actions each turn:
1. **Thrust** - Accelerate forward or backward
2. **Turn** - Rotate to change heading
3. **Shoot Laser** - Fire an energy beam
4. **Charge Shield** - Add energy to defensive shields
5. **Jettison** - Eject cargo or fuel into space

## Resources

### Vinyl (Primary Resource)
- **Color:** Purple asteroids
- **Collection:** Ships collect vinyl by colliding with vinyl asteroids
- **Storage:** Stored in ship's cargo hold
- **Scoring:** Deposited at your station to score points
- **Victory:** Team with most vinyl at their station wins

### Uranium (Fuel)
- **Color:** Green asteroids
- **Collection:** Ships collect uranium by colliding with uranium asteroids
- **Storage:** Directly added to ship's fuel tank
- **Usage:** Powers all ship operations

## Movement and Physics

### Thrust Mechanics
- **Fuel Cost:** 1 ton of fuel accelerates a 40-ton hull from 0 to 180 units/second (6 × max speed)
- **Formula:** Fuel = |thrust| × ship_mass / (6 × max_speed × empty_mass)
- **Direction:** Can thrust forward (positive) or backward (negative)
- **While Docked:** No fuel consumed for thrust

### Rotation Mechanics
- **Fuel Cost:** 1 ton of fuel rotates a 40-ton hull 6 full circles (12π radians)
- **Formula:** Fuel = |rotation| × ship_mass / (6 × 2π × empty_mass)
- **While Docked:** No fuel consumed for rotation
- **No Angular Momentum:** After each turn order to a desired heading a ship does not continue to rotate

### Physics
- **Momentum:** Ships maintain velocity when not thrusting (Newtonian physics)
- **Mass Effects:** More cargo/fuel makes ship heavier, requiring more fuel to maneuver
- **Maximum Velocity:** 30 units/second (enforced)

### Maximum Velocity Enforcement

Ships can issue **thrust orders up to ±60** units/second of **Δv** along their current **orientation** (forward = positive, reverse = negative).  
**Orientation is independent of velocity**: a ship “faces” its orientation; it may be traveling another way.

**How thrust is applied (per second):**
1. The order’s Δv is split into **five equal instantaneous impulses** at **t = 0.0, 0.2, 0.4, 0.6, 0.8 s**.  
2. After each impulse, the engine computes \(v_{\text{des}} = v_{\text{old}} + \Delta v\).  
3. If \(\lVert v_{\text{des}}\rVert \le 30\) (**speed circle**), it is accepted.  
4. If \(\lVert v_{\text{des}}\rVert > 30\), the engine **projects** \(v_{\text{des}}\) **back to the circle** along the same ray (Option‑A projection).

**Governor penalty (fuel):** When projection is needed, the **overshoot length**
\[
\text{overshoot} \;=\; \big(\ \lVert v_{\text{des}}\rVert - 30\ \big)_+
\]
is charged as extra fuel using the **same per-Δv fuel slope** as base thrust for that impulse. (Think of it as paying for the “red” piece the engine chopped off.)  
**Base thrust fuel** for an impulse of magnitude \(\lVert \Delta v\rVert\) is:
\[
\text{fuel}_\text{base} \;=\; \lVert \Delta v\rVert \cdot
\frac{\text{ship\_mass}}{6 \cdot \text{max\_speed} \cdot \text{empty\_mass}}
\]
**Governor fuel** for that impulse is:
\[
\text{fuel}_\text{gov} \;=\; \text{overshoot} \cdot
\frac{\text{ship\_mass}}{6 \cdot \text{max\_speed} \cdot \text{empty\_mass}}
\]
Totals over the second are the sums of the five impulses.  
**While docked:** thrust/turn remain **free**; projection still limits speed to 30 but **no governor fuel** is charged.

Consider these examples:

| Current Velocity + Thrust Delta-V <= Maxspeed | Current Velocity + Thrust Delta-V > Maxspeed |
|:---:|:---:|
| **Positive thrust case:**<br/>![Positive thrust within limit](diagrams/thrust_positive_within_limit.svg) | **Positive thrust case:**<br/>![Positive thrust exceeds limit](diagrams/thrust_positive_exceeds_limit.svg) |
| **Negative thrust case:**<br/>![Negative thrust within limit](diagrams/thrust_negative_within_limit.svg) | **Negative thrust case:**<br/>![Negative thrust exceeds limit](diagrams/thrust_negative_exceeds_limit.svg) |
## Combat System

### Damage Overview
- **Ships** lose shield points whenever they collide with asteroids or other ships, or when they are struck by lasers. If incoming damage exceeds the remaining shields the ship is destroyed. Docked ships cannot be harmed and take no damage when docking with their own station.
- **Asteroids** are damaged by ship collisions and laser fire. Any qualifying hit fractures the rock into three equal-mass pieces unless the resulting fragments would fall below the minimum mass (`g_thing_minmass`, 3 tons), in which case the asteroid vaporises. Asteroids bounce off stations and do not collide with one another.
- **Stations** only take laser damage. Each point of effective laser damage removes one ton of stored vinyl, floored at zero.

### Turn Resolution Order
After both teams submit orders, the server resolves each turn in four phases:
1. Apply shield-charge orders.
2. Run one physics sub-step (`g_physics_simulation_dt`, default 0.2 s) that applies thrust/turn/drift and handles collisions.
3. Resolve every laser shot, applying damage immediately.
4. Run the remaining four physics sub-steps to complete the turn’s movement.

### Laser Mechanics
- **Range:** Up to `min(fWXMax, fWYMax)` units (512 with default map).
- **Fuel Cost:** `beam_length / g_laser_range_per_fuel_unit` (defaults to 50 units of beam per ton of fuel). Ships cannot fire while docked.
- **Impact Mass:** The engine treats a beam hit as a collision with a virtual object of mass  
  `g_laser_mass_scale_per_remaining_unit × max(0, beam_length − distance_to_target)` (default scale: 30).
- **Shield Damage:** The virtual mass feeds into the collision damage equation (see below), so longer beams are only efficient when the target is close. At point-blank range, every ton of fuel spent on a laser forces the target to spend roughly 1.5 tons to restore shields. Beyond one third of the chosen beam length the laser becomes less fuel-efficient than shield charging, and at two thirds it costs twice as much fuel as the defender spends.
- **Line of Sight:** Beams strike the first object along their path, including friendly ships or stations.

### Collision Damage (Ships)
- When a ship collides with an asteroid or another ship, it loses  
  `damage = mass_other × relative_speed / g_laser_damage_mass_divisor` shield points (`g_laser_damage_mass_divisor` defaults to 1000).
- Only the other object’s mass is used in the calculation; the striking ship’s mass is irrelevant to the damage it receives.
- Ships weigh their 40-ton hull plus carried fuel and vinyl (0–60 tons combined), while naturally spawned asteroids range from roughly 3 to 40 tons. Because each object’s velocity is capped at `g_game_max_speed` (30 units/s), peak relative speeds hover around 60 units/s, yielding worst-case hits of ~6 shield points (ship vs. ship) or ~2.4 (ship vs. heavy asteroid). Engine subtleties can briefly push the ceiling a bit higher (~8.4), but those spikes are rare.

### Laser Targets by Object Type
- **Ships:** Lose shields as described above; destruction occurs when shields drop below zero.
- **Asteroids:** Shatter if the computed damage is ≥1 shield point; otherwise the shot has no effect (damage does not accumulate between hits).
- **Stations:** Lose vinyl equal to the computed damage, clamped at zero. Friendly fire is possible.

### Shields
- Charging shields consumes fuel at a 1:1 ratio and can be done every turn (even while docked, though fuel still comes out of the tank). There is no automatic regeneration.
- Default maximum shield capacity is extremely high (`g_ship_default_shield_capacity` = 8000), so the practical limit is usually the ship’s fuel reserves.

### Default Combat Constants
The examples above assume the standard constants defined in `team/src/GameConstants.C` and `team/src/Coord.h`:

| Constant | Default | Description |
| --- | --- | --- |
| `g_laser_range_per_fuel_unit` | `50.0` | Beam length purchasable per ton of fuel. |
| `g_laser_mass_scale_per_remaining_unit` | `30.0` | Converts remaining beam length into virtual impact mass. |
| `g_laser_damage_mass_divisor` | `1000.0` | Scales impact mass into shield damage. |
| `g_game_max_speed` | `30.0` | Top speed for any object. |
| `g_thing_minmass` | `3.0` | Minimum fragment mass when asteroids shatter. |
| `fWXMax`, `fWYMax` | `512.0` | Half-width/height of the map; sets the laser range ceiling. |

## Collisions

### Ship-Asteroid Collisions
- **Small Asteroids (fits in hold):**
  - Uranium → Added to fuel tank
  - Vinyl → Added to cargo hold
  - Asteroid destroyed
- **Large Asteroids (doesn't fit):**
  - Ship takes collision damage (relative_velocity*asteroid_mass / 1000)
  - Asteroid destroyed and either 3 smaller asteroids are created, or the asteroid is reduced to useless space dust if the resulting size would be too small
  - Both objects bounce apart

### Ship-Ship Collisions
- **Damage:** Both ships take relative_momentum / 1000 shield damage
- **Physics:** Elastic collision (both ships bounce)

### Ship-Station Collisions

#### Your Own Station
- **Docking:** Ship becomes stationary at station
- **Cargo Transfer:** All vinyl automatically deposited (added to team score)
- **Fuel Conservation:** While docked, thrust and turn consume NO fuel (free maneuvering)
- **Fuel Tank:** NOT refilled - remains at pre-docking level
- **Shield Recharge:** Manual (use shield charge order with actual fuel from tank)
- **Protection:** Immune to damage while docked
- **Orientation:** You can change your heading while docked (costs no fuel)
- **Departure:** Thrust to leave station (costs no fuel while still docked)

#### Enemy Station
- **Same as your station:** THIS MEANS YOU CAN GIVE VINYL TO THE ENEMY STATION!

### Station-Asteroid Collisions
- **Damage:** No damage - stations are indestructible to asteroid impacts
- **Physics:** Asteroid bounces off (perfectly elastic collision)

## Orders and Actions

### Order System
- **Simultaneous:** All orders can be issued in same turn
- **Compatibility:** Shield and Lasers can be used every turn. Only one of Jettison, Turn, or Thrust may be used per turn.
- **Execution:** Physics motion happens before combat each turn

### Order Limits
- **Shield:** Limited by available fuel
- **Laser:** Limited by fuel (Beam length/50 fuel cost), maximum beam length is 512, and cannot fire while docked
- **Thrust:** ±60 units/second change (limited by fuel)
- **Turn:** ±2π radians (limited by fuel). Note - you turn to the desired heading and stop - ships don't continue to spin after turning.
- **Jettison:** Limited by cargo/fuel carried

### Jettison Mechanics
- **Purpose:** Create new asteroids from ship resources, lightens ship to reduce fuel costs of maneuvering, prevent docking at enemy station with Vinyl, ??throw rocks at enemy ships??
- **Minimum:** Must jettison at least 3 tons, specifying a lower amount will have no effect
- **Result:** Creates new asteroid at ship location

## Information Available to Teams

### Complete Knowledge
Teams have **complete visibility** of the entire game state through all public methods on world objects:
- Position and velocity of all objects (ships, asteroids, stations)
- Type and size of all asteroids
- **All ships' fuel, cargo, and shield levels** (via `GetAmount(S_FUEL)`, `GetAmount(S_CARGO)`, `GetAmount(S_SHIELD)`)
- All ships' team affiliations (via `GetTeam()`)
- All stations' vinyl stores (via `GetVinylStore()`)
- All teams' total vinyl scores
- Total game time elapsed
- Prior turn orders for all ships (via `GetOrder(O_THRUST)`, `GetOrder(O_SHOOT)`, etc.)

### Limited Knowledge
Teams do NOT know:
- Enemy ships' **current turn** planned orders (orders are only visible after they execute)

### Communication
- Teams can print text messages visible to observers
- Messages displayed in observer console
- Maximum 512 characters per message buffer

## Victory Conditions

### Game Duration
- **Length:** 300 seconds of simulated time
- **Time Step:** Variable (typically 1 second per simulation cycle)

### Scoring
- **Primary Score:** Total vinyl stored at team's station
- **Measurement:** In tons (floating point)
- **Winner:** Team with highest vinyl score when time expires
- **Note:** The server (mm4serv) only prints final scores - it does not announce a winner. Determining the winner is left to observers/judges.

### Ties
- **No tiebreaker logic exists in the code** - determining the winner in case of a tie would be left to human judges
- In practice, ties are extremely unlikely due to floating-point vinyl values

## Strategic Considerations

### Fuel Management
- Starting fuel must last entire game (unless collecting uranium)
- **Stations do NOT refill fuel** - only provide free thrust/turn while docked
- More cargo capacity means less fuel capacity
- Heavier ships (full of cargo) require more fuel to maneuver
- Can jettison cargo in emergency to reduce mass
- Uranium asteroids are the ONLY way to refuel during the game

### Combat vs Collection
- Combat consumes significant fuel (lasers, maneuvering)
- Destroyed ships cannot collect resources
- Can attack enemy stations to reduce their score
- Shield maintenance requires fuel investment

### Asteroid Management
- Large asteroids can be broken into smaller pieces with lasers (or crashing into them with ships)
- Small pieces easier to collect but require more trips
- Uranium asteroids provide fuel but don't score points
- Can jettison resources to create obstacles/distractions

### Station Strategy
- Regular returns to base required to deposit vinyl
- Docking provides free rotation and departure thrust
- Stations are vulnerable to laser attacks

## Technical Notes

### Turn Sequence
1. Teams submit orders
2. Physics simulation (movement, collisions)
   * Internally handled in five, 0.2 second tics, per 1 second game turn
3. Combat resolution (lasers, damage)
4. Resource collection
5. Score updates
6. World state broadcast

### Coordinate System
- Origin (0,0) at world center
- Positive X to the right
- Positive Y upward
- Angles in radians (0 = east,πPI/2 = north)

### Units
- Distance: miles
- Mass: tons
- Time: seconds
- Speed: miles/second
- Fuel: tons
- Damage: shield units
