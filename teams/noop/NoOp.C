/* NoOp.C
 * Implementation of the do-nothing team
 * Ships idle in space, no actions taken
 */

#include "NoOp.h"
#include "Ship.h"

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() {
  return new NoOp;
}

NoOp::NoOp() : CTeam() {
  // Constructor
}

NoOp::~NoOp() {
  // Nothing to clean up
}

void NoOp::Init() {
  // Initialize ships with default configuration
  // All ships get equal fuel/cargo split
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 30.0);   // 30 tons fuel
    GetShip(i)->SetCapacity(S_CARGO, 30.0);  // 30 tons cargo
  }
}

void NoOp::SelectTeamName() {
  SetName("Void Station");
  GetStation()->SetName("Void Station");
}

void NoOp::SelectShipNames() {
  GetShip(0)->SetName("Drift-1");
  GetShip(1)->SetName("Drift-2");
  GetShip(2)->SetName("Drift-3");
  GetShip(3)->SetName("Drift-4");
}

void NoOp::Turn() {
  // Do absolutely nothing
  // Ships will drift with whatever momentum they have
  // No commands issued to any ship
}