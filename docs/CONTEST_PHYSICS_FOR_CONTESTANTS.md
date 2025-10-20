# Contest Physics Reference (Contestants)

> **Scope.** This guide documents the **default modern collision and physics model** used in the current tournament server. Historically, MechMania IV shipped with several legacy modes that replicate the 1998 behavior; those variants remain available via command-line flags but are **out of scope** here.

## 1. World Basics
- **Playfield:** A 1024 × 1024 unit continuous torus. Crossing one edge wraps to the opposite edge with the same velocity.
- **Coordinate axes:** +X points right, +Y points up in the mathematical model. The observer UI renders +Y downward, so visuals appear flipped vertically.
- **Object sizes (radius):** Station 30, Ship 12, procedural asteroids typically 4–25 (min fragment radius ≈ 3). Collisions use circular footprints.
- **Docked immunity:** A ship that is docked cannot be struck by any other object. It may still launch (see Navigation guide).

## 2. Collision Overview
All collisions are handled pairwise using idealised Newtonian mechanics plus a few quality-of-life “kicks” to avoid overlapping objects. Every collision conserves total momentum. Energy conservation depends on the scenario:

| Participants | Physics Model | Energy Outcome | Notes |
| --- | --- | --- | --- |
| Ship ↔ Ship | Perfectly elastic along the collision normal. | Kinetic energy conserved. | Normal is the relative intercept line. If relative velocity is zero, the line of centres is used. If positions and velocities coincide, a shared random heading is chosen for both ships. |
| Ship ↔ Small Asteroid (fits) | Perfectly inelastic. | Kinetic energy not conserved. | Asteroid mass is absorbed into fuel (uranium) or cargo (vinyl). |
| Ship ↔ Large Asteroid (does not fit) | Perfectly elastic. | Kinetic energy conserved. | Asteroid breaks up (see below). |
| Ship ↔ Station | Docking (no physics impulse). | — | Ship teleports to station centre, velocity zeroed, and becomes docked. |
| Station ↔ Asteroid/Ship | Elastic reflection for asteroids; docking for ships. | Energy conserved for asteroid bounces. | Stations never move, but asteroids striking them bounce off. Ships that collide with their own station dock instead of bouncing. |
| Laser ↔ Ship | Treated as a perfectly inelastic mass-on-impact. | Energy decreases (beam dissipates). | Damage equals remaining beam energy divided by 1000. Target absorbs momentum along beam direction. |
| Laser ↔ Station | Same as ship, but damage subtracts vinyl instead of shields. |
| Laser ↔ Asteroid | If damage ≥ 1, asteroid shatters into three. Otherwise no effect. |

### Collision sequencing safeguards
- **Most-overlap-first:** Overlaps with greatest depth resolve before shallower overlaps. Ties are shuffled.
- **Single resolution:** Once an object is marked for destruction, it cannot participate in additional collisions that turn. This prevents multi-hit exploits.
- **Docked filtering:** Ships that dock on a collision immediately become intangible to all non-station objects for the rest of the turn.

## 3. Ship–Asteroid Specifics
1. **Fit check:** If the remaining capacity (`max_capacity − current_load`) is at least the asteroid mass, the asteroid is eaten.
   - **Vinyl asteroid:** Ship cargo increases by asteroid mass (capped at capacity). The asteroid is destroyed.
   - **Uranium asteroid:** Ship fuel increases by asteroid mass (capped at capacity). The asteroid is destroyed.
2. **Overflow:** If the asteroid is too large:
   - Compute fragment mass = asteroid_mass / 3.
   - If fragment mass ≥ 3 (the minimum playable mass), three child asteroids are spawned at that mass with spread headings.
   - Otherwise the asteroid simply vanishes.
   - In both cases the ship’s cargo/fuel do not change.

Asteroid material never changes when it fragments: vinyl stays vinyl, uranium stays uranium.

## 4. Ship Separation “Kicks”
- **Ship ↔ Ship:** After an elastic collision the simulation nudges both ships apart along the chosen collision axis so their circles are no longer overlapping (adds 3 units of safety on top of their radii). This prevent ships from sticking due to round-off.
- **Undocking launch:** When a docked ship thrusts, it is teleported to a point 48 units away from the station centre in the thrust direction (station radius 30 + ship radius 12 + half ship radius 6). This guarantees immediate clearance.

## 5. Practical Implications
- **Momentum matters:** Heavier ships (full fuel/cargo) are notably harder to deflect. Light “scout” builds can be bounced or slingshot more easily.
- **Docking is safe:** Once the docking collision fires, the ship has zero speed and is intangible to everything except its station.
- **Asteroid racing:** When multiple ships hit the same asteroid in the same turn, only the first collision (after shuffling equal overlaps) takes effect. Later ships see the updated cargo/fuel numbers before their collision decision.
- **Laser pushes:** Even if the laser doesn’t destroy a target, the momentum impulse can significantly alter the target’s velocity vector.

Refer to the navigation and damage guides for order timing, fuel costs, and shield interactions.
