#include "GetVinyl.h"
#include "Team.h"
#include "FuelTraj.h"
#include "Thing.h"
#include "Groogroo.h"
#include "MagicBag.h"
#include "Ship.h"

GetVinyl::GetVinyl()
{ 
}

GetVinyl::~GetVinyl()
{ }


void GetVinyl::Decide()
{
  //can't fire and drive cause of alcohol breath

  CTeam *pmyTeam = pShip->GetTeam();
  CWorld *pmyWorld = pmyTeam->GetWorld();

  UINT shipnum=pShip->GetShipNumber();
  MagicBag *mbp=((Groogroo *)pmyTeam)->mb;

  Entry *e;
  Entry *best_e=NULL;

  if(pShip->GetAmount(S_CARGO) > 0.01) {

    FuelTraj ft=((Groogroo *)pmyTeam)->determine_orders(pmyTeam->GetStation(), 10, pShip);
    printf("Ima Goin Home: %d %g\n", ft.order_kind, ft.order_mag);
    if(ft.fuel_used >= 0.0) {
      pShip->SetOrder(ft.order_kind,ft.order_mag);
    } else {
      printf("Awe shucks\n");
    }
    
  } else {

    UINT i=0;
    printf("Entering Loop.\n");
    for(e=mbp->getEntry(shipnum, 0);e!=NULL;e=mbp->getEntry(shipnum, i),i++) {
      if(e->thing != NULL) {
	if((e->thing->GetKind()) != ASTEROID) {
	  continue;
	}
	if((best_e == NULL) || ((e->fueltraj).traj.rho < (best_e->fueltraj).traj.rho)) {
	  best_e=e;
	  printf("Accept.\n");
	}
      }
    }
    
    pShip->ResetOrders();
    if(best_e != NULL) {
      printf("Setting Order.\n");
      pShip->SetOrder((best_e->fueltraj).order_kind,(best_e->fueltraj).order_mag);
    }
  }
}
