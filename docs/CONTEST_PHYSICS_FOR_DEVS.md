# Contest Physics Reference (Developers)

> **Scope.** This document explains the **default modern collision system** shipped with the tournament server. The historical 1998 modes (`--legacy-mode`, `--legacy-collision-handling`, `--legacy-physics`, etc.) intentionally preserve the bugs of the original release and are not covered here.

## 1. Design Goals
- Deterministic, bias-free collision handling regardless of team order.
- Physics mirroring Newtonian momentum conservation with selective energy losses for “stick” events (e.g., asteroid ingestion).
- Snapshot isolation: collision logic derives from immutable copies of state captured at the start of the subturn.
- Command pattern: collision handlers emit declarative commands; the world applies them once per frame in a well-defined priority order.

## 2. Simulation Timeline
- **Turn duration:** `g_game_turn_duration = 1.0` seconds.
- **Physics sub-step:** `g_physics_simulation_dt` (default 0.2 s). Each turn runs `ceil(turn_duration / dt)` sub-steps. A five-step default loop is typical.
- **Per sub-step:**
  1. Ships resolve jettison orders (`O_JETTISON`) and enqueue spawn requests.
  2. Shield orders consume fuel and adjust shield totals.
  3. Turn/thrust orders are integrated for the current dt slice using the modern velocity governor (triangular angular velocity, fuel-per-dv model, clamp to `g_game_max_speed`).
  4. Positions advance (drift) for all objects.
  5. `CWorld::CollisionEvaluationNew()` runs using snapshots captured at the beginning of the step.
  6. Deferred additions (spawned asteroids) are appended; dead objects are culled.
- **Laser phase:** After the final physics slice, `CWorld::LaserModelNew()` synthesizes laser impacts and feeds them through the same command pipeline.

## 3. Collision Pipeline Internals
### 3.1 Snapshot collection
`CollisionState` captures:
- Identity: pointer, `ThingKind`, `world_index`, owning team.
- Continuous state: position, velocity, orientation, angular velocity, size, total mass.
- Ship extras: docked flag, current shield/cargo/fuel amounts **and capacities**.
- Station extras: stored vinyl.
- Asteroid extras: material (`VINYL` or `URANIUM`).

Snapshots are stored in both an immutable map (`snapshots`) and a mutable working copy (`current_states`). The working copy is updated incrementally as commands are emitted so later collisions observe resource changes made earlier in the same frame.

### 3.2 Pair generation
- Every live world object is tested against every team-controlled object (ships + stations).
- Pairs are canonicalised by `world_index` to avoid duplicate `(A,B)` / `(B,A)` processing.
- Overlaps are computed against the circular radii.
- Collisions are stored with their overlap depth. Pairs where either object is docked (and the partner is not its own station) are filtered.

### 3.3 Ordering and randomisation
- Collision list sorted descending by overlap depth.
- Groups whose overlap difference is < `0.001` are shuffled using a RNG seeded per evaluation, ensuring no deterministic bias when distances tie.

### 3.4 Command generation
For each collision pair:
1. Skip pair if either object is already dead or marked for kill.
2. Apply docking guards: once a ship queues `kSetDocked`, further non-station collisions are ignored.
3. Build two `CollisionContext` instances (A→B and B→A) using the **current** snapshots from `current_states`. Both contexts share a pre-generated random angle used as a tie-breaker when velocity and position coincide.
4. Each object’s `GenerateCollisionCommands` fills a `CollisionOutcome`.
5. Commands are appended to `all_commands`, and each command is mirrored into `current_states` via `apply_command_to_state`.
6. Spawn requests are queued for after command execution.

### 3.5 Command priority
`GetCommandTypePriority()` returns:
1. `kKillSelf`
2. `kSetPosition`
3. `kSetVelocity`
4. `kSetDocked`
5. `kAdjustShield`, `kAdjustCargo`, `kAdjustFuel`
6. `kRecordEatenBy`
7. `kAnnounceMessage`

`kNoOp` commands are deprioritised (value 99) and effectively skipped.

### 3.6 Application
Commands are applied in priority order. Non-metadata commands targeting dead objects are skipped. Announcer commands append to the world’s message queue. Spawn requests instantiate asteroids using their stored mass/material after command execution finishes.

