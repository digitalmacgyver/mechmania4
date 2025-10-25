# MechMania IV: The Vinyl Frontier - Contest Rules

## Game Overview
MechMania IV is a 2D space resource collection and combat game where two teams compete to collect vinyl (the primary resource) while managing fuel and avoiding or engaging in combat. The contest runs for 300 seconds of simulated time.

> **Looking for detail?** This rules summary targets players. Mechanics are documented in depth in the specialized references:
> - `CONTEST_PHYSICS_FOR_CONTESTANTS.md` / `CONTEST_PHYSICS_FOR_DEVS.md`
> - `CONTEST_NAVIGATION_FOR_CONTESTANTS.md` / `CONTEST_NAVIGATION_FOR_DEVS.md`
> - `CONTEST_DAMAGE_FOR_CONTESTANTS.md` / `CONTEST_DAMAGE_FOR_DEVS.md`

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

### Ship Initialization Timing

**IMPORTANT:** Ship capacity configuration (fuel/cargo split) must be set during the `Init()` method at game start. This is the ONLY time when capacity changes affect the actual game world.

- **During Init():** `SetCapacity()` calls are serialized and sent to the server, establishing each ship's configuration for the entire game
- **After Init():** The server sends a fresh world state to teams every turn, which includes all ship capacities
- **No capacity orders exist:** There are no O_ orders (like O_THRUST, O_LASER, etc.) that can modify ship capacities during gameplay
- **Result:** Any `SetCapacity()` calls after `Init()` will only modify your team's local view, creating a discrepancy between what your team sees and the actual game state

### Ship Capabilities
Ships can perform the following actions each turn:
1. **Thrust** - Accelerate forward or backward
2. **Turn** - Rotate to change heading
3. **Shoot Laser** - Fire an energy beam
4. **Charge Shield** - Add energy to defensive shields
5. **Jettison** - Eject cargo or fuel into space

### Initial Ship Orientation
All ships spawn docked at their team's station, **facing toward the map center** for competitive balance:
- **Team at (-256, -256):** Ships face **east** (orientation = 0 radians, pointing right toward center)
- **Team at (256, 256):** Ships face **west** (orientation = π radians, pointing left toward center)

This ensures both teams have symmetric starting conditions.

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
- **While Docked:** No fuel consumed for thrust orders
- **Launch Turn:** When launching from a station, the entire turn is free (no fuel consumed even after undocking)

### Rotation Mechanics
- **Physics Model:** Ship modeled as uniform disk with triangular angular velocity profile (accelerates to peak at 0.5s, decelerates to zero at 1.0s)
- **Fuel Cost:** Based on rotational kinetic energy. For a 40-ton empty hull, 1 ton of fuel provides approximately 1.42 full rotations (8.95 radians)
- **Formula:** Fuel = 2 × ship_mass × ship_radius² × angle² / (turn_duration² × energy_per_fuel_ton)
  - Where energy_per_fuel_ton = 648,000 (same energy budget as thrust physics)
  - Ship radius = 12 units
  - Turn duration = 1.0 second
- **While Docked:** No fuel consumed for rotation
- **No Angular Momentum:** After each turn order completes, angular velocity returns to zero (ship does not continue to spin)

### Physics
- **Momentum:** Ships maintain velocity when not thrusting (Newtonian physics)
- **Mass Effects:** More cargo/fuel makes ship heavier, requiring more fuel to maneuver
- **Maximum Velocity:** 30 units/second (enforced)

### Maximum Velocity Enforcement

Ships can issue **thrust orders up to ±60** units/second of **Δv** along their current **orientation** (forward = positive, reverse = negative).  
**Orientation is independent of velocity**: a ship “faces” its orientation; it may be traveling another way.

**How thrust is applied (per second):**
1. The order’s Δv is split into **five equal instantaneous impulses** at **t = 0.0, 0.2, 0.4, 0.6, 0.8 s**.  
2. After each impulse, the engine computes $v_{\text{des}} = v_{\text{old}} + \Delta v$.
3. If $\lVert v_{\text{des}}\rVert \le 30$ (**speed circle**), it is accepted.
4. If $\lVert v_{\text{des}}\rVert > 30$, the engine **projects** $v_{\text{des}}$ **back to the circle** along the same ray (Option‑A projection).

**Governor penalty (fuel):** When projection is needed, the **overshoot length**

$$
\text{overshoot} \;=\; \big(\ \lVert v_{\text{des}}\rVert - 30\ \big)_+
$$

is charged as extra fuel using the **same per-Δv fuel slope** as base thrust for that impulse. (Think of it as paying for the "red" piece the engine chopped off.)
**Base thrust fuel** for an impulse of magnitude $\lVert \Delta v\rVert$ is:

$$
\text{fuel}_\text{base} \;=\; \lVert \Delta v\rVert \cdot
\frac{\text{ship\_mass}}{6 \cdot \text{max\_speed} \cdot \text{empty\_mass}}
$$

**Governor fuel** for that impulse is:

$$
\text{fuel}_\text{gov} \;=\; \text{overshoot} \cdot
\frac{\text{ship\_mass}}{6 \cdot \text{max\_speed} \cdot \text{empty\_mass}}
$$
Totals over the second are the sums of the five impulses.  
**While docked:** thrust/turn remain **free**; projection still limits speed to 30 but **no governor fuel** is charged.

Consider these examples:

| Current Velocity + Thrust Delta-V <= Maxspeed | Current Velocity + Thrust Delta-V > Maxspeed |
|:---:|:---:|
| **Positive thrust case:**<br/>![Positive thrust within limit](diagrams/thrust_positive_within_limit.svg) | **Positive thrust case:**<br/>![Positive thrust exceeds limit](diagrams/thrust_positive_exceeds_limit.svg) |
| **Negative thrust case:**<br/>![Negative thrust within limit](diagrams/thrust_negative_within_limit.svg) | **Negative thrust case:**<br/>![Negative thrust exceeds limit](diagrams/thrust_negative_exceeds_limit.svg) |
## Combat System

### Damage Overview
- **Ships** lose shields whenever they collide with asteroids or other ships, or when they are struck by lasers (1000 damage = 1 shield unit depleted). If incoming damage exceeds the remaining shields the ship is destroyed. Docked ships cannot be harmed and take no damage when docking with their own station.
- **Asteroids** are damaged by ship collisions and laser fire. Any qualifying hit fractures the rock into three equal-mass pieces unless the resulting fragments would fall below the minimum mass (`g_thing_minmass`, 3 tons), in which case the asteroid vaporises. Asteroids **do collide with stations** (perfectly elastic bounce, treating station as infinite mass) but **do not collide with other asteroids**.
- **Stations** only take laser damage. Each point of effective laser damage removes one ton of stored vinyl, floored at zero. Stations are immovable and treat asteroid collisions as perfectly elastic bounces (asteroid reflects off without station movement).

### Turn Resolution Order
Each turn spans one simulated second divided into five physics sub-steps (default `g_physics_simulation_dt = 0.2`). Within **every** sub-step the engine performs:

1. **Jettison resolution** – queued cargo/fuel dumps spawn asteroids and adjust ship mass.
2. **Shield charging** – fuel is spent to raise shields (clamped to capacity).
3. **Turn + thrust integration** – the requested rotation/thrust increment is applied with velocity clamping.
4. **Drift** – positions and orientations advance by the sub-step duration.
5. **Collision pass** – snapshot-based collision detection and resolution.
6. **Spawn/cull** – add newly spawned objects and remove destroyed ones.

After the final sub-step, **laser orders fire** once for the turn. A full breakdown of the timing is documented in `CONTEST_NAVIGATION_FOR_DEVS.md`; the contestant-facing summary appears in `CONTEST_NAVIGATION_FOR_CONTESTANTS.md`.

### Laser Mechanics
> Detailed numbers and examples live in `CONTEST_DAMAGE_FOR_CONTESTANTS.md`.
- **Range:** Up to `min(fWXMax, fWYMax)` units (512 with default map).
- **Fuel Cost:** `beam_length / g_laser_range_per_fuel_unit` (defaults to 50 units of beam per ton of fuel). Ships cannot fire while docked.
- **Damage Formula:** `damage = g_laser_mass_scale_per_remaining_unit × max(0, beam_length − distance_to_target)`
  - With default scale of 30: `damage = 30 × (beam_length − distance_to_target)`
  - **Example:** A beam with 100 units of remaining length deals 3000 damage
- **Shield Damage:** The damage feeds into shield depletion (1000 damage = 1 shield unit), so longer beams are only efficient when the target is close. At point-blank range, every ton of fuel spent on a laser forces the target to spend roughly 1.5 tons to restore shields. Beyond one third of the chosen beam length the laser becomes less fuel-efficient than shield charging, and at two thirds it costs twice as much fuel as the defender spends.
- **Line of Sight:** Beams strike the first object along their path, including friendly ships or stations.

### Collision Damage (Ships)
- When a ship collides with an asteroid or another ship, both objects take damage based on the **momentum change** they experience:
  ```
  damage = |Δp| / g_laser_damage_mass_divisor
  ```
  where `|Δp|` is the magnitude of momentum change and `g_laser_damage_mass_divisor` defaults to 1000.

- **Damage to shield conversion:** 1000 damage points = 1 shield unit depleted

