/* TrenchRun.C
 * Violence and combat logic implementation for Groonew team
 * Extracted from Groonew.C for better code organization
 */

#include <algorithm>
#include <cmath>
#include <set>  // Original include
#include <vector>

#include "Asteroid.h"
#include "GameConstants.h"
#include "Groonew.h"  // For groonew::constants
#include "LaserUtils.h"
#include "ParserModern.h"
#include "Pathfinding.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "Thing.h"
#include "TrenchRun.h"
#include "World.h"

// External reference to global parser instance
extern CParser* g_pParser;

namespace TrenchRun {

//////////////////////////////////////////////
// Private Implementation Details
//////////////////////////////////////////////

namespace detail {

// Context structure containing all data needed for violence execution
struct ViolenceContext {
  CShip* ship = nullptr;
  unsigned int shipnum = 0;
  CTeam* team = nullptr;
  CWorld* world = nullptr;

  // Fuel and Laser capabilities
  double current_fuel = 0.0;
  double available_fuel = 0.0;  // Fuel above reserve
  double max_beam_length = 0.0;
  double emergency_fuel_reserve = 0.0;
  double fuel_replenish_threshold = 0.0;

  // Game state information
  double enemy_base_vinyl = 0.0;
  bool uranium_available = false;
  bool zero_reserve_phase = false;  // End-game or no uranium left

