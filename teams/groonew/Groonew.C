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

#include <set>

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
2025-10-10: Reorganized code: Pathfinding into its own module, most planning into Groonew, implemented basic target contention prevention.
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

  // PHASE 2: Centralized strategic planning - assign base orders to all ships
  AssignShipOrders();

  // PHASE 3: Tactical overrides - let each ship's Brain handle emergencies
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

    // GetVinyl::Decide() will now only handle tactical overrides (collisions, shields)
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

        // TODO: Check for obstacles on path (currently returns dummy)
        Collision collision = Pathfinding::detect_collisions_on_path(ship, athing, turn_i);
        
        // If we found a valid trajectory, save it and move to next object
        if (fueltraj.path_found) {
          // Create PathInfo object on the stack
          PathInfo path;
          path.traveler = ship;
          path.dest = athing;          // Target object
          path.fueltraj = fueltraj;     // How to get there
          path.collision = collision;   // Obstacles (TODO: fix)
          
          // Add to this ship's list of possible targets (will be copied)
          mb->addEntry(ship_i, athing, path);
          break;  // Found valid path, move to next object
        }
      }
    }
  }
}

void Groonew::AssignShipOrders() {
  std::set<CThing*> claimed_targets;

  // Process each ship and assign base orders for target interception
  for (unsigned int shipnum = 0; shipnum < GetShipCount(); ++shipnum) {
    CShip* pShip = GetShip(shipnum);
    if (pShip == NULL || !pShip->IsAlive()) {
      continue;  // Skip dead ships
    }

    // DEBUG: ONLY TESTING ONE SHIP FOR NOW.
    if (DEBUG_MODE && strcmp(pShip->GetName(), "Gold Leader") != 0) {
      // Skip other ships in debug mode
      continue;
    }

    CWorld* pmyWorld = GetWorld();

    // Verbose logging header
    if (g_pParser && g_pParser->verbose) {
      printf("t=%.1f\t%s [strategic planning]:\n", pmyWorld->GetGameTime(), pShip->GetName());
    }

    double cur_fuel = pShip->GetAmount(S_FUEL);
    double cur_cargo = pShip->GetAmount(S_CARGO);

    // Determine preferred asteroid type based on current state
    AsteroidKind prefered_asteroid = VINYL;
    bool uranium_available = (uranium_left > 0.0);
    bool vinyl_available = (vinyl_left > 0.0);
    if (!vinyl_available || (cur_fuel <= 5.0 && uranium_available)) {
      prefered_asteroid = URANIUM;
    }

    // TODO: In the future once we've gathered all the vinyl maybe we'll start
    // shooting things.
    bool commence_primary_ignition = false;

    ShipWants wants = NOTHING;
    if ((pShip->GetAmount(S_CARGO) >= (2*40/3 - g_fp_error_epsilon)) ||
        (!vinyl_available && pShip->GetAmount(S_CARGO) > 0.01)) {
      // Return to base if we've got at least 2 medium sized asteroids in cargo,
      // or if there's no vinyl available and we've got any cargo.
      wants = HOME;
    } else if (prefered_asteroid == VINYL) {
      wants = POINTS;
    } else if (prefered_asteroid == URANIUM) {
      wants = FUEL;
    } else if (commence_primary_ignition) {
      wants = VIOLENCE;
    }

    // Check if we should return to base
    if (wants == HOME) {
      if (g_pParser && g_pParser->verbose) {
        printf("\t→ Returning to base (cargo=%.1f)\n", cur_cargo);
      }
      for (unsigned int j = 0; j < 50; ++j) {
        FuelTraj ft = Pathfinding::DetermineOrders(pShip, GetStation(), j, calculator_ship);
        if (ft.path_found) {
          // DEBUG - fix this - this is a hack were using right now when we want
          // to drift, we set the order to O_SHIELD with mag 0.
          if (ft.order_kind != O_SHIELD) {
            pShip->SetOrder(ft.order_kind, ft.order_mag);
          }
          // Either we set the order above, or we didn't need an order this turn
          // to achieve our goal.
          break;
        }
      }
    } else if (wants == POINTS || wants == FUEL) {
      // Harvest resources case - select best target from MagicBag

      // Our best target.
      const PathInfo* best_e = NULL;

      // TODO: Targets can only be uniquely identified by their operator==/!=, so we
      // need to save the targets we're trying to deconflict, or dynamically pull
      // them out of PathInfo->dest.

      auto& ship_paths = mb->getShipPaths(shipnum);
      for (auto&& pair : ship_paths) {
        PathInfo& e = pair.second;  // pair.second is the PathInfo

        if (claimed_targets.find(e.dest) != claimed_targets.end()) {
          continue;
        }

        if (e.dest != NULL) {
          bool is_asteroid = (e.dest->GetKind() == ASTEROID);
          // Note: Can't call GetMaterial on non-asteroids, hence the short
          // circuit &&.
          bool is_prefered_asteroid = (is_asteroid && (static_cast<CAsteroid*>(e.dest)->GetMaterial() == prefered_asteroid));

          if (!is_asteroid || !is_prefered_asteroid) {
            continue;
          }

          e.utility = CalculateUtility(pShip, wants, e);

          if ((best_e == NULL) || (e.utility > best_e->utility)) {
            best_e = &e;
          }
        }
      }

      // If we found something with positive utility, set the order.
      if (best_e != NULL && best_e->utility > 0.0) {
        if (g_pParser && g_pParser->verbose) {
          CThing* target = best_e->dest;
          CAsteroid* ast = (CAsteroid*)target;
          printf("\t→ Following %s asteroid %u:\n",
                 (ast->GetMaterial() == VINYL) ? "vinyl" : "uranium",
                 target->GetWorldIndex());

          // Ship state
          CCoord ship_pos = pShip->GetPos();
          CTraj ship_vel = pShip->GetVelocity();
          double ship_orient = pShip->GetOrient();
          printf("\t  Ship:\tpos(%.1f,%.1f)\tvel(%.1f,%.2f)\torient %.2f\n",
                 ship_pos.fX, ship_pos.fY, ship_vel.rho, ship_vel.theta, ship_orient);

          // Asteroid state
          CCoord ast_pos = target->GetPos();
          CTraj ast_vel = target->GetVelocity();
          double ast_orient = target->GetOrient();
          printf("\t  Asteroid:\tpos(%.1f,%.1f)\tvel(%.1f,%.2f)\torient %.2f\tmass %.1f\n",
                 ast_pos.fX, ast_pos.fY, ast_vel.rho, ast_vel.theta, ast_orient, target->GetMass());

          // Trajectory info
          printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\n",
                 best_e->fueltraj.time_to_intercept,
                 ((best_e->fueltraj).order_kind == O_THRUST) ? "thrust" :
                 ((best_e->fueltraj).order_kind == O_TURN) ? "turn" : "other/none",
                 (best_e->fueltraj).order_mag);
        }

        claimed_targets.insert(best_e->dest);

        pShip->SetOrder((best_e->fueltraj).order_kind,
                        (best_e->fueltraj).order_mag);


      }
    }
  }
}

