/* Groogather.h
 * Team Groogather: "Groogather eat Groogather!"
 * MechMania IV Contest Team Implementation
 *
 * Strategy: Predictive path planning using "Magic Bag" data structure
 * Each turn, calculates optimal paths to all objects for next 28 turns
 * Ships then select best targets based on time-to-reach
 */

#ifndef _GROONEW_
#define _GROONEW_

#include "Brain.h"
#include "FuelTraj.h"
#include "MagicBag.h"
#include "Ship.h"
#include "Team.h"
#include "Traj.h"

#include <map>
#include <vector>

namespace groogather {
namespace constants {
inline constexpr double FUEL_RESERVE = 5.0;
}  // namespace constants
}  // namespace groogather

// Ship wants are a high level goal for the ship.
enum ShipWants { HOME, POINTS, FUEL, NOTHING };

//////////////////////////////////////
// Main class: Groogather team
// Uses centralized planning with biological/hive-mind naming theme

class Groogather : public CTeam {
 public:
  Groogather();
  ~Groogather();

  // Central planning data structure - stores precalculated paths for all ships
  // Recreated each turn with fresh calculations
  MagicBag* mb;

  // Global resource tracking (updated during MagicBag population)
  double uranium_left;  // Total uranium in world
  double vinyl_left;    // Total vinyl in world

  // Scratchpad ship used for accurate fuel simulations.
  // Initialized once and reused throughout the game.
  CShip* calculator_ship;

  void Init();  // Configure ships with 20 fuel/40 cargo split
  void Turn();  // Main turn logic - populate MagicBag then execute

  // Fills MagicBag with path orders and related information to all objects
  // for each ship.
  void PopulateMagicBag();

  // Centralized target selection and order assignment for all ships
  // Called after PopulateMagicBag to assign base orders based on strategic planning
  void AssignShipOrders();

 private:
  ShipWants DetermineShipWants(CShip* ship, double cur_fuel, double cur_cargo,
                               double max_fuel, double max_cargo,
                               bool uranium_available,
                               bool vinyl_available) const;

  void HandleGoHome(CShip* ship, double cur_cargo);

  void EvaluateResourceUtilities(
      CShip* ship, ShipWants wants, unsigned int shipnum,
      std::vector<CShip*>* ships_seeking_resources,
      std::map<CShip*, unsigned int>* ship_ptr_to_shipnum);

  // Track the resource target each ship pursued on the prior turn so we can
  // reward plan continuity when utilities tie.
  std::map<CShip*, CThing*> last_turn_targets_;

  // Calculate the utility of a given path.
  double CalculateUtility(CShip* pShip, ShipWants wants, const PathInfo& e,
                          bool favor_previous_target);

  // Helper function to apply orders and log the decision.
  void ApplyOrders(CShip* pShip, const PathInfo& best_e);

  // Solves the assignment problem for resource collection using a lightweight
  // brute-force approach.
  void SolveResourceAssignment(
      const std::vector<CShip*>& agents,
      const std::map<CShip*, unsigned int>& ship_ptr_to_shipnum);
};

/////////////////////////////////////

#endif  // _GROONEW_
