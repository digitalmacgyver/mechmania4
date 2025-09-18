#include "GetVinyl.h"
#include "Team.h"
#include "FuelTraj.h"
#include "Thing.h"
#include "Groogroo.h"
#include "MagicBag.h"
#include "Ship.h"
#include "Asteroid.h"
#include "Station.h"

GetVinyl::GetVinyl()
{ 
}

GetVinyl::~GetVinyl()
{ }


void GetVinyl::Decide()
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
      if(((kind == STATION) && 
	  (((CStation*)athing)->GetTeam()->GetTeamNumber() !=
	   pmyTeam->GetTeamNumber())) && 
	 (pShip->GetAmount(S_CARGO) > 0.01)) {
	// if its enemy base and we have vinyl
	printf("Jabba will not take kindly to this!\n");
	pShip->SetJettison(VINYL, cur_cargo);
	lock_orders=1;
      } else if (kind == ASTEROID) {
	if((((CAsteroid*)athing)->GetMaterial() == URANIUM)) {
	  if(athing->GetMass() <= max_fuel) {
	    // uranium less than max fuel
	    pShip->SetOrder(O_SHIELD, athing->GetMass() - (max_fuel - cur_fuel));
	    lock_orders=1;
	  } else {
	    // uranium greater than max fuel
	    // WRITE this later! (shoot the asteroid)
	  }
	} else {
	  if((athing->GetMass() <= max_cargo)) {
	    if(athing->GetMass() >= (max_cargo-cur_cargo)) {
	      // fits in cargo hold but we're holding too much
	      // WRITE this later (maximum packing)
	    } else {
	      // it fits, just ram it, do nothing
	    }
	  } else {
	    // doesn't fit in cargo hold
	    // WRITE this later! (shoot the asteroid)
	  }
	}
      } else if(kind == SHIP) {
	if((((CShip*)athing)->GetTeam()->GetTeamNumber() !=
	    pmyTeam->GetTeamNumber())) {
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
      if(((kind == STATION) && 
	  (((CStation*)athing)->GetTeam()->GetTeamNumber() !=
	   pmyTeam->GetTeamNumber())) && 
	 (pShip->GetAmount(S_CARGO) > 0.01)) {
	// if its enemy base and we have vinyl
	// turn away for jettison
	printf("Turning away from their base!\n");
	pShip->SetOrder(O_TURN, PI); // should check where we are pointing
	lock_orders=1;
      } else if(kind == ASTEROID) {
	if((((CAsteroid*)athing)->GetMaterial() == URANIUM)) {
	  if(athing->GetMass() <= max_fuel) {
	    // uranium less than max fuel, do nothing
	  } else {
	    // uranium greater than max fuel
	    // SHOOT IT (later)
	  }
	} else {
	  if((athing->GetMass() <= max_cargo)) {
	    if(athing->GetMass() >= (max_cargo-cur_cargo)) {
	      // fits in cargo hold but we're holding too much, do nothing
	    } else {
	      // vinyl fits, just ram it, do nothing
	    }
	  } else {
	    // vinyl doesn't fit in cargo hold
	    // SHOOT IT (later)
	  }
	}
      } else if(kind == SHIP) {
	if((((CShip*)athing)->GetTeam()->GetTeamNumber() !=
	    pmyTeam->GetTeamNumber())) {
	  //enemy ship
	  // SHOOT IT (later)
	} else {
	  //our ship, do nothing
	}
      }
      break;
    }
  }

  // get vinyl or fuel
   if(!lock_orders) {
     AsteroidKind prefered_asteroid;
     if(!(((cur_fuel <= 5.0) && ((((Groogroo*)pmyTeam)->uranium_left > 0.0))) || ((((Groogroo*)pmyTeam)->vinyl_left < 0.01) && (((Groogroo*)pmyTeam)->uranium_left > 0.0)))) {
       prefered_asteroid = VINYL;
     } else {
       prefered_asteroid = URANIUM;
     }

    if((pShip->GetAmount(S_CARGO) > 13.01) || ((((Groogroo*)pmyTeam)->vinyl_left < 0.01) && (pShip->GetAmount(S_CARGO) > 0.01))) {
      
      //make the return to station better
      for(UINT j=0;j<50;j++) {
	FuelTraj ft=((Groogroo *)pmyTeam)->determine_orders(pmyTeam->GetStation(), j, pShip);
	if(ft.fuel_used >= 0.0) {
	  pShip->SetOrder(ft.order_kind,ft.order_mag);
	  break;
	}
      }
      
    } else {
      
      UINT i=0;
      for(e=mbp->getEntry(shipnum, 0);e!=NULL;e=mbp->getEntry(shipnum, i),i++) {
	if(e->thing != NULL) {
	  if((e->thing->GetKind()) != ASTEROID) {
	    continue;
	  } else if(((CAsteroid *)(e->thing))->GetMaterial() != prefered_asteroid) {
	    continue;
	  }
	  
	  //	  if(e->claimed_by_mech == 1) {
	  // continue;
	  //}

	  //	if((best_e == NULL) || ((e->fueltraj).traj.rho < (best_e->fueltraj).traj.rho)) {
	  if((best_e == NULL) || (e->turns_total < best_e->turns_total)) {
	    best_e=e;
	  }
	}
      }
      if(best_e != NULL) {
	pShip->SetOrder((best_e->fueltraj).order_kind,(best_e->fueltraj).order_mag);
	//best_e->claimed_by_mech=1;
       }
    }      
  }

  
double fuel_used = pShip->GetOrder(O_SHIELD) + pShip->GetOrder(O_LASER) + pShip->GetOrder(O_THRUST) + pShip->GetOrder(O_TURN) + pShip->GetOrder(O_JETTISON);
  cur_fuel-=fuel_used;  
  if( cur_shields < 11.0) {
    cur_fuel-=5.0; // save an emergency tank
    double wanted_shields=11.0-cur_shields;
    pShip->SetOrder(O_SHIELD, 
		    (wanted_shields < cur_fuel) ? wanted_shields : cur_fuel);
  }

}
