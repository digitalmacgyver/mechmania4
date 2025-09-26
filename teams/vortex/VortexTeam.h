/* VortexTeam.h
 * Team Vortex: "Spiral to Victory"
 *
 * Strategy: Efficient resource collection with aggressive area control
 * - Balanced fuel/cargo configuration
 * - Zone-based collection strategy
 * - Smart collision avoidance
 * - Opportunistic combat
 */

#ifndef _VORTEX_TEAM_H_
#define _VORTEX_TEAM_H_

#include "Asteroid.h"
#include "Brain.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"

class VortexTeam : public CTeam {
 public:
  VortexTeam();
  ~VortexTeam();

  void Init();
  void Turn();

  // Track resource distribution for strategic decisions
  double total_vinyl;
  double total_uranium;
  int turn_count;
};

// Smart collector AI with zone control
class VortexCollector : public CBrain {
 public:
  CThing* current_target;
  int assigned_zone;  // 0-3 quadrants
  bool returning_home;
  double last_fuel_check;

  VortexCollector(int zone);
  ~VortexCollector();

  void Decide();

 private:
  void FindTarget();
  void NavigateToTarget();
  void HandleEmergency();
  void MaintainDefenses();
  bool ShouldReturnHome();
  bool IsInMyZone(CThing* thing);
  void AvoidCollisions();
  void ConsiderCombat();
};

#endif