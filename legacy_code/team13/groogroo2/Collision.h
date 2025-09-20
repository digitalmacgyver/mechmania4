
#ifndef _COLLISION_H_
#define _COLLISION_H_

class Collision {
 public:
  // The first collision on the way
  CThing *collision_thing;
  double collision_when;
  CCoord collision_where;
};

#endif
