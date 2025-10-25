#ifndef GROONEW_LASER_UTILS_H_
#define GROONEW_LASER_UTILS_H_

#include "GameConstants.h"
#include "ParserModern.h"
#include "Pathfinding.h"
#include "Ship.h"
#include "Thing.h"

#include <algorithm>
#include <cstdio>

extern CParser* g_pParser;

namespace groogather {
namespace laser {

struct BeamEvaluation {
  double beam_length;
  double target_distance;
  double expected_damage;
  double fuel_cost;
  double efficiency;
};

inline double ComputeLaserFuelCost(double beam_length) {
  return beam_length / g_laser_range_per_fuel_unit;
}

inline double DamagePerExtraUnit() {
  return g_laser_mass_scale_per_remaining_unit / g_laser_damage_mass_divisor;
}

inline double DamageForExtraLength(double extra_length) {
  return extra_length * DamagePerExtraUnit();
}

inline double NormalizeAngle(double angle) {
  while (angle > PI) angle -= PI2;
  while (angle < -PI) angle += PI2;
  return angle;
}

struct LaserResources {
  double available_fuel = 0.0;
  double max_beam_length = 0.0;
  double damage_per_unit = 0.0;
};

struct FiringPredictability {
  bool shooter_reliable = true;
  bool target_reliable = true;

  bool BothReliable() const { return shooter_reliable && target_reliable; }
};

inline FiringPredictability EvaluateFiringPredictability(const CShip* shooter,
                                                         const CThing* target,
                                                         double horizon =
                                                             g_game_turn_duration) {
  FiringPredictability result;
  if (shooter == nullptr) {
    result.shooter_reliable = false;
    result.target_reliable = false;
    return result;
  }

  const auto shooter_collision = Pathfinding::GetFirstCollision(
      const_cast<CShip*>(shooter));
  if (shooter_collision.HasCollision() &&
      shooter_collision.time <= horizon + g_fp_error_epsilon) {
    result.shooter_reliable = false;
  }

  if (target != nullptr && target->GetKind() == SHIP) {
    const CShip* target_ship = static_cast<const CShip*>(target);
    const auto target_collision =
        Pathfinding::GetFirstCollision(const_cast<CShip*>(target_ship));
    if (target_collision.HasCollision() &&
        target_collision.time <= horizon + g_fp_error_epsilon) {
      result.target_reliable = false;
    }
  }

  return result;
}

inline LaserResources ComputeLaserResources(const CShip* ship,
                                            double fuel_reserve) {
  LaserResources resources;
  resources.damage_per_unit = DamagePerExtraUnit();
  resources.available_fuel = ship->GetAmount(S_FUEL) - fuel_reserve;
  if (resources.available_fuel > g_fp_error_epsilon) {
    resources.max_beam_length = std::min(
        512.0, resources.available_fuel * g_laser_range_per_fuel_unit);
  } else {
    resources.available_fuel = 0.0;
    resources.max_beam_length = 0.0;
  }
  return resources;
}

inline double ComputeLaserDamage(double beam_length, double target_distance) {
  double extra_length = beam_length - target_distance;
  if (extra_length <= g_fp_error_epsilon) {
    return 0.0;
  }
  return DamageForExtraLength(extra_length);
}

inline bool FutureLineOfFire(const CShip* shooter,
                             const CThing* target,
                             double* predicted_distance = nullptr,
                             unsigned int turns = 1) {
  if (shooter == nullptr || target == nullptr) {
    return false;
  }

  const double lookahead = g_game_turn_duration *
                           static_cast<double>(std::max(1u, turns));
  const CCoord future_shooter = shooter->PredictPosition(lookahead);
  const CCoord future_target  = target->PredictPosition(lookahead);
  const double distance = future_shooter.DistTo(future_target);

  const CTraj desired = future_shooter.VectTo(future_target);
  const double future_orient =
      shooter->GetOrient() + shooter->GetOrder(O_TURN);
  const CTraj actual{distance, NormalizeAngle(future_orient)};

  const double miss =
      desired.ConvertToCoord().DistTo(actual.ConvertToCoord());
  if (miss > target->GetSize() * 0.5) {
    return false;
  }

  if (predicted_distance != nullptr) {
    *predicted_distance = distance;
  }
  return true;
}

inline BeamEvaluation EvaluateBeam(double beam_length, double target_distance) {
  BeamEvaluation eval;
  eval.beam_length = beam_length;
  eval.target_distance = target_distance;
  eval.expected_damage = ComputeLaserDamage(beam_length, target_distance);
  eval.fuel_cost = ComputeLaserFuelCost(beam_length);
  eval.efficiency = (eval.fuel_cost > g_fp_error_epsilon)
                        ? (eval.expected_damage / eval.fuel_cost)
                        : 0.0;
  return eval;
}

inline void LogPotshotDecision(const CShip* shooter,
                               const CThing* target,
                               const BeamEvaluation& eval,
                               const char* reason) {
  if (!(g_pParser && g_pParser->verbose)) {
    return;
  }

  const CCoord shooter_pos = shooter->GetPos();
  const CCoord target_pos = target->GetPos();
  const char* target_kind =
      (target->GetKind() == STATION)
          ? "Station"
          : (target->GetKind() == SHIP) ? "Ship" : "Thing";

  std::printf("\t[Potshot] %s -> %s '%s'\n",
              shooter->GetName(),
              target_kind,
              target->GetName());
  std::printf("\t  shooter_pos(%.1f, %.1f) target_pos(%.1f, %.1f)\n",
              shooter_pos.fX,
              shooter_pos.fY,
              target_pos.fX,
              target_pos.fY);
  std::printf("\t  dist=%.1f beam=%.1f dmg=%.2f fuel=%.2f eff=%.2f : %s\n",
              eval.target_distance,
              eval.beam_length,
              eval.expected_damage,
              eval.fuel_cost,
              eval.efficiency,
              reason);
}

inline double ClampBeamToRange(double beam_length) {
  return std::clamp(beam_length, 0.0, 512.0);
}

}  // namespace laser
}  // namespace groogather

#endif  // GROONEW_LASER_UTILS_H_
