# Legacy Collision Engine Notes

This document summarizes quirks that surface when the legacy collision loop is left unchanged. It is intended as a companion reference while modernizing `CollisionEvaluation` and the object-specific `HandleCollision` routines.

## Collision Pass Structure

The world builds a temporary list of team-owned objects (stations first, then each active ship) and iterates every live world entity against that list. For each pairing it calls the non-team object’s `Collide()` method, then makes the symmetric call on the team-owned object. None of these calls are short-circuited if the target dies mid-pass, so every pairing for the current iteration still runs.

The `CollisionEvaluation()` counter (`URes`) is just a local tally of how many team objects reported a collision. The physics step currently ignores the return value, so the counter has no downstream effect.

## Double-Consumption of Asteroids

When two ships overlap the same asteroid that each can “eat,” the first symmetric call marks the asteroid dead and records the consuming ship. Because the main loop continues, the next ship still receives its turn against the now-dead asteroid. The asteroid-side handler overwrites its “eaten by” record with the new ship, so each overlapping ship proceeds through its cargo-transfer branch and claims the mass. Result: multiple ships can ingest the same asteroid during a single pass.

## Repeated Fragmentation

Asteroids that do not fit inside a ship (or are destroyed by lasers) set their dead flag but otherwise keep executing collision logic for the rest of the pass. Every subsequent symmetric call therefore re-enters the fragmentation routine and queues another full set of child asteroids. Multiple high-energy lasers or a swarm of ships can all trigger independent fragmentation runs on the same original rock before the turn ends.

## Docking Order Artifacts

Stations are inserted into the pairing list before their ships. Any ship overlapping its station therefore processes the docking branch first. That branch teleports the ship to the station center, zeroes velocity, and sets the docked flag. Later pairings for that ship occur with the docked guard active, so collision handling exits early. The non-station partner still ran its own collision handler earlier in the sequence, which can mark an asteroid dead without the ship ever receiving cargo. The behavior is an artifact of the fixed station-first ordering.

## Multi-Ship Collision Ordering

For ship–ship overlaps the symmetric callbacks exchange momentum and apply separation impulses immediately. Because all results feed forward within the same evaluation pass, the outcome for three or more ships depends on the order they appear in the team roster. Early pairings can push ships apart or change trajectories before later pairings are evaluated, so the final state is not rotationally symmetric when more than two ships occupy the same space.

## Ship–Ship Symmetry Spam

A single ship–ship overlap triggers four `HandleCollision()` executions per ship in one physics tick. World collision evaluation first calls `shipA->Collide(shipB)` and then the symmetric call `shipB->Collide(shipA)`. Each `CShip::HandleCollision()` in turn clears its team pointer and invokes the peer’s `Collide()` to guarantee a response, so the first outer-loop pair already runs eight collision handlers combined. Later in the pass the outer loop reaches the ships again with roles reversed and repeats the cycle. The guard that nulled the team pointer prevents infinite recursion but does not stop this “collision echo,” so ship logic continues to see redundant work every frame.

## Stationary Laser Impacts

`CWorld::LaserModel()` is the only subsystem that manually invokes `Collide()` outside of the main collision pass. Each fired laser synthesizes a temporary `CThing` and delivers exactly one `Collide()` call to the chosen target. Because laser handling sits outside the main loop, any safeguards added to `CollisionEvaluationOld()` do not automatically apply to laser impacts.

## Idempotence Assumptions Broken

Many `HandleCollision()` implementations assume they will only be called once per event. In reality the legacy loop replays the same asteroid or ship collision multiple times within a single tick. Ships rely on `pThEat` to detect prior asteroid claims, but asteroids still overwrite that pointer on each call, letting multiple ships ingest the same mass. Large asteroids likewise fragment repeatedly when several ships or lasers touch them in the same frame. These bugs are a direct side effect of the redundant `Collide()` invocations described above.

## Systemic Ordering Biases

The engine’s deterministic iteration order introduces persistent mechanical biases:

- Teams are processed strictly by their index in `apTeams`, so team zero always resolves station/ship collisions first. That team can consume asteroids or shatter targets before later teams ever get a symmetric call, producing advantage in simultaneous events.
- Within each team the station is always handled before any ship. Station-related side effects (auto-docking, cargo delivery, shield resets) therefore preempt every other interaction for that team, reinforcing the docking artifacts described earlier.
- World objects are iterated by their array/linked-list index. Long-lived entities (low indices) resolve contacts before newly spawned ones, which means “older” asteroids or ships can alter state before late arrivals even participate.

Because none of these passes are randomized or physics-driven, the same spatial arrangement can yield different results solely from bookkeeping order, leading to predictable edge cases that favor specific teams or object lifetimes.

## Multi-Hit Bugs Summary

The legacy collision system suffers from several **multi-hit bugs** where the same collision is processed multiple times in a single frame:

**Asteroid Multi-Fragmentation:**
- When multiple ships or lasers hit the same large asteroid in one frame, each collision triggers a full fragmentation sequence
- Result: Instead of spawning 3 fragments, the asteroid spawns 6, 9, or more child asteroids depending on how many hits occurred
- Root cause: Asteroids set their dead flag but continue executing collision logic for the entire pass

**Ship-Ship Double Damage:**
- A single ship-ship collision can process multiple times within the same frame
- Result: Ships take damage 2x, 4x, or more times from a single collision event
- Root cause: The symmetric collision callback structure (A→B, B→A) combined with no deduplication

**Asteroid Multi-Eating:**
- When two ships overlap the same small asteroid, both ships can "eat" it and gain its mass
- Result: Mass is duplicated - both ships gain the asteroid's full mass
- Root cause: Each ship's collision handler marks the asteroid eaten and transfers mass, with no global coordination

**Dead Object Collisions:**
- Objects marked dead during a collision pass continue processing additional collisions
- Result: Destroyed asteroids can still fragment, destroyed ships can still take damage
- Root cause: No filtering of dead entities between collision pair evaluations

These bugs are documented in detail in the sections above and are fixed in the new collision system (see `NEW_COLLISION_ENGINE.md`).

## General Race Surface

The engine assumes each `HandleCollision` implementation will manage repeated calls against dead or already-processed entities. Without explicit guards, any object that dies mid-pass can keep interacting with every remaining partner, causing duplicate effects such as stacked explosions, duplicated cargo transfers, or inconsistent announcer messages. Modernizing the loop will likely require either filtering dead entries out before the symmetric call or having each handler detect and ignore repeat work.