  // Pathfinding information for the selected target
  PathInfo best_path;
};

// Phases of station attack sequence
enum class StationPhase { Navigate, ExitDock, HoldPosition, LostLock };

// --- Utility ---
// Helper to normalize angles to [-PI, PI]
inline double NormalizeAngle(double angle) {
  // Original normalization logic:
  while (angle > PI)
    angle -= PI2;
  while (angle < -PI)
    angle += PI2;
  return angle;
}

// --- Function Prototypes (Internal Linkage) ---

// --- Context Building ---
ViolenceContext BuildContext(CShip* ship, unsigned int shipnum,
                             double uranium_left_in_world);

// --- Target Selection ---
// Select best enemy target based on priority system
ViolenceTarget PickTarget(ViolenceContext* ctx, MagicBag* mb);
std::vector<ViolenceTarget> IdentifyAndPrioritizeTargets(
    const ViolenceContext& ctx);
ViolenceTarget SelectTarget(ViolenceContext* ctx,
                            const std::vector<ViolenceTarget>& targets,
                            MagicBag* mb);

// --- Laser Firing Logic ---
// Evaluate whether to fire laser and execute if conditions met
bool EvaluateAndMaybeFire(CShip* shooter, const CThing* target,
                          const ViolenceContext& ctx, double distance,
                          const char* reason_if_fired,
                          bool require_efficiency = true);
// Try an opportunistic shot if target is in line of fire
bool TryOpportunisticShot(CShip* shooter, const ViolenceContext& ctx,
                          const CThing* target, const char* reason,
                          bool require_efficiency = true);

// --- Station Attack Strategy ---
void ExecuteAgainstStation(const ViolenceContext& ctx,
                           const ViolenceTarget& target);
// Determine which phase of station attack we're in
StationPhase DetermineStationPhase(double distance_to_station,
                                   bool docked_at_enemy, bool facing_station,
                                   const CTraj& ship_velocity);
void HandleCombatExitDock(const ViolenceContext& ctx, bool rotate_before_exit = true);
void HandleStationNavigate(const ViolenceContext& ctx);
void HandleStationHoldPosition(const ViolenceContext& ctx,
                               CStation* enemy_station, double distance);
void HandleStationLostLock(const ViolenceContext& ctx);

// --- Ship Combat Strategy ---
void ExecuteAgainstShip(const ViolenceContext& ctx,
                        const ViolenceTarget& target,
                        double uranium_left_in_world,
                        double vinyl_left_in_world, bool ramming_speed);
void ExecuteRamming(const ViolenceContext& ctx);
void ExecuteShipCombat(const ViolenceContext& ctx, const ViolenceTarget& target,
                       double uranium_left_in_world,
                       double vinyl_left_in_world);
CShip* FindNearestUndockedEnemy(const ViolenceContext& ctx,
                                double* distance_out);
bool HandleCloseEngagement(const ViolenceContext& ctx, const ViolenceTarget& target,
                           double uranium_left,
                           double vinyl_left);
void HandleIntercept(const ViolenceContext& ctx, const ViolenceTarget& target);

// ============================================================================
// Context Implementation
// ============================================================================

ViolenceContext BuildContext(CShip* ship, unsigned int shipnum,
                             double uranium_left_in_world) {
  ViolenceContext ctx;
  ctx.ship = ship;
  ctx.shipnum = shipnum;
  ctx.team = (ship != NULL) ? ship->GetTeam() : NULL;
  ctx.world = (ctx.team != NULL) ? ctx.team->GetWorld() : NULL;
  ctx.current_fuel = (ship != NULL) ? ship->GetAmount(S_FUEL) : 0.0;
  ctx.uranium_available = (uranium_left_in_world > g_fp_error_epsilon);

  if (ship != NULL) {
    // No fuel reserve if: (1) turn >= GAME_NEARLY_OVER, OR (2) no uranium left
    // in world
    ctx.zero_reserve_phase =
        ((ctx.world != NULL &&
          ctx.world->GetGameTime() >= groonew::constants::GAME_NEARLY_OVER) ||
         uranium_left_in_world <= g_fp_error_epsilon);

    ctx.emergency_fuel_reserve = ctx.zero_reserve_phase
                                     ? groonew::constants::FINAL_FUEL_RESERVE
                                     : groonew::constants::FUEL_RESERVE;
    ctx.fuel_replenish_threshold = ctx.zero_reserve_phase
                                       ? groonew::constants::FINAL_FUEL_RESERVE
                                       : groonew::constants::FUEL_RESERVE;

    groonew::laser::LaserResources resources =
        groonew::laser::ComputeLaserResources(ship, ctx.emergency_fuel_reserve);
    ctx.available_fuel = resources.available_fuel;
    ctx.max_beam_length = resources.max_beam_length;
  }

  if (ctx.world != NULL && ctx.team != NULL) {
    // Check if enemy base has vinyl (for end-game determination)
    for (unsigned int idx = ctx.world->UFirstIndex; idx != BAD_INDEX;
         idx = ctx.world->GetNextIndex(idx)) {
      CThing* thing = ctx.world->GetThing(idx);
      if (thing == NULL || !thing->IsAlive()) {
        continue;
      }
      if (thing->GetKind() != STATION) {
        continue;
      }
      CTeam* thing_team = thing->GetTeam();
      if (thing_team == NULL) {
        continue;
      }
      if (thing_team->GetTeamNumber() == ctx.team->GetTeamNumber()) {
        continue;
      }
      ctx.enemy_base_vinyl = static_cast<CStation*>(thing)->GetVinylStore();
      break;  // Only one enemy station
    }
  }

  return ctx;
}

// ============================================================================
// Target Selection Implementation
// ============================================================================

ViolenceTarget PickTarget(ViolenceContext* ctx, MagicBag* mb) {
  ViolenceTarget best;
  if (ctx == NULL || ctx->ship == NULL || mb == NULL) {
    return best;
  }

  CTeam* team = ctx->team;
  CWorld* world = ctx->world;
  if (team == NULL || world == NULL) {
    return best;
  }

  // 1. Identify and prioritize all potential targets.
  std::vector<ViolenceTarget> targets = IdentifyAndPrioritizeTargets(*ctx);

  if (targets.empty()) {
    return best;
  }

  // 2. Select the best target based on strategy and pathfinding data.
  return SelectTarget(ctx, targets, mb);
}

std::vector<ViolenceTarget> IdentifyAndPrioritizeTargets(
    const ViolenceContext& ctx) {
  std::vector<ViolenceTarget> targets;
  CWorld* world = ctx.world;
  CTeam* team = ctx.team;

  // Scan world for enemy targets
  for (unsigned int idx = world->UFirstIndex; idx != BAD_INDEX;
       idx = world->GetNextIndex(idx)) {
    CThing* thing = world->GetThing(idx);
    if (thing == NULL || !thing->IsAlive()) {
      continue;
    }

    ThingKind kind = thing->GetKind();
    if (kind != STATION && kind != SHIP) {
      continue;
    }

    CTeam* thing_team = thing->GetTeam();
    if (thing_team == NULL ||
        thing_team->GetTeamNumber() == team->GetTeamNumber()) {
      continue;
    }

    ViolenceTarget target;
    target.thing = thing;

    if (kind == STATION) {
      double vinyl = static_cast<CStation*>(thing)->GetVinylStore();
      if (vinyl > g_fp_error_epsilon) {
        target.priority_class = 1;
      } else {
        // If we couldn't find any other targets, just hang around the enemy base.
        target.priority_class = 4;
      }
    } else {
      CShip* enemy = static_cast<CShip*>(thing);
      if (enemy->IsDocked()) {
        continue;  // Skip docked enemy ships - they're safe at their base
      }

      double cargo = enemy->GetAmount(S_CARGO);
      double shields = enemy->GetAmount(S_SHIELD);
      double fuel = enemy->GetAmount(S_FUEL);

      if (cargo > g_fp_error_epsilon) {
        // Second priority: ships with vinyl
        // Sort by: most vinyl (desc), least shields (asc), least fuel (asc)
        target.priority_class = 2;
        target.sort_key1 = -cargo;   // Negate for descending
        target.sort_key2 = shields;  // Ascending
        target.sort_key3 = fuel;     // Ascending
      } else {
        // Third priority: other enemy ships
        // Sort by: least shields (asc), least fuel (asc)
        target.priority_class = 3;
        target.sort_key1 = shields;
        target.sort_key2 = fuel;
        target.sort_key3 = 0.0;
      }
    }

    targets.push_back(target);
  }

  // Sort targets by priority using the overloaded operator<
  std::sort(targets.begin(), targets.end());

  // DEBUG: Log all identified targets
  if (g_pParser && g_pParser->verbose) {
    printf("\t[VIOLENCE] Identified %zu potential targets:\n", targets.size());
    for (const auto& t : targets) {
      const char* type = (t.thing->GetKind() == STATION) ? "STATION" : "SHIP";
      printf("\t  - Priority %d: %s %s\n", t.priority_class, type, t.thing->GetName());
    }
  }

  return targets;
}

ViolenceTarget SelectTarget(ViolenceContext* ctx,
                            const std::vector<ViolenceTarget>& targets,
                            MagicBag* mb) {
  // DEBUG - A/B test of different target selection methods: false = path-based, true = value-based
  bool value_based = true;

  const auto& ship_paths = mb->getShipPaths(ctx->shipnum);

  // DEBUG: Log which targets have paths in MagicBag
  if (g_pParser && g_pParser->verbose) {
    printf("\t[VIOLENCE] Checking %zu targets for paths in MagicBag:\n", targets.size());
    for (const auto& target : targets) {
      auto it = ship_paths.find(target.thing);
      const char* type = (target.thing->GetKind() == STATION) ? "STATION" : "SHIP";
      printf("\t  - Priority %d %s %s: %s\n",
             target.priority_class, type, target.thing->GetName(),
             (it != ship_paths.end()) ? "HAS PATH" : "NO PATH");
    }
  }

  if (value_based) {
    // Select targets based on highest value.
    for (const auto& target : targets) {
      // Check if we have a path to this target in MagicBag
      auto it = ship_paths.find(target.thing);
      if (it != ship_paths.end()) {
        ctx->best_path = it->second;
        printf("DEBUG: Best target found: %s\n", target.thing->GetName());
        return target;
      }
    }
  } else {
    // Select targets based on:
    // Enemy stations with vinyl first
    // Then ships that we have the shortest time to intercept to regardless of priority class.
    // Then enemy stations without vinyl.

    // First, look for priority class 1 (enemy station with vinyl)
    for (const auto& target : targets) {
      if (target.priority_class == 1) {
        auto it = ship_paths.find(target.thing);
        if (it != ship_paths.end()) {
          ctx->best_path = it->second;
          printf("DEBUG: Best target found (priority 1): %s\n",
                 target.thing->GetName());
          return target;
        }
      }
    }

    // For priority class 2 and 3, find the one with fastest time_to_intercept
    double fastest_time = 1e9;
    const ViolenceTarget* fastest_target = NULL;
    const PathInfo* fastest_path = NULL;

    for (const auto& target : targets) {
      if (target.priority_class == 2 || target.priority_class == 3) {
        auto it = ship_paths.find(target.thing);
        if (it != ship_paths.end()) {
          if (it->second.time_to_intercept < fastest_time) {
            fastest_time = it->second.time_to_intercept;
            // Store pointers to the target and path
            fastest_target = &target;
            fastest_path = &(it->second);
          }
        }
      }
    }

    if (fastest_target != NULL) {
      // Update the context path before returning the target
      ctx->best_path = *fastest_path;
      printf("DEBUG: Best target found (fastest intercept %.2f): %s\n",
             fastest_time, fastest_target->thing->GetName());
      return *fastest_target;
    }

    // If we couldn't find any targets with priority class 2 or 3, 
    // look for priority class 4 (enemy station without vinyl)
    for (const auto& target : targets) {
      if (target.priority_class == 4) {
        auto it = ship_paths.find(target.thing);
        if (it != ship_paths.end()) {
          ctx->best_path = it->second;
          printf("DEBUG: Best target found (priority 4): %s\n",
                 target.thing->GetName());
          return target;
        }
      }
    }
  }
  return ViolenceTarget();  // No target found
}

// ============================================================================
// Laser Firing Logic Implementation
// ============================================================================

bool EvaluateAndMaybeFire(CShip* shooter, const CThing* target,
                          const ViolenceContext& ctx, double distance,
                          const char* reason_if_fired,
                          bool require_efficiency) {
  double beam_length = ctx.max_beam_length;

  bool is_station = target->GetKind() == STATION;
  bool is_ship = target->GetKind() == SHIP;
  bool is_docked = is_ship && static_cast<const CShip*>(target)->IsDocked();

  if (target->GetKind() == STATION) {
    // Calculate max useful beam length plus margin (30.0).
    double max_useful_beam = groonew::laser::BeamLengthForExactDamage(
                                 distance, ctx.enemy_base_vinyl) +
                             Config::STATION_BEAM_OVERKILL_MARGIN;
    beam_length = std::min(beam_length, max_useful_beam);
  }

  groonew::laser::BeamEvaluation eval =
      groonew::laser::EvaluateBeam(beam_length, distance);
  bool efficient = (!require_efficiency) ||
                   groonew::laser::IsEfficientShot(beam_length, distance) ||
                   ctx.zero_reserve_phase;

  bool fired = false;
  // Check fuel conditions: must be above replenish threshold AND have available
  // fuel above reserve.
  if (ctx.current_fuel > ctx.fuel_replenish_threshold + g_fp_error_epsilon &&
      ctx.available_fuel > g_fp_error_epsilon && efficient) {
    if (is_station || (is_ship && !is_docked)) {
      groonew::laser::LogPotshotDecision(shooter, target, eval, reason_if_fired);
      shooter->SetOrder(O_LASER, beam_length);
      fired = true;
    }
  } else {
    // Log the decision to skip the shot.
    groonew::laser::LogPotshotDecision(
        shooter, target, eval,
        efficient ? reason_if_fired : "skip (poor efficiency)");
  }

  return fired;
}

// Try an opportunistic shot if target is in line of fire
bool TryOpportunisticShot(CShip* shooter, const ViolenceContext& ctx,
                          const CThing* target, const char* reason,
                          bool require_efficiency) {
  if (target == nullptr) {
    return false;
  }

  double future_distance = 0.0;
  // Check if the target will be in the line of fire next turn.
  if (!groonew::laser::FutureLineOfFire(shooter, target, &future_distance)) {
    return false;
  }

  if (future_distance >= ctx.max_beam_length) {
    return false;
  }

  // Evaluate if the trajectories of both shooter and target are predictable (no
  // imminent collisions).
  auto predictability =
      groonew::laser::EvaluateFiringPredictability(shooter, target);
  if (!predictability.BothReliable()) {
    return false;
  }

  return EvaluateAndMaybeFire(shooter, target, ctx, future_distance, reason,
                              require_efficiency);
}

// ============================================================================
// Station Attack Implementation
// ============================================================================

void ExecuteAgainstStation(const ViolenceContext& ctx,
                           const ViolenceTarget& target) {
  if (ctx.ship == NULL || target.thing == NULL) {
    return;
  }

  CShip* ship = ctx.ship;
  CStation* enemy_station = static_cast<CStation*>(target.thing);
  double distance = ship->GetPos().DistTo(enemy_station->GetPos());

  // Check if docked at the enemy station (e.g., accidental delivery).
  // The original check included a small buffer (5.0).
  bool docked_at_enemy =
      (ship->IsDocked() && distance < g_ship_default_docking_distance + 5.0);

  const bool facing_station = ship->IsFacing(*enemy_station);
  const CTraj ship_velocity = ship->GetVelocity();

  // Determine the current phase
  StationPhase phase = DetermineStationPhase(distance, docked_at_enemy,
                                             facing_station, ship_velocity);

  // Execute the phase handler
  switch (phase) {
    case StationPhase::ExitDock:
      HandleCombatExitDock(ctx);
      return;  // ExitDock issues orders and returns early.
    case StationPhase::Navigate:
      HandleStationNavigate(ctx);
      break;
    case StationPhase::HoldPosition:
      HandleStationHoldPosition(ctx, enemy_station, distance);
      break;
    case StationPhase::LostLock:
      HandleStationLostLock(ctx);
      break;
  }

  // Opportunistic Firing (if safe from immediate collisions)
  const auto upcoming = Pathfinding::GetFirstCollision(ship);
  // Check if collision is more than 1 turn away.
  if (!upcoming.HasCollision() ||
      upcoming.time > g_game_turn_duration + g_fp_error_epsilon) {
    double future_distance = distance;
    // Check line of fire for the next turn and ensure we remain within
    // engagement range (100.0).
    if (groonew::laser::FutureLineOfFire(ship, enemy_station,
                                         &future_distance) &&
        future_distance < Config::STATION_ENGAGEMENT_RANGE) {
      EvaluateAndMaybeFire(ship, enemy_station, ctx, future_distance,
                           "fire (maintain pressure)");
    }
  }
}

StationPhase DetermineStationPhase(double distance_to_station,
                                   bool docked_at_enemy, bool facing_station,
                                   const CTraj& ship_velocity) {
  if (docked_at_enemy) {
    return StationPhase::ExitDock;
  }

  // Check if outside engagement range (100.0).
  if (distance_to_station >= Config::STATION_ENGAGEMENT_RANGE) {
    return StationPhase::Navigate;
  }

  // We are close. Hold position if facing or moving slowly (velocity < 1.0).
  if (facing_station ||
      ship_velocity.rho < Config::STATION_LOW_VELOCITY_THRESHOLD) {
    return StationPhase::HoldPosition;
  }

  // Close but not facing and moving fast. Lost lock.
  return StationPhase::LostLock;
}

void HandleCombatExitDock(const ViolenceContext& ctx, bool rotate_before_exit) {
  CShip* ship = ctx.ship;

  if (rotate_before_exit) {
    // Station combat mode: Rotate to fixed exit angle, then thrust backward
    // Assign fixed exit angles based on ship number to spread them out (Original
    // logic).
    const double exit_angles[4] = {PI / 2.0, 0.0, -PI / 2.0, -PI};
    double target_exit_angle = exit_angles[ctx.shipnum % 4];

    double current_orient = ship->GetOrient();
    double angle_diff = target_exit_angle - current_orient;
    // Normalize angle difference (Original logic)
    while (angle_diff > PI)
      angle_diff -= PI2;
    while (angle_diff < -PI)
      angle_diff += PI2;

    // Check if alignment is needed (tolerance 0.1).
    if (fabs(angle_diff) > Config::EXIT_DOCK_ANGLE_TOLERANCE) {
      if (g_pParser && g_pParser->verbose) {
        printf("\t→ PHASE: Combat ExitDock (turn) %.2f -> %.2f (diff=%.2f)\n",
               current_orient, target_exit_angle, angle_diff);
      }
      ship->SetOrder(O_TURN, angle_diff);
    } else {
      // Aligned, thrust backward.
      if (g_pParser && g_pParser->verbose) {
        printf("\t→ PHASE: Combat ExitDock (thrust backward)\n");
      }
      ship->SetOrder(O_THRUST, -1.0);
    }
  } else {
    // Ship combat mode: No rotation, just thrust forward to undock
    // (Ship should already be facing the enemy from previous turn's rotation)
    if (g_pParser && g_pParser->verbose) {
      printf("\t→ PHASE: Combat ExitDock (thrust forward, no rotation)\n");
    }
    // May as well ram them...
    ship->SetOrder(O_THRUST, 60.0);
  }
}

void HandleStationNavigate(const ViolenceContext& ctx) {
  CShip* ship = ctx.ship;
  if (g_pParser && g_pParser->verbose) {
    // Calculate current distance for logging purposes (distance was local in
    // ExecuteAgainstStation)
    double distance = ship->GetPos().DistTo(ctx.best_path.dest->GetPos());
    printf("\t→ PHASE: Navigate to station (dist=%.1f)\n", distance);
    printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\n",
           ctx.best_path.fueltraj.time_to_arrive,
           (ctx.best_path.fueltraj.order_kind == O_THRUST) ? "thrust"
           : (ctx.best_path.fueltraj.order_kind == O_TURN) ? "turn"
                                                           : "other/none",
           ctx.best_path.fueltraj.order_mag);
  }
  // Follow the precomputed path.
  ship->SetOrder(ctx.best_path.fueltraj.order_kind,
                 ctx.best_path.fueltraj.order_mag);
}

void HandleStationHoldPosition(const ViolenceContext& ctx,
                               CStation* enemy_station, double distance) {
  CShip* ship = ctx.ship;
  const CTraj ship_velocity = ship->GetVelocity();
  bool facing_station = ship->IsFacing(*enemy_station);

  double angle_to_station = ship->GetPos().AngleTo(enemy_station->GetPos());

  // Calculate radial velocity (component towards or away from the station).
  double radial_velocity =
      ship_velocity.rho * cos(ship_velocity.theta - angle_to_station);

  // Check if drift exceeds tolerance (0.5).
  if (facing_station &&
      fabs(radial_velocity) > Config::STATION_RADIAL_VELOCITY_TOLERANCE) {
    // Counteract drift.
    if (g_pParser && g_pParser->verbose) {
      printf("\t→ PHASE: HoldPosition (counter drift %.2f)\n", radial_velocity);
    }
    if (radial_velocity > 0.0) {
      // We are moving towards the station, back up.
      ship->SetOrder(O_THRUST, -1.0);
    } else {
      // We are moving away from the station, move forward.
      ship->SetOrder(O_THRUST, 1.0);
    }
  } else if (!facing_station) {
    // Turn to face the station.
    double angle_diff = angle_to_station - ship->GetOrient();
    // Normalize angle difference (Original logic)
    while (angle_diff > PI)
      angle_diff -= PI2;
    while (angle_diff < -PI)
      angle_diff += PI2;

    if (g_pParser && g_pParser->verbose) {
      printf("\t→ PHASE: HoldPosition (turn to face) diff=%.2f\n", angle_diff);
    }
    ship->SetOrder(O_TURN, angle_diff);

    // Fire while aligning (efficiency not required).
    EvaluateAndMaybeFire(ship, enemy_station, ctx, distance,
                         "fire (station alignment)",
                         /*require_efficiency=*/false);
  }
  // If facing and velocity is low, we drift and rely on opportunistic firing in
  // the main loop.
}

void HandleStationLostLock(const ViolenceContext& ctx) {
  CShip* ship = ctx.ship;
  if (g_pParser && g_pParser->verbose) {
    // Recalculate distance for logging.
    double distance = ship->GetPos().DistTo(ctx.best_path.dest->GetPos());
    printf("\t→ PHASE: LostLock (reacquire) dist=%.1f\n", distance);
  }
  // Revert to the pathfinding plan.
  ship->SetOrder(ctx.best_path.fueltraj.order_kind,
                 ctx.best_path.fueltraj.order_mag);
}

// ============================================================================
// Ship Combat Implementation
// ============================================================================

void ExecuteAgainstShip(const ViolenceContext& ctx,
                        const ViolenceTarget& target,
                        double uranium_left_in_world,
                        double vinyl_left_in_world, bool ramming_speed) {
  // Most have valid ship and target and they must differ.
  if (ctx.ship == NULL || target.thing == NULL || ctx.ship == target.thing) {
    return;
  }

  // Ensure the target is actually a ship (Original check)
  if (target.thing->GetKind() != SHIP) {
    return;
  }

  // Check if ramming strategy is active (endgame condition: ramming_speed
  // enabled AND enemy base empty).
  if (ramming_speed && ctx.enemy_base_vinyl <= g_fp_error_epsilon) {
    ExecuteRamming(ctx);
  } else {
    ExecuteShipCombat(ctx, target, uranium_left_in_world, vinyl_left_in_world);
  }
}

void ExecuteRamming(const ViolenceContext& ctx) {
  CShip* ship = ctx.ship;
  CWorld* world = ctx.world;

  // Ramming strategy: Boost shields if needed and follow the path.
  if (world != NULL && ctx.available_fuel > 0.0) {
    double current_shields = ship->GetAmount(S_SHIELD);
    // Determine shield target based on game phase.
    // If nearly over, conserve fuel (target 0.0), otherwise maintain buffer
    // (13.0).
    double shield_target =
        (world->GetGameTime() >= groonew::constants::GAME_NEARLY_OVER) ? 0.0
                                                                       : 13.0;
    if (current_shields < shield_target) {
      double shield_boost =
          std::min(shield_target - current_shields, ctx.available_fuel);
      ship->SetOrder(O_SHIELD, shield_boost);
    }
  }
  // Follow the path to ram the target.
  ship->SetOrder(ctx.best_path.fueltraj.order_kind,
                 ctx.best_path.fueltraj.order_mag);
}

CShip* FindNearestUndockedEnemy(const ViolenceContext& ctx,
                                double* distance_out) {
  CShip* nearest_enemy = nullptr;
  // Initialize slightly outside the maximum engagement range.
  double min_distance = groonew::constants::MAX_SHIP_ENGAGEMENT_DIST + 1.0;

  CWorld* world = ctx.world;
  CTeam* team = ctx.team;
  CShip* ship = ctx.ship;

  if (world == NULL || team == NULL || ship == NULL) {
    if (distance_out)
      *distance_out = min_distance;
    return nullptr;
  }

  // Scan the world for the nearest enemy ship.
  for (unsigned int idx = world->UFirstIndex; idx != BAD_INDEX;
       idx = world->GetNextIndex(idx)) {
    CThing* thing_iter = world->GetThing(idx);
    if (thing_iter == NULL || !thing_iter->IsAlive() ||
        thing_iter->GetKind() != SHIP) {
      continue;
    }

    CTeam* thing_team = thing_iter->GetTeam();
    if (thing_team == NULL ||
        thing_team->GetTeamNumber() == team->GetTeamNumber()) {
      continue;
    }

    CShip* enemy_ship_iter = static_cast<CShip*>(thing_iter);
    if (enemy_ship_iter->IsDocked())
      continue;

    double distance = ship->GetPos().DistTo(enemy_ship_iter->GetPos());
    if (distance < min_distance) {
      min_distance = distance;
      nearest_enemy = enemy_ship_iter;
    }
  }

  if (distance_out)
    *distance_out = min_distance;
  return nearest_enemy;
}

void ExecuteShipCombat(const ViolenceContext& ctx, const ViolenceTarget& target,
                       double uranium_left_in_world,
                       double vinyl_left_in_world) {
  // Flag to track if an order was issued during engagement logic (Original
  // logic used 'issued_order').
  bool engaged = false;
  double distance_to_target = ctx.ship->GetPos().DistTo(target.thing->GetPos());
  // Close Engagement: If an enemy is within range (160.0), prioritize
  // maneuvering and firing.
  if (distance_to_target <= groonew::constants::MAX_SHIP_ENGAGEMENT_DIST) {
    engaged = HandleCloseEngagement(ctx, target,
                                    uranium_left_in_world, vinyl_left_in_world);
  }

  // Intercept/Pursuit: If no order issued yet (either no close enemy, or
  // engagement logic deferred).
  if (!engaged) {
    HandleIntercept(ctx, target);
    // HandleIntercept always issues a navigation order and potentially a laser
    // order potshot if something is in line of fire.
  }
}

bool HandleCloseEngagement(const ViolenceContext& ctx, const ViolenceTarget& target, double uranium_left,
                           double vinyl_left) {
  CShip* ship = ctx.ship;

  if (!target.IsValid()) {
    return false;
  }

  CThing *target_thing = target.thing;
  bool is_station = target_thing->GetKind() == STATION;
  bool is_ship = target_thing->GetKind() == SHIP;
  bool is_docked = is_ship && static_cast<CShip*>(target_thing)->IsDocked();
  if (is_ship && is_docked) {
    // Can't shoot docked ships - however the logic here assumes our target isn't docked.
    // Here we just give up if it happens, but PickTarget shouldn't send us docked
    // targets.
    return false;
  }

  bool end_game = false;
  // We engage in endgame behavior if there are no resources left in the world and either:
  // we're shooting an enemy station with vinyl, or the enemy station has no vinyl.
  if ((uranium_left + vinyl_left) <= g_fp_error_epsilon) {
    bool attacking_enemy_station_with_vinyl = (is_station && (ctx.enemy_base_vinyl > g_fp_error_epsilon));
    bool enemy_station_has_no_vinyl = (ctx.enemy_base_vinyl < g_fp_error_epsilon);
    end_game = attacking_enemy_station_with_vinyl || enemy_station_has_no_vinyl;
  }

  // 1. Determine facing and firing opportunity
  double future_distance = ship->GetPos().DistTo(target_thing->GetPos());
  // Check future line of fire.
  // Future distance set to the distance to enemy if they will be in LoF next turn.
  bool has_line_of_fire =
      groonew::laser::FutureLineOfFire(ship, target_thing, &future_distance);

  // Check predictability.
  auto predictability =
      groonew::laser::EvaluateFiringPredictability(ship, target_thing);

  if (predictability.BothReliable()) {
    if (has_line_of_fire) {
      // If we're docked and would shoot, undock first (DO NOT SHOOT while docked)
      if (ship->IsDocked()) {
        HandleCombatExitDock(ctx, /*rotate_before_exit=*/false);
        return true;  // Undocking takes priority, shoot next turn
      }

      // Not docked, safe to fire
      bool shot_taken = EvaluateAndMaybeFire(
          ship, target_thing, ctx, future_distance,
          end_game ? "fire (handle-close fire end-game full blast)" : "fire (handle-close fire efficient)",
          /*require_efficiency=*/!end_game);

      if (shot_taken) {
        return true;  // Fired laser, engagement handled
      }
    } else {
      // 2. We need to issue a turn order to face the target before we can fire.

      // Predict positions for the next turn (T+1)
      double lookahead_time = g_game_turn_duration;
      CCoord enemy_future_pos = target_thing->PredictPosition(lookahead_time);
      CCoord our_future_pos = ship->PredictPosition(lookahead_time);
      double predicted_distance_t1 = our_future_pos.DistTo(enemy_future_pos);

      // If we remain within engagement range (160.0), turn to face the enemy's
      // predicted position (can turn while docked)
      if (predicted_distance_t1 <= groonew::constants::MAX_SHIP_ENGAGEMENT_DIST) {
        double angle_to_target_t1 = our_future_pos.AngleTo(enemy_future_pos);
        // Calculate required turn amount using normalization helper.
        double angle_diff = NormalizeAngle(angle_to_target_t1 - ship->GetOrient());

        ship->SetOrder(O_TURN, angle_diff);
        EvaluateAndMaybeFire(
          ship, target_thing, ctx, predicted_distance_t1,
          end_game ? "fire (handle-close turn-fire end-game full blast)" : "fire (handle-close turn-fire efficient)",
          /*require_efficiency=*/!end_game);
        return true;  // Issued turn order
      }
    }
  }

  // Either the scenario is too unreliable to fire, or we're moving out of range,
  // return false to signal reversion to intercept/pursuit logic.
  return false;
}

void HandleIntercept(const ViolenceContext& ctx, const ViolenceTarget& target) {
  CShip* ship = ctx.ship;

  // Follow the pre-calculated intercept path
  ship->SetOrder(ctx.best_path.fueltraj.order_kind,
                 ctx.best_path.fueltraj.order_mag);

  // Try an opportunistic shot while intercepting/pursuing.

  // Determine the appropriate reason string based on context (Intercept vs
  // Pursuit). We approximate the original intent by checking the current
  // distance.

  double distance = ship->GetPos().DistTo(target.thing->GetPos());
  const char* reason;

  // If the target is within engagement distance, we are in "pursuit"
  // (HandleCloseEngagement failed to issue order).
  if (distance <= groonew::constants::MAX_SHIP_ENGAGEMENT_DIST) {
    reason = "fire (pursuit opportunist)";
  } else {
    // Otherwise, we are intercepting from afar.
    reason = "fire (intercept opportunist)";
  }

  bool its_away = TryOpportunisticShot(ship, ctx, target.thing, reason);

  if (!its_away) {
    // We didn't get a shot off on our target - but let's see if there is
    // a closer enemy who happens to be in line of fire.
    double nearest_distance;
    CShip* nearest_enemy = FindNearestUndockedEnemy(ctx, &nearest_distance);
    if (nearest_enemy != NULL && nearest_distance <= groonew::constants::MAX_SHIP_ENGAGEMENT_DIST) {
      TryOpportunisticShot(ship, ctx, nearest_enemy, "fire (intercept potshot)");    
    }
  }
}

}  // namespace detail

