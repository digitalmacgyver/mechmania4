#include "Team.h"
#include "FuelTraj.h"
#include "Thing.h"
#include "Groogroo.h"
#include "MagicBag.h"
#include "Ship.h"
#include "Asteroid.h"
#include "Station.h"
#include "AttackBase.h"
#include "KillShip.h"

KillShip::KillShip()
{ 
}

KillShip::~KillShip()
{ }

void KillShip::Decide()
{
  pShip->ResetOrders();

  //can't fire and drive cause of alcohol breath
  CTeam *pmyTeam = pShip->GetTeam();
  CWorld *pmyWorld = pmyTeam->GetWorld();

  UINT shipnum=pShip->GetShipNumber();
  MagicBag *mbp=((Groogroo *)pmyTeam)->mb;

  Entry *e;
  Entry *best_e=NULL;

  double cur_shields=pShip->GetAmount(S_SHIELD);
  double cur_fuel=pShip->GetAmount(S_FUEL);
  double cur_cargo=pShip->GetAmount(S_CARGO);
  double max_fuel=pShip->GetCapacity(S_FUEL);
  double max_cargo=pShip->GetCapacity(S_CARGO);
  
  BOOL lock_orders = 0;
  
  CThing *enemy_ship;
  for (UINT thing_i=pmyWorld->UFirstIndex; thing_i <= pmyWorld->ULastIndex; thing_i=pmyWorld->GetNextIndex(thing_i)) {
    CThing *athing=pmyWorld->GetThing(thing_i);
    if(athing == NULL || !(athing->IsAlive())) {
      continue;
    } 
    
    ThingKind kind = athing->GetKind();
    if (kind != SHIP) {
      continue;
    } 
    
    if((((CShip*)athing)->GetTeam()->GetTeamNumber() !=
	pmyTeam->GetTeamNumber()) && !(((CShip*)athing)->IsDocked())) {
      enemy_ship = (CShip*)athing;
      break;
    } else {
      continue;
    }
    
  }

  // Collision Handling.
  for (UINT thing_i=pmyWorld->UFirstIndex; thing_i <= pmyWorld->ULastIndex; thing_i=pmyWorld->GetNextIndex(thing_i)) {
    CThing *athing=pmyWorld->GetThing(thing_i);
    if(athing == NULL || !(athing->IsAlive())) {
      continue;
    } 
    
    ThingKind kind = athing->GetKind();
    if (kind == GENTHING) {
      continue;
    } 
    
    double turns = pShip->DetectCollisionCourse(*athing);
    if(turns < 0.0) {
      continue;
    }

    // Collision next turn
    if(turns < 2.0) {
      if((kind == STATION) && 
	  (((CStation*)athing)->GetTeam()->GetTeamNumber() !=
	   pmyTeam->GetTeamNumber())) {
	if (((CStation*)athing)->GetVinylStore() > 0.0) {
	  if (cur_fuel > 15.0) {
	    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	    pShip->SetOrder(O_LASER, 500);
	    lock_orders = 1;
	  } else if (cur_fuel > 5.0) {
	    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	    pShip->SetOrder(O_LASER, (cur_fuel - 5.0) * 50);
	    lock_orders = 1;
	  }
	}
      } else if (kind == ASTEROID) {
	if(athing->GetMass() <= max_fuel) {
	  // uranium less than max fuel
	  pShip->SetOrder(O_SHIELD, 0.4 + athing->GetMass() - (max_fuel - cur_fuel));
	  //	  lock_orders=1;
	} else {
	  pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	  pShip->SetOrder(O_LASER, 
			  (pShip->GetPos()).DistTo(athing->PredictPosition(1.0)) + 35);
	  lock_orders = 1;
							    
	  // uranium greater than max fuel
	  // WRITE this later! (shoot the asteroid)
	}
      } else if(kind == SHIP) {
	if((((CShip*)athing)->GetTeam()->GetTeamNumber() !=
	    pmyTeam->GetTeamNumber())) {
	  if (cur_fuel > 15.0) {
	    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	    pShip->SetOrder(O_LASER, 500);
	    lock_orders = 1;
	  } else if (cur_fuel > 5.0) {
	    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	    pShip->SetOrder(O_LASER, (cur_fuel - 5.0) * 50);
	    lock_orders = 1;
	  }
	  //enemy ship
	  // WRITE this later! (shoot it)
	} else {
	  //our ship
	  // back away from the keyboard
	  //	  pShip->SetOrder(O_THRUST, -1);
	  //      lock_orders=1;
	}
      }
      break;
    } else if(turns<3.0) {
      if((kind == STATION) && 
	 (((CStation*)athing)->GetTeam()->GetTeamNumber() !=
	  pmyTeam->GetTeamNumber())) { 
	//if (((CStation*)athing)->GetVinylStore() > 0.0) {
	//  if (cur_fuel > 15.0) {
	//    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	//    pShip->SetOrder(O_LASER, 500);
	//    lock_orders = 1;
	//  } else if (cur_fuel > 5.0) {
	//    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	//    pShip->SetOrder(O_LASER, (cur_fuel - 5.0) * 50);
	//    lock_orders = 1;
	//  }
	//}
      } else if(kind == ASTEROID) {
	if((((CAsteroid*)athing)->GetMaterial() == URANIUM)) {
	  if(athing->GetMass() <= max_fuel) {
	    // uranium less than max fuel, do nothing
	  } else {
	    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	    pShip->SetOrder(O_LASER, 
			    (pShip->GetPos()).DistTo(athing->PredictPosition(turns)) + 35);
	    lock_orders = 1;
	    // uranium greater than max fuel
	    // SHOOT IT (later)
	  }
	}
      } else if(kind == SHIP) {
	if((((CShip*)athing)->GetTeam()->GetTeamNumber() !=
	    pmyTeam->GetTeamNumber())) {
	  //enemy ship
	  // SHOOT IT (later)
	  if (cur_fuel > 15.0) {
	    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	    pShip->SetOrder(O_LASER, 500);
	    lock_orders = 1;
	  } else if (cur_fuel > 5.0) {
	    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*athing, 1.0));
	    pShip->SetOrder(O_LASER, (cur_fuel - 5.0) * 50);
	    lock_orders = 1;
	  }	
	} else {
	  //our ship, do nothing
	}
      }
      break;
    }
  }
  
  double e_dist = (pShip->GetPos()).DistTo(enemy_ship->GetPos());
  //  printf("Just Got Position\n");
  if (e_dist < 100) {
    pShip->SetOrder(O_TURN, pShip->AngleToIntercept(*enemy_ship, 1.0));
    pShip->SetOrder(O_LASER, 800);
    lock_orders = 1;
  }

  // get vinyl or fuel
   if(!lock_orders) {
     for (int j = 0 ; j < 50 ; j++) {
       FuelTraj ft=((Groogroo *)pmyTeam)->determine_orders(enemy_ship, j, pShip);
       if (ft.fuel_used > 0.0) {
	 pShip->SetOrder(ft.order_kind,ft.order_mag);
	 break;
       }
     }
   }

   double fuel_used = pShip->GetOrder(O_SHIELD) 
     + pShip->GetOrder(O_LASER) 
     + pShip->GetOrder(O_THRUST) 
     + pShip->GetOrder(O_TURN) 
     + pShip->GetOrder(O_JETTISON);
   cur_fuel-=fuel_used;  
   if( cur_shields < 11.0) {
     cur_fuel-=5.0; // save an emergency tank
     double wanted_shields=11.0-cur_shields;
     pShip->SetOrder(O_SHIELD, 
		     (wanted_shields < cur_fuel) ? wanted_shields : cur_fuel);
   }
   
}
