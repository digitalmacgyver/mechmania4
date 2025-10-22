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
  // ramming_speed defaults to true for endgame ramming tactics
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
  GetStation()->SetName("Tatooine");  // Base station name

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

    // DEBUG: ONLY TESTING ONE SHIP FOR NOW.
    if (DEBUG_MODE && strcmp(pSh->GetName(), "Gold Leader") != 0) {
      // All the other ships do nothing, we let gold leader's brain decide.
      pSh->ResetOrders();
      continue;
    }

    // GetVinyl::Decide() will now only handle tactical overrides (collisions,
    // shields)
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

      unsigned int max_intercept_turns = 21;

      // Calculate optimal intercept time
      for (unsigned int turn_i = 1; turn_i <= max_intercept_turns; ++turn_i) {
        // Calculate required thrust/turn to reach target in turn_i seconds
        FuelTraj fueltraj = Pathfinding::DetermineOrders(ship, athing, turn_i,
                                                         this->calculator_ship);

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
          path.collision = collision;  // Obstacles (TODO: fix)

          // Add to this ship's list of possible targets (will be copied)
          mb->addEntry(ship_i, athing, path);
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
    // We know it's an asteroid because the solver only considered valid
    // asteroids.
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
           best_e.fueltraj.time_to_intercept,
           ((best_e.fueltraj).order_kind == O_THRUST) ? "thrust"
           : ((best_e.fueltraj).order_kind == O_TURN) ? "turn"
                                                      : "other/none",
           (best_e.fueltraj).order_mag);
  }

  // Set the order on the ship
  pShip->SetOrder((best_e.fueltraj).order_kind, (best_e.fueltraj).order_mag);
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
void FindMaxAssignment(int agent_idx, std::vector<bool>& used_tasks,
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
      if (utilities[agent_idx][task_idx] <= 0.0)
        continue;

      assignment_attempted = true;

      // 1. Choose
      used_tasks[task_idx] = true;
      current_assignment[agent_idx] = task_idx;
      double utility_gain = utilities[agent_idx][task_idx];

      // 2. Explore
      FindMaxAssignment(agent_idx + 1, used_tasks,
                        current_utility + utility_gain, utilities, result,
                        current_assignment);

      // 3. Unchoose (Backtrack)
      current_assignment[agent_idx] = -1;  // Reset assignment
      used_tasks[task_idx] = false;
    }
  }

  // Handle the case where the agent is not assigned to any task
  // (e.g., fewer tasks than agents, or no positive utility tasks available).
  // If we didn't find a positive utility assignment to attempt for this agent,
  // we must still proceed to the next agent.
  if (!assignment_attempted) {
    FindMaxAssignment(agent_idx + 1, used_tasks, current_utility, utilities,
                      result, current_assignment);
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

  CWorld* pmyWorld = GetWorld();

  // PHASE A: Determine wants, calculate utilities, and handle non-contentious
  // goals. Process each ship to determine its high-level goal.
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

    // Verbose logging header
    if (g_pParser && g_pParser->verbose) {
      printf("t=%.1f\t%s [strategic planning]:\n", pmyWorld->GetGameTime(),
             pShip->GetName());
    }

    double cur_fuel = pShip->GetAmount(S_FUEL);
    double cur_cargo = pShip->GetAmount(S_CARGO);
    double max_fuel = pShip->GetCapacity(S_FUEL);
    double max_cargo = pShip->GetCapacity(S_CARGO);

    // Determine preferred asteroid type based on current state
    AsteroidKind prefered_asteroid = VINYL;
    bool uranium_available = (uranium_left > (0.0 + g_fp_error_epsilon));
    bool vinyl_available = (vinyl_left > (0.0 + g_fp_error_epsilon));

    // Prioritize fuel if low and available; otherwise prefer vinyl if
    // available.
    if (cur_fuel <= 5.0 && uranium_available) {
      prefered_asteroid = URANIUM;
    } else if (!vinyl_available && uranium_available) {
      prefered_asteroid = URANIUM;
    }

    // E.g. if we don't have enough room for 1 more medium size asteroid.
    bool cargo_nearly_full = ((max_cargo - cur_cargo) < ((40.0 / 3.0) + g_fp_error_epsilon));
    bool has_cargo = (cur_cargo > g_fp_error_epsilon);

    ShipWants wants = NOTHING;

    // TODO: As it is in the case where: we have a little vinyl, there is
    // vinyl left in the world, we're low on fuel, and there's no fuel
    // left in the world - we try to continue harvesting vinyl. Perhaps 
    // we should head home instead. (NOTE: In practical gameplay this never
    // happens - vinyl is collected before uranium typically.)
    if (cargo_nearly_full || (!vinyl_available && has_cargo)) {
      // Return to base if our cargo is nearly full, or if there's no
      // vinyl available and we've got any cargo.
      // TODO: Maybe we should offer priority here to getting Uranium
      // if we're low on fuel?
      wants = HOME;
    } else if (prefered_asteroid == VINYL && vinyl_available) {
      wants = POINTS;
    } else if (prefered_asteroid == URANIUM && uranium_available && vinyl_available) {
      // While there's still vinyl we just get vinyl and fuel if we need it.
      wants = FUEL;
    } else if (prefered_asteroid == URANIUM && uranium_available && cur_fuel <= 6.6) {
      // If there's still uranium but no vinyl, and we're low on fuel, stock up
      // for battle.
      wants = FUEL;
    } else {
      // Those who can't create will destroy.
      wants = VIOLENCE;
    }

    // Execute non-contentious goals or prepare for assignment problem.
    if (wants == HOME) {
      // We're heading home; clear any remembered resource target.
      last_turn_targets_.erase(pShip);

      // Find path home (This logic is okay as the station is unique)
      // Start j at 1 turn out, as pathfinding often requires time > 0.
      for (unsigned int j = 1; j < 50; ++j) {
        FuelTraj ft = Pathfinding::DetermineOrders(pShip, GetStation(), j,
                                                   calculator_ship);
        if (ft.path_found) {
          // DEBUG - fix this - this is a hack were using right now when we want
          // to drift, we set the order to O_SHIELD with mag 0.
          if (ft.order_kind != O_SHIELD) {
            pShip->SetOrder(ft.order_kind, ft.order_mag);
          }

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
      }
    } else if (wants == POINTS || wants == FUEL) {
      // Harvest resources case - this ship participates in the optimization
      // problem.
      ships_seeking_resources.push_back(pShip);
      ship_ptr_to_shipnum[pShip] = shipnum;

      // Calculate utilities for all potential targets in the MagicBag.
      auto& ship_paths = mb->getShipPaths(shipnum);
      for (auto&& pair : ship_paths) {
        PathInfo& e = pair.second;

        // We must ensure we only calculate utility for the correct type of
        // asteroid.
        if (e.dest != NULL && e.dest->GetKind() == ASTEROID) {
          // Check if the asteroid material matches what the ship wants.
          AsteroidKind material =
              static_cast<CAsteroid*>(e.dest)->GetMaterial();

          bool favor_previous_target = false;
          auto remembered = last_turn_targets_.find(pShip);
          if (remembered != last_turn_targets_.end() &&
              remembered->second == e.dest) {
            favor_previous_target = true;
          }

          if ((wants == POINTS && material == VINYL) ||
              (wants == FUEL && material == URANIUM)) {
            e.utility = CalculateUtility(pShip, wants, e, favor_previous_target);
          } else {
            e.utility = 0.0;  // Not the desired material
          }
        } else {
          e.utility = 0.0;  // Not an asteroid or destination is null
        }
      }
    } else if (wants == VIOLENCE) {
      last_turn_targets_.erase(pShip);
      // VIOLENCE mode: Converge on enemy ships/stations
      // We issue orders directly - no need for utility optimization since
      // combat targeting is generally robust against uncoordinated action.

      CTeam* pmyTeam = pShip->GetTeam();

      // Dynamic fuel management: If we're low on fuel and uranium is available,
      // temporarily switch to FUEL seeking to restock before continuing combat
      if (cur_fuel <= 5.5 && uranium_available) {
        if (g_pParser && g_pParser->verbose) {
          printf("\t→ [VIOLENCE override] Low fuel (%.1f), seeking uranium before combat\n", cur_fuel);
        }
        wants = FUEL;
        ships_seeking_resources.push_back(pShip);
        ship_ptr_to_shipnum[pShip] = shipnum;

        // Calculate utilities for uranium asteroids
        auto& ship_paths = mb->getShipPaths(shipnum);
        for (auto&& pair : ship_paths) {
          PathInfo& e = pair.second;
          if (e.dest != NULL && e.dest->GetKind() == ASTEROID) {
            AsteroidKind material = static_cast<CAsteroid*>(e.dest)->GetMaterial();
            bool favor_previous_target = false;
            auto remembered = last_turn_targets_.find(pShip);
            if (remembered != last_turn_targets_.end() &&
                remembered->second == e.dest) {
              favor_previous_target = true;
            }

            if (material == URANIUM) {
              e.utility = CalculateUtility(pShip, wants, e, favor_previous_target);
            } else {
              e.utility = 0.0;
            }
          } else {
            e.utility = 0.0;
          }
        }
        continue;  // Skip to next ship, let resource assignment handle this
      }

      // No fuel reserve if: (1) turn >= 280, OR (2) no uranium left in world
      const double emergency_fuel_reserve =
          (pmyWorld->GetGameTime() >= 280.0 || uranium_left <= g_fp_error_epsilon) ? 0.0 : 5.0;
      const double available_fuel = pShip->GetAmount(S_FUEL) - emergency_fuel_reserve;
      const double max_beam_length = min(512.0, available_fuel * g_laser_range_per_fuel_unit);

      // Structure to hold potential targets
      struct ViolenceTarget {
        CThing* thing;
        int priority_class;  // 1=station with vinyl, 2=ship with vinyl, 3=other ship
        double sort_key1;    // For stations: 0, For ships: cargo (desc) or shields (asc)
        double sort_key2;    // For ships with cargo: shields, For others: fuel
        double sort_key3;    // For ships with cargo: fuel, For others: 0
      };

      std::vector<ViolenceTarget> targets;

      // Check if enemy base has vinyl (for end-game determination)
      double enemy_base_vinyl = 0.0;
      for (unsigned int idx = pmyWorld->UFirstIndex; idx != BAD_INDEX;
           idx = pmyWorld->GetNextIndex(idx)) {
        CThing* thing = pmyWorld->GetThing(idx);
        if (thing == NULL || !thing->IsAlive()) continue;
        if (thing->GetKind() != STATION) continue;

        CTeam* thing_team = thing->GetTeam();
        if (thing_team == NULL) continue;
        if (thing_team->GetTeamNumber() == pmyTeam->GetTeamNumber()) continue;

        CStation* station = static_cast<CStation*>(thing);
        enemy_base_vinyl = station->GetVinylStore();
        break;  // Only one enemy station
      }

      // Scan world for enemy targets
      for (unsigned int idx = pmyWorld->UFirstIndex; idx != BAD_INDEX;
           idx = pmyWorld->GetNextIndex(idx)) {
        CThing* thing = pmyWorld->GetThing(idx);
        if (thing == NULL || !thing->IsAlive()) continue;

        ThingKind kind = thing->GetKind();
        if (kind != STATION && kind != SHIP) continue;

        CTeam* thing_team = thing->GetTeam();
        if (thing_team == NULL) continue;
        if (thing_team->GetTeamNumber() == pmyTeam->GetTeamNumber()) continue;

        ViolenceTarget target;
        target.thing = thing;

        if (kind == STATION) {
          CStation* station = static_cast<CStation*>(thing);
          double vinyl = station->GetVinylStore();
          if (vinyl > g_fp_error_epsilon) {
            target.priority_class = 1;
            target.sort_key1 = 0.0;
            target.sort_key2 = 0.0;
            target.sort_key3 = 0.0;
            targets.push_back(target);
          }
        } else if (kind == SHIP) {
          CShip* enemy = static_cast<CShip*>(thing);

          // Skip docked enemy ships - they're safe at their base
          if (enemy->IsDocked()) {
            continue;
          }

          double cargo = enemy->GetAmount(S_CARGO);
          double shields = enemy->GetAmount(S_SHIELD);
          double fuel = enemy->GetAmount(S_FUEL);

          if (cargo > g_fp_error_epsilon) {
            // Second priority: ships with vinyl
            // Sort by: most vinyl (desc), least shields (asc), least fuel (asc)
            target.priority_class = 2;
            target.sort_key1 = -cargo;  // Negate for descending
            target.sort_key2 = shields;  // Ascending
            target.sort_key3 = fuel;     // Ascending
            targets.push_back(target);
          } else {
            // Third priority: other enemy ships
            // Sort by: least shields (asc), least fuel (asc)
            target.priority_class = 3;
            target.sort_key1 = shields;
            target.sort_key2 = fuel;
            target.sort_key3 = 0.0;
            targets.push_back(target);
          }
        }
      }

      // Sort targets by priority
      std::sort(targets.begin(), targets.end(),
                [](const ViolenceTarget& a, const ViolenceTarget& b) {
        if (a.priority_class != b.priority_class) return a.priority_class < b.priority_class;
        if (fabs(a.sort_key1 - b.sort_key1) > g_fp_error_epsilon) return a.sort_key1 < b.sort_key1;
        if (fabs(a.sort_key2 - b.sort_key2) > g_fp_error_epsilon) return a.sort_key2 < b.sort_key2;
        return a.sort_key3 < b.sort_key3;
      });

      // Find best target we can path to
      CThing* best_target = NULL;
      PathInfo best_path;

      for (const auto& target : targets) {
        // Check if we have a path to this target in MagicBag
        auto& ship_paths = mb->getShipPaths(shipnum);
        auto it = ship_paths.find(target.thing);
        if (it != ship_paths.end()) {
          best_target = target.thing;
          best_path = it->second;
          break;
        }
      }

      // Issue navigation orders if we found a target
      if (best_target != NULL) {
        const char* target_type = (best_target->GetKind() == STATION) ? "enemy station" : "enemy ship";

        if (g_pParser && g_pParser->verbose) {
          printf("t=%.1f\t%s [VIOLENCE]:\n", pmyWorld->GetGameTime(), pShip->GetName());
          printf("\t→ Engaging %s '%s'\n", target_type, best_target->GetName());
        }

        // Phase-based station attack: navigate → exit dock → maintain firing position
        if (best_target->GetKind() == STATION) {
          CStation* enemy_station = static_cast<CStation*>(best_target);
          double distance = pShip->GetPos().DistTo(enemy_station->GetPos());

          // Determine current phase based on ship state
          bool docked_at_enemy = (pShip->IsDocked() && distance < g_ship_default_docking_distance + 5.0);

          if (docked_at_enemy) {
            // PHASE 2: Exit dock with staggered angles to avoid collisions
            // Assign each ship a unique exit angle based on shipnum
            // Angles: 3*PI/4 (135°), PI/4 (45°), -PI/4 (-45°), -3*PI/4 (-135°)
            const double exit_angles[4] = {3.0*PI/4.0, PI/4.0, -PI/4.0, -3.0*PI/4.0};
            double target_exit_angle = exit_angles[shipnum % 4];

            double current_orient = pShip->GetOrient();
            double angle_diff = target_exit_angle - current_orient;

            // Normalize angle difference to [-PI, PI]
            while (angle_diff > PI) angle_diff -= PI2;
            while (angle_diff < -PI) angle_diff += PI2;

            if (fabs(angle_diff) > 0.1) {
              // Not facing exit angle, turn to face it
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ PHASE 2a: Turning to exit angle %.2f (current=%.2f, diff=%.2f)\n",
                       target_exit_angle, current_orient, angle_diff);
              }
              pShip->SetOrder(O_TURN, angle_diff);
            } else {
              // Facing exit angle, thrust backwards to exit
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ PHASE 2b: Exiting dock at angle %.2f (dist=%.1f)\n",
                       target_exit_angle, distance);
              }
              pShip->SetOrder(O_THRUST, -1.0);
            }
          } else if (distance < 100.0) {
            // PHASE 3: Maintain firing position within 100 units
            bool facing = pShip->IsFacing(*enemy_station);
            CTraj velocity = pShip->GetVelocity();

            if (facing) {
              // Case 3a: Facing station - maintain position and prepare to shoot
              double angle_to_station = pShip->GetPos().AngleTo(enemy_station->GetPos());
              double radial_velocity = velocity.rho * cos(velocity.theta - angle_to_station);

              if (radial_velocity > 0.5) {
                // Moving away from station, counter with positive thrust
                if (g_pParser && g_pParser->verbose) {
                  printf("\t→ PHASE 3a: Countering drift away (radial_vel=%.2f, dist=%.1f)\n",
                         radial_velocity, distance);
                }
                pShip->SetOrder(O_THRUST, 2.0);
              } else {
                if (g_pParser && g_pParser->verbose) {
                  printf("\t→ PHASE 3a: Holding firing position (radial_vel=%.2f, dist=%.1f)\n",
                         radial_velocity, distance);
                }
                // No thrust order - let natural drift continue
              }
            } else if (velocity.rho < 1.0) {
              // Case 3b: Not facing but velocity low - turn to face
              double angle_to_station = pShip->GetPos().AngleTo(enemy_station->GetPos());
              double current_orient = pShip->GetOrient();
              double angle_diff = angle_to_station - current_orient;

              // Normalize angle difference to [-PI, PI]
              while (angle_diff > PI) angle_diff -= PI2;
              while (angle_diff < -PI) angle_diff += PI2;

              if (g_pParser && g_pParser->verbose) {
                printf("\t→ PHASE 3b: Turning to face station (angle_diff=%.2f, vel=%.2f, dist=%.1f)\n",
                       angle_diff, velocity.rho, distance);
              }
              pShip->SetOrder(O_TURN, angle_diff);
            } else {
              // Case 3c: Lost target lock (high velocity, not facing) - return to Phase 1
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ PHASE 3c: Lost lock (vel=%.2f, facing=%d, dist=%.1f) - returning to Phase 1\n",
                       velocity.rho, facing, distance);
                printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\n",
                       best_path.fueltraj.time_to_intercept,
                       (best_path.fueltraj.order_kind == O_THRUST) ? "thrust"
                       : (best_path.fueltraj.order_kind == O_TURN) ? "turn" : "other/none",
                       best_path.fueltraj.order_mag);
              }
              // Use MagicBag navigation to re-acquire target
              pShip->SetOrder(best_path.fueltraj.order_kind, best_path.fueltraj.order_mag);
            }
          } else {
            // PHASE 1: Navigate to station using MagicBag pathfinding
            if (g_pParser && g_pParser->verbose) {
              printf("\t→ PHASE 1: Navigating to enemy station (dist=%.1f)\n", distance);
              printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\n",
                     best_path.fueltraj.time_to_intercept,
                     (best_path.fueltraj.order_kind == O_THRUST) ? "thrust"
                     : (best_path.fueltraj.order_kind == O_TURN) ? "turn" : "other/none",
                     best_path.fueltraj.order_mag);
            }
            pShip->SetOrder(best_path.fueltraj.order_kind, best_path.fueltraj.order_mag);
          }

          // Shooting logic - attempt to fire if within range and facing
          if (distance < 100.0 && pShip->IsFacing(*enemy_station) && available_fuel > g_fp_error_epsilon) {
            double beam_length = min(512.0, available_fuel * g_laser_range_per_fuel_unit);
            bool good_efficiency = (beam_length >= 3.0 * distance);

            if (good_efficiency) {
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ Firing laser at enemy station (beam=%.1f, dist=%.1f)\n",
                       beam_length, distance);
              }
              pShip->SetOrder(O_LASER, beam_length);
            } else if (g_pParser && g_pParser->verbose) {
              printf("\t→ Holding fire (poor efficiency: beam=%.1f, dist=%.1f)\n",
                     beam_length, distance);
            }
          }
        } else {
          // Ship-to-ship combat: Choose between ramming or shooting

          // RAMMING SPEED MODE: When enabled and enemy base has no vinyl,
          // ram enemy ships instead of shooting them
          if (ramming_speed && enemy_base_vinyl <= g_fp_error_epsilon) {
            // === RAMMING SPEED TACTICS ===
            // Strategy: Use shields as a battering ram, maintain shields >= 13 until t >= 280

            if (g_pParser && g_pParser->verbose) {
              printf("\t→ [RAMMING SPEED] Engaging '%s' for collision attack\n",
                     best_target->GetName());
            }

            // Shield management: Keep shields high until very endgame
            double current_shields = pShip->GetAmount(S_SHIELD);
            double shield_target = (pmyWorld->GetGameTime() >= 280.0) ? 0.0 : 13.0;

            if (current_shields < shield_target && cur_fuel > 1.0) {
              // Recharge shields to maintain ramming protection
              double shield_boost = std::min(shield_target - current_shields, cur_fuel);
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ [RAMMING SPEED] Boosting shields %.1f->%.1f (target=%.1f)\n",
                       current_shields, current_shields + shield_boost, shield_target);
              }
              pShip->SetOrder(O_SHIELD, shield_boost);
            }

            // Navigation: Just head straight for the enemy ship using MagicBag pathfinding
            // No fancy shooting logic - we WANT to collide!
            if (g_pParser && g_pParser->verbose) {
              printf("\t→ [RAMMING SPEED] Ramming course to '%s' (dist=%.1f)\n",
                     best_target->GetName(), pShip->GetPos().DistTo(best_target->GetPos()));
              printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\tshields=%.1f\n",
                     best_path.fueltraj.time_to_intercept,
                     (best_path.fueltraj.order_kind == O_THRUST) ? "thrust"
                     : (best_path.fueltraj.order_kind == O_TURN) ? "turn" : "other/none",
                     best_path.fueltraj.order_mag, current_shields);
            }

            // Use MagicBag pathfinding to ram the enemy
            pShip->SetOrder(best_path.fueltraj.order_kind, best_path.fueltraj.order_mag);

          } else {
            // === STANDARD SHOOTING TACTICS ===
            // Phase-based ship-to-ship combat
            const double SHOOTING_DISTANCE = 160.0;

          // Scan for NON-DOCKED enemy ships within shooting distance
          // Re-evaluated every turn - no mode locking
          CShip* nearest_enemy = NULL;
          double nearest_distance = SHOOTING_DISTANCE + 1.0;

          for (unsigned int idx = pmyWorld->UFirstIndex; idx != BAD_INDEX;
               idx = pmyWorld->GetNextIndex(idx)) {
            CThing* thing = pmyWorld->GetThing(idx);
            if (thing == NULL || !thing->IsAlive() || thing->GetKind() != SHIP) continue;

            CTeam* thing_team = thing->GetTeam();
            if (thing_team == NULL || thing_team->GetTeamNumber() == pmyTeam->GetTeamNumber()) continue;

            CShip* enemy_ship = static_cast<CShip*>(thing);
            if (enemy_ship->IsDocked()) continue;  // CRITICAL: Skip docked ships - they don't trigger combat mode

            double distance = pShip->GetPos().DistTo(enemy_ship->GetPos());
            if (distance < nearest_distance) {
              nearest_enemy = enemy_ship;
              nearest_distance = distance;
            }
          }

          if (nearest_enemy != NULL && nearest_distance <= SHOOTING_DISTANCE) {
            // PHASE 2: Within shooting distance of a non-docked enemy
            bool facing = pShip->IsFacing(*nearest_enemy);

            if (facing) {
              // Case 2a: Facing the enemy - shoot them
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ PHASE 2a: Engaging enemy ship '%s' at close range (dist=%.1f)\n",
                       nearest_enemy->GetName(), nearest_distance);
              }

              // Shoot if we have fuel and they're in range
              if (available_fuel > g_fp_error_epsilon && nearest_distance < max_beam_length) {
                double beam_length = min(512.0, available_fuel * g_laser_range_per_fuel_unit);

                // End-game condition: no resources left and enemy base empty
                bool end_game = (uranium_left <= g_fp_error_epsilon &&
                                vinyl_left <= g_fp_error_epsilon &&
                                enemy_base_vinyl <= g_fp_error_epsilon);

                bool good_efficiency = (beam_length >= 3.0 * nearest_distance);

                if (end_game || good_efficiency) {
                  if (g_pParser && g_pParser->verbose) {
                    if (end_game) {
                      printf("\t→ Firing laser FULL BLAST [END-GAME] (beam=%.1f, dist=%.1f)\n",
                             beam_length, nearest_distance);
                    } else {
                      printf("\t→ Firing laser (beam=%.1f, dist=%.1f)\n",
                             beam_length, nearest_distance);
                    }
                  }
                  pShip->SetOrder(O_LASER, beam_length);
                }
              }
              // No thrust order - maintain current trajectory while engaging
            } else {
              // Case 2b: Not facing - evaluate if we should turn or pursue
              CTraj our_vel = pShip->GetVelocity();
              CTraj enemy_vel = nearest_enemy->GetVelocity();

              // Predict enemy position one turn from now
              CCoord enemy_pos_t0 = nearest_enemy->GetPos();
              CCoord enemy_pos_t1 = enemy_pos_t0 + CCoord(enemy_vel.rho * cos(enemy_vel.theta),
                                                           enemy_vel.rho * sin(enemy_vel.theta));

              // Predict our position one turn from now (considering our current velocity)
              CCoord our_pos_t0 = pShip->GetPos();
              CCoord our_pos_t1 = our_pos_t0 + CCoord(our_vel.rho * cos(our_vel.theta),
                                                       our_vel.rho * sin(our_vel.theta));

              // Check if we'll still be within shooting distance next turn
              double predicted_distance_t1 = our_pos_t1.DistTo(enemy_pos_t1);

              if (predicted_distance_t1 <= SHOOTING_DISTANCE) {
                // Case 2b1: Still in range next turn - turn to face predicted position
                double angle_to_target_t1 = our_pos_t1.AngleTo(enemy_pos_t1);
                double current_orient = pShip->GetOrient();
                double angle_diff = angle_to_target_t1 - current_orient;

                // Normalize angle difference to [-PI, PI]
                while (angle_diff > PI) angle_diff -= PI2;
                while (angle_diff < -PI) angle_diff += PI2;

                if (g_pParser && g_pParser->verbose) {
                  printf("\t→ PHASE 2b1: Turning to face enemy ship '%s' (angle_diff=%.2f)\n",
                         nearest_enemy->GetName(), angle_diff);
                  printf("\t  Current dist=%.1f, Predicted dist=%.1f\n",
                         nearest_distance, predicted_distance_t1);
                  printf("\t  Enemy: (%.1f,%.1f) -> (%.1f,%.1f)\n",
                         enemy_pos_t0.fX, enemy_pos_t0.fY, enemy_pos_t1.fX, enemy_pos_t1.fY);
                  printf("\t  Us: (%.1f,%.1f) -> (%.1f,%.1f)\n",
                         our_pos_t0.fX, our_pos_t0.fY, our_pos_t1.fX, our_pos_t1.fY);
                }

                pShip->SetOrder(O_TURN, angle_diff);
              } else {
                // Case 2b2: Will be out of range - resume Phase 1 pursuit
                if (g_pParser && g_pParser->verbose) {
                  printf("\t→ PHASE 2b2: Enemy moving out of range, resuming pursuit\n");
                  printf("\t  Current dist=%.1f, Predicted dist=%.1f (> %.1f)\n",
                         nearest_distance, predicted_distance_t1, SHOOTING_DISTANCE);
                  printf("\t  Switching to MagicBag pursuit of '%s'\n", best_target->GetName());
                  printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\n",
                         best_path.fueltraj.time_to_intercept,
                         (best_path.fueltraj.order_kind == O_THRUST) ? "thrust"
                         : (best_path.fueltraj.order_kind == O_TURN) ? "turn" : "other/none",
                         best_path.fueltraj.order_mag);
                }

                // Use MagicBag navigation to pursue
                pShip->SetOrder(best_path.fueltraj.order_kind, best_path.fueltraj.order_mag);

                // Opportunistic shooting during pursuit
                if (pShip->IsFacing(*best_target) && available_fuel > g_fp_error_epsilon) {
                  double distance = pShip->GetPos().DistTo(best_target->GetPos());
                  if (distance < max_beam_length) {
                    double beam_length = min(512.0, available_fuel * g_laser_range_per_fuel_unit);
                    bool good_efficiency = (beam_length >= 3.0 * distance);
                    if (good_efficiency) {
                      if (g_pParser && g_pParser->verbose) {
                        printf("\t→ Opportunistic shot during pursuit (beam=%.1f, dist=%.1f)\n",
                               beam_length, distance);
                      }
                      pShip->SetOrder(O_LASER, beam_length);
                    }
                  }
                }
              }
            }
          } else {
            // PHASE 1: No enemies within shooting distance - navigate to intercept
            if (g_pParser && g_pParser->verbose) {
              printf("\t→ PHASE 1: Navigating to intercept enemy ship '%s' (dist=%.1f)\n",
                     best_target->GetName(), pShip->GetPos().DistTo(best_target->GetPos()));
              printf("\t  Plan:\tturns=%.1f\torder=%s\tmag=%.2f\n",
                     best_path.fueltraj.time_to_intercept,
                     (best_path.fueltraj.order_kind == O_THRUST) ? "thrust"
                     : (best_path.fueltraj.order_kind == O_TURN) ? "turn" : "other/none",
                     best_path.fueltraj.order_mag);
            }

            // Use MagicBag navigation to intercept
            pShip->SetOrder(best_path.fueltraj.order_kind, best_path.fueltraj.order_mag);

            // Opportunistic shooting - if we happen to be facing them during intercept
            if (pShip->IsFacing(*best_target) && available_fuel > g_fp_error_epsilon) {
              double distance = pShip->GetPos().DistTo(best_target->GetPos());

              if (distance < max_beam_length) {
                double beam_length = min(512.0, available_fuel * g_laser_range_per_fuel_unit);
                bool good_efficiency = (beam_length >= 3.0 * distance);

                if (good_efficiency) {
                  if (g_pParser && g_pParser->verbose) {
                    printf("\t→ Opportunistic shot during intercept (beam=%.1f, dist=%.1f)\n",
                           beam_length, distance);
                  }
                  pShip->SetOrder(O_LASER, beam_length);
                }
              }
            }
          }
          }  // End of ramming_speed vs shooting tactics
        }
      } else if (g_pParser && g_pParser->verbose) {
        printf("t=%.1f\t%s [VIOLENCE]:\n", pmyWorld->GetGameTime(), pShip->GetName());
        printf("\t→ No valid enemy targets found\n");
      }
    }
    // If wants == NOTHING or VIOLENCE, the ship currently does nothing
    // strategic.
  }  // End of ship loop

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
  const double multiplier_sq = multiplier * multiplier;
  const double multiplier_cubed = multiplier_sq * multiplier;
  double prior_penalty = favor_previous_target ? 0.0 : 1.0;

  if (wants == POINTS) {
    // TODO: This relies on our ships 40 ton cargo hold being big enough to hold
    // any vinyl asteroid, and assumes we'll jettison the difference before
    // trying to catch this.
    double vinyl_gained = std::min(e.dest->GetMass(), max_cargo - cur_cargo);
    double fuel_spent = e.fueltraj.fuel_total;
    double time_to_intercept = e.fueltraj.time_to_intercept;
    unsigned int num_orders = e.fueltraj.num_orders;

    // Prevent division by zero if time_to_intercept is somehow 0.
    if (time_to_intercept < g_fp_error_epsilon)
      return 0.0;

    double utility_per_second = vinyl_gained / time_to_intercept;

    // This should be positive due to min asteroid size of 3, however just in
    // case we wish to preserve utility=0.0 as a sentinel value meaning "issue
    // no orders."
    utility = utility_per_second * multiplier_cubed -
              fuel_spent * multiplier_sq -
              prior_penalty * multiplier -
              num_orders;
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
    double fuel_gained =
        std::min(uranium_size, max_fuel - cur_fuel - fuel_spent) - fuel_spent;
    double time_to_intercept = e.fueltraj.time_to_intercept;
    unsigned int num_orders = e.fueltraj.num_orders;

    // Prevent division by zero.
    if (time_to_intercept < g_fp_error_epsilon)
      return 0.0;

    double utility_per_second = fuel_gained / time_to_intercept;

    // TODO: This doesn't grant any positive utility to the way we'll buff up
    // our shields when eating fuel.

    // Only grant positive utility if we're actually gaining fuel.
    utility = utility_per_second * multiplier_cubed -
              fuel_spent * multiplier_sq -
              prior_penalty * multiplier -
              num_orders;
    if (utility < 0.0) {
      utility = 0.0;
    }
  }
  return utility;
}

//////////////////////////////////////////////
