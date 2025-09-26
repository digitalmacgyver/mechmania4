
#ifndef _COLLISION_H_
#define _COLLISION_H_

#include "Thing.h"
#include "Coord.h"

class Collision {
 public:
  // The first collision on the way
  CThing *collision_thing;
  double collision_when;
  CCoord collision_where;
};

#endif
