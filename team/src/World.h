/* World.h
 * Declaration of CWorld
 * Essentially a list of Things
 *   with a few functions to detect
 *   collisions and handle object
 *   creation and deletion
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#ifndef _WORLD_H_DSDFJSFLJKSEGFKLESF
#define _WORLD_H_DSDFJSFLJKSEGFKLESF

#include "stdafx.h"
#include "Thing.h"
#include "Asteroid.h"
#include "Sendable.h"

#define MAX_THINGS  512

const UINT BAD_INDEX = ((UINT)-1);

class CTeam;

class CWorld : public CSendable
{
 public:
  CWorld (UINT nTm);
  ~CWorld();
  CWorld* CreateCopy();

  CTeam* GetTeam(UINT nt) const;      // Returns ptr to team, NULL on error
  UINT GetNumTeams() const;    // Tells how many teams
  double GetGameTime() const;  // Tells elapsed game time

  UINT PhysicsModel (double dt=1.0);  // Specify amt of time to pass
  void LaserModel();        // Compute all laser firings
  void AddThingToWorld(CThing* pNewThing);

  void CreateAsteroids(AsteroidKind mat, UINT numast, double mass);
  CTeam* SetTeam(UINT n, CTeam* pTm);    // Returns previous team ptr, NULL on fail

  CThing* GetThing(UINT index) const ;     // returns NULL on failure
  UINT GetNextIndex(UINT curindex) const;   // returns (UINT)-1 if at end of list
  UINT GetPrevIndex(UINT curindex) const;   // returns (UINT)-1 if at beginning of list
  UINT UFirstIndex,ULastIndex;

  // Serialization routines
  unsigned GetSerialSize() const;
  unsigned SerialPack (char *buf, unsigned buflen) const;
  unsigned SerialUnpack (char *buf, unsigned buflen);
  CThing* CreateNewThing (ThingKind TKind, UINT iTm);

  // For internal use only
  double GetTimeStamp();   // Returns #sec, to 1msec res
  double *atstamp;
  double *auClock;

  bool bGameOver;

 protected:
  CThing* apThings[MAX_THINGS];
  UINT aUNextInd[MAX_THINGS];
  UINT aUPrevInd[MAX_THINGS];
  
  CThing* apTAddQueue[MAX_THINGS];
  UINT numNewThings;

  void RemoveIndex(UINT index);
  UINT AddNewThings();
  UINT KillDeadThings();
  UINT CollisionEvaluation();
  void ReLinkList();

  double gametime;
  UINT numTeams;
  CTeam **apTeams;
};

#endif // ! _WORLD_H_DSDFJSFLJKSEGFKLESF
