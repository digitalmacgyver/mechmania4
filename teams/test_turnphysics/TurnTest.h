/* TurnTest.h
 * Test team for turn physics verification
 * Issues various turn orders to test fuel consumption
 */

#ifndef _TURNTEST_H_
#define _TURNTEST_H_

#include "Team.h"

class TurnTest : public CTeam {
public:
  TurnTest();
  ~TurnTest();

  void Init();
  void Turn();
  void SelectShipNames();
  void SelectTeamName();

private:
  int turn_count;
};

#endif