double Groonew::CalculateUtility(CShip* pShip, ShipWants wants, const PathInfo& e) {
  double utility = 0.0;

  double cur_fuel = pShip->GetAmount(S_FUEL);
  double cur_cargo = pShip->GetAmount(S_CARGO);
  double max_fuel = pShip->GetCapacity(S_FUEL);
  double max_cargo = pShip->GetCapacity(S_CARGO);

  // For POINTS and FUEL we want to tiebreak the material/time utility so that:
  // 1. All things being equal we prefer lower fuel consumption.
  // 2. All things being equal after that we prefer fewer orders
  //   (e.g. more certain plans).
  //
  // We approach this with the "big Multiplier" method where we multiply
  // each teir of the utility by a number which is larger than the sum
  // of all assigned utilities in the lower tier.
  // 
  // We have 4 ships, and our utilities naturally fall in these ranges:
  //   Materials: 40 units next turn = 40
  //   Fuel: 0 to 60
  //   Orders: 1 to 3 but in the future we might plan further, up to time.
  //
  // So we have 4 tiers, we cap each utility at 250 and multiply by 4 for the 
  // number of agents to get an Multiplier of 1000 per tier, so:
  // 
  // Materials *= 1000^2
  // Fuel *= 1000^1
  // Orders = base value
  // Total utilitiy = Materials - Fuel - Orders
  double multiplier = 1000.0;

  if (wants == POINTS) {
    // TODO: This relies on our ships 40 ton cargo hold being big enough to hold
    // any vinyl asteroid, and assumes we'll jettison the difference before
    // trying to catch this.
    double vinyl_gained = std::min(e.dest->GetMass(), max_cargo - cur_cargo);
    double fuel_spent = e.fueltraj.fuel_total;
    double time_to_intercept = e.fueltraj.time_to_intercept;
    unsigned int num_orders = e.fueltraj.num_orders;

    double utility_per_second = vinyl_gained / time_to_intercept;

    // This should be positive due to min asteroid size of 3, however just in
    // case we wish to preserve utility=0.0 as a sentinel value meaning "issue
    // no orders."
    utility = utility_per_second * std::pow(multiplier, 2) - fuel_spent * multiplier - num_orders;
    if (utility < 0.0) {
      utility = 0.0;
    }
  } else if (wants == FUEL) {
    double fuel_spent = e.fueltraj.fuel_total;

    // TODO: Estimating fuel utility is more tricky than vinylbecause there are
    // uranium asteroids we can't fit in our 20 ton hold (also because we spend
    // fuel to acquire fuel). Here we'll just assume if we hit a big one we'll
    // have access to 1/3rd of it's fragments.
    double uranium_size = e.dest->GetMass();
    if (uranium_size > max_fuel) {
       uranium_size /= 3.0;
    }
    // We acquire the lesser of the uranium size or how much room we'll have in
    // our tank when we get there.
    // We spend fuel_spent.
    // Our gain is acquired - spent.
    double fuel_gained = std::min(uranium_size, max_fuel - cur_fuel - fuel_spent) - fuel_spent;
    double time_to_intercept = e.fueltraj.time_to_intercept;
    unsigned int num_orders = e.fueltraj.num_orders;

    double utility_per_second = fuel_gained / time_to_intercept;

    // TODO: This doesn't grant any positive utility to the way we'll buff up
    // our shields when eating fuel.

    // Only grant positive utility if we're actually gaining fuel.
    utility = utility_per_second * std::pow(multiplier, 2) - fuel_spent * multiplier - num_orders;
    if (utility < 0.0) {
      utility = 0.0;
    }
  }
  return utility;
}

///////////////////////////////////////////////