//////////////////////////////////////////////
// Public Interface Implementation
//////////////////////////////////////////////

ViolenceResult ExecuteViolence(CShip* ship, unsigned int shipnum,
                               double cur_fuel, bool uranium_available,
                               MagicBag* mb, double uranium_left_in_world,
                               double vinyl_left_in_world, bool ramming_speed) {
  if (ship == nullptr) {
    return ViolenceResult::NO_TARGET_FOUND;
  }

  // 1. Build the context using the detail implementation.
  detail::ViolenceContext ctx =
      detail::BuildContext(ship, shipnum, uranium_left_in_world);

  // 2. Dynamic fuel management. Check if refueling is needed before combat.
  // Note: We check against the globally known uranium availability (passed in)
  // as in the original code.
  if (!ctx.zero_reserve_phase && uranium_available &&
      cur_fuel <= ctx.fuel_replenish_threshold + g_fp_error_epsilon) {
    if (g_pParser && g_pParser->verbose) {
      printf(
          "\t→ [VIOLENCE override] Low fuel (%.1f <= %.1f), seeking uranium "
          "before combat...\n",
          cur_fuel, ctx.fuel_replenish_threshold);
    }
    return ViolenceResult::NEED_FUEL_FIRST;
  }

  // 3. Select the best target.
  ViolenceTarget target = detail::PickTarget(&ctx, mb);

  if (!target.IsValid()) {
    // Original Groonew.C/TrenchRun.C did not have a specific log message here
    // for "No target found".
    return ViolenceResult::NO_TARGET_FOUND;
  }

  // 4. Execute violence.
  if (g_pParser && g_pParser->verbose) {
    // Original log message
    printf("\t→ [VIOLENCE] Executing against target: %s (Priority %d)\n",
           target.thing->GetName(), target.priority_class);
  }

  // Dispatch to the specialized execution handlers in the detail namespace.
  if (target.thing->GetKind() == STATION) {
    detail::ExecuteAgainstStation(ctx, target);
  } else if (target.thing->GetKind() == SHIP && !static_cast<const CShip*>(target.thing)->IsDocked()) {
    detail::ExecuteAgainstShip(ctx, target, uranium_left_in_world,
                               vinyl_left_in_world, ramming_speed);
  }

  return ViolenceResult::VIOLENCE_EXECUTED;
}