## 4. Collision Semantics
### 4.1 Ships
- Mass = hull (40) + cargo + fuel. Capacities draw from `g_ship_default_*` constants.
- Docking sets velocity to zero, teleports the ship to station position, and flags `bDockFlag = true`. Docked ships are filtered from subsequent collisions that turn.
- Departure thrust triggers the launch teleport: distance = `station_radius + ship_radius + ship_radius / 2`.

### 4.2 Ship ↔ Ship
- Elastic collision along the separation axis. If relative velocity has non-zero normal component, that direction defines the normal; otherwise use line of centres; if both zero, use shared random angle from context.
- Separation impulse: each ship is moved along ±normal by `(ship_radius + g_ship_collision_bump)` to clear overlap. `g_ship_collision_bump` defaults to 3.
- Damage: equal for both ships, proportional to momentum change divided by `g_laser_damage_mass_divisor`.

### 4.3 Ship ↔ Asteroid
- Fit test uses snapshot capacities:
  - If `ship_cargo + mass ≤ ship_cargo_capacity` (vinyl) or `ship_fuel + mass ≤ ship_fuel_capacity` (uranium), the asteroid is absorbed (perfectly inelastic). Commands: add resource, kill asteroid, record eater.
  - Otherwise the asteroid elastically rebounds and fragments. Fragment mass = `mass / 3`; child asteroids spawn only if `fragment_mass ≥ g_thing_minmass`.
- Momentum is conserved in both branches. Energy is conserved only in the non-fit branch (elastic). Fit branch loses kinetic energy.

### 4.4 Ship ↔ Station
- Treated as a docking interaction. Ship is set docked/zero velocity; station cargo is credited/debited with delivered vinyl. Announcements are queued for home/enemy delivery messages.

### 4.5 Laser Interactions
- Lasers synthesise a temporary “beam” object whose mass equals remaining beam length × `g_laser_mass_scale_per_remaining_unit`. Impact direction aligns with the firing ship’s facing.
- Targets receive `kAdjustShield` or `kAdjustCargo` (for stations) equal to `mass / g_laser_damage_mass_divisor`.
- Under the new physics flag, the target’s momentum is updated via a perfectly inelastic merge with the beam mass.

### 4.6 Stations
- Immovable. Asteroid collisions bounce elastically off them; stations themselves never translate.
- Cargo adjustments come from docking deposits, enemy laser depletion, or game scripts.

### 4.7 Lasers vs Asteroids
- If computed damage ≥ 1000, asteroid shatters into three fragments as in the ship case. Otherwise command sequence is empty (laser glances off).
- Damage formula: `damage = 30.0 × (beam_length - distance_to_target)` (with default scale)

## 5. Object Properties (Defaults)
| Object | Radius | Hull Mass | Notes |
| --- | --- | --- | --- |
| Station | 30 | Immovable | Stores vinyl score |
| Ship | 12 | 40 | Mass increases with cargo + fuel |
| Asteroid | varies | equals mass | Material = VINYL or URANIUM |

Key constants (`GameConstants.C`):
- `g_game_max_speed = 30.0`
- `g_thing_minmass = 3.0`
- `g_ship_collision_bump = 3.0`
- `g_laser_damage_mass_divisor = 1000.0`

## 6. Invariants and Safety Checks
- **Momentum conservation:** All collision handlers must leave total momentum unchanged unless explicitly modelling energy loss (e.g., asteroid ingestion).
- **Snapshot ownership:** `CollisionState` must remain read-only; only `current_states` may be mutated.
- **Single kill:** Once an object queues `kKillSelf` in `pending_kills`, it must not emit further commands that frame.
- **Capacity clamping:** Resource adjustments clamp to `[0, capacity]` using capacities captured in the snapshot, not live `GetCapacity`.
- **Docking exclusivity:** Ships flagged docked or in `pending_docks` cannot participate in non-station collisions for the remainder of the frame.

## 7. Extending the System
When adding new collision effects:
1. Extend `CollisionState` with any derived fields required for snapshot evaluation.
2. Capture those fields in the relevant `MakeCollisionState` override.
3. Update `apply_command_to_state` in `World.C` if the new command mutates snapshot-visible values.
4. Assign a priority in `GetCommandTypePriority`.
5. Ensure multi-hit and docking guard logic is updated if the new behavior affects object lifetimes.

For detailed rationale behind individual choices, see `docs/NEW_COLLISION_ENGINE.md`.
