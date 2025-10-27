/* Groonew Eat Groonew
 * "Groonew don't eat Groonew; Groonew do."
 * MechMania IV: The Vinyl Frontier
 * Team 13: Zach, Arun, Matt 10/3/1998
 * based on Sample file by Misha Voloshin 9/26/98
 */

#include <algorithm>  // Required for std::max
#include <cmath>      // Required for std::pow, std::min
#include <map>
#include <set>
#include <vector>

#include "Asteroid.h"
#include "GameConstants.h"
#include "GetVinyl.h"
#include "Groonew.h"
#include "LaserUtils.h"
#include "ParserModern.h"
#include "Pathfinding.h"

// NOTE: OR-Tools includes are removed as we are replacing the solver.
// #include "ortools/linear_solver/linear_solver.h"

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
2025-10-12: Implemented optimal resource assignment using lightweight Brute Force Optimization (Removed LP Solver dependency). 
2025-10-13: Fixed numerous bugs in pathfinding (incorrect max_speed calculations, don't try drift intercepts that occur after another collision, etc.)
2025-10-13: Fleshed out opportunistic / emergency orders: taking potshots and enemy ships and stations, breaking apart asteroids we're about to collide with.
2025-10-13: Basic / barely functional endgame logic of switching goals from gathering vinyl to destorying enemy vinyl reserves and then ships; lowering shield and fuel retention standards as game conditions evolve.
2025-10-22: Implemented 1 turn lookahead in positions for potshot and collision lasers.
2025-10-23: Better Potshots: no targeting data if us or them will collide with something first (excepting enemy stations)
2025-10-23: Better emergency orders: Don't boost shields before midsize uranium collisions once the world is out of vinyl to preserve uranium in game world.
2025-10-24: Changed pathfinding to be more forgiving of imperfect intercepts and dynamically adjust this based on target distance.
2025-10-25: Always launch from bases with thrust 60 when doing normal navigation (free and performs better in competition).
2025-10-25: Multiply all thrust order by 5/3rds. This will put our location after 1 game engine PhysicsModel turn in the position we calculated in Pathfinding (but with a greater velocity change).
2025-10-25: Increased magic bag horizon to 25 turns for theoretical maximum distance we can travel in 24 turns.
2025-10-26: Major improvements to combat logic, and testing having some ships be combat focused.
TBD: Change magic bag population to gracefully handle floating point rounding errors when reasoning about how many "turns" we have left to get our orders in for intercept.

*/

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() { return new Groonew; }

// TODO: Remove this
static const bool DEBUG_MODE = false;

//////////////////////////////////////////
// Groonew class implementation

Groonew::Groonew() : calculator_ship(NULL), mb(NULL), ramming_speed(false) {
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
  SetName("VRogue Squadron");
  GetStation()->SetName("VTatooine");  // Base station name

  GetShip(0)->SetName("VGold Leader");
  GetShip(1)->SetName("VAluminum Falcon");
  GetShip(2)->SetName("VRed 5");
  GetShip(3)->SetName("VEcho 3");

  // Configure all ships with high cargo, low fuel strategy
  // Total: 60 tons (20 fuel + 40 cargo)
  // Strategy: Aggressive collection, rely on uranium asteroids for refueling
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 20.0);   // Only 20 tons fuel
    GetShip(i)->SetCapacity(S_CARGO, 40.0);  // Large 40 ton cargo hold
    GetShip(i)->SetBrain(new GetVinyl);      // Assign GetVinyl AI brain
  }

  // Red 5 goes after the enemy.
  GetShip(2)->SetCapacity(S_FUEL, 60.0);
  GetShip(2)->SetCapacity(S_CARGO, 0.0);

  // Initialize the calculator ship.
  // NOTE: We assume CShip has an accessible constructor (as suggested by
  // PathInfo.C).
  if (calculator_ship == NULL) {
    // Assuming CShip constructor takes initial coordinates if available in the
    // codebase.
    calculator_ship = new CShip(CCoord(0.0, 0.0));
  }

  // Configure the simulator ship to match the fleet configuration (20/40
  // split). This is crucial if capacities affect mass or fuel usage rules.
  calculator_ship->SetCapacity(S_FUEL, 20.0);
  calculator_ship->SetCapacity(S_CARGO, 40.0);

  // Initialize refueling state tracking (all ships start not refueling)
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    ships_refueling_[GetShip(i)] = false;
  }
}

