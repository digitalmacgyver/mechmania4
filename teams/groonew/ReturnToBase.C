#include "ReturnToBase.h"
#include "Team.h"

ReturnToBase::ReturnToBase() {}

ReturnToBase::~ReturnToBase() {}

void ReturnToBase::Decide() {
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

  sprintf(pmyTeam->MsgText, "Fuel Used: %g\n", fuel);
}
