/* LowThrust.h
 * Test team to demonstrate launch re-docking bug
 * Uses very low thrust (1.0 units/sec) to trigger re-docking behavior
 */

#ifndef _LOWTHRUST_H_
#define _LOWTHRUST_H_

#include "Brain.h"
#include "Ship.h"
#include "Team.h"

class LowThrust : public CTeam {
public:
  LowThrust();
  ~LowThrust();

  void Init();
  void Turn();
};

// Simple brain that just thrusts with low power
class LowThrustBrain : public CBrain {
public:
  void Decide();
};

#endif  // _LOWTHRUST_H_