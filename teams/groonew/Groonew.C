/* Groonew Eat Groonew
 * "Groonew don't eat Groonew; Groonew do."
 * MechMania IV: The Vinyl Frontier
 * Team 13: Zach, Arun, Matt 10/3/1998
 * based on Sample file by Misha Voloshin 9/26/98
 */

#include "Asteroid.h"
#include "GameConstants.h"
#include "GetVinyl.h"
#include "Groonew.h"

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() { return new Groonew; }

// TODO: Remove this
static const bool DEBUG_MODE = true;

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
  SetTeamNumber(14);
  SetName("Rogue Squadron");  
  GetStation()->SetName("Tatooine");     // Base station name

  GetShip(0)->SetName("Gold Leader");    
  GetShip(1)->SetName("Aluminum Falcon");      
  GetShip(2)->SetName("Red 5");
  GetShip(3)->SetName("Echo 3");

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

    // DEBUG: ONLY TESTING ONE SHIP FOR NOW.
    if (DEBUG_MODE && strcmp(pSh->GetName(), "Gold Leader") != 0) {
      // All the other ships do nothing, we let gold leader's brain decide.
      pSh->ResetOrders();
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
          entry->claimed_by_mech = 0;  // TODO: Ship coordination not implemented

          // Add to this ship's list of possible targets
          mb->addEntry(ship_i, entry);
          break;  // Found valid path, move to next object
        }
      }
    }
  }
}

//Helper functions.
namespace {
  // Returns true if the game engine will clamp us as a result sending a thrust
  // order with rho for this ship.
  bool is_speeding(CShip* ship, double rho, double maxspeed = -1.0) {
    // Emulate the behavior of a default parameter for this global that isn't
    // known till runtime.
    if (maxspeed < 0) {
      maxspeed = g_game_max_speed;
    }
    return ((ship->GetVelocity() + CTraj(rho, ship->GetOrient())).rho > maxspeed);
  }
}

