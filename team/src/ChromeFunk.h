/* ChromeFunk.h
 * Header for the Chrome Funkadelic
 * Sample team
 */

#ifndef _CHROME_FUNKADELIC_
#define _CHROME_FUNKADELIC_

#include "Team.h"
#include "Brain.h"

//////////////////////////////////////
// Main class: Chrome Funkadelic team

// ChromeFunk: Example implementation of strategic context switching
// This team demonstrates the Brain system with basic tactical behaviors
class ChromeFunk : public CTeam
{
 public:
  ChromeFunk();
  ~ChromeFunk();

  // Strategic AI: Analyzes game state and assigns appropriate brains
  void Init();   // Initialize ships with default Gatherer brains
  void Turn();   // Execute tactical AI for each ship's current brain
};

/////////////////////////////////////
// Ship AI classes

//-----------------------------

// Voyager: Temporary tactical context for station departure
// Demonstrates dynamic context switching - replaces current brain temporarily
class Voyager : public CBrain
{
 public:
  CBrain *pLastBrain;  // Store previous brain for restoration

  Voyager(CBrain* pLB=NULL);
  ~Voyager();

  void Decide();  // Handle station departure, then restore previous brain
};

//-----------------------------

// Stalker: Tactical context for pursuing and intercepting targets
// Focused behavior for navigation and target tracking
class Stalker : public CBrain
{
 public:
  CThing *pTarget;  // Current target to pursue

  Stalker() { pTarget=NULL; }
  ~Stalker() { }

  void Decide();  // Navigate toward target using interception logic

  // Legacy collision detection preserving original ChromeFunk behavior
  // Uses the old engine's incorrect closest approach calculation that ChromeFunk's
  // AI logic was designed around. The rest of ChromeFunk's behavior depends on
  // this specific collision detection behavior.
  double LegacyDetectCollisionCourse(const CThing& OthThing) const;
};

//----------------------------

// Shooter: Tactical context for combat and laser engagement
// Inherits Stalker's navigation abilities and adds combat logic
class Shooter : public Stalker
{
 public:
  Shooter() { }
  ~Shooter() { }

  void Decide();  // Engage targets with lasers when in range
};

//-----------------------------

// Gatherer: Default tactical context for resource collection
// Combines navigation (Stalker), combat (Shooter), and resource management
// This is the primary brain for ChromeFunk's ships
class Gatherer : public Shooter
{
 public:
  Gatherer();
  ~Gatherer();

  void Decide();  // Main resource collection logic with context switching

  UINT SelectTarget();  // Choose best resource target
  void AvoidCollide();  // Collision avoidance logic
};

#endif  // _CHROME_FUNKADELIC_
