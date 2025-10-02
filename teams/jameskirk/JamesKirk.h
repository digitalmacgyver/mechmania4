/* JamesKirk.h
 * Header for the James Kirk team
 * Demonstrates engine exploits with combat-focused AI
 * MechMania IV: The Vinyl Frontier
 *
 * PURPOSE: This team demonstrates exploits that existed in the original
 * MechMania IV engine. Currently implements the laser power exploit
 * which allows firing 2000-mile lasers while only paying for 512 miles.
 *
 * NOTE: May require --legacy flag for exploits to function properly.
 */

#ifndef _JAMES_KIRK_
#define _JAMES_KIRK_

#include "Brain.h"
#include "Team.h"

//////////////////////////////////////
// Main class: James Kirk team

// JamesKirk: Combat-focused team demonstrating the KobayashiMaru laser exploit
// Pure aggressor AI with friend-or-foe detection
class JamesKirk : public CTeam {
 public:
  JamesKirk();
  ~JamesKirk();

  void Init();  // Initialize ships as combat vessels
  void Turn();  // Execute combat AI with exploit
};

/////////////////////////////////////
// Ship AI classes

//-----------------------------

// Voyager: Temporary tactical context for station departure
// Handles undocking to get ships into combat
class Voyager : public CBrain {
 public:
  CBrain* pLastBrain;  // Store previous brain for restoration

  Voyager(CBrain* pLB = NULL);
  ~Voyager();

  void Decide();  // Handle station departure, then restore previous brain
};

//-----------------------------

// Stalker: Tactical context for pursuing and intercepting targets
// Focused behavior for navigation and target tracking
class Stalker : public CBrain {
 public:
  CThing* pTarget;  // Current target to pursue

  Stalker() { pTarget = NULL; }
  ~Stalker() {}

  void Decide();  // Pursue and intercept logic

  // Legacy collision detection for compatibility
  double LegacyDetectCollisionCourse(const CThing& OthThing) const;
};

//-----------------------------

// Shooter: Combat brain with laser exploit implementation
// Focused on attacking enemy ships and stations
class Shooter : public Stalker {
 public:
  Shooter() {}
  ~Shooter() {}

  void Decide();                // Combat and firing logic
  unsigned int SelectTarget();  // Target selection (enemies only)
};

#endif