void Groonew::Turn() {
  CShip* pSh;

  // PHASE 1: Calculate paths to all objects for all ships
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

    // GetVinyl::Decide() will now only handle tactical overrides (collisions,
    // shields)
    brain->Decide();
  }

  // DEBUG: ONLY TESTING ONE SHIP FOR NOW.
  if (DEBUG_MODE && strcmp(pSh->GetName(), "Gold Leader") != 0) {
    // All the other ships do nothing, we let gold leader's brain decide.
    pSh->ResetOrders();
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
  bool first_iteration = true;

  // For each of our ships, calculate paths to all objects
  for (unsigned int ship_i = 0; ship_i < GetShipCount(); ++ship_i) {
    CShip* ship = GetShip(ship_i);
    if (ship == NULL || !ship->IsAlive()) {
      continue;  // Skip dead ships
    }

    // Iterate through all objects in the world
    for (unsigned int idx = pmyWorld->UFirstIndex; 
      idx != BAD_INDEX; 
      idx = pmyWorld->GetNextIndex(idx)) {
  
      CThing* athing = pmyWorld->GetThing(idx);
  
      // Always check both null and alive
      if (!athing || !athing->IsAlive()) {
        continue;
      }

      if (athing->GetKind() == GENTHING) {
        continue;  // Skip generic things (laser beams, etc.)
      }

      // Track global resource availability
      if (first_iteration) {
        if (athing->GetKind() == ASTEROID) {
          if (((CAsteroid*)athing)->GetMaterial() == VINYL) {
            vinyl_left += athing->GetMass();  // Track total vinyl in world
          } else if (((CAsteroid*)athing)->GetMaterial() == URANIUM) {
            uranium_left += athing->GetMass();  // Track total uranium in world
          } else {
            printf("ERROR: Unknown asteroid material!\n");
          }
        }
      }

      unsigned int max_intercept_turns = 25;

      // Calculate optimal intercept time
      for (unsigned int turn_i = 1; turn_i <= max_intercept_turns; ++turn_i) {
        // Calculate required thrust/turn to reach target in turn_i seconds
        FuelTraj fueltraj = Pathfinding::DetermineOrders(ship, athing, turn_i,
                                                         this->calculator_ship);

        // DEBUG: Log when pathfinding fails for stations
        if (g_pParser && g_pParser->verbose && athing->GetKind() == STATION && !fueltraj.path_found) {
          printf("DEBUG MagicBag: Pathfinding failed for station %s at turn_i=%d (ship %s)\n",
                 athing->GetName(), turn_i, ship->GetName());
        }

        // TODO: Check for obstacles on path (currently returns dummy)
        Collision collision =
            Pathfinding::detect_collisions_on_path(ship, athing, turn_i);

        // If we found a valid trajectory, save it and move to next object
        if (fueltraj.path_found) {
          // Create PathInfo object on the stack
          PathInfo path;
          path.traveler = ship;
          path.dest = athing;          // Target object
          path.fueltraj = fueltraj;    // How to get there
          // Note: fueltraj.time_to_arrive is the time we expect _the ship_ to
          // arrive at the intercept point, however the target might not be there yet.
          path.time_to_intercept = turn_i; // Time to intercept the target on fueltraj.
          path.collision = collision;  // Obstacles (TODO: fix)

          // Add to this ship's list of possible targets (will be copied)
          mb->addEntry(ship_i, athing, path);

          // DEBUG: Log successful path addition for stations
          if (g_pParser && g_pParser->verbose && athing->GetKind() == STATION) {
            printf("DEBUG MagicBag: Added station %s to MagicBag for ship %s at turn_i=%d\n",
                   athing->GetName(), ship->GetName(), turn_i);
          }

          break;  // Found valid path, move to next object
        }
      }
    }
    first_iteration = false;
  }
}

