/* Pathfinding.h
 * Pathfinding algorithms for MechMania IV
 * Extracted from Groogo team implementation for better maintainability
 *
 * This namespace contains stateless pathfinding algorithms used to
 * determine optimal orders (thrust/turn) for ships to intercept targets.
 */

#ifndef _PATHFINDING_H_
#define _PATHFINDING_H_

#include "FuelTraj.h"
#include "GameConstants.h"
#include "Ship.h"
#include "Thing.h"

namespace Pathfinding {
    struct CollisionInfo {
        CThing* thing = nullptr;
        double time = g_no_collide_sentinel;

        bool HasCollision() const {
            return thing != nullptr && time != g_no_collide_sentinel;
        }
    };

    // The core algorithmic function.
    // Calculates orders (thrust/turn) to reach target in given time.
    FuelTraj DetermineOrders(CShip* ship, CThing* thing, double time, CShip* calculator);

    // Returns information about the earliest collision (or no-collision sentinel).
    CollisionInfo GetFirstCollision(CShip* ship);
}

#endif // _PATHFINDING_H_
