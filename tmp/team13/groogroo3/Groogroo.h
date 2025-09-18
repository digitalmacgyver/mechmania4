/* Groogroo.h
 * Header for the Chrome Funkadelic
 * Sample team
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

class Groogroo : public CTeam
{
 public:
  Groogroo();
  ~Groogroo();
  MagicBag * mb;
  double uranium_left;
  double vinyl_left;

  void Init();
  void Turn();
  void PopulateMagicBag();
  
  FuelTraj determine_orders(CThing *thing, double time, CShip *ship);
  double determine_probable_fuel_cost(CThing *thing, double time, CShip *ship);
  Collision detect_collisions_on_path(CThing *thing, double time, CShip *ship);
};

/////////////////////////////////////

#endif  // _GROOGROO_