// Helper function to apply orders and log the decision.
void Groonew::ApplyOrders(CShip* pShip, const PathInfo& best_e) {
  CWorld* pmyWorld = GetWorld();
  if (g_pParser && g_pParser->verbose) {
    CThing* target = best_e.dest;

    if (target->GetKind() == ASTEROID) {
      CAsteroid* ast = (CAsteroid*)target;

      // Use a distinct header for the assignment action
      printf("t=%.1f\t%s [assignment]:\n", pmyWorld->GetGameTime(),
             pShip->GetName());
      printf("\t→ Following %s asteroid %u (Utility: %.2f):\n",
             (ast->GetMaterial() == VINYL) ? "vinyl" : "uranium",
             target->GetWorldIndex(), best_e.utility);

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
      printf(
          "\t  Asteroid:\tpos(%.1f,%.1f)\tvel(%.1f,%.2f)\torient %.2f\tmass "
          "%.1f\n",
          ast_pos.fX, ast_pos.fY, ast_vel.rho, ast_vel.theta, ast_orient,
          target->GetMass());

      // Trajectory info
      printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\n",
             best_e.fueltraj.time_to_arrive,
             ((best_e.fueltraj).order_kind == O_THRUST) ? "thrust"
             : ((best_e.fueltraj).order_kind == O_TURN) ? "turn"
                                                        : "other/none",
             (best_e.fueltraj).order_mag);
    }
  }

  // Set the order on the ship
  pShip->SetOrder((best_e.fueltraj).order_kind, (best_e.fueltraj).order_mag);
}

ShipWants Groonew::DetermineShipWants(CShip* ship,
                                      double cur_fuel,
                                      double cur_cargo,
                                      double max_fuel,
                                      double max_cargo,
                                      bool uranium_available,
                                      bool vinyl_available) const {

  bool low_fuel = (cur_fuel <= groonew::constants::FUEL_RESERVE);
  // E.g. all we could hold is the smallest shattered asteroid from the initial ones.
  // TODO: The math here breaks if we change the world asteroid sizes.
  bool fuel_nearly_full = (cur_fuel >= (max_fuel - (40.0 / 9.0) - g_fp_error_epsilon));

  // E.g. if we don't have enough room for 1 more medium size asteroid.
  bool cargo_nearly_full =
      ((max_cargo - cur_cargo) < ((g_initial_vinyl_asteroid_mass / g_asteroid_split_child_count) + g_fp_error_epsilon));
  bool has_cargo = (cur_cargo > g_fp_error_epsilon);

  // TODO: In the case where: we have a little vinyl, there is vinyl
  // left in the world, we're low on fuel, and there's no fuel left in the
  // world - we try to continue harvesting vinyl. Perhaps we should head home
  // instead. (NOTE: In practical gameplay this never happens - vinyl is
  // collected before uranium typically.) Maybe we should offer priority here
  // to getting Uranium if we're low on fuel?
  if (cargo_nearly_full || (!vinyl_available && has_cargo)) {
    // Return to base if our cargo is nearly full, or if there's no vinyl
    // available and we've got any cargo.
    return HOME;
  }

  if (low_fuel && uranium_available) {
    return FUEL;
  }

  if (vinyl_available) {
    return POINTS;
  }

  if (uranium_available && !fuel_nearly_full) {
    // If there's still uranium but no vinyl, and we're low on fuel, stock up
    // for battle. This amount fills us up till all we can hold is the smallest
    // shattered asteroid from the initial ones.
    return FUEL;
  }

  // Those who can't create will destroy.
  return VIOLENCE;
}

void Groonew::HandleGoHome(CShip* ship, double cur_cargo) {
  if (ship == NULL) {
    return;
  }

  // We're heading home; clear any remembered resource target.
  last_turn_targets_.erase(ship);

  // Check if we already have a path to the base in MagicBag.
  unsigned int shipnum = ship->GetShipNumber();
  const PathInfo* path = mb->getEntry(shipnum, GetStation());
  if (path != NULL && path->fueltraj.path_found) {
    if (g_pParser && g_pParser->verbose) {
      printf("Returning to base on MB path: %s fuel: %.1f Pos(%.1f,%.1f) Vel(%.1f,%.2f) Orient(%.2f) (tti=%f)\n", ship->GetName(), ship->GetAmount(S_FUEL), ship->GetPos().fX, ship->GetPos().fY, ship->GetVelocity().rho, ship->GetVelocity().theta, ship->GetOrient(), path->time_to_intercept);
    }

    ApplyOrders(ship, *path);
    return;
  }

  // Otherwise look further out for a path home (This logic is okay
  // as the station is unique)
  // Start j at 1 turn out, as pathfinding often requires time > 0.
  for (unsigned int j = 1; j < 51; ++j) {
    FuelTraj ft =
        Pathfinding::DetermineOrders(ship, GetStation(), j, calculator_ship);
    if (ft.path_found) {
      if (ft.order_kind != O_SHIELD) {
        ship->SetOrder(ft.order_kind, ft.order_mag);
      }
      // DEBUG - fix this - this is a hack we're using right now when we want
      // to drift, we set the order to O_SHIELD with mag 0.
      if (g_pParser && g_pParser->verbose) {
        printf("\t→ Returning to base (cargo=%.1f) (tti=%d) (Order=%d %.1f)\n", cur_cargo, j, ft.order_kind, ft.order_mag);
      }
      // Either we set the order above, or we didn't need an order this turn
      // to achieve our goal.
      break;
    }
    if (g_pParser && g_pParser->verbose) {
      printf("\t→ No path found to base for tti=%d\n", j);
    }
    if (j > 25) {
      printf("ERROR: %s couldn't find path to base in under 25 turns fuel: %.1f Pos(%.1f,%.1f) Vel(%.1f,%.2f) Orient(%.2f) (tti=%d)\n", ship->GetName(), ship->GetAmount(S_FUEL), ship->GetPos().fX, ship->GetPos().fY, ship->GetVelocity().rho, ship->GetVelocity().theta, ship->GetOrient(), j);
    }
  }
}

