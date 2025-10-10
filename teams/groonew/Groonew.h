/* Groonew.h
 * Team Groonew: "Groonew eat Groonew!"
 * MechMania IV Contest Team Implementation
 *
 * Strategy: Predictive path planning using "Magic Bag" data structure
 * Each turn, calculates optimal paths to all objects for next 28 turns
 * Ships then select best targets based on time-to-reach
 */

#ifndef _GROONEW_
#define _GROONEW_

#include "Brain.h"
#include "Collision.h"
#include "FuelTraj.h"
#include "MagicBag.h"
#include "Ship.h"
#include "Team.h"
#include "Traj.h"

#include <map>

//////////////////////////////////////
// Main class: Groonew team
// Uses centralized planning with biological/hive-mind naming theme

class Groonew : public CTeam {
 public:
  Groonew();
  ~Groonew();

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

  // Path planning functions for individual targets
  // Calculate orders (thrust/turn) to reach target in given time
  FuelTraj determine_orders(CShip* ship, CThing* thing, double time);

  // TODO: Currently returns hardcoded 5.0 - should calculate actual fuel cost
  double determine_probable_fuel_cost(CThing* thing, double time, CShip* ship);

  // TODO: Currently returns dummy collision - should check path for obstacles
  Collision detect_collisions_on_path(CThing* thing, double time, CShip* ship);

  private:
    // TODO:
    // We need a way to store and retrieve the prior targets we had last turn.
    std::map<CShip*,CThing*> PriorTargets();
};

/////////////////////////////////////

#endif  // _GROONEW_
