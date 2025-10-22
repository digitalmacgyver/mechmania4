/* CombatPredictor.h
 * Experimental Combat Prediction Utility for Groonew Team
 *
 * PURPOSE:
 * ========
 * Simulate future world states to answer: "What will I actually hit if I execute these orders?"
 *
 * This is critical for combat planning because:
 * 1. Simple prediction (PredictPosition) doesn't account for thrust/turn orders
 * 2. Need to check if obstacles will move into firing line
 * 3. Need to detect friendly fire before executing orders
 *
 * TECHNIQUE:
 * ==========
 * Uses CWorld::CreateCopy() to create a deep copy of the entire game world, then:
 * 1. Apply planned orders to your ship in the copy
 * 2. Run PhysicsModel() to simulate 1 second forward
 * 3. Call LaserTarget() in that future state
 * 4. Map the result back to the original world using team+ship numbers
 *
 * OBJECT MAPPING:
 * ===============
 * The key insight: Team numbers and ship numbers are STABLE across world copies.
 * - Team number (e.g., 14 for Groonew) is serialized and preserved
 * - Ship number (0-3) within each team is serialized and preserved
 * - World indices are NOT stable (they can be renumbered)
 *
 * LIMITATIONS:
 * ============
 * 1. NO ENEMY AI STATE: Enemy ships will continue their current velocity but won't
 *    execute new orders (their Brain pointers aren't copied)
 * 2. PERFORMANCE: Full world copy + physics simulation is expensive (~1-2ms)
 *    - Suitable for 1-2 predictions per turn, not per-ship per-frame
 * 3. ASSUMES LINEAR ENEMY MOTION: Enemies predicted using current velocity only
 * 4. COLLISIONS: If collision occurs during simulation, results may be unexpected
 *
 * WHEN TO USE:
 * ============
 * Use full prediction when:
 * - Planning laser shots (check firing line is clear)
 * - Coordinating multi-ship attacks (simulate team orders)
 * - Validating complex maneuvers (turn + thrust combinations)
 *
 * Use lightweight prediction (AngleToIntercept, PredictPosition) when:
 * - Initial target selection (faster filtering)
 * - Simple collision avoidance
 * - Navigation to stationary targets
 *
 * EXPERIMENTAL STATUS:
 * ====================
 * This code is experimental and provided as a demonstration.
 * Test thoroughly before integrating into core decision logic!
 *
 * For MechMania IV - Groonew Team
 * Created: 2025
 */

#ifndef _COMBAT_PREDICTOR_H_GROONEW
#define _COMBAT_PREDICTOR_H_GROONEW

#include "../../team/src/Ship.h"
#include "../../team/src/Team.h"
#include "../../team/src/World.h"
#include "../../team/src/Thing.h"
#include "../../team/src/GameConstants.h"

// Groonew's team number (set in Groonew.C::Init via SetTeamNumber(14))
#define GROONEW_TEAM_NUMBER 14

class CombatPredictor {
public:
    //==========================================================================
    // PRIMARY INTERFACE: Predict Laser Target in Future World State
    //==========================================================================

    /* PredictLaserTargetInFuture
     *
     * Simulates a future world state where your ship executes the given orders,
     * then checks what LaserTarget() would return in that future state.
     *
     * PARAMETERS:
     *   my_ship      - Your ship (in the original world)
     *   my_team      - Your team (in the original world)
     *   thrust_order - Planned thrust order value
     *   turn_order   - Planned turn order value (radians)
     *   dt           - Time to simulate forward (default 1.0 second)
     *
     * RETURNS:
     *   Pointer to the Thing you would hit in the ORIGINAL world
     *   NULL if no target would be hit
     *
     * MEMORY:
     *   Automatically cleans up the copied world before returning.
     *
     * EXAMPLE USAGE:
     * ==============
     *
     * // Plan your orders
     * double turn = pShip->AngleToIntercept(*enemy, 1.0);
     * double thrust = 10.0;
     *
     * // Check what you'd actually hit
     * CThing* would_hit = CombatPredictor::PredictLaserTargetInFuture(
     *     pShip, pTeam, thrust, turn
     * );
     *
     * if (would_hit == enemy) {
     *     // Clear shot! Execute
     *     pShip->SetOrder(O_TURN, turn);
     *     pShip->SetOrder(O_THRUST, thrust);
     *     pShip->SetOrder(O_LASER, 200.0);
     * } else if (would_hit && would_hit->GetTeam() == pTeam) {
     *     // Would friendly fire!
     *     // Don't execute laser
     * }
     */
    static CThing* PredictLaserTargetInFuture(
        CShip* my_ship,
        CTeam* my_team,
        double thrust_order,
        double turn_order,
        double dt = 1.0)
    {
        if (!my_ship || !my_team) {
            return NULL;
        }

        CWorld* original = my_team->GetWorld();
        if (!original) {
            return NULL;
        }

        // Step 1: Create deep copy of entire world
        CWorld* future = original->CreateCopy();
        if (!future) {
            return NULL;
        }

        // Step 2: Find "my ship" in the copied world using team+ship numbers
        unsigned int my_team_num = my_team->GetTeamNumber();
        unsigned int my_ship_num = my_ship->GetShipNumber();

        CTeam* future_my_team = future->GetTeam(my_team_num);
        if (!future_my_team) {
            delete future;
            return NULL;
        }

        CShip* future_my_ship = future_my_team->GetShip(my_ship_num);
        if (!future_my_ship) {
            delete future;
            return NULL;
        }

        // Step 3: Apply planned orders to my ship in the future world
        future_my_ship->SetOrder(O_TURN, turn_order);
        future_my_ship->SetOrder(O_THRUST, thrust_order);

        // Step 4: Simulate physics forward
        future->PhysicsModel(dt);

        // Step 5: Check what we'd hit in that future state
        CThing* future_target = future_my_ship->LaserTarget();

        // Step 6: Map the target back to original world (if found)
        CThing* original_target = NULL;
        if (future_target != NULL) {
            original_target = MapThingToOriginalWorld(future_target, original);
        }

        // Step 7: Clean up copied world
        delete future;

        return original_target;
    }

