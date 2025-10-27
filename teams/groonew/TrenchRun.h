/* TrenchRun.h
 * Violence and combat logic for Groonew team
 * Extracted from Groonew.C for better code organization
 *
 * This module handles all aggressive behaviors:
 * - Target selection (enemy ships and stations)
 * - Attack execution (lasers, ramming, positioning)
 * - Combat tactics (station siege, ship pursuit)
 */

#ifndef GROONEW_TRENCH_RUN_H_
#define GROONEW_TRENCH_RUN_H_

#include <cmath>  // Required for fabs
#include <limits>

#include "GameConstants.h"  // Required for g_fp_error_epsilon
#include "LaserUtils.h"     // Required for LaserResources
#include "MagicBag.h"
#include "PathInfo.h"
#include "Ship.h"
#include "Station.h"
#include "Thing.h"
#include "Traj.h"
#include "TomorrowLand.h"

namespace TrenchRun {

// Configuration constants for combat behaviors (New addition for clarity)
namespace Config {

// Range at which station attack transitions from navigation to holding position
// (Original: 100.0)
constexpr double STATION_ENGAGEMENT_RANGE = 100.0;

// Tolerance for radial velocity when holding position near a station (Original:
// 0.5)
constexpr double STATION_RADIAL_VELOCITY_TOLERANCE = 0.5;

// Angular tolerance when aligning to exit dock (Original: 0.1)
constexpr double EXIT_DOCK_ANGLE_TOLERANCE = 0.1;

// Extra beam length added when calculating max useful beam against station
// (Original: 30.0)
constexpr double STATION_BEAM_OVERKILL_MARGIN = 30.0;

// Velocity threshold below which a ship is considered stationary during station
// attack (Original: 1.0)
constexpr double STATION_LOW_VELOCITY_THRESHOLD = 1.0;

}  // namespace Config

// Structure to hold potential targets considered during violence mode.
struct ViolenceTarget {
  CThing* thing = NULL;

  // 1=station with vinyl, 2=ship with vinyl, 3=other ship
  int priority_class = 0;  
  // For stations: 0, For ships: cargo (desc) or shields (asc)
  double sort_key1 = 0.0;  
  // For ships with cargo: shields, For others: fuel
  double sort_key2 = 0.0;  
  // For ships with cargo: fuel, For others: 0
  double sort_key3 = 0.0;  

  bool IsValid() const { return thing != NULL; }

  // Comparison operator for sorting
  bool operator<(const ViolenceTarget& other) const {
    if (priority_class != other.priority_class) {
      return priority_class < other.priority_class;
    }
    // Use epsilon comparison for floating point keys
    if (fabs(sort_key1 - other.sort_key1) > g_fp_error_epsilon) {
      return sort_key1 < other.sort_key1;
    }
    if (fabs(sort_key2 - other.sort_key2) > g_fp_error_epsilon) {
      return sort_key2 < other.sort_key2;
    }
    return sort_key3 < other.sort_key3;
  }
};

// Result of violence execution attempt
enum class ViolenceResult {
  VIOLENCE_EXECUTED,  // Violence orders were successfully issued
  NEED_FUEL_FIRST,  // Ship needs fuel first, caller should handle FUEL seeking
  NO_TARGET_FOUND   // No valid target available for violence
};

//////////////////////////////////////////////
// Main Interface
//////////////////////////////////////////////

// Main entry point for violence execution
// Returns result indicating what action was taken or what is needed
ViolenceResult ExecuteViolence(CShip* ship, unsigned int shipnum,
                               double cur_fuel, bool uranium_available,
                               MagicBag* mb, double uranium_left_in_world,
                               double vinyl_left_in_world, bool ramming_speed);

//////////////////////////////////////////////
// Shared Combat Utilities (Public for use by GetVinyl/others)
// (Refactored from GetVinyl to centralize logic)
//////////////////////////////////////////////

// Helper structure to find targets currently in the line of fire
struct FacingTargets {
  CStation* station = NULL;
  double station_dist = std::numeric_limits<double>::max();
  CShip* ship = NULL;
  double ship_dist = std::numeric_limits<double>::max();
};

// Standardized description of a ship-to-ship firing decision.
struct ShipFirePlan {
  bool should_fire = false;
  double beam_length = 0.0;
  const char* fire_reason = NULL;
  const char* skip_reason = NULL;
  bool bypass_efficiency_check = false;
};

// Computes how we should configure a laser shot against a ship, reusing the
// historic kill/force-dock heuristic. Callers can inspect the returned plan to
// decide whether to fire and which log message to use.
ShipFirePlan ComputeShipFirePlan(double max_beam_length,
                                 double damage_per_unit,
                                 double distance_to_target,
                                 double enemy_shield,
                                 bool require_efficiency);

// Scans the world for enemy targets that are predictable and in the line of
// fire.
FacingTargets FindEnemyFacingTargets(CShip* ship);

// Attempts to fire at an enemy station, prioritizing destroying all vinyl.
bool TryStationPotshot(const groonew::laser::LaserResources& laser,
                       CShip* shooter, CStation* enemy_station,
                       double distance_to_target);

// Attempts to fire at an enemy ship, prioritizing a kill shot or reducing
// shields to one-shot range.
bool TryShipPotshot(const groonew::laser::LaserResources& laser, CShip* shooter,
                    CShip* enemy_ship, double distance_to_target);

// Validates that a shot from shooter toward target with the specified beam
// length will not intersect any other predicted object first.
bool ShotIsClear(const CShip* shooter, const CThing* target,
                 double beam_length, CThing** obstructing = nullptr);

}  // namespace TrenchRun

#endif  // GROONEW_TRENCH_RUN_H_
