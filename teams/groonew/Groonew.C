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
#include "ParserModern.h"
#include "Pathfinding.h"

// External reference to global parser instance
extern CParser* g_pParser;

/*

Feature change log:

2025-09-26: Fixed off by one error in in calculating thrust vector for next turn thrusts.
2025-09-26: Improved collision detection (from the engine).
2025-09-26: Don't break the speed limit (Note: worsened performace due to engine behavior).
2025-09-28: Allow no-order turns when we're drifting into a target in MagicBag.
2025-09-29: Reduced magic bag horizon to 21 turns. (Shouldn't reduce game outcomes, we should be able to get anywhere in around 20 turns - there are some low velocity paths but we're not optimizing for that now).
2025-10-01: Pathing updates to consider overthrust aware thrust-turning to get on desired trajectory as an option.
2025-10-09: Refactored code and used modern containers in MagicBag.
TBD: Change magic bag population to gracefully handle floating point rounding errors when reasoning about how many "turns" we have left to get our orders in for intercept.

*/

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() { return new Groonew; }

// TODO: Remove this
static const bool DEBUG_MODE = false;

//////////////////////////////////////////
// Groonew class implementation

Groonew::Groonew() : calculator_ship(NULL), mb(NULL) {
  // Constructor - initialize member pointers to NULL
}

Groonew::~Groonew() {
  // Destructor - clean up ship AI brains to prevent memory leaks
  CShip* pSh;
  CBrain* pBr;
  
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
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
  
  // Clean up the calculator ship.
  if (calculator_ship != NULL) {
    delete calculator_ship;
    calculator_ship = NULL;
  }
  
  // Clean up the MagicBag
  if (mb != NULL) {
    delete mb;
    mb = NULL;
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
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 20.0);   // Only 20 tons fuel
    GetShip(i)->SetCapacity(S_CARGO, 40.0);  // Large 40 ton cargo hold
    GetShip(i)->SetBrain(new GetVinyl);      // Assign GetVinyl AI brain
  }
  
  // Initialize the calculator ship.
  // NOTE: We assume CShip has an accessible default constructor (as suggested by PathInfo.C). 
  if (calculator_ship == NULL) {
    calculator_ship = new CShip(CCoord(0.0, 0.0));
  }
  
  // Configure the simulator ship to match the fleet configuration (20/40 split).
  // This is crucial if capacities affect mass or fuel usage rules.
  calculator_ship->SetCapacity(S_FUEL, 20.0);
  calculator_ship->SetCapacity(S_CARGO, 40.0);
}

void Groonew::Turn() {
  CShip* pSh;
  
  // PHASE 1: Calculate paths to all objects for all ships
  // Creates new MagicBag and fills it with orders for
  // fast time to intercept considering a planning horizon of 1-3 turns.
  PopulateMagicBag();
  
  // PHASE 2: Each ship's AI uses the MagicBag to make decisions
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
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
}

void Groonew::PopulateMagicBag() {
  // Create new MagicBag (delete old one if exists)
  if (mb != NULL) {
    delete mb;
  }
  mb = new MagicBag();
  CWorld* worldp = GetWorld();
  
  // Reset global resource counters
  uranium_left = 0.0;
  vinyl_left = 0.0;
  
  // For each of our ships, calculate paths to all objects
  for (unsigned int ship_i = 0; ship_i < GetShipCount(); ++ship_i) {
    CShip* ship = GetShip(ship_i);
    if (ship == NULL || !ship->IsAlive()) {
      continue;  // Skip dead ships
    }
    
    // Iterate through all objects in the world
    for (unsigned int thing_i = worldp->UFirstIndex; thing_i <= worldp->ULastIndex;thing_i = worldp->GetNextIndex(thing_i)) {
      CThing* athing = worldp->GetThing(thing_i);
      
      if (athing == NULL || !(athing->IsAlive())) {
        continue;  // Skip dead objects
      }
      
      if (athing->GetKind() == GENTHING) {
        continue;  // Skip generic things (laser beams, etc.)
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
      
      // How far out in the future should we search for intercepts? Considering on
      // our toroidal world we're never more than 512 units away from anything, and
      // our max speed is 30, so we can get there in 18 turns. Add 2 turns for
      // planning and 1 for slop.
      unsigned int max_intercept_turns = 21;
      
      // TODO: Rewrite this to use global world time steps instead of assuming it's 1 sec per turn.
      //
      // Calculate optimal intercept time
      // We try each time and take the first valid solution
      
      for (unsigned int turn_i = 1; turn_i < max_intercept_turns; ++turn_i) {
        // Calculate required thrust/turn to reach target in turn_i seconds
        FuelTraj fueltraj = Pathfinding::DetermineOrders(ship, athing, turn_i, this->calculator_ship);
        
        // IMPORTANT CHANGE: We no longer need the side-effect/reset pattern.
        // determine_orders now uses the calculator and has no side effects on the real ship.
        
        // TODO: Calculate actual fuel cost (currently returns 5.0)
        double fuel_cost = Pathfinding::determine_probable_fuel_cost(ship, athing, turn_i);
        
        // TODO: Check for obstacles on path (currently returns dummy)
        Collision collision = Pathfinding::detect_collisions_on_path(ship, athing, turn_i);
        
        // If we found a valid trajectory, save it and move to next object
        if (fueltraj.path_found) {
          // Create PathInfo object on the stack
          PathInfo path;
          path.traveler = ship;
          path.dest = athing;          // Target object
          path.turns_total = turn_i;    // Time to reach
          path.fueltraj = fueltraj;     // How to get there
          path.total_fuel = fuel_cost;  // Fuel required (TODO: fix)
          path.collision = collision;   // Obstacles (TODO: fix)
          
          // Add to this ship's list of possible targets (will be copied)
          mb->addEntry(ship_i, athing, path);
          break;  // Found valid path, move to next object
        }
      }
    }
  }
}

///////////////////////////////////////////////