    //==========================================================================
    // ADVANCED: Predict Multiple Ships Simultaneously
    //==========================================================================

    /* PredictMultiShipLaserTargets
     *
     * Simulate a coordinated attack where multiple ships execute orders simultaneously.
     * Useful for checking if ships would interfere with each other's firing lines.
     *
     * EXAMPLE:
     * ========
     * struct ShipOrders {
     *     CShip* ship;
     *     double thrust;
     *     double turn;
     * };
     *
     * ShipOrders orders[4];
     * // ... fill in orders for each ship ...
     *
     * CThing* targets[4];
     * CombatPredictor::PredictMultiShipLaserTargets(
     *     orders, 4, targets, pTeam
     * );
     *
     * // Check results
     * for (int i = 0; i < 4; i++) {
     *     if (targets[i] && targets[i]->GetTeam() == pTeam) {
     *         // Ship i would friendly fire!
     *     }
     * }
     */
    struct ShipOrders {
        CShip* ship;
        double thrust;
        double turn;
    };

    static void PredictMultiShipLaserTargets(
        const ShipOrders* orders,
        unsigned int num_ships,
        CThing** out_targets,  // Output array of targets (one per ship)
        CTeam* my_team,
        double dt = 1.0)
    {
        if (!orders || !out_targets || !my_team || num_ships == 0) {
            return;
        }

        CWorld* original = my_team->GetWorld();
        if (!original) {
            return;
        }

        CWorld* future = original->CreateCopy();
        if (!future) {
            return;
        }

        unsigned int my_team_num = my_team->GetTeamNumber();
        CTeam* future_my_team = future->GetTeam(my_team_num);

        if (!future_my_team) {
            delete future;
            return;
        }

        // Apply orders to all ships in the future world
        for (unsigned int i = 0; i < num_ships; i++) {
            if (!orders[i].ship) continue;

            unsigned int ship_num = orders[i].ship->GetShipNumber();
            CShip* future_ship = future_my_team->GetShip(ship_num);

            if (future_ship) {
                future_ship->SetOrder(O_TURN, orders[i].turn);
                future_ship->SetOrder(O_THRUST, orders[i].thrust);
            }
        }

        // Simulate physics
        future->PhysicsModel(dt);

        // Check what each ship would hit
        for (unsigned int i = 0; i < num_ships; i++) {
            out_targets[i] = NULL;

            if (!orders[i].ship) continue;

            unsigned int ship_num = orders[i].ship->GetShipNumber();
            CShip* future_ship = future_my_team->GetShip(ship_num);

            if (future_ship) {
                CThing* future_target = future_ship->LaserTarget();
                if (future_target) {
                    out_targets[i] = MapThingToOriginalWorld(future_target, original);
                }
            }
        }

        delete future;
    }

private:
    //==========================================================================
    // HELPER: Map Objects Between Copied and Original Worlds
    //==========================================================================