// This will be the first thing we refine:
// Step 0: As I'm checking the stuff below, I want to validate with hello world style code checks, for instance that VectTo does what I expect.
// Step 0: CONSIDER USING A NEW SHIP, SETTING IT'S POSITION AND VELOCITY, AND USING DETECT COLLISION OR OTHER METHODS TO DO THE NON-POINT MASS ANALYSI
//         MAYBE WE CAN USE DETECTCOLISIONCOURSE INSTEAD OF THETA <= 0.1
// Step 1: Check and validate the logic below is correct and optimal for point masses and dt=1.
//         * Account for max velocity - we'll get clamped to 30 so don't chase stuff we can't catch - if our resulting vel > 30 to get there in time t fail or limit to 30.
//         * Double check engine behavior - it seems it allows us to acclerate to > 30, by going up to 30 in each dimension or soemthing - this will quickly get capped later but understand current behavior for that tick.
//         * After making some fixes, it seems we're chasing things - is there an off by 1 error where we're 1 second behind? Or a failure to recognize when we're on maxspeed?
//           IT'S PROBABLY A MAXSPEED ISSUE - NEED HELLO WORLD DIAGNOSIS OF TRYING TO CHASE SOMETHING JUST AHEAD OF US GOING AT MAXSPEED.
//           IT MAY ALSO BE A FACING ISSUE - WHEN WE ARE CLOSE WE MAY NOT BE COMPLETELY DEAD ON BUT WE MAY WASTE TURNS DOING TINY MICRO ADJUSTMENTS INSTEAD OF THRUSTING.
//           THE OLD LOGIC OF DIVIDING BY TIME INSTEAD OF (TIME-1) MAY HAVE MASKED THIS ISSUE BY OVERSHOOTING THE TARGET ON APPROACH.
// Step 2: Adapt the logic given that the underlying simulation uses dt=0.2.
// Step 3: Consider how to make our estimates fuel aware, and our order selections fuel aware.
// Step 3: Consider how to prevent two ships going after the same target, and making the one who can get there best (e.g. fastest and with least fuel) the one who gets it.
// Step 3: Consider how to make the logic toroid aware and relative velocity aware - e.g. ship at -200, asteroid at 200, ship going 30 left, asteroid going 30 right - if we do nothing we'll collide in ~624/60 = 11 turns. But if we chase it by turning to the right we'll never cath it.
// Step 3: Consider how to adapt the logic for the actual size of our objects.
// Step 4: Consider how to adapt logic to be collision aware.
FuelTraj Groonew::determine_orders(CThing* thing, double time, CShip* ship) {
  FuelTraj fj;  // Contains: fuel_used, order_kind, order_mag

  // Calculate where target will be in 'time' seconds
  // TODO: This doesn't account for collisions.
  CCoord destination = thing->PredictPosition(time);

  // Get ship's current and next-turn positions
  CCoord us_now = ship->GetPos();
  CCoord us_later = ship->PredictPosition(g_game_turn_duration);  // Position after 1 turn drift

  // STRATEGY 1: Try immediate thrust
  // Calculate thrust vector needed if we thrust right now
  CTraj dist_vec_now = us_now.VectTo(destination);  // Vector to target
  CTraj vel_vec_now = dist_vec_now;
  vel_vec_now.rho /= time;  // Velocity needed to reach target in time
  CTraj thrust_vec_now = vel_vec_now - ship->GetVelocity();  // Required change

  // Adjust angle relative to ship's current orientation
  thrust_vec_now.theta = thrust_vec_now.theta - ship->GetOrient();

  DEBUG - we need to adapt to new physics here - right now we refuse to thrust if
  we can't get there in time even if we're on the right heading. But it may be that 
  thursting on that heading this turn puts us:

  !!!NEXT STEP - LETS THINK ABOUT OVERTHRUSTS THAT CAN PUT US ON THE TRAJECTORY WE WANT,
  AND REASON ABOUT OUR TIME TO ARRIVE BY LOOKING AT OUR VELOCITY ON THAT TRAJ THIS TURN +
  30 ON FUTURE TURNS.!!! This is promising.


     a. In a position to get there on future turns.
     b. So here we're starting to think about optimal paths...
     c. Before going full optimal path, maybe there is a better heuristic we can do for a 2-4 turn planning horizon.
        Right now we consider 1-2 turns, either thrust now, or turn and then thrust.
        We should consider:
          Drift
          Thrust
          Drift, Drift
          Thrust, Drift
          Thrust, Thurst
          Turn, Thrust
          Drift, Drift, Drift
          Drift, Drift, Thrust
          Drift, Thrust, Drift
          Dirft, Thrust, Thrust
          Thrust, Drift, Drift
          Thrust, Drift, Thrust
          Thrust, Thrust, Drift
          Thrust, Thrust, Thrust
          Thrust, Turn, Thrust
          Turn, Drift, Thrust
          Turn, Thrust, Drift
          Turn, Thrust, Thrust

          My intuition tells me the other options are redundant other than facing (facing could set us up for optimal paths), but here Im trying to think of a quick fix that could dramatically improve groonew without going all the way to optimal paths.


  // Check if we're already facing the right direction and thrust is reasonable
  // TODO: We sometimes rotate around in a circle for nearby flybys, rather than
  // having a fixed arc of 0.1, we should try something more precise - perhaps
  // we can use the laser systems facing logic to see if we'll just get it of we
  // thrust (probablyu should limit that to 1 turn lookahead).
  if (fabs(thrust_vec_now.theta) < .1) {
    if (!is_speeding(ship, thrust_vec_now.rho)) { 
      // We're aligned - thrust immediately
      fj.order_kind = O_THRUST;
      fj.order_mag = thrust_vec_now.rho;
      fj.fuel_used = ship->SetOrder(O_THRUST, thrust_vec_now.rho);
    } else {
      // Can't reach target - thrust required exceeds maximum
      fj.fuel_used = -1;  // Signal impossible trajectory
    }
  } else {
    // STRATEGY 2: Turn now, thrust next turn
    // Calculate what we'll need if we spend this turn rotating
    if (time <= 1.0) {
      fj.fuel_used = -1;  // Signal impossible trajectory
    } else {
      CTraj dist_vec_later = us_later.VectTo(destination);
      CTraj vel_vec_later = dist_vec_later;
      vel_vec_later.rho /= (time - 1.0);  // Required velocity from next position

      // Our current velocity won't change if we only rotate
      CTraj thrust_vec_later = vel_vec_later - ship->GetVelocity();
      thrust_vec_later.theta = thrust_vec_later.theta - ship->GetOrient();

      if (is_speeding(ship, thrust_vec_later.rho)) {
        // Can't reach target - thrust required exceeds maximum
        fj.fuel_used = -1;  // Signal impossible trajectory
      } else {
        // Turn this frame to face target
        fj.order_kind = O_TURN;
        fj.order_mag = thrust_vec_later.theta;
        fj.fuel_used = ship->SetOrder(O_TURN, thrust_vec_later.theta);
      }
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
  // - Turn angle required Currently returns hardcoded value We should
  //   reconsider this - we can get the fuel costs for each order from SetOrder,
  //   however the issue here is ourl magic bag planning may use two orders, a
  //   rotation and a thrust. We should consider if the rotation cost is
  //   typically trivial to just use thrust, or if we should be building up a
  //   total fuel cost in the magic bag calculation where were working though
  //   that stuff anyway.
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
