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
TBD: Change magic bag population to gracefully handle floating point rounding errors when reasoning about how many "turns" we have left to get our orders in for intercept.

*/

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() { return new Groonew; }

// TODO: Remove this
static const bool DEBUG_MODE = false;

//////////////////////////////////////////
// Groonew class implementation

Groonew::Groonew() {
  // Constructor - no initialization needed here
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
}

void Groonew::Turn() {
  CShip* pSh;

  // PHASE 1: Calculate optimal paths to all objects for all ships
  // Creates new MagicBag and fills it with precalculated trajectories
  PopulateMagicBag();  // Allocates new Groonew::mb

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
  for (unsigned int ship_i = 0; ship_i < GetShipCount(); ++ship_i) {
    CShip* ship = GetShip(ship_i);
    if (ship == NULL || !ship->IsAlive()) {
      continue;  // Skip dead ships
    }

    // Iterate through all objects in the world
    for (unsigned int thing_i = worldp->UFirstIndex; thing_i <= worldp->ULastIndex;
         thing_i = worldp->GetNextIndex(thing_i)) {
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
        FuelTraj fueltraj = determine_orders(athing, turn_i, ship);
        // TODO: Maybe clean this up so determine_orders has no side effects.
        // determine_orders may have set some orders in planning mode, we clear them here.
        ship->ResetOrders();  // Clear orders (we're just calculating, not executing)

        // TODO: Calculate actual fuel cost (currently returns 5.0)
        double fuel_cost = determine_probable_fuel_cost(athing, turn_i, ship);

        // TODO: Check for obstacles on path (currently returns dummy)
        Collision collision = detect_collisions_on_path(athing, turn_i, ship);

        // If we found a valid trajectory, save it and move to next object
        if (fueltraj.path_found) {
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

  // Returns true if the trajectories vtraj and vtarget are nearly parallel.
  // This is determined by checking if after projecting unit vectors of each out
  // dist their endpoints are within epsilon of each other.
  //
  // The default for dist is 724.1 which is the 1/2 the diagonal of the game
  // world. If the target is much closer and you want to allow more leeway,
  // try setting dist to the distance from the current position to the target
  // position (this becomes extremly permissive as dist approaches 8.9)
  //
  // The default for epsilon is half the sum of the default radius of a ship and
  // the smallest asteroids = (12+5.8)/2 = 8.9
  //  
  // These defaults mean a ship on vtraj will collide with any world object
  // whose actual intercept trajectory is vtarget as long as the distance
  // traversed is less than 1/2 the game world max separation which is a good
  // proxy for maximum reasonble travel distance in our toroidal world.
  bool on_target(CTraj vtraj, CTraj vtarget, double dist=724.1, double epsilon = 8.9) {
    vtraj.rho = dist;
    vtarget.rho = dist;
    CCoord traj_end = vtraj.ConvertToCoord();
    CCoord target_end = vtarget.ConvertToCoord();
    return traj_end.DistTo(target_end) <= epsilon;
  }

  // Checks if vtraj and vtarget are nearly parallel.
  bool mostly_parallel(CTraj vtraj, CTraj vtarget, double dist=724.1, double epsilon = 8.9) {
    return (on_target(vtraj, vtarget, dist, epsilon) || on_target(-vtraj, vtarget, dist, epsilon));
  }
}

// This will be the first thing we refine:
// Step 0: As I'm checking the stuff below, I want to validate with hello world style code checks, for instance that VectTo does what I expect.

// Step 0: Try is facing fix below once my greedy nav is online.

// Step 0: Rudimetnary collision fixes below - our collision detection doesn't
//         consider the closest bonk to us - which is the only relevant one.
//         CONSIDER USING A NEW SHIP, SETTING IT'S POSITION AND VELOCITY, AND
//         USING DETECT COLLISION OR OTHER METHODS TO DO THE NON-POINT MASS
//         ANALYSI MAYBE WE CAN USE DETECTCOLISIONCOURSE INSTEAD OF THETA <= 0.1

// Step 1: Check and validate the logic below is correct and optimal for point masses and dt=1.
//         * Account for max velocity - we'll get clamped to 30 so don't chase stuff we can't catch - if our resulting vel > 30 to get there in time t fail or limit to 30.
//         * Double check engine behavior - it seems it allows us to acclerate to > 30, by going up to 30 in each dimension or soemthing - this will quickly get capped later but understand current behavior for that tick.
//         * After making some fixes, it seems we're chasing things - is there an off by 1 error where we're 1 second behind? Or a failure to recognize when we're on maxspeed?
//           IT'S PROBABLY A MAXSPEED ISSUE - NEED HELLO WORLD DIAGNOSIS OF TRYING TO CHASE SOMETHING JUST AHEAD OF US GOING AT MAXSPEED.
//           IT MAY ALSO BE A FACING ISSUE - WHEN WE ARE CLOSE WE MAY NOT BE COMPLETELY DEAD ON BUT WE MAY WASTE TURNS DOING TINY MICRO ADJUSTMENTS INSTEAD OF THRUSTING.
//           THE OLD LOGIC OF DIVIDING BY TIME INSTEAD OF (TIME-1) MAY HAVE MASKED THIS ISSUE BY OVERSHOOTING THE TARGET ON APPROACH.
// Step 1: Think about our treatment of time as a double below - are we creating signpost problems where we are cutting things too close and are going to be 1 turn late on stuff (or 1 turn early?)
// Step 3: Consider how to prevent two ships going after the same target, and making the one who can get there best (e.g. fastest and with least fuel) the one who gets it.
// Step 3: Consider how to make planning stable so we don't pingping between targets with equal distance from us, e.g. we pick asteroid A that is 4 turns away this turn (but B is also 4 turns away) and plan to intercept A, but next round we pick asteroid B (both A and B now 3 turns away) and turn again, etc. and never execute on a plan for either.0
// Step 3: Consider how to make the logic toroid aware and relative velocity aware - e.g. ship at -200, asteroid at 200, ship going 30 left, asteroid going 30 right - if we do nothing we'll collide in ~624/60 = 11 turns. But if we chase it by turning to the right we'll never cath it.
// Step 3: Consider how to adapt the logic for the actual collision detection accounting for object radius.
// Step 3: Consider where we need to special case things below for IsDocked for optimal handling (already have a check in the Case 1a because nothing collides with us while docked.)
// Step 4: Think about how to decide if 2aii or 2b is better.
// Step 2: Adapt the logic given that the underlying simulation uses dt=0.2.
// Step 3: Consider how to make our estimates fuel aware, and our order selections fuel aware.
// Step 4: Consider how to adapt logic to be collision aware.
FuelTraj Groonew::determine_orders(CThing* thing, double time, CShip* ship) {
  /*
    The idea in this part of the code is to implement a sort of greedy
    pathfinding algorithm.

    We consider exhastively (I think?) the cases for a 2 turn planniing horizon
    to arrive at thing at or before time.

    Time is an upper bound. Normally we aim to arrive exaclty in time, because
    our architecture calls this function iteratively for lower times, so we
    don't need to worry about doing better than time here. That being said, for
    case 1a and 2ai we might arrive early.

    Note that this approach will not, in general, produce optimal shortest time
    paths in all scenarios. That is something to refine later.

    It differs from legacy Groogroo in that legacy Groogroo more or less ignored
    the engine limits and just assumed any thrust up to 30 was obtainable, and
    course corrected en route for any game engine interference with those plans.

    Here we explicitly consider how the overthrust mechanics work and try to use
    them to get on desired trajectories. We ignore for now the implementation
    details of the 5 engine subticks and plan at the turn level, leaving subtick
    analysis for future enhancement.
  */
  // Initialize our return value with the failure case.
  FuelTraj fj = FuelTraj(false, -1.0, O_SHIELD, 0.0);

  // Overview:
  // Case 0: Even at the game's max speed we can't get there in time.
  // Case 1: We're already on an intercept trajectory.
  // Case 1a: And we'll drift into it at or before the desired time. => No order.
  // Case 1b: We won't, but our orient is also on the trajectory and we can trust
  //          to arrive on time. => O_THURST
  // Case 1c: We won't, and our orient is not on the trajectory, but we can turn
  //          and thrust to arrive on time. => O_TURN (and plan to thrust next turn)
  // Case 2: We're not on the intercept trajectory.
  // Case 2a: There is a thrust we can issue this turn that will get us on the 
  //          intercept trajectory.
  //          NOTE: Sometimes this thrust will actually put us further away from
  //          the target, and increase our relative velocity. This is not a bug
  //          - we are choosing to do that being on a line where we can thrust
  //          straight at the target lets us get there in time.
  // Case 2ai: With final velocity such that we reduce to case 1a in 1 turn.
  //           => O_THRUST
  // Case 2aii: With final velocity such that we reduce to case 1c in 1 turn.
  //           => O_THRUST (and plan to turn next turn and thrust the turn after that)
  //            NOTE: Logically we can't go from case 2a to case 1b without an external event so it's not considered in planning.
  // Case 2b: There is a turn we can issue that will get us onto case 2ai in 1 turn.
  //          => O_TURN (and plan to thrust next turn)

  // Calculate where target will be in 'time' seconds
  // TODO: This doesn't account for collisions.
  CCoord destination = thing->PredictPosition(time);

  // Calculate thrust vector needed if we thrust right now
  CCoord ship_pos_t0 = ship->GetPos();
  double ship_orient_t0 = ship->GetOrient();
  CTraj ship_orient_vec_t0 = CTraj(1.0, ship_orient_t0);
  CTraj ship_vel_t0 = ship->GetVelocity();
  CCoord ship_pos_t1 = ship->PredictPosition(g_game_turn_duration);

  CTraj dest_vec_t0 = ship_pos_t0.VectTo(destination);  // Vector to target
  CTraj intercept_vec_t0 = dest_vec_t0;
  intercept_vec_t0.rho /= time;  // Velocity needed to reach target in time
  CTraj thrust_vec_t0 = intercept_vec_t0 - ship_vel_t0;  // Required change
  // So now intercep_vec_t0 = ship_vel_t0 + thrust_vec_t0, e.g. if we
  // apply thrust_vec to our current velocity, we should arrive at 
  // destination in around ~time seconds.

  // Figure similar variables for if we let the ship drift for 1 turn.
  CTraj dest_vec_t1 = ship_pos_t1.VectTo(destination);
  CTraj intercept_vec_t1 = dest_vec_t1;
  intercept_vec_t1.rho /= (time - g_game_turn_duration);  // Note - we have 1 turn less to get there
  CTraj thrust_vec_t1 = intercept_vec_t1 - ship_vel_t0; // Note - ship_vel hasn't changed

  // Case 0: Even at the game's max speed we can't get there in time.
  if (intercept_vec_t0.rho > g_game_max_speed) {
    return fj;
  }

  // Case 1a: And we'll drift into it at or before the desired time. => No order.
  //
  // NOTE: Since our obects aren't point masses, we don't need to be on an
  // intercept trajectory always to collide - so we do this test before checking
  // if we're on a (point mass) intercept trajectory.
  double its_coming_right_for_us = ship->DetectCollisionCourse(*thing);
  if (!ship-> IsDocked() && its_coming_right_for_us != g_no_collide_sentinel &&
      its_coming_right_for_us <= time) {
    // TODO: We have to issue some kind of order in FuelTraj but we don't actually
    // want our planner to take note of the order - O_SHIELD seems the safest bet,
    // but we should clean this up.
    fj.path_found = true;
    fj.fuel_used = 0.0;
    fj.order_kind = O_SHIELD;
    fj.order_mag = 0.0;
    return fj;
  }    

  // Case 1: We're already on an intercept trajectory.
  //
  // If our ship isn't moving, consider our trajectory for the purposes of
  // trajectory matching to be along the direction of our orientation.
  CTraj ship_trajectory_t0 = ship_vel_t0;
  if (ship_vel_t0.rho <= g_fp_error_epsilon) {
    ship_trajectory_t0 = ship_orient_vec_t0;
  }
  if (mostly_parallel(ship_trajectory_t0, dest_vec_t0, dest_vec_t0.rho)) {
    // Case 1a: Handled above to catch corner cases where we collide even
    // without being on a point mass intercept trajectory.

    // Case 1b: We won't arrive in time as is, but our orient is also on the 
    //          trajectory and we can trust to arrive on time. => O_THURST
    if (mostly_parallel(ship_orient_vec_t0, dest_vec_t0, dest_vec_t0.rho)) {
      // Note - above we checked if the ships velocity was heading towards dest
      // - here we're checking if this ships thrusters are pointed that way.
      double thrust_order_amt = thrust_vec_t0.rho;
      if (fabs(ship_orient_vec_t0.theta - dest_vec_t0.theta) > PI) {
        thrust_order_amt *= -1.0;
      }
      if (!is_speeding(ship, thrust_order_amt)) {
        fj.path_found = true;
        // TODO: Fuel awareness - check if our order was reduced due to fuel limits.
        fj.fuel_used = ship->SetOrder(O_THRUST, thrust_order_amt);
        fj.order_kind = O_THRUST;
        fj.order_mag = thrust_order_amt;
        return fj;
      }
    }
    
    // Case 1c: We won't arrive on time as it is, and our orient is not on the
    //          trajectory, but we can turn and thrust to arrive on time. =>
    //          O_TURN (should reduce to Case 1b next turn)
    if (intercept_vec_t1.rho <= g_game_max_speed) {
      // Even through we can thrust forward and backward, prefer to be facing our
      // target so we can shoot it if we want to.

      // Verbose logging header
      if (g_pParser && g_pParser->verbose) {
        printf("t=%.1f\t%s:\n\tO_TURN FROM CASE 1C\n", pmyWorld->GetGameTime(), ship->GetName());
      }

      double turn_order_amt = thrust_vec_t1.theta - ship_orient_t0;
      fj.path_found = true;
      fj.fuel_used = ship->SetOrder(O_TURN, turn_order_amt);
      fj.order_kind = O_TURN;
      fj.order_mag = turn_order_amt;
      return fj;
    }
  } else {
    // Case 2: We're not on the intercept trajectory.

    // Case 2a: There is a thrust we can issue this turn that will get us on the 
    //          intercept trajectory.
    
    // Via trigonometry we can figure out the thrust amount on our current
    // orientation that would convert our current trajectory onto our desired
    // trajectory. In general this is possible so long as our orientation isn't
    // parallel to our desired trajectory (and various corner cases like our
    // current velocity is zero).
    //
    // However these resulting thursts may not be feasible in game due to
    // excdeeding the maximum thrust amount, and the resulting velocity may be
    // small (in this case the geometry will determine the velocity we can
    // obtain along our desired trajectory).
    //
    // In this case we'll thrust along our current ship_orient_t0 with some
    // magnitude, call this signed thrust amount: k, and let:
    //
    // s = ship_vel_t0.rho
    // theta = ship_vel_t0.theta
    // phi = dest_vec_t0.theta
    //
    // Via trig (see README.md):
    //
    // k = s * sin(phi - theta) / sin(ship_orient_t0 - phi)
    //
    // (negative k means we issue reverse thurst).
    //
    // Note - sometimes to put us on the desired trajectory we will actually
    // move away from the target in this approach!
    //
    // TODO: We have two possible ways of getting on the target heading with a
    // specific speed under consideration here: This approach which thrusts now,
    // and then will probably turn and thrust again on later turns, and the
    // approach below where we turn now and thrust next turn. We should think
    // about how to choose between these approaches when they are both possible
    // - e.g. fuel use, whether we think that ending up oriented towards the
    // target on the approach is important, etc.
    //
    // Below we'll consider case 2aii and compute our orders for 2aii, however if
    // our preference for orders is: 2ai > 2b > 2aii, so we save any computed
    // 2aii order here but only use it if 2b fails.
    FuelTraj fj_thurst_turn_thrust = fj;
    {
      double denominator = sin(ship_orient_t0 - dest_vec_t0.theta);
      if (denominator != 0.0) {
        // TODO - What if ship_vel_t0.rho is zero - as it will be when we're
        // docked? Analyze the cases - hopefully if we're docked or at zero
        // velcoity and facing the right way we catch that up above in case 1.
        double k = ship_vel_t0.rho * sin(dest_vec_t0.theta - ship_vel_t0.theta) / denominator;
        if (fabs(k) <= g_game_max_thrust_order_mag) {
          // Note - due to how the engine works, even if this thrust results in
          // speeding we'll end up on our desired trajectory - but we need to
          // issue the full thrust to end up on that trajectory after engine
          // correction - so no speed limits for us here!

          // Let's reason about the outcomes of issuing this thrust:
          CTraj t_ship_vel_t1 = ship_vel_t0 + CTraj(k, ship_orient_t0);
          if (t_ship_vel_t1.rho > g_game_max_speed) {
            // This is how the game engine will clamp us if we try to speed
            // (more or less, the game engine will actually break our thrust
            // into 5 subturns...)
            t_ship_vel_t1.rho = g_game_max_speed;
          }
          CCoord t_ship_pos_t1 = ship_pos_t0 + t_ship_vel_t1.ConvertToCoord();
          CTraj t_dest_vec_t1 = t_ship_pos_t1.VectTo(destination);
          // TODO - delete these 2 lines if we don't need them.
          //t_intercept_vec_t1.rho /= (time - g_game_turn_duration);  // Note - we have 1 turn less to get there
          //CTraj t_thrust_vec_t1 = t_intercept_vec_t1 - t_ship_vel_t1;
          // TODO - we're about to do math here to see basically if we reduce to
          // some of the aligned cases above - we should break out that code
          // into a function.

          // Case 2ai: With final velocity such that we'll either arrive on time
          //           or early. => O_THRUST

          // Check if we'll reach the target next turn due to this thrust.
          bool thrust_reaches_target = (t_dest_vec_t1.rho <= (ship->GetSize() + thing->GetSize() / 2.0));

          // It is logically possible, if perhaps not geometrically possible,
          // that our thrust would push us throug the position of the object -
          // if so we'd expect the game engine to register a collision and so
          // that's a thrust we'd like to execute. For now figuring that out
          // seems hard so we'll leave that for future enhancement.
          //
          // Note - if we try to implement this we need to be careful about
          // differentiating about the case where we thrust _around_ the object
          // with no collision.
          bool thrusted_through = false;

          // Check if the thrust put us on a trajectory that will collide in
          // time.
          double time_left = time - g_game_turn_duration;
          bool thrust_and_drift = (
            on_target(t_ship_vel_t1, t_dest_vec_t1)
            && time_left > 0.0
            && t_ship_vel_t1.rho >= (t_dest_vec_t1.rho / time_left)
          );

          if (thrust_reaches_target || thrusted_through || thrust_and_drift) {
            // Check if we're heading the right way fast enough.
            fj.path_found = true;
            // TODO: Fuel awareness
            fj.fuel_used = ship->SetOrder(O_THRUST, k);
            fj.order_kind = O_THRUST;
            fj.order_mag = k;
            return fj;
          }

          // Case 2aii: With final velocity such that we reduce to case 1c in 1 turn.
          //           => O_THRUST (and plan to turn next turn and thrust the turn after that)
          if (time > 2*g_game_turn_duration) {
            CCoord t_ship_pos_t2 = t_ship_pos_t1 + t_ship_vel_t1.ConvertToCoord();
            CTraj t_dest_vec_t2 = t_ship_pos_t2.VectTo(destination);
            CTraj t_intercept_vec_t2 = t_dest_vec_t2;
            t_intercept_vec_t2.rho /= (time - 2*g_game_turn_duration);  // Note - we have 2 turns less to get there
            CTraj t_thrust_vec_t2 = t_intercept_vec_t2 - t_ship_vel_t1;
            if (mostly_parallel(t_ship_vel_t1, t_intercept_vec_t2, t_dest_vec_t2.rho) 
                && t_intercept_vec_t2.rho <= g_game_max_speed) {
              fj_thurst_turn_thrust.path_found = true;
              // TODO: Fuel awareness
              fj_thurst_turn_thrust.fuel_used = ship->SetOrder(O_THRUST, k);
              fj_thurst_turn_thrust.order_kind = O_THRUST;
              fj_thurst_turn_thrust.order_mag = k;
              // Fall through to case 2b - only use this order if 2b fails.
            }
          }
        }
      }
    }

    // Case 2b: There is a turn we can issue that will get us onto case 2ai in 1 turn.
    //          => O_TURN (and plan to thrust next turn)
    // TODO: Case 2b is not always optimal - it may be better to do some
    // thrust->overthrust->turn sequence instead. Consider the case where we are
    // oriented right, and are trying to get to something mostly right and
    // slightly up. We can thrust right on turn 1, rotate slightly up and on turn
    // 2, and thrust up/right on turn 3, having benefitted from our initial
    // thrus's velocity on both turns 1 and 2 to reduce the desired distance to
    // our object for our thurst onto intercept on turn 3.
    //
    // TODO: Actually - the comment above gave me an idea - maybe we should
    // analyze our current thrust options in terms of decompositon of parallel to,
    // and perpendicualr to, our desired thurst, and consider strongly thrusting
    // where the parallel to dimension is high.
    //
    // TODO: Definitely fix this kind of comparison - we're never calling this
    // function with time = 1.5, so here due to FP rounding errors we might be
    // saying we don't have time to issue orders, or we do have time - in here we
    // are reasoning about turns, not tie durations - see us_later for example!
    if (time > g_game_turn_duration) {
      if (!is_speeding(ship, thrust_vec_t1.rho)) {
        if (g_pParser && g_pParser->verbose) {
          printf("t=%.1f\t%s:\n\tO_TURN FROM CASE 2B\n", pmyWorld->GetGameTime(), ship->GetName());
        }
        double turn_order_amt = thrust_vec_t1.theta - ship_orient_t0;
        // Rotate to face target this turn.
        fj.path_found = true;
        // TODO: Fuel awareness - check if our order was reduced due to fuel limits.
        fj.fuel_used = ship->SetOrder(O_TURN, turn_order_amt);
        fj.order_kind = O_TURN;
        fj.order_mag = turn_order_amt;
        return fj;
      }
    } else if(fj_thurst_turn_thrust.path_found) {
      return fj_thurst_turn_thrust;
    }
  }

  // If we got down here we couldn't find any way to intercept in time, return
  // our initial failure indicating fj.
  return fj;
}  

/*
  For future enhancements, before going to full perfect path detection, we might consider
  playing around in this space:
     a. In a position to get there on future turns.
     b. So here we're starting to think about optimal paths...
     c. Before going full optimal path, maybe there is a better heuristic we can do for a 2-4 turn planning horizon.
        Right now we consider 1-2 turns, either thrust now, or turn and then thrust.
        We should consider:
        1 turn of actions:
          Drift
          Thrust
        2 turns of actions:
          Drift, Drift
          Thrust, Drift
          Thrust, Thurst
          Turn, Thrust
        3 turns of actions:
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
*/

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
