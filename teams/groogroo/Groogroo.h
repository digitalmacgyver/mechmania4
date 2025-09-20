/* Groogroo.h
 * Team Groogroo: "Groogroo eat Groogroo!"
 * MechMania IV Contest Team Implementation
 *
 * Strategy: Predictive path planning using "Magic Bag" data structure
 * Each turn, calculates optimal paths to all objects for next 28 turns
 * Ships then select best targets based on time-to-reach
 */

#ifndef _GROOGROO_
#define _GROOGROO_

#include "Team.h"
#include "Brain.h"
#include "MagicBag.h"
#include "Collision.h"
#include "Traj.h"
#include "FuelTraj.h"
#include "Ship.h"

//////////////////////////////////////
// Main class: Groogroo team
// Uses centralized planning with biological/hive-mind naming theme

class Groogroo : public CTeam
{
 public:
  Groogroo();
  ~Groogroo();

  // Central planning data structure - stores precalculated paths for all ships
  // Recreated each turn with fresh calculations
  MagicBag * mb;

  // Global resource tracking (updated during MagicBag population)
  double uranium_left;  // Total uranium in world
  double vinyl_left;    // Total vinyl in world

  void Init();  // Configure ships with 20 fuel/40 cargo split
  void Turn();  // Main turn logic - populate MagicBag then execute

  // Fills MagicBag with optimal paths to all objects for each ship
  // Calculates up to 28 turns into the future
  void PopulateMagicBag();

  // Path planning functions for individual targets
  // Calculate optimal order (thrust/turn) to reach target in given time
  FuelTraj determine_orders(CThing *thing, double time, CShip *ship);

  // TODO: Currently returns hardcoded 5.0 - should calculate actual fuel cost
  double determine_probable_fuel_cost(CThing *thing, double time, CShip *ship);

  // TODO: Currently returns dummy collision - should check path for obstacles
  Collision detect_collisions_on_path(CThing *thing, double time, CShip *ship);
};

/////////////////////////////////////

#endif  // _GROOGROO_