- **Both objects take the same damage** in a collision (Newton's 3rd Law: equal and opposite momentum changes)

- The momentum change depends on the collision type:
  - **Elastic collisions** (ship-ship, ship-large asteroid): `|Δp| = (2 × m₁ × m₂ / (m₁ + m₂)) × v_rel_normal`
  - **Inelastic collisions** (ship-small asteroid): `|Δp| = (m₁ × m₂ / (m₁ + m₂)) × v_rel`
- `v_rel_normal` is measured along the geometric line-of-centers. If both centres coincide, the engine uses a shared random heading for the impulse direction.
- Ships weigh their 40-ton hull plus carried fuel and vinyl (0–60 tons combined), while naturally spawned asteroids range from roughly 3 to 40 tons. With velocity capped at `g_game_max_speed` (30 units/s), typical collisions cause 1000-3000 damage (1-3 shield units depleted) per ship.

### Laser Targets by Object Type
- **Ships:** Lose shields as described above; destruction occurs when shields drop below zero.
  - **Shield depletion:** 1000 damage = 1 shield unit depleted
- **Asteroids:** Shatter if the computed damage is ≥1000; otherwise the shot has no effect (damage does not accumulate between hits).
  - **Example:** Since damage = 30 × (beam_length − distance), you need at least 33.33 units of remaining beam to reach the 1000 damage threshold
- **Stations:** Lose vinyl based on damage dealt.
  - **Vinyl depletion:** 1000 damage = 1 vinyl ton removed
  - Vinyl is clamped at zero. Friendly fire is possible.

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

The engine models collisions with Newtonian physics. All collision handling follows conservation of momentum and energy principles.

### Ship-Asteroid Collisions

#### Small Asteroids (fits in cargo hold)
- **Collection:** Asteroid absorbed by ship
  - Uranium → Added to fuel tank (up to capacity)
  - Vinyl → Added to cargo hold (up to capacity)
  - Overflow is lost (no jettison on collection)
- **Physics:** Perfectly inelastic collision
  - Final velocity = `(m_ship × v_ship + m_asteroid × v_asteroid) / (m_ship + m_asteroid)`
  - Momentum conserved, kinetic energy lost
- **Damage:** No damage (successful collection)
- **Result:** Asteroid destroyed (absorbed into ship)

#### Large Asteroids (doesn't fit in cargo hold)
- **Damage:** Both ship and asteroid take damage based on momentum change (see Collision Damage formula above)
- **Physics:** Perfectly elastic collision
  - Both objects bounce according to elastic collision formulas
  - Momentum and kinetic energy conserved
  - Each object's velocity changes based on mass ratio
- **Fragmentation:** Asteroid shatters into 3 equal-mass pieces **regardless of damage amount**
  - This is different from laser hits, which require ≥1 damage threshold to shatter
  - Fragments inherit asteroid's post-collision velocity (center of mass)
  - Plus symmetric spread pattern at 120° intervals
  - If resulting fragments < 3 tons each, asteroid vaporizes (no fragments)

### Ship-Ship Collisions
- **Damage:** Both ships take equal damage based on momentum change (see Collision Damage formula above)
  - Damage is symmetric: both ships experience equal |Δp| (Newton's 3rd Law)
  - Heavier ships experience smaller velocity changes but same total damage
- **Physics:** Perfectly elastic collision
  - Both ships bounce using 2D elastic collision formulas
  - Momentum and kinetic energy conserved
  - Velocity changes depend on mass ratio and impact angle
- **Result:** Both ships survive (unless shields depleted)

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
- **Physics:** Ship velocity set to zero (inelastic docking)

**Launch Position (Undocking):**
When a ship issues `O_THRUST` while docked, it is **teleported to a safe distance** before physics begins:
- **Launch Distance:** `Station_Size + Ship_Size + (Ship_Size / 2)` = **48 units from station center** (with default sizes)
- **Launch Direction:**
  - Forward thrust (`O_THRUST > 0`): Ship placed **forward** along its orientation vector
  - Reverse thrust (`O_THRUST < 0`): Ship placed **backward** opposite to its orientation vector
- **Purpose:** Ensures ship starts beyond collision range (30+12=42 units) to prevent immediate re-docking
- **Example:** Ship docked at (256, 256) with orientation 0° (east):
  - `O_THRUST = 10.0` → Teleport to (304, 256) - 48 units east
  - `O_THRUST = -10.0` → Teleport to (208, 256) - 48 units west

#### Enemy Station
- **Same as your station:** THIS MEANS YOU CAN GIVE VINYL TO THE ENEMY STATION!
- Docking mechanics identical regardless of team affiliation
- No concept of "locked" stations or combat while docked

### Asteroid-Station Collisions
- **Damage:** No damage to station (indestructible)
- **Physics:** Perfectly elastic collision with infinite mass
  - Asteroid velocity reflects off station surface (specular reflection)
  - Asteroid speed unchanged, only direction reversed
  - Station does not move (infinite mass approximation)
  - Momentum NOT conserved (station has infinite mass)
  - Kinetic energy IS conserved (asteroid speed constant)

### Asteroid-Laser "Collisions"
- **Threshold:** Laser must deal ≥1000 damage to shatter asteroid
  - Damage = `30.0 × max(0, beam_length - distance_to_target)` (with default scale)
  - **Example:** Need at least 33.33 units of remaining beam to reach 1000 damage threshold
- **Physics:** Perfectly inelastic collision with laser energy
  - Laser treated as virtual object with mass proportional to beam energy
  - Asteroid absorbs laser momentum
  - Final velocity = `(m_asteroid × v_asteroid + m_laser × v_laser) / (m_asteroid + m_laser)`
- **Fragmentation:** If damage ≥1000, asteroid shatters into 3 pieces
  - Fragments inherit post-collision velocity (center of mass)
  - Plus spread pattern scaled by laser energy
  - If fragments < 3 tons each, asteroid vaporizes

### Collision Physics Summary

| Collision Type | Physics Model | Momentum | Energy | Fragments |
|---|---|---|---|---|
| Ship ↔ Small Asteroid | Perfectly inelastic | Conserved | Lost | No (absorbed) |
| Ship ↔ Large Asteroid | Perfectly elastic | Conserved | Conserved | Yes (3 pieces) |
| Ship ↔ Ship | Perfectly elastic | Conserved | Conserved | No |
| Ship ↔ Own Station | Inelastic docking | Not conserved | Lost | No |
| Asteroid ↔ Station | Elastic (infinite mass) | Not conserved* | Conserved | No |
| Asteroid ↔ Laser | Perfectly inelastic | Conserved | Lost | Yes (if damage ≥1) |

\* Momentum not conserved due to infinite station mass; station acts as immovable object.

## Orders and Actions

### Order System
- **Order restrictions:**
  - **Movement orders (mutually exclusive):** At most ONE of `O_THRUST`, `O_TURN`, or `O_JETTISON` may be issued per turn. Issuing any of these cancels the other two.
  - **Combat/defense orders (combinable):** Both `O_LASER` and `O_SHIELD` may be issued every turn, and can be combined with any movement order.
- **Execution:** Physics motion happens before combat each turn

### Order Limits
- **Shield:** Limited by available fuel
- **Laser:** Limited by fuel (Beam length/50 fuel cost), maximum beam length is 512, and cannot fire while docked
- **Thrust:** ±60 units/second change (limited by fuel)
- **Turn:** Rotation in radians (normalized to [-π, π], limited by fuel). Ships always take the shortest path - you cannot choose to turn "the long way around". After each turn completes, angular velocity returns to zero (ships don't continue to spin).
- **Jettison:** Limited by cargo/fuel carried

### Jettison Mechanics
- **Purpose:** Create new asteroids from ship resources, lightens ship to reduce fuel costs of maneuvering, prevent docking at enemy station with Vinyl, create obstacles or projectiles
- **Minimum:** Must jettison at least 3 tons, specifying a lower amount will have no effect
- **Docking:** Jettison requests issued while a ship is docked are ignored (no cargo/fuel is ejected and orders remain unchanged).
- **Physics:** Momentum is conserved during jettison
  - **Asteroid spawn position:** Placed at distance `(ship_radius + asteroid_radius) × 1.15` along ship's **orientation** vector (not velocity vector)
  - **Asteroid velocity:** Magnitude equals ship's current speed, direction equals ship's **orientation** (the direction the ship is facing)
  - **Ship recoil:** Ship's new velocity = `(ship_momentum - asteroid_momentum) / new_ship_mass`
  - **Result:** Ship experiences recoil opposite to its facing direction; heavier jettisons cause more recoil
- **Strategic notes:**
  - Jettisoning while moving sideways (velocity ≠ orientation) will cause the asteroid to move in a different direction than the ship
  - Can be used to shed mass for more fuel-efficient maneuvering
  - Jettisoned asteroids behave identically to natural asteroids and can be collected by any ship

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
- **Score:** Total vinyl stored at team's station
- **Measurement:** In tons (floating point)
- **Winner:** Team with highest vinyl score when time expires
- **Note:** The server (mm4serv) only prints final scores - it does not announce a winner. Determining the winner is left to observers/judges.

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
- Angles in radians (0 = east, π/2 = north)

### Units
- Distance: miles
- Mass: tons
- Time: seconds
- Speed: miles/second
- Fuel: tons
- Damage: shield units