//////////////////////////////////////////////
// Shared Combat Utilities Implementation
// (These functions were moved from GetVinyl.C)
//////////////////////////////////////////////

// Uses functionality from LaserUtils.h
using groonew::laser::BeamEvaluation;
using groonew::laser::BeamLengthForExactDamage;
using groonew::laser::EvaluateBeam;
using groonew::laser::EvaluateFiringPredictability;
using groonew::laser::FutureLineOfFire;
using groonew::laser::IsEfficientShot;
using groonew::laser::LogPotshotDecision;

FacingTargets FindEnemyFacingTargets(CShip* ship) {
  FacingTargets targets;
  if (ship == NULL) {
    return targets;
  }
  CTeam* team = ship->GetTeam();
  if (team == NULL) {
    return targets;
  }
  CWorld* world = team->GetWorld();
  if (world == NULL) {
    return targets;
  }

  // If we'll collide with something in the next turn, further reasoning would
  // be invalidated.
  const auto self_reliability = EvaluateFiringPredictability(ship, nullptr);
  if (!self_reliability.shooter_reliable) {
    return targets;
  }

  for (unsigned int idx = world->UFirstIndex; idx != BAD_INDEX;
       idx = world->GetNextIndex(idx)) {
    CThing* thing = world->GetThing(idx);
    if (thing == NULL || thing == ship || !(thing->IsAlive())) {
      continue;
    }

    ThingKind kind = thing->GetKind();
    if (kind != STATION && kind != SHIP) {
      continue;
    }

    CTeam* thing_team = thing->GetTeam();
    if (thing_team == NULL) {
      continue;
    }
    if (thing_team->GetTeamNumber() == team->GetTeamNumber()) {
      continue;
    }
    double future_distance = 0.0;  // Updated by FutureLineOfFire on success.
    if (!FutureLineOfFire(ship, thing, &future_distance)) {
      continue;
    }

    // Re-check predictability per target; stations never adjust position, so we
    // only care about movable ships colliding before the shot resolves.
    const auto reliability = EvaluateFiringPredictability(ship, thing);
    if (!reliability.shooter_reliable) {
      return targets;
    }

    if (kind == STATION) {
      // Note: We intentionally don't do collision checking
      // for stations as we know they'll still be in the same position.
      if (future_distance < targets.station_dist) {
        targets.station = static_cast<CStation*>(thing);
        targets.station_dist = future_distance;
      }
    } else {
      // Skip docked enemy ships - they're safe at their base
      CShip* enemy_ship = static_cast<CShip*>(thing);
      if (enemy_ship->IsDocked()) {
        continue;
      }

      if (!reliability.target_reliable) {
        continue;
      }

      if (future_distance < targets.ship_dist) {
        targets.ship = enemy_ship;
        targets.ship_dist = future_distance;
      }
    }
  }

  return targets;
}

