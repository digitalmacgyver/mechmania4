/* PhysicsUtils.C
 * Implementation of shared physics utilities for MechMania IV
 */

#include <cmath>

#include "GameConstants.h"
#include "PhysicsUtils.h"

namespace PhysicsUtils {

ElasticCollisionResult CalculateElastic2DCollision(double m1, const CTraj& v1, const CCoord& p1,
                                                   double m2, const CTraj& v2, const CCoord& p2,
                                                   double random_angle, bool has_random) {
  ElasticCollisionResult result;

  if (m1 < 0.001 || m2 < 0.001) {
    result.v1_final = v1;
    result.v2_final = v2;
    return result;
  }

  CCoord v1_cart = v1.ConvertToCoord();
  CCoord v2_cart = v2.ConvertToCoord();

  CCoord normal = p2 - p1;
  double normal_mag_sq = normal.fX * normal.fX + normal.fY * normal.fY;

  if (normal_mag_sq < g_fp_error_epsilon) {
    if (has_random) {
      normal.fX = std::cos(random_angle);
      normal.fY = std::sin(random_angle);
      normal_mag_sq = normal.fX * normal.fX + normal.fY * normal.fY;
    } else {
      normal.fX = 1.0;
      normal.fY = 0.0;
      normal_mag_sq = 1.0;
    }
  }

  double normal_mag = std::sqrt(normal_mag_sq);

  CCoord n;
  n.fX = normal.fX / normal_mag;
  n.fY = normal.fY / normal_mag;

  CCoord t;
  t.fX = -n.fY;
  t.fY = n.fX;

  double v1_n = v1_cart.fX * n.fX + v1_cart.fY * n.fY;
  double v1_t = v1_cart.fX * t.fX + v1_cart.fY * t.fY;

  double v2_n = v2_cart.fX * n.fX + v2_cart.fY * n.fY;
  double v2_t = v2_cart.fX * t.fX + v2_cart.fY * t.fY;

  double total_mass = m1 + m2;
  double v1_n_final = ((m1 - m2) * v1_n + 2.0 * m2 * v2_n) / total_mass;
  double v2_n_final = ((m2 - m1) * v2_n + 2.0 * m1 * v1_n) / total_mass;

  double v1_t_final = v1_t;
  double v2_t_final = v2_t;

  CCoord v1_final_cart, v2_final_cart;
  v1_final_cart.fX = v1_n_final * n.fX + v1_t_final * t.fX;
  v1_final_cart.fY = v1_n_final * n.fY + v1_t_final * t.fY;
  v2_final_cart.fX = v2_n_final * n.fX + v2_t_final * t.fX;
  v2_final_cart.fY = v2_n_final * n.fY + v2_t_final * t.fY;

  result.v1_final = CTraj(v1_final_cart);
  result.v2_final = CTraj(v2_final_cart);

  return result;
}

double CalcTurnCostPhysical(double angle_radians, double ship_mass, double ship_radius) {
  const double T = 1.0;
  const double T_squared = T * T;

  double KE_peak =
      ship_mass * ship_radius * ship_radius * angle_radians * angle_radians / T_squared;

  return 2.0 * KE_peak / g_ship_turn_energy_per_fuel_ton;
}

}  // namespace PhysicsUtils
