#ifndef _ENTRY_H_
#define _ENTRY_H_

#include "Thing.h"
#include "Traj.h"
#include "Ship.h"
#include "Collision.h"
#include "FuelTraj.h"

class Entry {
 public:
  // The thing to which this entry refers
  CThing *thing;

  // The velocity trajectory we eventually want to be on and the fuel
  // of the current move.
  FuelTraj fueltraj;
  // The amount of fuel the trip to the thing is likely to take
  double total_fuel;

  // The number of turns the trip is likely to take
  double turns_total;

  // The first collision on the way
  Collision collision;

  // The ship number of the mech which is going after this object this
  // turn.
  int claimed_by_mech;
  
  Entry();
  ~Entry();
};

#endif
