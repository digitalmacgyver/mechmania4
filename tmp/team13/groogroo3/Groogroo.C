/* Groogroo Eat Groogroo
 * "Groogroo don't eat Groogroo; Groogroo do."
 * MechMania IV: The Vinyl Frontier
 * Team 13: Zach, Arun, Matt 10/3/1998
 * based on Sample file by Misha Voloshin 9/26/98
 */

#include "Groogroo.h"
#include "GetVinyl.h"
#include "Asteroid.h"
#include "AttackBase.h"
#include "KillShip.h"
#include "Thing.h"
#include "Station.h"

// Tell the game to use our class
CTeam* CTeam::CreateTeam()
{
  return new Groogroo;
}

//////////////////////////////////////////
// GrooGroo class

Groogroo::Groogroo()
{ }

Groogroo::~Groogroo()
{ 
  CShip *pSh;
  CBrain *pBr;

  for (UINT i=0; i<GetShipCount(); i++) {
    pSh = GetShip(i);
    if (pSh==NULL) continue;  // Ship is dead

    pBr = pSh->GetBrain();
    if (pBr!=NULL) delete pBr;  
    // Clean up after ourselves
  }
}

void Groogroo::Init()
{
  srand(time(NULL));
  SetTeamNumber(14);
  SetName("Matthew eat Matthew!");
  GetStation()->SetName("Tatooine");

  GetShip(0)->SetName("Slave 1");
  GetShip(1)->SetName("Millenium Falcon");
  GetShip(2)->SetName("Red 5");
  GetShip(3)->SetName("Echo 3");

  for (UINT i=0; i<4; i++) {
    GetShip(i)->SetCapacity(S_FUEL,20.0);
    GetShip(i)->SetCapacity(S_CARGO,40.0); // Redundant, but be safe
    GetShip(i)->SetBrain(new GetVinyl);
  }/*
  for (UINT i=2; i<4; i++) {
    GetShip(i)->SetCapacity(S_FUEL,33.3);
    GetShip(i)->SetCapacity(S_CARGO,26.7); // Redundant, but be safe
    GetShip(i)->SetBrain(new GetVinyl);
  }*/
}

void Groogroo::Turn()
{

  CShip *pSh;
  PopulateMagicBag(); //new's Groogroo::mb
  CTeam *pmyTeam; //= pShip->GetTeam();
  CWorld *pmyWorld;// = pmyTeam->GetWorld();
  double cur_fuel; //=pShip->GetAmount(S_FUEL);
  pSh=GetShip(1);
  pmyTeam = pSh->GetTeam();
  pmyWorld = pmyTeam->GetWorld();
  CThing *enemy_base;

  for (UINT thing_i=pmyWorld->UFirstIndex; thing_i <= pmyWorld->ULastIndex; thing_i=pmyWorld->GetNextIndex(thing_i)) {
    CThing *athing=pmyWorld->GetThing(thing_i);
    if(athing == NULL || !(athing->IsAlive())) {
      continue;
    } 
    ThingKind kind = athing->GetKind();
    if (kind != STATION) {
      continue;
    } 
    if  (((CStation*)athing)->GetTeam()->GetTeamNumber() !=
    	 pmyTeam->GetTeamNumber()) {
      enemy_base = (CStation*)athing;
      break;
    } else {
      continue;
    }
  }
  double vinnie = ((CStation*)enemy_base)->GetVinylStore();

  for (UINT i=0; i<GetShipCount(); i++) {
    pSh=GetShip(i);
    if (pSh != NULL && !(pSh->IsAlive())) continue;
    pmyTeam = pSh->GetTeam();
    cur_fuel = pSh->GetAmount(S_FUEL);
    pmyWorld = pmyTeam->GetWorld();
    if (pSh==NULL) continue;
    if (((((Groogroo*)pmyTeam)->vinyl_left < 3.0) &&
	((pSh->GetAmount(S_FUEL) > 15.0) || (((Groogroo*)pmyTeam)->uranium_left < 3.0))) &&
	(pSh->GetAmount(S_CARGO) < 3.0)) {
      if (vinnie > 0.0) {
       	pSh->SetBrain(new AttackBase);
      } else {
	//pSh->SetBrain(new AttackBase);
	pSh->SetBrain(new KillShip);
      }
    } else {
      pSh->SetBrain(new GetVinyl);
    }
    CBrain *brain=pSh->GetBrain();
    if(brain == NULL) continue;
    brain->Decide();
  }
  delete mb; //clean it up!
}

