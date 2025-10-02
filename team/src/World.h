/* World.h
 * Declaration of CWorld
 * Essentially a list of Things
 *   with a few functions to detect
 *   collisions and handle object
 *   creation and deletion
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 *
 * COORDINATE SYSTEM:
 * World coordinates range from (-512,-512) to (512,512)
 * +Y direction points DOWNWARD on the display screen
 * Angles: 0 = right (+X), PI/2 = down (+Y), PI = left (-X), -PI/2 = up (-Y)
 */

#ifndef _WORLD_H_DSDFJSFLJKSEGFKLESF
#define _WORLD_H_DSDFJSFLJKSEGFKLESF

#include "Asteroid.h"
#include "Sendable.h"
#include "Thing.h"
#include "stdafx.h"
#include <vector>
#include <algorithm>
#include <random>

#define MAX_THINGS 512

const unsigned int BAD_INDEX = ((unsigned int)-1);

class CTeam;
class CShip;
class CAsteroid;

// Structure to store collision events for two-phase processing
struct CollisionEvent {
  CThing* pThing1;      // First thing in collision (usually asteroid/world object)
  CThing* pThing2;      // Second thing in collision (usually ship/station)
  double distance;      // Distance between centers (for prioritization)

  enum Type {
    STATION_DOCK = 0,     // Highest priority - docking provides immunity
    ASTEROID_CLAIM = 1,   // Resource collection
    LASER_DAMAGE = 2,     // Combat damage
    SHIP_COLLISION = 3    // Physical impacts (lowest priority)
  } type;

  // For sorting by priority then distance (deterministic mode)
  bool operator<(const CollisionEvent& other) const {
    if (type != other.type) return type < other.type;
    return distance < other.distance;  // Closest wins ties
  }
};

class CWorld : public CSendable {
 public:
  CWorld(unsigned int nTm);
  ~CWorld();
  CWorld* CreateCopy();

  CTeam* GetTeam(unsigned int nt) const;  // Returns ptr to team, NULL on error
  unsigned int GetNumTeams() const;       // Tells how many teams
  double GetGameTime() const;     // Tells elapsed game time

  // Announcer system
  void AddAnnouncerMessage(const char* message);

  unsigned int PhysicsModel(double dt = 1.0);  // Specify amt of time to pass
  void LaserModel();                   // Compute all laser firings
  void AddThingToWorld(CThing* pNewThing);

  void CreateAsteroids(AsteroidKind mat, unsigned int numast, double mass);
  CTeam* SetTeam(unsigned int n,
                 CTeam* pTm);  // Returns previous team ptr, NULL on fail

  CThing* GetThing(unsigned int index) const;      // returns NULL on failure
  unsigned int GetNextIndex(unsigned int curindex) const;  // returns (unsigned int)-1 if at end of list
  unsigned int GetPrevIndex(
      unsigned int curindex) const;  // returns (unsigned int)-1 if at beginning of list
  unsigned int UFirstIndex, ULastIndex;

  // Serialization routines
  unsigned GetSerialSize() const;
  unsigned SerialPack(char* buf, unsigned buflen) const;
  unsigned SerialUnpack(char* buf, unsigned buflen);
  CThing* CreateNewThing(ThingKind TKind, unsigned int iTm);

  // Legacy mode asteroid claim tracking (must be public for Ship access)
  void RecordLegacyAsteroidClaim(CAsteroid* pAst, CShip* pShip);

  // For internal use only
  double GetTimeStamp();  // Returns #sec, to 1msec res
  double* atstamp;
  double* auClock;

  bool bGameOver;

  // Announcer system - larger buffer to handle multiple events per turn
  static const int maxAnnouncerTextLen = 2048;
  char AnnouncerText[maxAnnouncerTextLen];

 protected:
  CThing* apThings[MAX_THINGS];
  unsigned int aUNextInd[MAX_THINGS];
  unsigned int aUPrevInd[MAX_THINGS];

  CThing* apTAddQueue[MAX_THINGS];
  unsigned int numNewThings;

  void RemoveIndex(unsigned int index);
  unsigned int AddNewThings();
  unsigned int KillDeadThings();
  unsigned int CollisionEvaluation();
  void ReLinkList();

  // Two-phase collision system methods
  unsigned int CollisionEvaluationFair();  // New fair collision system
  unsigned int CollisionEvaluationLegacy();  // Original collision system
  void DetectCollisions(std::vector<CollisionEvent>& events);
  CollisionEvent::Type DetermineCollisionType(CThing* pThing1, CThing* pThing2);
  void ProcessCollisionEvent(const CollisionEvent& event);
  bool ValidateCollision(const CollisionEvent& event);

  // Legacy mode bookkeeping for preserving double-claim bug
  struct LegacyAsteroidClaim {
    CAsteroid* pAsteroid;
    CShip* pShip;
    AsteroidKind material;
    double mass;
    bool fits;
  };
  std::vector<LegacyAsteroidClaim> legacyAsteroidClaims;
  void ProcessLegacyAsteroidClaims();

  double gametime;
  unsigned int numTeams;
  CTeam** apTeams;
};

#endif  // ! _WORLD_H_DSDFJSFLJKSEGFKLESF
