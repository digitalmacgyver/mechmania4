/* Groonew Eat Groonew
 * "Groonew don't eat Groonew; Groonew do."
 * MechMania IV: The Vinyl Frontier
 * Team 13: Zach, Arun, Matt 10/3/1998
 * based on Sample file by Misha Voloshin 9/26/98
 */

#include "Asteroid.h"
#include "GetVinyl.h"
#include "Groonew.h"

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() { return new Groonew; }

//////////////////////////////////////////
// Groonew class implementation

Groonew::Groonew() {
  // Constructor - no initialization needed here
}

Groonew::~Groonew() {
  // Destructor - clean up ship AI brains to prevent memory leaks
  CShip* pSh;
  CBrain* pBr;

  for (UINT i = 0; i < GetShipCount(); i++) {
    pSh = GetShip(i);
    if (pSh == NULL) {
      continue;  // Ship is dead
    }

    pBr = pSh->GetBrain();
    if (pBr != NULL) {
      delete pBr;
    }
    // Clean up after ourselves
  }
}

void Groonew::Init() {
  // Initialize random number generator for any random decisions
  srand(time(NULL));

  // Set team identity
  SetTeamNumber(13);                  // Lucky 13!
  SetName("Groonew eat Groonew!");  // Team motto
  GetStation()->SetName("Tree!");     // Base station name

  // Biological/symbiotic naming theme for ships
  GetShip(0)->SetName("Larvae");    // Young organism
  GetShip(1)->SetName("Tree");      // Matches station name
  GetShip(2)->SetName("Host");      // Symbiotic relationship
  GetShip(3)->SetName("Symbiant");  // Symbiotic partner

  // Configure all ships with high cargo, low fuel strategy
  // Total: 60 tons (20 fuel + 40 cargo)
  // Strategy: Aggressive collection, rely on uranium asteroids for refueling
  for (UINT i = 0; i < GetShipCount(); i++) {
    GetShip(i)->SetCapacity(S_FUEL, 20.0);   // Only 20 tons fuel
    GetShip(i)->SetCapacity(S_CARGO, 40.0);  // Large 40 ton cargo hold
    GetShip(i)->SetBrain(new GetVinyl);      // Assign GetVinyl AI brain
  }
}

void Groonew::Turn() {
  CShip* pSh;

  // PHASE 1: Calculate optimal paths to all objects for all ships
  // Creates new MagicBag and fills it with precalculated trajectories
  PopulateMagicBag();  // Allocates new Groonew::mb

  // PHASE 2: Each ship's AI uses the MagicBag to make decisions
  for (UINT i = 0; i < GetShipCount(); i++) {
    pSh = GetShip(i);
    if (pSh == NULL) {
      continue;  // Skip dead ships
    }

    CBrain* brain = pSh->GetBrain();
    if (brain == NULL) {
      continue;
    }

    // GetVinyl::Decide() will access the MagicBag to choose targets
    brain->Decide();
  }

  // PHASE 3: Clean up - MagicBag is recreated fresh each turn
  delete mb;
}

void Groonew::PopulateMagicBag() {
  // Create MagicBag: 4 ships × 100 potential targets each
  mb = new MagicBag(4, 100);
  CWorld* worldp = GetWorld();

  // Reset global resource counters
  uranium_left = 0.0;
  vinyl_left = 0.0;

  // For each of our ships, calculate paths to all objects
  for (UINT ship_i = 0; ship_i < GetShipCount(); ship_i++) {
    CShip* ship = GetShip(ship_i);
    if (ship == NULL || !ship->IsAlive()) {
      continue;  // Skip dead ships
    }

    // Iterate through all objects in the world
    for (UINT thing_i = worldp->UFirstIndex; thing_i <= worldp->ULastIndex;
         thing_i = worldp->GetNextIndex(thing_i)) {
      CThing* athing = worldp->GetThing(thing_i);

      if (athing == NULL || !(athing->IsAlive())) {
        continue;  // Skip dead objects
      }

      if (athing->GetKind() == GENTHING) {
        continue;  // Skip generic things (laser beams, etc)
      }

      // Track global resource availability
      if (athing->GetKind() == ASTEROID) {
        if (((CAsteroid*)athing)->GetMaterial() == VINYL) {
          vinyl_left += athing->GetMass();  // Track total vinyl in world
        } else if (((CAsteroid*)athing)->GetMaterial() == URANIUM) {
          uranium_left += athing->GetMass();  // Track total uranium in world
        } else {
          printf("ERROR: Unknown asteroid material!\n");
        }
      }

      // Calculate optimal intercept time (1-28 turns into future)
      // We try each time and take the first valid solution
      for (UINT turn_i = 1; turn_i < 28; turn_i++) {
        // Calculate required thrust/turn to reach target in turn_i seconds
        FuelTraj fueltraj = determine_orders(athing, turn_i, ship);

        // TODO: Calculate actual fuel cost (currently returns 5.0)
        double fuel_cost = determine_probable_fuel_cost(athing, turn_i, ship);

        // TODO: Check for obstacles on path (currently returns dummy)
        Collision collision = detect_collisions_on_path(athing, turn_i, ship);

        // If we found a valid trajectory, save it and move to next object
        if (fueltraj.fuel_used >= 0.0) {
          Entry* entry = new Entry;
          entry->thing = athing;          // Target object
          entry->turns_total = turn_i;    // Time to reach
          entry->fueltraj = fueltraj;     // How to get there
          entry->total_fuel = fuel_cost;  // Fuel required (TODO: fix)
          entry->collision = collision;   // Obstacles (TODO: fix)
          entry->claimed_by_mech =
              0;  // TODO: Ship coordination not implemented

          // Add to this ship's list of possible targets
          mb->addEntry(ship_i, entry);
          break;  // Found valid path, move to next object
        }
      }
    }
  }
}