void Groonew::EvaluateResourceUtilities(
    CShip* ship, ShipWants wants, unsigned int shipnum,
    std::vector<CShip*>* ships_seeking_resources,
    std::map<CShip*, unsigned int>* ship_ptr_to_shipnum) {
  if (ship == NULL || mb == NULL) {
    return;
  }

  ships_seeking_resources->push_back(ship);
  (*ship_ptr_to_shipnum)[ship] = shipnum;

  // Calculate utilities for all potential targets in the MagicBag.
  auto& ship_paths = mb->getShipPaths(shipnum);
  for (auto& pair : ship_paths) {
    PathInfo& path_info = pair.second;
    path_info.utility = 0.0;
    if (path_info.dest != NULL && path_info.dest->GetKind() == ASTEROID) {
      // We must ensure we only calculate utility for the correct type of asteroid.
      AsteroidKind material =
          static_cast<CAsteroid*>(path_info.dest)->GetMaterial();
      bool favor_previous_target = false;
      auto remembered = last_turn_targets_.find(ship);
      if (remembered != last_turn_targets_.end() &&
          remembered->second == path_info.dest) {
        favor_previous_target = true;
      }

      // Check if the asteroid material matches what the ship wants.
      if ((wants == POINTS && material == VINYL) ||
          (wants == FUEL && material == URANIUM)) {
        path_info.utility =
            CalculateUtility(ship, wants, path_info, favor_previous_target);
      }  // Not the desired material -> utility remains 0
    }
  }
}

namespace {
// Helper struct to manage the state of the brute-force search.
struct AssignmentResult {
  double max_utility = -1.0;
  // Stores the task index assigned to each agent index. -1 means unassigned.
  std::vector<int> best_assignments;
};

// Recursive function (Backtracking search) to find the maximum utility
// assignment. agent_idx: The current agent we are trying to assign. used_tasks:
// A boolean vector tracking which tasks are already assigned. current_utility:
// The utility accumulated so far in this path. utilities: The utility matrix.
// result: The structure storing the overall best result found so far.
// current_assignment: The assignments made so far in this path.
void FindMaxAssignment(int agent_idx,
                       std::vector<bool>& used_tasks,
                       double current_utility,
                       const std::vector<std::vector<double>>& utilities,
                       AssignmentResult& result,
                       std::vector<int>& current_assignment) {
  const int num_agents = utilities.size();
  const int num_tasks = used_tasks.size();

  // Base Case: All agents have been considered.
  if (agent_idx == num_agents) {
    if (current_utility > result.max_utility) {
      result.max_utility = current_utility;
      result.best_assignments = current_assignment;
    }
    return;
  }

  // Recursive Step: Try assigning this agent to any available task.
  bool assignment_attempted = false;
  for (int task_idx = 0; task_idx < num_tasks; ++task_idx) {
    if (!used_tasks[task_idx]) {
      // Optimization: Skip if utility is non-positive.
      if (utilities[agent_idx][task_idx] <= 0.0) {
        continue;
      }

      assignment_attempted = true;
      // 1. Choose
      used_tasks[task_idx] = true;
      current_assignment[agent_idx] = task_idx;
      double utility_gain = utilities[agent_idx][task_idx];

      // 2. Explore
      FindMaxAssignment(agent_idx + 1,
                        used_tasks,
                        current_utility + utility_gain,
                        utilities,
                        result,
                        current_assignment);

      // 3. Unchoose (Backtrack)
      current_assignment[agent_idx] = -1;  // Reset assignment
      used_tasks[task_idx] = false;
    }
  }

  if (!assignment_attempted) {
    // Handle the case where the agent is not assigned to any task
    // (e.g., fewer tasks than agents, or no positive utility tasks available).
    // If we didn't find a positive utility assignment to attempt for this agent,
    // we must still proceed to the next agent.
    FindMaxAssignment(agent_idx + 1,
                      used_tasks,
                      current_utility,
                      utilities,
                      result,
                      current_assignment);
  }
}
}  // namespace

