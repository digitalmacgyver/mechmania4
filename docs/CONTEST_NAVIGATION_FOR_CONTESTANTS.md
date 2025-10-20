# Contest Navigation Reference (Contestants)

> **Scope.** This guide describes the **default modern flight model** used by the tournament server. Legacy compatibility modes that recreate the 1998 mechanics behave differently and are not covered here.

## 1. Orders at a Glance
- **One update per second:** Every ship issues at most one order packet per second. The engine then simulates a full second of movement using that input.
- **Available orders:**
  - `O_THRUST` (Δv along current facing, forward positive, reverse negative).
  - `O_TURN` (change in orientation, radians).
  - `O_LASER` (beam length).
  - `O_SHIELD` (shield increase).
  - `O_JETTISON` (separate helper to eject vinyl or fuel).
- **Mutual exclusivity:** Thrust and turn can be combined with laser and shield orders. Jettison is handled before either movement order; jettisoning cargo/fuel and thrusting in the same turn is allowed.

## 2. Coordinate System & World Geometry
- **World:** 1024 × 1024 torus. Leaving one edge wraps to the opposite edge without losing speed.
- **Facing vs motion:** Orientation is independent of velocity. A ship can drift sideways relative to its nose.
- **Angles:** Radians, increasing counter-clockwise. 0 radians points right, π/2 points “up” in mathematical terms. The observer UI renders +Y downward, so visuals appear inverted.

## 3. Movement Fundamentals
- **Velocity clamping:** Ship speed is limited to 30 units/second. If cumulative thrust would exceed that, the engine trims the change to stay on the 30-unit circle. Excess thrust still consumes fuel (see developer guide).
- **Thrust range:** ±60 units/second of Δv per turn along the ship’s nose.
- **Turning:** Up to ±2π radians per turn. Once a turn completes, angular velocity resets to zero (no spinning).
- **Drift:** In the absence of thrust or collisions, ships retain their current velocity indefinitely.
- **Fine timestep:** Behind the scenes the engine applies thrust and turn in several smaller slices inside each one-second turn. This improves numerical stability but may produce slight deviations from simple “thrust × dt” mental math.

## 4. Docking & Undocking
- **Docking:** Colliding with your own station teleports the ship to the station centre, zeroes velocity, and sets docked state. Docked ships cannot be hit by other objects.
- **Launch:** The first thrust order while docked places the ship 48 units away from the station centre along (or opposite) its orientation (station_radius + ship_radius + ship_radius/2). The entire launch turn costs no fuel. After launch, normal costs resume.

## 5. Jettison
- Ships can eject vinyl (cargo) or uranium (fuel) as small asteroids.
- Jettison occurs before movement. A ship can clear cargo space mid-turn and immediately collect a new asteroid in the same second.

## 6. Lasers & Shields Timing
- **Shields:** Applied at the start of the turn before movement. Fuel is consumed immediately.
- **Lasers:** Fired after the full one-second movement finishes. Firing while docked is not allowed.

## 7. Practical Tips
- Plan manoeuvres with the 30 u/s cap in mind. Attempting to thrust past it wastes fuel.
- Remember orientation resets after each turn. Use `O_TURN` to face the direction you expect to thrust next.
- Launch turns are free—use them to gain momentum even when low on fuel.
- Wraparound matters: shortest travel vectors often cross an edge.

For deeper implementation details (fuel formulas, sub-step scheduling, collision sequencing), see the developer navigation guide.
