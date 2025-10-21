# Contest Navigation Reference (Developers)

> **Scope.** This document describes the **default modern flight pipeline**. Legacy compatibility switches (`--legacy-mode`, `--legacy-physics`, etc.) intentionally diverge and are not detailed here.

## 1. Simulation Cadence
- **Turn length:** `g_game_turn_duration = 1.0` second.
- **Physics slices:** `g_physics_simulation_dt` (default 0.2 s). The server executes `ceil(turn_duration / dt)` slices per turn (five with defaults).
- **Order ingestion:** Each team submits orders once per turn. Orders are latched until the next turn; there is no mid-turn reissue.

### 1.1 Per-slice Execution Order
1. **Handle jettison:** Convert queued jettison orders into asteroid spawn requests. Remove the mass from the ship.
2. **Charge shields:** `O_SHIELD` consumes fuel immediately; shield amount clamps to max capacity.
3. **Integrate turn/thrust:** `ProcessThrustOrderNew` / `ProcessThrustDriftNew` and the triangular rotation model apply the portion of the order scheduled for this slice.
4. **Drift:** Positions advance via `Pos += Vel * dt`; orientation integrates angular velocity.
5. **Collisions:** `CollisionEvaluationNew()` runs using snapshots captured at the start of the slice.
6. **Apply additions/removals:** Spawn queued objects, remove corpses.
- **Post-slices:** After the last slice, `LaserModelNew()` handles laser orders for the turn.

## 2. Order Semantics
- **Thrust (`O_THRUST`):** Requested Δv ∈ [-60, 60] u/s. Split evenly across slices. Fuel cost uses `CalcThrustCost`, scaling with total mass. While docked, fuel cost is forced to 0; the entire launch turn is free.
- **Turn (`O_TURN`):** Requested angle may be any value (e.g., +3π/2 or -5π). The engine normalizes it to the shortest path ∈ [-π, π] to achieve the target heading (e.g., +3π/2 → -π/2). The ship is modeled as a **uniform disk** for rotational physics. The triangular angular velocity profile linearly accelerates from rest to peak angular velocity at the midpoint, then linearly decelerates back to rest. Ships never spin continuously - orientation changes only via turn orders and angular velocity always returns to zero at turn end. **Fuel cost** is calculated from the rotational kinetic energy changes during the acceleration/deceleration phases (not the total turn KE delta, which is zero since the ship starts and ends at rest). Total fuel = 2 × (peak rotational KE) / `g_ship_turn_energy_per_fuel_ton`.
- **Laser (`O_LASER`):** Request beam length. Maximum beam length is clamped to `min(map_width / 2, map_height / 2)` (default: 512 units for a 1024×1024 map). Fuel cost `length / g_laser_range_per_fuel_unit`. Cannot fire while docked.
- **Shield (`O_SHIELD`):** Consumes fuel 1:1 with requested shield gain (clamped to capacity). Applies before thrust.
- **Jettison:** Use the `SetJettison(MaterialKind, amount)` helper function, which takes a cargo type (`URANIUM` or `VINYL`) and a positive amount in tons. Internally, this converts to an `O_JETTISON` order with positive magnitude for uranium and negative magnitude for vinyl, but this is an implementation detail - **all jettison orders should be issued through `SetJettison()`**, not by calling `SetOrder(O_JETTISON, ...)` directly.
- **Order clearing:** After execution each turn, `SetOrder(..., 0)` resets orders. Laser/shield/thrust/turn orders do not persist.

## 3. Velocity Governor
- Velocity is clamped to `g_game_max_speed = 30` units/second. After each thrust impulse, the engine calculates the desired velocity and clamps it to the speed circle if necessary.
- **Governor fuel penalty:** `CalcThrustCost` **is aware of velocity clamping** and applies an additional fuel penalty equal to the overshoot amount. The ship pays:
  1. **Base thrust cost** for the requested thrust magnitude
  2. **Governor penalty cost** for the overshoot amount (how much the velocity exceeds 30 u/s before clamping)
  3. **Total fuel cost** = base cost + governor penalty (effectively paying for more thrust than was actually applied)
- **Example:** Ship moving at 15 u/s issues thrust of 45 u/s in the same direction:
  - Desired velocity = 15 + 45 = 60 u/s
  - Clamped to 30 u/s (overshoot = 30 u/s)
  - Fuel cost = equivalent to **75 u/s of thrust** (45 requested + 30 penalty)