// Solves the assignment problem for resource collection using a lightweight
// brute-force approach.
void Groonew::SolveResourceAssignment(
    const std::vector<CShip*>& agents,
    const std::map<CShip*, unsigned int>& ship_ptr_to_shipnum) {
  // 1. Identify Tasks (Asteroids) and build the utility matrix.

  // Find unique list of all asteroids considered by the agents with positive
  // utility.
  std::set<CThing*> tasks_set;
  for (CShip* pShip : agents) {
    unsigned int shipnum = ship_ptr_to_shipnum.at(pShip);
    const auto& ship_paths = mb->getShipPaths(shipnum);
    for (const auto& pair : ship_paths) {
      // Utilities were pre-calculated in AssignShipOrders Phase A.
      const PathInfo& path = pair.second;

      // Only include targets with positive utility.
      if (path.utility > 0.0) {
        tasks_set.insert(pair.first);
      }
    }
  }

  if (tasks_set.empty())
    return;  // No viable targets found.

  // Create index mappings for tasks.
  std::vector<CThing*> tasks(tasks_set.begin(), tasks_set.end());
  std::map<CThing*, int> task_to_idx;
  for (int j = 0; j < (int)tasks.size(); ++j) {
    task_to_idx[tasks[j]] = j;
  }

  const int num_agents = agents.size();
  const int num_tasks = tasks.size();

  // Build the utility matrix. Initialize with 0.0.
  std::vector<std::vector<double>> utilities(
      num_agents, std::vector<double>(num_tasks, 0.0));

  for (int i = 0; i < num_agents; ++i) {
    CShip* pShip = agents[i];
    unsigned int shipnum = ship_ptr_to_shipnum.at(pShip);
    const auto& ship_paths = mb->getShipPaths(shipnum);

    // Populate the row for this agent.
    for (const auto& pair : ship_paths) {
      CThing* target = pair.first;
      // Check if this target is one of the identified tasks (it should be if
      // utility > 0).
      if (task_to_idx.count(target)) {
        int j = task_to_idx.at(target);
        // Utility was already pre-calculated in AssignShipOrders.
        utilities[i][j] = pair.second.utility;
      }
    }
  }

  // 2. Solve the problem using recursive brute-force (Backtracking).
  AssignmentResult result;
  // Initialize state for the search.
  std::vector<bool> used_tasks(num_tasks, false);
  std::vector<int> current_assignment(num_agents,
                                      -1);  // -1 indicates unassigned.

  // Start the recursive search from the first agent (index 0).
  FindMaxAssignment(0, used_tasks, 0.0, utilities, result, current_assignment);

  // 3. Process results and assign orders.
  if (result.max_utility > 0.0 && !result.best_assignments.empty()) {
    if (g_pParser && g_pParser->verbose) {
      CWorld* pmyWorld = GetWorld();
      printf(
          "t=%.1f\t[Optimal Assignment (Brute Force)]: Total utility = %.2f\n",
          pmyWorld->GetGameTime(), result.max_utility);
    }

    for (int i = 0; i < num_agents; ++i) {
      int task_idx = result.best_assignments[i];

      if (task_idx != -1) {
        CShip* pShip = agents[i];
        CThing* target = tasks[task_idx];

        // Retrieve the pre-calculated path info from the MagicBag.
        unsigned int shipnum = ship_ptr_to_shipnum.at(pShip);
        // We use .at() for safe access as the entry must exist if the solver
        // chose it.
        const PathInfo& best_e = mb->getShipPaths(shipnum).at(target);

        // Apply the orders and log the decision.
        ApplyOrders(pShip, best_e);
        last_turn_targets_[pShip] = target;
      } else {
        CShip* pShip = agents[i];
        last_turn_targets_.erase(pShip);
      }
    }
  } else {
    // This should generally not happen if there were positive utilities, but
    // serves as a safety check.
    if (g_pParser && g_pParser->verbose) {
      CWorld* pmyWorld = GetWorld();
      printf(
          "t=%.1f\t[Optimal Assignment]: No positive utility assignments "
          "found.\n",
          pmyWorld->GetGameTime());
    }
  }
}