bool TryStationPotshot(const groonew::laser::LaserResources& laser,
                       CShip* shooter, CStation* enemy_station,
                       double distance_to_target) {
  if (enemy_station == NULL) {
    return false;
  }
  if (distance_to_target > laser.max_beam_length) {
    return false;
  }

  double station_vinyl = enemy_station->GetVinylStore();
  double max_extra = laser.max_beam_length - distance_to_target;
  double max_damage = max_extra * laser.damage_per_unit;

  if (station_vinyl <= g_fp_error_epsilon || max_extra <= g_fp_error_epsilon) {
    return false;
  }

  // max_damage is already in vinyl units (damage_per_unit = 30 / 1000),
  // so compare directly against the station’s stored vinyl.
  double beam_length = laser.max_beam_length;

  if (max_damage >= station_vinyl) {
    beam_length = BeamLengthForExactDamage(distance_to_target, station_vinyl);
    BeamEvaluation eval = EvaluateBeam(beam_length, distance_to_target);
    LogPotshotDecision(shooter, enemy_station, eval,
                       "fire (destroy all vinyl)");
    shooter->SetOrder(O_LASER, beam_length);
    return true;
  }

  BeamEvaluation eval = EvaluateBeam(beam_length, distance_to_target);
  bool good_efficiency = IsEfficientShot(beam_length, distance_to_target);

  if (good_efficiency) {
    LogPotshotDecision(shooter, enemy_station, eval, "fire (partial damage)");
    shooter->SetOrder(O_LASER, beam_length);
    return true;
  }

  LogPotshotDecision(shooter, enemy_station, eval, "skip (poor efficiency)");
  return false;
}