- Thrust is applied along the ship's current orientation; reverse thrust uses the opposite vector.
- Orientation is independent of velocity; the ship may drift sideways relative to its nose until thrust or collisions alter the trajectory.

## 4. Docking / Undocking Mechanics
- **Docking:** When a ship docks (collides with any station):
  - Sets `bDockFlag = true` and `Vel = 0`
  - **Immediately transfers all vinyl cargo** to the station's vinyl reserves (deducted from ship's cargo, added to station's score)
  - Ship is filtered from collision pairing except with their own station
- **Launch:** When issuing a thrust order while docked:
  - `ProcessThrustDriftNew` teleports the ship along orientation by `station_radius + ship_radius + ship_radius / 2` (48 units for default sizes)
  - Launch occurs before the first thrust impulse of the turn
  - **Thrust orders cost 0 fuel** for the entire launch turn (shield and laser orders still consume fuel normally)
- `pending_docks` set in `CollisionEvaluationNew` prevents newly docked ships from colliding again later in the same slice.

## 5. World Geometry
- Map extents: `[-fWXMax, fWXMax)` × `[-fWYMax, fWYMax)` (default ±512). `CCoord::Wrap()` and `PredictPosition()` implement toroidal projection.
- Angles: 0 radians = +X, π/2 = +Y. All physics use the standard mathematical orientation.
- Observer UI: renders +Y downward for display purposes. Internal math remains unchanged.

## 6. Fuel Accounting
- `SetOrder` returns the estimated fuel cost actually scheduled (after clamping to available fuel). Callers should rely on the returned value for planning.
- During execution, fuel is re-validated. If remaining fuel is insufficient for a slice, the engine scales the impulse proportionally.
- Shield charges happen before thrust; shield fuel draw can therefore reduce thrust capability within the same turn.
- Thrust orders on launch turns bypass fuel validation entirely (special-case in `ProcessThrustDriftNew`). Shield and laser orders still consume fuel normally.

## 7. Implementation Notes
- Orders live in `CShip::adOrders`. Values persist until consumed during `Drift`.
- Thrust/turn decisions rely on `g_pParser->UseNewFeature("velocity-limits")` to select new vs legacy logic; defaults enable the modern code paths.
- Random launch logging and collision ties draw from the world RNG seeded once at startup (`0x4D4D3434` by default). No team-owned RNGs influence physics, keeping runs reproducible.
- When extending navigation, ensure new orders respect the sub-step schedule and interact correctly with the velocity governor.
- Perfectly elastic collision normals (ship ↔ ship, ship ↔ non-fit asteroids) now use the geometric line-of-centers. When both centres coincide the engine falls back to the shared random heading stored in the collision context.

### 7.1 Initial Ship Orientation Fix

**Location:** `team/src/Ship.C`, lines 102-121 (CShip constructor)

**Feature Flag:** `initial-orientation` (enabled by default, disabled in `--legacy-mode`)

**Problem:** Legacy engine initialized all ships with `orient = 0.0` (facing east), creating an asymmetry:
- Station at (-256, -256): Ships face east (0°), which is ~45° **away** from map center (southeast)
- Station at (256, 256): Ships face east (0°), which is ~45° **toward** map center (southwest)

This gave one team an unfair advantage in initial positioning for resource collection toward the center.

**Solution:** Ships now face toward the map center for balanced gameplay:
```cpp
// In CShip::CShip() constructor (Ship.C:111-121):
extern CParser* g_pParser;
if (g_pParser && !g_pParser->UseNewFeature("initial-orientation")) {
    // Legacy mode: all ships face east (asymmetric)
    orient = 0.0;
} else {
    // New mode: ships face toward map center (balanced)
    // Negative X stations: face east (0)
    // Positive X stations: face west (π)
    orient = (StPos.fX < 0.0) ? 0.0 : PI;
}
```

**Result:**
- Team at (-256, -256): `orient = 0.0` (east, toward center)
- Team at (256, 256): `orient = π` (west, toward center)

Both teams now have symmetric starting conditions. The orientation is set once during ship construction and affects only initial spawn state - teams can rotate freely after that.

**Testing:** Run with `--legacy-initial-orientation` to restore the old asymmetric behavior for comparison.

For contest-level descriptions, see `CONTEST_NAVIGATION_FOR_CONTESTANTS.md`. For collision specifics, refer to the physics guides.