void Groonew::AssignShipOrders() {
  // Data structures to map ships to the assignment problem agents.
  std::vector<CShip*> ships_seeking_resources;
  // Map Ship Ptr to the internal ship number (index in GetShip()).
  std::map<CShip*, unsigned int> ship_ptr_to_shipnum;

  CWorld* world = GetWorld();

  // PHASE A: Determine wants, calculate utilities, and handle non-contentious
  // goals. Process each ship to determine its high-level goal.
  for (unsigned int shipnum = 0; shipnum < GetShipCount(); ++shipnum) {
    CShip* ship = GetShip(shipnum);
    if (ship == NULL || !ship->IsAlive()) {
      continue;  // Skip dead ships
    }

    // Verbose logging header
    if (g_pParser && g_pParser->verbose) {
      printf("t=%.1f\t%s [strategic planning]:\n", world->GetGameTime(),
             ship->GetName());
    }

    double cur_fuel = ship->GetAmount(S_FUEL);
    double cur_cargo = ship->GetAmount(S_CARGO);
    double max_fuel = ship->GetCapacity(S_FUEL);
    double max_cargo = ship->GetCapacity(S_CARGO);

    // Determine preferred asteroid type based on current state
    bool uranium_available = (uranium_left > (0.0 + g_fp_error_epsilon));
    bool vinyl_available = (vinyl_left > (0.0 + g_fp_error_epsilon));

    // Prioritize fuel if low and available; otherwise prefer vinyl if
    // available.
    ShipWants wants = DetermineShipWants(ship, cur_fuel, cur_cargo, max_fuel,
                                         max_cargo, uranium_available,
                                         vinyl_available);

    // DEBUG - Manual override here for testing purposes.
    // Red 5 is the ship that goes after the enemy.
    if (strcmp(ship->GetName(), "VRed 5") == 0) {
      // Thermostat-style fuel management to prevent ping-ponging
      bool& refueling = ships_refueling_[ship];

      if (!uranium_available) {
        // No uranium left in world - always VIOLENCE mode
        refueling = false;
        wants = VIOLENCE;
        if (g_pParser && g_pParser->verbose) {
          printf("\t→ [THERMOSTAT] No uranium available, staying in VIOLENCE mode (fuel=%.1f)\n", cur_fuel);
        }
      } else if (refueling) {
        // Currently refueling - check if we've reached upper threshold
        if (cur_fuel >= groonew::constants::VIOLENCE_REFUEL_HIGH_THRESHOLD) {
          // Exit refueling mode
          refueling = false;
          wants = VIOLENCE;
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ [THERMOSTAT] Exiting refueling mode (fuel=%.1f >= %.1f threshold)\n",
                   cur_fuel, groonew::constants::VIOLENCE_REFUEL_HIGH_THRESHOLD);
          }
        } else {
          // Continue refueling
          wants = FUEL;
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ [THERMOSTAT] Continuing refueling (fuel=%.1f < %.1f threshold)\n",
                   cur_fuel, groonew::constants::VIOLENCE_REFUEL_HIGH_THRESHOLD);
          }
        }
      } else {
        // Not currently refueling - check if we've hit lower threshold
        if (cur_fuel <= groonew::constants::VIOLENCE_REFUEL_LOW_THRESHOLD) {
          // Enter refueling mode
          refueling = true;
          wants = FUEL;
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ [THERMOSTAT] Entering refueling mode (fuel=%.1f <= %.1f threshold)\n",
                   cur_fuel, groonew::constants::VIOLENCE_REFUEL_LOW_THRESHOLD);
          }
        } else {
          // Stay in VIOLENCE mode
          wants = VIOLENCE;
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ [THERMOSTAT] Staying in VIOLENCE mode (fuel=%.1f > %.1f threshold)\n",
                   cur_fuel, groonew::constants::VIOLENCE_REFUEL_LOW_THRESHOLD);
          }
        }
      }
    }

    switch (wants) {
      case HOME:
        // Execute non-contentious goals or prepare for assignment problem.
        HandleGoHome(ship, cur_cargo);
        break;
      case POINTS:
      case FUEL:
        // Harvest resources case - this ship participates in the optimization
        // problem.
        EvaluateResourceUtilities(ship, wants, shipnum, &ships_seeking_resources,
                                   &ship_ptr_to_shipnum);
        break;
      case VIOLENCE: {
        // Those who can't create will destroy - delegate to TrenchRun module
        last_turn_targets_.erase(ship);
        TrenchRun::ViolenceResult result = TrenchRun::ExecuteViolence(
            ship, shipnum, cur_fuel, uranium_available, mb, uranium_left,
            vinyl_left, ramming_speed);

        if (result == TrenchRun::ViolenceResult::NEED_FUEL_FIRST) {
          // Ship needs fuel first - fall back to FUEL seeking
          EvaluateResourceUtilities(ship, FUEL, shipnum,
                                    &ships_seeking_resources,
                                    &ship_ptr_to_shipnum);
        }
        break;
      }
      case NOTHING:
      default:
        // If wants == NOTHING the ship currently does nothing strategic.
        break;
    }
  }

  // PHASE B: Solve the resource assignment problem.
  if (!ships_seeking_resources.empty()) {
    SolveResourceAssignment(ships_seeking_resources, ship_ptr_to_shipnum);
  }
}

