#ifndef _PATHINFO_H_
#define _PATHINFO_H_

#include "Collision.h"
#include "FuelTraj.h"
#include "Ship.h"
#include "Thing.h"
#include "Traj.h"


// A budnle of useful data which we'll store as entries in our MagicBag for
// navigation data.

class PathInfo {
 public:
  // The thing moving along this path.
  CThing* traveler;

  // The destination for this path.
  CThing* dest;

  // The first order along our path, and estimated values for the path.
  FuelTraj fueltraj;

  // The time we expect to intercept the target on fueltraj.
  // NOTE: FuelTraj's time_to_arrive records when the ship will arrive
  // at the intercept point - however we might have to wait for the
  // target to arrive.
  double time_to_intercept;

  // TODO: This is here for future development - use it or remove it. The
  // first collision on the way
  Collision collision;

  // The utility of this path as determined by the Groonew team.
  double utility;

  PathInfo();
  ~PathInfo();
};

#endif