    /* MapThingToOriginalWorld
     *
     * Given an object pointer from a copied world, find the corresponding
     * object in the original world using stable identifiers (team+ship numbers).
     *
     * MAPPING STRATEGY:
     * -----------------
     * Ships:     Use team number + ship number (ROBUST)
     * Stations:  Use team number (ROBUST)
     * Asteroids: Use world index (FRAGILE - no better option)
     * Lasers:    Use world index (FRAGILE - no better option)
     *
     * WHY TEAM+SHIP IS BETTER THAN WORLD INDEX:
     * ------------------------------------------
     * World indices can be renumbered by ReLinkList() during serialization.
     * Team numbers and ship numbers are stable identifiers that never change.
     *
     * LIMITATION:
     * -----------
     * Asteroids and lasers don't have stable identities beyond their world index.
     * If objects are created/destroyed between copy and original, mapping may fail.
     * In practice this is rare because we create the copy, simulate, and map immediately.
     */
    static CThing* MapThingToOriginalWorld(CThing* copied_thing, CWorld* original) {
        if (!copied_thing || !original) {
            return NULL;
        }

        ThingKind kind = copied_thing->GetKind();

        switch (kind) {
            case SHIP: {
                // SHIPS: Use team number + ship number for stable mapping
                CShip* copied_ship = (CShip*)copied_thing;
                CTeam* copied_team = copied_ship->GetTeam();

                if (!copied_team) {
                    return NULL;
                }

                unsigned int team_num = copied_team->GetTeamNumber();
                unsigned int ship_num = copied_ship->GetShipNumber();

                CTeam* orig_team = original->GetTeam(team_num);
                if (!orig_team) {
                    return NULL;
                }

                return orig_team->GetShip(ship_num);
            }

            case STATION: {
                // STATIONS: Use team number (each team has one station)
                CTeam* copied_team = copied_thing->GetTeam();

                if (!copied_team) {
                    return NULL;
                }

                unsigned int team_num = copied_team->GetTeamNumber();

                CTeam* orig_team = original->GetTeam(team_num);
                if (!orig_team) {
                    return NULL;
                }

                return orig_team->GetStation();
            }

            case ASTEROID:
            case GENTHING:
                // ASTEROIDS/LASERS: No stable identity - fall back to world index
                // This is fragile but no better option exists for non-team objects
                unsigned int idx = copied_thing->GetWorldIndex();
                return original->GetThing(idx);
        }

        return NULL;
    }
};

//==============================================================================
// USAGE EXAMPLES
//==============================================================================

/*
 * EXAMPLE 1: Basic Usage from Groonew.C Turn() Method
 * ====================================================
 *
 * void Groonew::Turn() {
 *     CWorld* pWorld = GetWorld();
 *
 *     // For each ship, plan attack
 *     for (unsigned int i = 0; i < GetShipCount(); i++) {
 *         CShip* pShip = GetShip(i);
 *         if (!pShip || !pShip->IsAlive()) continue;
 *
 *         // Find target (simplified)
 *         CThing* enemy = FindNearestEnemy(pShip);
 *         if (!enemy) continue;
 *
 *         // Plan orders
 *         double turn = pShip->AngleToIntercept(*enemy, 1.0);
 *         double thrust = 10.0;
 *
 *         // CHECK: What would we actually hit with these orders?
 *         CThing* would_hit = CombatPredictor::PredictLaserTargetInFuture(
 *             pShip, this, thrust, turn
 *         );
 *
 *         // Decision logic
 *         if (would_hit == enemy) {
 *             // Perfect! Clear shot at intended target
 *             pShip->SetOrder(O_TURN, turn);
 *             pShip->SetOrder(O_THRUST, thrust);
 *             pShip->SetOrder(O_LASER, 200.0);
 *         }
 *         else if (would_hit && would_hit->GetTeam() == this) {
 *             // ABORT! Would hit friendly ship or station
 *             pShip->SetOrder(O_TURN, turn);
 *             pShip->SetOrder(O_THRUST, thrust);
 *             // Don't fire laser
 *         }
 *         else if (would_hit) {
 *             // Would hit different enemy or obstacle
 *             // Maybe still fire if it's a valid target
 *             if (would_hit->GetTeam() != this && would_hit->GetTeam() != NULL) {
 *                 // It's an enemy - fire anyway!
 *                 pShip->SetOrder(O_TURN, turn);
 *                 pShip->SetOrder(O_THRUST, thrust);
 *                 pShip->SetOrder(O_LASER, 200.0);
 *             }
 *         }
 *     }
 * }
 */

/*
 * EXAMPLE 2: Integration with GetVinyl.C Decision Logic
 * ======================================================
 *
 * In GetVinyl.C, when deciding whether to fire opportunistically:
 *
 * void GetVinyl::Decide() {
 *     // ... asteroid collection logic ...
 *
 *     // Opportunistic combat: Check if we can hit enemies without changing course
 *     CThing* current_target = pShip->LaserTarget();
 *
 *     if (current_target && current_target->GetKind() == SHIP) {
 *         CTeam* target_team = current_target->GetTeam();
 *
 *         if (target_team && target_team != pTeam) {
 *             // Enemy in current firing line!
 *             // But will they still be there after our planned movement?
 *
 *             double planned_thrust = 15.0;  // From asteroid collection logic
 *             double planned_turn = 0.0;     // Not planning to turn
 *
 *             CThing* future_target = CombatPredictor::PredictLaserTargetInFuture(
 *                 pShip, pTeam, planned_thrust, planned_turn
 *             );
 *
 *             if (future_target == current_target) {
 *                 // Still in firing line after movement! Take the shot!
 *                 pShip->SetOrder(O_LASER, 200.0);
 *             }
 *             // else: They'll move out of the way, don't waste fuel
 *         }
 *     }
 * }
 */

