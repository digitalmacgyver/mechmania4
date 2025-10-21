/* PhysicsUtils.h
 * Shared physics utility helpers for MechMania IV
 * Provides reusable stateless calculations used across multiple systems.
 */

#ifndef _PHYSICS_UTILS_H_MM4
#define _PHYSICS_UTILS_H_MM4

#include "Coord.h"
#include "Traj.h"

namespace PhysicsUtils {

struct ElasticCollisionResult {
  CTraj v1_final;         // Final velocity of object 1
  CTraj v2_final;         // Final velocity of object 2
  CTraj collision_normal; // Unit vector from object 1 toward object 2
  bool used_random_normal;
};

// Calculate perfectly elastic collision outcome between two moving circles.
// random_angle/has_random supply a fallback heading when positions coincide.
ElasticCollisionResult CalculateElastic2DCollision(double m1, const CTraj& v1, const CCoord& p1,
                                                   double m2, const CTraj& v2, const CCoord& p2,
                                                   double random_angle = 0.0,
                                                   bool has_random = false);

// Compute the fuel cost for rotating a ship using the physical energy model.
double CalcTurnCostPhysical(double angle_radians, double ship_mass, double ship_radius);

}  // namespace PhysicsUtils

#endif  // _PHYSICS_UTILS_H_MM4