double Groonew::CalculateUtility(CShip* pShip, ShipWants wants,
                                 const PathInfo& e,
                                 bool favor_previous_target) {
  double utility = 0.0;

  double cur_fuel = pShip->GetAmount(S_FUEL);
  double cur_cargo = pShip->GetAmount(S_CARGO);
  double max_fuel = pShip->GetCapacity(S_FUEL);
  double max_cargo = pShip->GetCapacity(S_CARGO);

  // Use a lexicographic-style scoring:
  //   (1) higher utility/sec (primary)
  //   (2) lower fuel spent
  //   (3) reuse prior target when tied
  //   (4) fewer issued orders
  // Implemented via large base multipliers.
  const double multiplier = 1000.0;
  const double multiplier2 = multiplier * multiplier;
  const double multiplier3 = multiplier2 * multiplier;
  const double multiplier4 = multiplier3 * multiplier;
  double prior_penalty = favor_previous_target ? 0.0 : 1.0;

  double fuel_spent = e.fueltraj.fuel_total;
  // When we'll intercept the target.
  double time_to_intercept = e.time_to_intercept;
  // When the ship arrives at the intercept point.
  double time_to_arrive = e.fueltraj.time_to_arrive;
  unsigned int num_orders = e.fueltraj.num_orders;

  if (wants == POINTS) {
    // TODO: This relies on our ships 40 ton cargo hold being big enough to hold
    // any vinyl asteroid, and assumes we'll jettison the difference before
    // trying to catch this.
    double vinyl_gained = std::min(e.dest->GetMass(), max_cargo - cur_cargo);

    // Prevent division by zero if time_to_intercept is somehow 0.
    if (time_to_intercept < g_fp_error_epsilon)
      return 0.0;

    double utility_per_second = vinyl_gained / time_to_intercept;

    // This should be positive due to min asteroid size of 3, however just in
    // case we wish to preserve utility=0.0 as a sentinel value meaning "issue
    // no orders."
    utility = utility_per_second * multiplier4 -
              fuel_spent * multiplier3 -
              prior_penalty * multiplier2 -
              num_orders * multiplier -
              time_to_arrive;
    if (utility < 0.0) {
      utility = 0.0;
    }
  } else if (wants == FUEL) {
    double fuel_spent = e.fueltraj.fuel_total;

    // TODO: Estimating fuel utility is more tricky than vinyl because there are
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
    double fuel_gained =
        std::min(uranium_size, max_fuel - cur_fuel - fuel_spent) - fuel_spent;

    // Prevent division by zero.
    if (time_to_intercept < g_fp_error_epsilon)
      return 0.0;

    double utility_per_second = fuel_gained / time_to_intercept;

    // TODO: This doesn't grant any positive utility to the way we'll buff up
    // our shields when eating fuel.

    // Only grant positive utility if we're actually gaining fuel.
    utility = utility_per_second * multiplier4 -
              fuel_spent * multiplier3 -
              prior_penalty * multiplier2 -
              num_orders * multiplier -
              time_to_arrive;
    if (utility < 0.0) {
      utility = 0.0;
    }
  }
  return utility;
}

//////////////////////////////////////////////
