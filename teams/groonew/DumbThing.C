#include "DumbThing.h"
#include "Team.h"

DumbThing::DumbThing() {}

DumbThing::~DumbThing() {}

void DumbThing::Decide() {
  static int flag = 0;
  double fuel;
  CTeam *pmyTeam = pShip->GetTeam();
  CWorld *pmyWorld = pmyTeam->GetWorld();
  if (flag >= 4) {
    fuel = pShip->SetOrder(O_THRUST, rand() / float(RAND_MAX) * 30.0);
  } else {
    fuel = pShip->SetOrder(O_TURN, rand() / float(RAND_MAX) * PI);
    flag++;
  }

  snprintf(pmyTeam->MsgText, maxTextLen, "Fuel Used: %g\n", fuel);
}