FuelTraj Groonew::determine_orders(CThing* thing, double time, CShip* ship) {
  FuelTraj fj;  // Will contain the optimal order and fuel cost

  // Calculate where target will be in 'time' seconds
  CCoord destination = thing->PredictPosition(time);

  // Get ship's current and next-turn positions
  CCoord us_now = ship->GetPos();
  CCoord us_later = ship->PredictPosition(1.0);  // Position after 1 sec drift

  // STRATEGY 1: Try immediate thrust
  // Calculate thrust vector needed if we thrust right now
  CTraj dist_vec_now = us_now.VectTo(destination);  // Vector to target
  CTraj final_vel_vec_now = dist_vec_now;
  final_vel_vec_now.rho /= time;  // Velocity needed to reach target in time
  CTraj vel_vec_now = ship->GetVelocity();                 // Current velocity
  CTraj thrust_vec_now = final_vel_vec_now - vel_vec_now;  // Required change

  // Adjust angle relative to ship's current orientation
  thrust_vec_now.theta = thrust_vec_now.theta - ship->GetOrient();

  fj.traj = dist_vec_now;  // Store the trajectory vector

  // Check if we're already facing the right direction and thrust is reasonable
  if ((fabs(thrust_vec_now.theta) < .1) && (thrust_vec_now.rho <= 30.0)) {
    // We're aligned - thrust immediately
    fj.order_kind = O_THRUST;
    fj.order_mag = thrust_vec_now.rho;
    fj.fuel_used = ship->SetOrder(O_THRUST, thrust_vec_now.rho);
  } else {
    // STRATEGY 2: Turn now, thrust next turn
    // Calculate what we'll need if we spend this turn rotating
    CTraj dist_vec_later = us_later.VectTo(destination);
    CTraj final_vel_vec_later = dist_vec_later;
    final_vel_vec_later.rho /= time;  // Required velocity from next position

    // Our current velocity won't change if we only rotate
    CTraj thrust_vec_later = final_vel_vec_later - vel_vec_now;
    thrust_vec_later.theta = thrust_vec_later.theta - ship->GetOrient();

    if (thrust_vec_later.rho > 30.0) {
      // Can't reach target - thrust required exceeds maximum
      fj.fuel_used = -1;  // Signal impossible trajectory
    } else {
      // Turn this frame to face target
      fj.order_kind = O_TURN;
      fj.order_mag = thrust_vec_later.theta;
      fj.fuel_used = ship->SetOrder(O_TURN, thrust_vec_later.theta);
    }
  }
  ship->ResetOrders();  // Clear orders (we're just calculating, not executing)
  return fj;
}

double Groonew::determine_probable_fuel_cost(CThing* thing, double time,
                                              CShip* ship) {
  // TODO: Should calculate actual fuel cost based on:
  // - Distance to target
  // - Ship mass (including cargo)
  // - Required velocity change
  // - Turn angle required
  // Currently returns hardcoded value
  return (double)5.0;
}

Collision Groonew::detect_collisions_on_path(CThing* thing, double time,
                                              CShip* ship) {
  Collision collision;

  // TODO: Should check path from ship to target for:
  // - Other ships (friendly and enemy)
  // - Asteroids that might block the path
  // - Stations
  // Should return first collision on path or null if clear
  // Currently returns dummy collision data
  collision.collision_thing = thing;         // Placeholder: target itself
  collision.collision_when = (double)0.0;    // Placeholder: no collision time
  collision.collision_where = CCoord(0, 0);  // Placeholder: origin

  return collision;
}

///////////////////////////////////////////////
