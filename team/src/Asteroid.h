/* Asteroid.h
 * Header for CAsteroid
 * Inherited off of CThing
 * For use with MechMania IV
 * Misha Voloshin and Jason Govig 5/26/98
 * cvs test
 */

#ifndef _ASTEROID_H_KEFLKJSEHFLKJWEHFKWEHFWEHFLJHEF
#define _ASTEROID_H_KEFLKJSEHFLKJWEHFKWEHFWEHFLJHEF

#include "Thing.h"

enum AsteroidKind { GENAST, VINYL, URANIUM };

class CAsteroid : public CThing {
 public:
  CAsteroid(double dm = 40.0, AsteroidKind mat = GENAST);
  virtual ~CAsteroid();

  AsteroidKind GetMaterial() const;
  CThing* EatenBy() const;  // Probably not needed in the API

  // Serialization routines
  unsigned GetSerialSize() const;
  unsigned SerialPack(char* buf, unsigned buflen) const;
  unsigned SerialUnpack(char* buf, unsigned buflen);

 protected:
  AsteroidKind material;
  CThing* pThEat;  // Ptr to ship which captures asteroid, initially NULL

  virtual CAsteroid* MakeChildAsteroid(double dm = 40.0);
  virtual void HandleCollision(CThing* pOthThing, CWorld* pWorld = NULL);

 private:
  // Collision processing implementations
  void HandleCollisionOld(CThing* pOthThing, CWorld* pWorld);
  void HandleCollisionNew(CThing* pOthThing, CWorld* pWorld);

  // Fragmentation implementations
  void CreateFragmentsOld(CThing* pOthThing, CWorld* pWorld, ThingKind OthKind);
  void CreateFragmentsNew(CThing* pOthThing, CWorld* pWorld, ThingKind OthKind);
};

#endif  // _ASTEROID_H_KEFLKJSEHFLKJWEHFKWEHFWEHFLJHEF