void Groogroo::PopulateMagicBag() {
  vinyl_left = 0;
  uranium_left = 0;
  mb = new MagicBag (4,100);
  CWorld * worldp = GetWorld();
  // iterate over all the ships
  for (UINT ship_i=0; ship_i<GetShipCount(); ship_i++) {
    CShip *ship=GetShip(ship_i);
    if(ship == NULL || !ship->IsAlive()) {
      continue;
    }

    // iterate over all Things
    for (UINT thing_i=worldp->UFirstIndex; thing_i <= worldp->ULastIndex; thing_i=worldp->GetNextIndex(thing_i)) {
      CThing *athing=worldp->GetThing(thing_i);

      if(athing == NULL || !(athing->IsAlive())) {
	continue;
      } 

      if (athing->GetKind() == GENTHING) {
	continue;
      }

      if(athing->GetKind() == ASTEROID) {
	if(((CAsteroid*)athing)->GetMaterial() == VINYL) {
	  vinyl_left+=athing->GetMass();
	  double tmp2 = athing->GetMass();
	} else if (((CAsteroid*)athing)->GetMaterial() == URANIUM) {
	  uranium_left+=athing->GetMass();
	} else {
	  printf("Thats impossible!\n");
	}
      }
      

      //iterate from now until threshold time
      for (UINT turn_i=1; turn_i<35; turn_i++) { 
      	FuelTraj fueltraj=determine_orders(athing, 
      					   turn_i, 
					   ship); 
      	double fuel_cost=determine_probable_fuel_cost(athing, 
      						      turn_i, 
						      ship); 
      	Collision collision=detect_collisions_on_path(athing, 
      						      turn_i, 
      						      ship); 
      	if(fueltraj.fuel_used >= 0.0) {
	  Entry *entry=new Entry;
	  entry->thing=athing;
	  entry->turns_total=turn_i;
	  entry->fueltraj=fueltraj;
      	  entry->total_fuel=fuel_cost;
      	  entry->collision = collision;
	  entry->claimed_by_mech =0;
	  mb->addEntry(ship_i, entry);
	  break;
      	} 
      }
    }
  }
}

FuelTraj Groogroo::determine_orders(CThing *thing, double time, CShip *ship) {
  FuelTraj fj;
  CCoord destination = thing->PredictPosition(time);
  CCoord us_now = ship->GetPos();
  CCoord us_later = ship->PredictPosition(1.0);
  // Determine the direction and speed of thrust needed if we thrust now.
  // If we are going in that direction then do it.
  CTraj dist_vec_now = us_now.VectTo(destination);
  CTraj final_vel_vec_now = dist_vec_now;
  final_vel_vec_now.rho /= time;
  CTraj vel_vec_now = ship->GetVelocity();
  CTraj thrust_vec_now = final_vel_vec_now - vel_vec_now;
  //  double t_ang = thrust_vec_now.theta;
  //  t_ang = fabs(t_ang)/t_ang * fmod(t_ang, PI);
  //  thrust_vec_now.theta = t_ang;
  thrust_vec_now.theta = thrust_vec_now.theta - ship->GetOrient();

  fj.traj=dist_vec_now;

  if((fabs(thrust_vec_now.theta) < .1) && (thrust_vec_now.rho <= 30.0)) {
    //thrust
    fj.order_kind=O_THRUST;
    fj.order_mag=thrust_vec_now.rho;
    fj.fuel_used = ship->SetOrder(O_THRUST, thrust_vec_now.rho);
  } else {
    // Determine the direction and speed of thrust that will be needed
    // later if we drift and thrust now.
    CTraj dist_vec_later = us_later.VectTo(destination);
    CTraj final_vel_vec_later = dist_vec_later;
    final_vel_vec_later.rho /= time;
    // vel_vec_now will be the same later if we only rotate now
    CTraj thrust_vec_later = final_vel_vec_later - vel_vec_now;
    thrust_vec_later.theta = thrust_vec_later.theta - ship->GetOrient();
    if (thrust_vec_later.rho > 30.0) {
      fj.fuel_used = -1;  // IMPOSSIBLE
    } else {
      //rotate
      fj.order_kind=O_TURN;
      fj.order_mag=thrust_vec_later.theta;
      fj.fuel_used = ship->SetOrder(O_TURN, thrust_vec_later.theta);
    } 
  } 
  ship->ResetOrders();
  return fj;
}

double Groogroo::determine_probable_fuel_cost(CThing *thing, double time, CShip *ship) {
  return (double)5.0;
}

Collision Groogroo::detect_collisions_on_path(CThing *thing, double time, CShip *ship) {
  Collision collision;

  collision.collision_thing = thing;
  collision.collision_when = (double)0.0;
  collision.collision_where = CCoord(0,0);

  return collision;
}
  

///////////////////////////////////////////////
