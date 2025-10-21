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

#include <random>

#include "Asteroid.h"
#include "MessageResult.h"
#include "Sendable.h"
#include "Thing.h"
#include "stdafx.h"

#define MAX_THINGS 512

const unsigned int BAD_INDEX = ((unsigned int)-1);

class CTeam;

class CWorld : public CSendable {
 public:
  CWorld(unsigned int nTm);
  ~CWorld();
  CWorld* CreateCopy();

  CTeam* GetTeam(unsigned int nt) const;  // Returns ptr to team, NULL on error
  unsigned int GetNumTeams() const;       // Tells how many teams
  double GetGameTime() const;     // Tells elapsed game time
  unsigned int GetCurrentTurn() const;    // Returns current turn number
  void IncrementTurn();                   // Increment turn counter

  // Announcer system
  void AddAnnouncerMessage(const char* message);  // Legacy interface (wraps AppendAnnouncerMessage)
  MessageResult SetAnnouncerMessage(const char* message);     // Replace entire announcer buffer
  MessageResult AppendAnnouncerMessage(const char* message);  // Append to announcer buffer
  void ClearAnnouncerMessage();                               // Clear announcer buffer

  unsigned int PhysicsModel(double dt = 1.0, double turn_phase = 0.0);  // Specify amt of time to pass
  void LaserModel();                   // Compute all laser firings (dispatcher)
  void LaserModelOld();                // Legacy laser processing (direct Collide() calls)
  void LaserModelNew();                // Deterministic laser processing (snapshot/command pipeline)
  void AddThingToWorld(CThing* pNewThing);
  void ResolvePendingOperations(bool resetTransientState = true);

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

  // For internal use only
  double GetTimeStamp();  // Returns #sec, to 1msec res
  double* atstamp;
  double* auClock;

  bool bGameOver;

  // Announcer system - larger buffer to handle multiple events per turn
  static const int maxAnnouncerTextLen = 2048;
  char AnnouncerText[maxAnnouncerTextLen];

  // Deterministic collision RNG controls
  void SeedCollisionRng(unsigned int seed) { collision_rng_.seed(seed); }

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
  unsigned int CollisionEvaluationOld();  // Legacy collision processing
  unsigned int CollisionEvaluationNew();  // Snapshot/command collision pipeline
  void ReLinkList();

  double gametime;
  unsigned int numTeams;
  CTeam** apTeams;
  unsigned int currentTurn;  // Track current turn number for logging
  std::mt19937 collision_rng_;
  std::uniform_real_distribution<double> ship_collision_angle_dist_;
};

#endif  // ! _WORLD_H_DSDFJSFLJKSEGFKLESF
