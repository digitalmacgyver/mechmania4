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
- **Turn (`O_TURN`):** Requested angle ∈ [-2π, 2π]. Triangular angular velocity profile ensures start/stop symmetry. Fuel cost equals rotational kinetic energy delta / `g_ship_turn_energy_per_fuel_ton`.
- **Laser (`O_LASER`):** Request beam length. Validation clamps by map half-width/height. Fuel cost `length / g_laser_range_per_fuel_unit`. Cannot fire while docked.
- **Shield (`O_SHIELD`):** Consumes fuel 1:1 with requested shield gain (clamped to capacity). Applies before thrust.
- **Jettison:** `SetJettison` queues positive amounts for fuel (uranium) or negative for cargo (vinyl). The helper converts to `O_JETTISON` order internally.
- **Order clearing:** After execution each turn, `SetOrder(..., 0)` resets orders. Laser/shield/thrust/turn orders do not persist.

## 3. Velocity Governor
- Velocity is clamped to `g_game_max_speed = 30`. After each thrust impulse, the engine projects the desired velocity onto the 30 u/s circle if necessary.
- When clamping triggers, the ship still pays the original fuel cost (`CalcThrustCost` is unaware of the clamp). Excess Δv is lost, producing an extra fuel penalty beyond what was needed to reach 30 u/s.
- Thrust is applied along the ship’s current orientation; reverse thrust uses the opposite vector.
- Orientation is independent of velocity; the ship may drift sideways relative to its nose until thrust or collisions alter the trajectory.

## 4. Docking / Undocking Mechanics
- Docked ships have `bDockFlag = true`, `Vel = 0`, and are filtered from collision pairing except with their own station.
- Launch logic (`ProcessThrustDriftNew`) teleports the ship along orientation by `station_radius + ship_radius + ship_radius / 2`. Launch occurs before the first thrust impulse of the turn.
- Launch turns are fuel-free even after teleport.
- `pending_docks` set in `CollisionEvaluationNew` prevents newly docked ships from colliding again later in the same slice.

## 5. World Geometry
- Map extents: `[-fWXMax, fWXMax)` × `[-fWYMax, fWYMax)` (default ±512). `CCoord::Wrap()` and `PredictPosition()` implement toroidal projection.
- Angles: 0 radians = +X, π/2 = +Y. All physics use the standard mathematical orientation.
- Observer UI: renders +Y downward for display purposes. Internal math remains unchanged.

## 6. Fuel Accounting
- `SetOrder` returns the estimated fuel cost actually scheduled (after clamping to available fuel). Callers should rely on the returned value for planning.
- During execution, fuel is re-validated. If remaining fuel is insufficient for a slice, the engine scales the impulse proportionally.
- Shield charges happen before thrust; shield fuel draw can therefore reduce thrust capability within the same turn.
- Launch turns bypass fuel validation entirely (special-case in `ProcessThrustDriftNew`).

## 7. Implementation Notes
- Orders live in `CShip::adOrders`. Values persist until consumed during `Drift`.
- Thrust/turn decisions rely on `g_pParser->UseNewFeature("velocity-limits")` to select new vs legacy logic; defaults enable the modern code paths.
- Random launch logging and collision ties use `std::random_device` seeded per evaluation. No team-owned RNGs influence physics.
- When extending navigation, ensure new orders respect the sub-step schedule and interact correctly with the velocity governor.

For contest-level descriptions, see `CONTEST_NAVIGATION_FOR_CONTESTANTS.md`. For collision specifics, refer to the physics guides.