bool TryShipPotshot(const groonew::laser::LaserResources& laser, CShip* shooter,
                    CShip* enemy_ship, double distance_to_target) {
  if (enemy_ship == NULL) {
    return false;
  }
  if (distance_to_target > laser.max_beam_length) {
    return false;
  }

  double max_extra = laser.max_beam_length - distance_to_target;
  double max_damage = max_extra * laser.damage_per_unit;
  if (max_damage <= g_fp_error_epsilon) {
    return false;
  }

  const double kill_margin = 0.01;
  double enemy_shield = enemy_ship->GetAmount(S_SHIELD);

  if (max_damage >= enemy_shield + kill_margin) {
    double damage_to_kill = enemy_shield + kill_margin;
    double beam_length =
        BeamLengthForExactDamage(distance_to_target, damage_to_kill);
    BeamEvaluation eval = EvaluateBeam(beam_length, distance_to_target);
    LogPotshotDecision(shooter, enemy_ship, eval, "fire (kill)");
    shooter->SetOrder(O_LASER, beam_length);
    return true;
  }

  double beam_length = laser.max_beam_length;
  BeamEvaluation eval = EvaluateBeam(beam_length, distance_to_target);
  bool good_efficiency = IsEfficientShot(beam_length, distance_to_target);

  if (good_efficiency) {
    LogPotshotDecision(shooter, enemy_ship, eval, "fire (efficient damage)");
    shooter->SetOrder(O_LASER, beam_length);
    return true;
  }

  if (enemy_shield > 6.0) {
    double min_damage_to_cross = enemy_shield - 6.0 + kill_margin;
    if (max_damage >= min_damage_to_cross) {
      LogPotshotDecision(shooter, enemy_ship, eval, "fire (force dock)");
      shooter->SetOrder(O_LASER, beam_length);
      return true;
    }

    LogPotshotDecision(shooter, enemy_ship, eval, "skip (insufficient damage)");
    return false;
  }

  LogPotshotDecision(shooter, enemy_ship, eval, "skip (already vulnerable)");
  return false;
}

}  // namespace TrenchRun