/*
 * EXAMPLE 3: Multi-Ship Coordination
 * ===================================
 *
 * Plan coordinated attack to avoid friendly fire:
 *
 * void Groonew::CoordinateAttack() {
 *     CombatPredictor::ShipOrders orders[4];
 *     CThing* targets[4];
 *
 *     // Plan orders for all ships
 *     for (unsigned int i = 0; i < GetShipCount(); i++) {
 *         CShip* pShip = GetShip(i);
 *         orders[i].ship = pShip;
 *
 *         if (pShip && pShip->IsAlive()) {
 *             CThing* enemy = FindTargetForShip(pShip, i);
 *             orders[i].turn = pShip->AngleToIntercept(*enemy, 1.0);
 *             orders[i].thrust = 10.0;
 *         } else {
 *             orders[i].thrust = 0.0;
 *             orders[i].turn = 0.0;
 *         }
 *     }
 *
 *     // Predict what ALL ships would hit
 *     CombatPredictor::PredictMultiShipLaserTargets(
 *         orders, GetShipCount(), targets, this
 *     );
 *
 *     // Execute safe orders
 *     for (unsigned int i = 0; i < GetShipCount(); i++) {
 *         CShip* pShip = orders[i].ship;
 *         if (!pShip) continue;
 *
 *         pShip->SetOrder(O_TURN, orders[i].turn);
 *         pShip->SetOrder(O_THRUST, orders[i].thrust);
 *
 *         // Only fire if won't hit friendly
 *         if (targets[i] && targets[i]->GetTeam() != this) {
 *             pShip->SetOrder(O_LASER, 200.0);
 *         }
 *     }
 * }
 */

/*
 * EXAMPLE 4: Performance-Conscious Usage Pattern
 * ===============================================
 *
 * Use lightweight prediction first, then full prediction for validation:
 *
 * void Groonew::Turn() {
 *     for (unsigned int i = 0; i < GetShipCount(); i++) {
 *         CShip* pShip = GetShip(i);
 *         if (!pShip || !pShip->IsAlive()) continue;
 *
 *         // PHASE 1: Fast filtering with lightweight prediction
 *         CThing* quick_target = FindTargetWithSimplePrediction(pShip);
 *         if (!quick_target) continue;
 *
 *         double turn = pShip->AngleToIntercept(*quick_target, 1.0);
 *         double thrust = 10.0;
 *
 *         // PHASE 2: Validate with full prediction (only for serious shots)
 *         double distance = pShip->GetPos().DistTo(quick_target->GetPos());
 *         if (distance < 300.0) {  // Only predict if close enough to matter
 *             CThing* accurate_target = CombatPredictor::PredictLaserTargetInFuture(
 *                 pShip, this, thrust, turn
 *             );
 *
 *             if (accurate_target == quick_target) {
 *                 // Validated! Execute
 *                 pShip->SetOrder(O_TURN, turn);
 *                 pShip->SetOrder(O_THRUST, thrust);
 *                 pShip->SetOrder(O_LASER, 200.0);
 *             }
 *         }
 *     }
 * }
 */

/*
 * DEBUGGING TIPS:
 * ===============
 *
 * 1. Add verbose logging to track predictions:
 *
 *    CThing* target = CombatPredictor::PredictLaserTargetInFuture(...);
 *    if (target) {
 *        char msg[256];
 *        snprintf(msg, sizeof(msg), "Ship %d would hit %s at distance %.1f",
 *                 pShip->GetShipNumber(), target->GetName(),
 *                 pShip->GetPos().DistTo(target->GetPos()));
 *        pTeam->AppendMessage(msg);
 *    }
 *
 * 2. Compare with LaserTarget() on current world to see prediction accuracy:
 *
 *    CThing* current = pShip->LaserTarget();
 *    CThing* future = CombatPredictor::PredictLaserTargetInFuture(...);
 *    if (current != future) {
 *        // Firing line will change! Prediction saved us from bad shot
 *    }
 *
 * 3. Performance profiling:
 *
 *    double t1 = pTeam->GetWallClock();
 *    CThing* target = CombatPredictor::PredictLaserTargetInFuture(...);
 *    double t2 = pTeam->GetWallClock();
 *
 *    if (t2 - t1 > 0.005) {  // > 5ms
 *        // Prediction is expensive! Consider using less frequently
 *    }
 */

#endif // _COMBAT_PREDICTOR_H_GROONEW
