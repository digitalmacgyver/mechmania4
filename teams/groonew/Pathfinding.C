/* Pathfinding.C
 * Implementation of pathfinding algorithms for MechMania IV
 * Extracted from Groonew team implementation for better maintainability
 */

#include "Pathfinding.h"
#include "Coord.h"
#include "GameConstants.h"
#include "ParserModern.h"
#include "Traj.h"

#include <cmath>

// External reference to global parser instance
extern CParser* g_pParser;

namespace Pathfinding {

  // Internal namespace for implementation details
  namespace {

    // Represents a failure case for pathfinding
    const FuelTraj FAILURE_TRAJ = FuelTraj(false, O_SHIELD, 0.0, -1.0, 0.0, 0, 0.0);

    // Lightweight struct representing the essential state of a ship for simulation.
    struct ShipState {
      // Physics
      CCoord pos;
      CTraj velocity;
      double orient;

      // Key Resources (Must include everything that affects mass or fuel usage)
      double fuel;
      double cargo;
    };

    // Utility to capture the current state from a ship.
    ShipState CaptureState(const CShip* ship) {
      ShipState state;
      state.pos = ship->GetPos();
      state.velocity = ship->GetVelocity();
      state.orient = ship->GetOrient();
      state.fuel = ship->GetAmount(S_FUEL);
      state.cargo = ship->GetAmount(S_CARGO);
      return state;
    }

    // The core calculation function. It guarantees synchronization before calculation.
    double CalculateAccurateFuelCost(CShip* calculator, const ShipState& state, OrderKind kind, double magnitude) {
      // 1. Synchronize State (Apply Memento)
      // NOTE: This relies on the availability of setters (SetPos, SetVelocity, etc.).
      // Make non-const copies since SetPos/SetVel require non-const references
      CCoord pos = state.pos;
      CTraj velocity = state.velocity;

      calculator->SetPos(pos);
      calculator->SetVel(velocity);
      calculator->SetOrient(state.orient);
      calculator->SetAmount(S_FUEL, state.fuel);
      calculator->SetAmount(S_CARGO, state.cargo);

      // Crucial: Clear any lingering orders from previous simulations.
      calculator->ResetOrders();

      // 2. Calculate Cost (Use authoritative game logic)
      double cost = calculator->SetOrder(kind, magnitude);

      return cost;
    }

    // --- Pathfinding Context ---

    // Struct to hold the context of the pathfinding attempt for a specific ship and target.
    struct PathfindingContext {
      CThing* thing;
      CShip* ship;
      double time;
      CShip* calculator_ship;
      ShipState state_t0;

      CCoord destination; // Predicted position of the target at 'time'.

      // State and vectors at t0 (now)
      CCoord ship_pos_t0;
      CTraj ship_vel_t0;
      double ship_orient_t0;
      CTraj ship_orient_vec_t0; // Unit vector of orientation.
      // The ship's trajectory on t0 - either ship_vel_t0 or ship_orient_vec_t0 when velocity ~= 0.
      CTraj ship_trajectory_t0;

      CTraj dest_vec_t0;        // Vector from ship to destination.
      CTraj intercept_vec_t0;   // Required velocity to reach destination in 'time'.

      // Vectors at t1 (after 1 turn drift)
      CTraj intercept_vec_t1;   // Required velocity to reach destination in 'time - 1'.
      CTraj thrust_vec_t1;      // Required change in velocity at t1.
      bool t1_intercept_feasible;
    };

    // TODO: Is tere a way to do this with a return rather than an in/out parameter?
    // Initializes the pathfinding context based on the current state and target.
    void InitializeContext(PathfindingContext& ctx, CShip* ship, CThing* thing, double time, CShip* calculator_ship) {
      ctx.thing = thing;
      ctx.ship = ship;
      ctx.time = time;

      // Calculate where target will be in 'time' seconds
      // TODO: This doesn't account for collisions.
      ctx.destination = thing->PredictPosition(time);

      // Initialize T0 state
      ctx.ship_pos_t0 = ship->GetPos();
      ctx.ship_orient_t0 = ship->GetOrient();
      ctx.ship_orient_vec_t0 = CTraj(1.0, ctx.ship_orient_t0);
      ctx.ship_vel_t0 = ship->GetVelocity();

      // If our ship isn't moving, consider our trajectory for the purposes of
      // trajectory matching to be along the direction of our orientation.
      ctx.ship_trajectory_t0 = ctx.ship_vel_t0;
      if (ctx.ship_vel_t0.rho <= g_fp_error_epsilon) {
        ctx.ship_trajectory_t0 = ctx.ship_orient_vec_t0;
      }

      // T0 Calculations
      ctx.dest_vec_t0 = ctx.ship_pos_t0.VectTo(ctx.destination);  // Vector to target
      ctx.intercept_vec_t0 = ctx.dest_vec_t0;
      ctx.intercept_vec_t0.rho /= time;  // Velocity needed to reach target in time
      // So now intercep_vec_t0 represents the ideal velocity starting now.

      // T1 Calculations (if we let the ship drift for 1 turn)
      CCoord ship_pos_t1 = ship->PredictPosition(g_game_turn_duration);
      CTraj dest_vec_t1 = ship_pos_t1.VectTo(ctx.destination);
      ctx.intercept_vec_t1 = dest_vec_t1;
      ctx.t1_intercept_feasible = false;
      // TODO: This condition is really trying to represent "we have at least 2
      // game turns to intercept - because we wish to issue 2 orders to arrive on
      // an intercept course."
      if (time >= (g_game_turn_duration + g_fp_error_epsilon)) {
        ctx.t1_intercept_feasible = true;
        ctx.intercept_vec_t1.rho /= (time - g_game_turn_duration);  // Note - we have 1 turn less to get there
        ctx.thrust_vec_t1 = ctx.intercept_vec_t1 - ctx.ship_vel_t0; // Note - ship_vel hasn't changed since T0
      }

      ctx.calculator_ship = calculator_ship;
      ctx.state_t0 = CaptureState(ship); // Capture T0 state
    }

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

    // --- Maneuver Helpers ---

    // Helper to create a successful FuelTraj and calculate fuel usage via SetOrder.
    FuelTraj CreateSuccessTraj(const PathfindingContext& ctx, CShip* ship, OrderKind kind,
      double mag, double fuel_used, unsigned int num_orders, double time_to_intercept, 
      double fuel_total, const char* case_label) {
      FuelTraj fj;
      fj.path_found = true;
      fj.order_kind = kind;
      fj.order_mag = mag;
      fj.fuel_used = fuel_used;
      fj.time_to_intercept = time_to_intercept;
      fj.num_orders = num_orders;
      fj.fuel_total = fuel_total;

      // Verbose logging of successful paths
      if (g_pParser && g_pParser->verbose) {
        const char* order_name = (kind == O_THRUST) ? "THRUST" :
                                (kind == O_TURN) ? "TURN" :
                                (kind == O_SHIELD) ? "DRIFT" : "OTHER";

        // Get target velocity
        CTraj target_vel = ctx.thing->GetVelocity();

        printf("[Pathfinding] Case %s: %s %.3f (fuel=%.1f)\n",
               case_label, order_name, mag, fj.fuel_used);
        printf("  Ship: pos(%.1f,%.1f) vel(%.1f,%.2f) orient=%.2f\n",
               ctx.ship_pos_t0.fX, ctx.ship_pos_t0.fY,
               ctx.ship_vel_t0.rho, ctx.ship_vel_t0.theta,
               ctx.ship_orient_t0);
        printf("  Target: pos(%.1f,%.1f) vel(%.1f,%.2f) dest(%.1f,%.1f)\n",
               ctx.thing->GetPos().fX, ctx.thing->GetPos().fY,
               target_vel.rho, target_vel.theta,
               ctx.destination.fX, ctx.destination.fY);
      }

      return fj;
    }

    // Case 1a: And we'll drift into it at or before the desired time. => No order.
    FuelTraj TryDriftIntercept(const PathfindingContext& ctx) {
      CShip* ship = ctx.ship;
      CThing* thing = ctx.thing;
      double time = ctx.time;

      // NOTE: Since our obects aren't point masses, we don't need to be on an
      // intercept trajectory always to collide - so case 1a shouldn't be guarded by a check of weather we're on an intercept trajectory.
      double its_coming_right_for_us = ship->DetectCollisionCourse(*thing);
      if (!ship->IsDocked() && its_coming_right_for_us != g_no_collide_sentinel &&
      its_coming_right_for_us <= time) {

        // TODO: We have to issue some kind of order in FuelTraj but we don't actually
        // want our planner to take note of the order - O_SHIELD seems the safest bet,
        // but we should clean this up.
        return CreateSuccessTraj(ctx, ship, O_SHIELD, 0.0, 0.0, 1, time, 0.0, "1a");
      }
      return FAILURE_TRAJ;
    }

    // Case 1b: On trajectory, aligned orientation, needs thrust.
    FuelTraj TryAlignedThrust(const PathfindingContext& ctx) {
      const CTraj& dest_vec_t0 = ctx.dest_vec_t0;
      const CTraj& ship_orient_vec_t0 = ctx.ship_orient_vec_t0;
      const CTraj& ship_vel_t0 = ctx.ship_vel_t0;
      const CTraj& ship_trajectory_t0  = ctx.ship_trajectory_t0;
      const CTraj& intercept_vec_t0 = ctx.intercept_vec_t0;
      CShip* ship = ctx.ship;

      bool correct_heading = mostly_parallel(ship_trajectory_t0, dest_vec_t0, dest_vec_t0.rho);
      bool correct_facing = mostly_parallel(ship_orient_vec_t0, dest_vec_t0, dest_vec_t0.rho);

      if (correct_heading && correct_facing) {
        // Calculate required thrust (Required change in velocity).
        CTraj thrust_vec_t0 = intercept_vec_t0 - ship_vel_t0;
        double thrust_order_amt = thrust_vec_t0.rho;

        // Do we need forward or reverse thrust?
        if (fabs(ship_orient_vec_t0.theta - dest_vec_t0.theta) > PI) {
          thrust_order_amt *= -1.0;
        }
        if (!is_speeding(ship, thrust_order_amt)) {
          // TODO: Check if our order was reduced due to fuel limits and return FAILURE_TRAJ instead.
          double fuel_used = CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_THRUST, thrust_order_amt);          
          unsigned int num_orders = 1;
          double fuel_total = fuel_used;
          double time_to_intercept = ctx.time;  
          return CreateSuccessTraj(ctx, ship, O_THRUST, thrust_order_amt, fuel_used, 
            num_orders, time_to_intercept, fuel_total, "1b");
        }
      }
      return FAILURE_TRAJ;
    }

    // Case 1c: On trajectory, misaligned orientation, needs turn (reduces to Case 1b next turn).
    FuelTraj TryTurnToAlign(const PathfindingContext& ctx) {
      const CTraj& intercept_vec_t1 = ctx.intercept_vec_t1;
      const CTraj& thrust_vec_t1 = ctx.thrust_vec_t1;
      CShip* ship = ctx.ship;
      double ship_orient_t0 = ctx.ship_orient_t0;
      bool t1_intercept_feasible = ctx.t1_intercept_feasible;

      // We won't arrive on time as it is, and our orient is not on the
      // trajectory, but we can turn and thrust to arrive on time. =>
      // O_TURN (should reduce to Case 1b next game turn)
      //
      // TODO: Clean up this opaque interface around t1_intercept_feasible;
      //
      // Note: intercept_vec_t1.rho and thrust_vec_t1 are only valid if
      // ti_intercept_feasible.
      if (t1_intercept_feasible && (intercept_vec_t1.rho <= g_game_max_speed)) {
        // Even through we can thrust forward and backward, prefer to be facing our
        // target so we can shoot it if we want to.

        double turn_order_amt = thrust_vec_t1.theta - ship_orient_t0;

        double fuel_used = CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_TURN, turn_order_amt);
        unsigned int num_orders = 1;
        double fuel_total = fuel_used;
        double time_to_intercept = ctx.time;
        return CreateSuccessTraj(ctx, ship, O_TURN, turn_order_amt, fuel_used, num_orders,
          time_to_intercept, fuel_total, "1c");
      }
      return FAILURE_TRAJ;
    }

    // Case 2b: Misaligned trajectory, turn now to enable thrust next turn (reduces to Case 2ai/1b).
    FuelTraj TryTurnThenThrust(const PathfindingContext& ctx) {
      CShip* ship = ctx.ship;
      double time = ctx.time;
      const CTraj& thrust_vec_t1 = ctx.thrust_vec_t1;
      double ship_orient_t0 = ctx.ship_orient_t0;
      bool t1_intercept_feasible = ctx.t1_intercept_feasible;

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
      // are reasoning about turns, not time durations!
      if (t1_intercept_feasible) {
        if (!is_speeding(ship, thrust_vec_t1.rho)) {
          double turn_order_amt = thrust_vec_t1.theta - ship_orient_t0;

          double fuel_used = CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_TURN, turn_order_amt);
          unsigned int num_orders = 2;
          // TODO: We introduce a slight error in fuel_total as we base the cost
          // on our current state not what our state will be when we issue the
          // next order. For now we accept this for brevity rather than
          // simulating state of our ship next turn for maginally more accurate
          // fuel estimates.
          double fuel_total = fuel_used + CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_THRUST, thrust_vec_t1.rho);
          double time_to_intercept = ctx.time;

          // Check this multi-order path doesn't run out of fuel.
          if (fuel_total < ctx.state_t0.fuel) {
            return CreateSuccessTraj(ctx, ship, O_TURN, turn_order_amt, fuel_used, num_orders,
              time_to_intercept, fuel_total, "2b");
          }
        }
      }
      return FAILURE_TRAJ;
    }


    // --- Case 2a Analysis ---

    // Case 2a: There is a thrust we can issue this turn that will get us on the
    //          intercept trajectory.

    // This struct holds the results of the analysis for Case 2a maneuvers.
    struct Case2aResult {
      FuelTraj fj_2ai = FAILURE_TRAJ;   // 2ai: Thrust and drift (preferred)
      FuelTraj fj_2aii = FAILURE_TRAJ;  // 2aii: Thrust-Turn-Thrust (fallback)
    };

    // Analyzes potential maneuvers where we thrust now to align with the target trajectory.
    Case2aResult AnalyzeThrustToAlign(const PathfindingContext& ctx) {
      Case2aResult result;
      CShip* ship = ctx.ship;
      CThing* thing = ctx.thing;
      double time = ctx.time;
      const CCoord& ship_pos_t0 = ctx.ship_pos_t0;
      const CTraj& ship_vel_t0 = ctx.ship_vel_t0;
      double ship_orient_t0 = ctx.ship_orient_t0;
      const CTraj& dest_vec_t0 = ctx.dest_vec_t0;
      const CCoord& destination = ctx.destination;

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

          // Case 2ai: We can issue a single thrust order with a resulting
          //           velocity such that we'll either arrive on time or early.
          //           => O_THRUST

          // Check if we'll reach the target next turn due to this thrust.
          bool thrust_reaches_target = (t_dest_vec_t1.rho <= ((ship->GetSize() + thing->GetSize()) / 2.0));

          // TODO: It is logically possible, if perhaps not geometrically possible,
          // that our thrust would push us through the position of the destination -
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
            on_target(t_ship_vel_t1, t_dest_vec_t1, t_dest_vec_t1.rho)
            && time_left > 0.0
            && t_ship_vel_t1.rho >= (t_dest_vec_t1.rho / time_left)
          );

          // Check if we're heading the right way fast enough.
          if (thrust_reaches_target || thrusted_through || thrust_and_drift) {
            double fuel_used = CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_THRUST, k);
            unsigned int num_orders = 1;
            double fuel_total = fuel_used;
            double time_to_intercept = ctx.time;
            if (thrust_reaches_target) {
              time_to_intercept = g_game_turn_duration;
            }
            result.fj_2ai = CreateSuccessTraj(ctx, ship, O_THRUST, k, fuel_used, num_orders,
              time_to_intercept, fuel_total, "2ai");
          }

          // Case 2aii: We can issue a single thrust order with a resulting
          //           velocity such that we'll reduce to case 1c in 1 turn. =>
          //           O_THRUST (and plan to turn next turn and thrust the turn
          //           after that)

          // NOTE: This condition is really trying to represent "we have at least 3
          // game turns to intercept - because we wish to issue 3 orders to arrive on
          // an intercept course."
          if (time > (2*g_game_turn_duration + g_fp_error_epsilon)) {
            CCoord t_ship_pos_t2 = t_ship_pos_t1 + t_ship_vel_t1.ConvertToCoord();
            CTraj t_dest_vec_t2 = t_ship_pos_t2.VectTo(destination);
            CTraj t_intercept_vec_t2 = t_dest_vec_t2;
            // Note - we have 2 turns less to arrive on this order because we'll
            // have issued a thrust and a turn to get into this position.
            t_intercept_vec_t2.rho /= (time - 2*g_game_turn_duration);  
            if (mostly_parallel(t_ship_vel_t1, t_intercept_vec_t2, t_dest_vec_t2.rho)
                && t_intercept_vec_t2.rho <= g_game_max_speed) {

              double fuel_used = CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_THRUST, k);
              unsigned int num_orders = 3;
              // TODO: We introduce a slight error in fuel_total as we base the
              // cost on our current state not what our state will be when we
              // issue the next order. For now we accept this for brevity rather
              // than simulating state of our ship next turn for maginally more
              // accurate fuel estimates.
              double fuel_total = fuel_used 
                // Add the rotation to bring our orient onto the direction the
                // first thrust got us on.
                + CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_TURN, t_ship_vel_t1.theta - ship_orient_t0);
                // Add the acceleration towards the target.
                + CalculateAccurateFuelCost(ctx.calculator_ship, ctx.state_t0, O_THRUST, fabs(t_intercept_vec_t2.rho - t_ship_vel_t1.rho));
              double time_to_intercept = ctx.time;

              if (fuel_total >= ctx.state_t0.fuel) {
                // Don't claim to have found multi-order paths that would run out of fuel.
                result.fj_2aii = FAILURE_TRAJ;
              } else {
                result.fj_2aii = CreateSuccessTraj(ctx, ship, O_THRUST, k, fuel_used, num_orders,
                  time_to_intercept, fuel_total, "2aii");

                // Diagnostic logging for Case 2aii orientation analysis
                if (g_pParser && g_pParser->verbose && result.fj_2aii.path_found) {
                  if (ship_vel_t0.rho > 1.0) {
                    // Calculate angle between ship orientation and velocity direction
                    double angle_diff = fabs(ship_orient_t0 - ship_vel_t0.theta);
                    // Normalize to [0, PI]
                    while (angle_diff > 2*PI) angle_diff -= 2*PI;
                    if (angle_diff > PI) angle_diff = 2*PI - angle_diff;

                    const char* facing_dir;
                    if (angle_diff <= PI/4) {
                      facing_dir = "FORWARD";  // Within 45 degrees of velocity
                    } else if (angle_diff >= 3*PI/4) {
                      facing_dir = "BACKWARD"; // Within 45 degrees of opposite velocity
                    } else {
                      facing_dir = "SIDEWAYS"; // In the 90 degree arcs to either side
                    }

                    printf("[Case 2aii Analysis] Thrust: %.2f, Facing: %s, Speed: %.1f/%.1f, Orient-Vel angle: %.2f rad\n",
                           k, facing_dir, ship_vel_t0.rho, g_game_max_speed, angle_diff);
                  } else {
                    // Low velocity case - ship is essentially stationary
                    printf("[Case 2aii Analysis] Thrust: %.2f, LOW_VELOCITY (%.2f), Orient: %.2f rad\n",
                           k, ship_vel_t0.rho, ship_orient_t0);
                  }
                }
              }
            }
          }
        }
      }
      return result;
    }

  } // End anonymous namespace

  // This will be the first thing we refine:

  // Step 0: As I'm checking the stuff below, I want to validate with hello
  // world style code checks, for instance that VectTo does what I expect.

  // Step 0: in case 1a We don't check if anything is going to collide with us
  //         in the case where we plan a drift intercept. If we're planning to
  //         drift we should see if anything will collide with us _before_ our
  //         target does. More broadly for all case where we drift for a turn we
  //         should check if anything other than what we're trying to hit
  //         collides first and return failure.
  
  // Step 1: Think about our treatment of time as a double below - are we
  // creating signpost problems where we are cutting things too close and are
  // going to be 1 turn late on stuff (or 1 turn early?)

  // Step 3: Consider how to make planning stable so we don't pingping between
  // targets with equal distance from us, e.g. we pick asteroid A that is 4
  // turns away this turn (but B is also 4 turns away) and plan to intercept A,
  // but next round we pick asteroid B (both A and B now 3 turns away) and turn
  // again, etc. and never execute on a plan for either.0

  // Step 3: Consider how to make the logic toroid aware and relative velocity
  // aware - e.g. ship at -200, asteroid at 200, ship going 30 left, asteroid
  // going 30 right - if we do nothing we'll collide in ~624/60 = 11 turns. But
  // if we chase it by turning to the right we'll never cath it.

  // Step 3: Consider how to adapt the logic for the actual collision detection
  // accounting for object radius.

  // Step 3: Consider where we need to special case things below for IsDocked
  // for optimal handling (already have a check in the Case 1a because nothing
  // collides with us while docked.)

  // Step 2: Adapt the logic given that the underlying simulation uses dt=0.2.

  // Step 4: Consider how to adapt logic to be collision aware.
  FuelTraj DetermineOrders(CShip* ship, CThing* thing, double time, CShip* calculator) {
    /*
    The idea in this part of the code is to implement a sort of greedy
    pathfinding algorithm.

    We consider exhastively (I think?) the cases for a 2 turn planniing horizon
    to intercept thing.

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
    PathfindingContext ctx;

    // Populates context as an in/out parameter.
    InitializeContext(ctx, ship, thing, time, calculator);

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
    // Case 2ai: With final velocity such that we reduce to case 1a in the following turn.
    //           => O_THRUST
    // Case 2aii: With final velocity such that we reduce to case 1c in the followimg turn.
    //           => O_THRUST (and plan to turn next turn and thrust the turn after that)
    //            NOTE: Logically we can't go from case 2a to case 1b without an external event so it's not considered in planning.
    // Case 2b: There is a turn we can issue that will get us onto case 2ai in the following turn.
    //          => O_TURN (and plan to thrust next turn)

    // Case 0: Even at the game's max speed we can't get there in time.
    if (ctx.intercept_vec_t0.rho > g_game_max_speed) {
      return FAILURE_TRAJ;
    }

    // Case 1a: Check for drift collision (handled outside the intercept trajectory check because our nonpoint massess can collide even when not on an (point mass) intercept trajectory.
    FuelTraj fj = TryDriftIntercept(ctx);
    if (fj.path_found) {
      return fj;
    }

    // Determine if we are currently on an intercept trajectory.
    // If our ship isn't moving, consider our trajectory for the purposes of
    // this check to be along the direction of our orientation.
    bool on_intercept_trajectory = mostly_parallel(ctx.ship_trajectory_t0, ctx.dest_vec_t0, ctx.dest_vec_t0.rho);

    if (on_intercept_trajectory) {
      // Case 1: We're already on an intercept trajectory.

      // Case 1b: We are aligned and can thrust now.
      fj = TryAlignedThrust(ctx);
      if (fj.path_found) {
        return fj;
      }

      // Case 1c: We are misaligned and can turn now (and thrust next turn).
      fj = TryTurnToAlign(ctx);
      if (fj.path_found) {
        return fj;
      }

    } else {
      // Case 2: We're not on the intercept trajectory.

      // We'll evaluate three cases:
      // Case 2ai: We can thrust now and then drift to the target in time.
      // Case 2aii: We can thrust now and then turn and thrust next turn to get on the intercept trajectory.
      // Case 2b: We can turn now and then thrust next turn to get on the intercept trajectory.

      // We'll check which one is the best according to thes metrics:
      // - Minimum time to intercept.
      // - Minimum fuel used.
      // - Minimum number of orders.
      Case2aResult result_2a = AnalyzeThrustToAlign(ctx);

      FuelTraj case_2ai = result_2a.fj_2ai;
      FuelTraj case_2aii = result_2a.fj_2aii;
      FuelTraj case_2b = TryTurnThenThrust(ctx);

      bool has_best_case = false;
      FuelTraj best_case;

      auto consider = [&](const FuelTraj& candidate) {
        if (!candidate.path_found) {
          return;
        }
        if (!has_best_case) {
          best_case = candidate;
          has_best_case = true;
          return;
        }
        if (candidate.time_to_intercept < best_case.time_to_intercept) {
          best_case = candidate;
          return;
        }
        if (candidate.time_to_intercept > best_case.time_to_intercept) {
          return;
        }
        if (candidate.fuel_total < best_case.fuel_total) {
          best_case = candidate;
          return;
        }
        if (candidate.fuel_total > best_case.fuel_total) {
          return;
        }
        if (candidate.num_orders < best_case.num_orders) {
          best_case = candidate;
        }
      };

      consider(case_2ai);
      consider(case_2aii);
      consider(case_2b);

      if (has_best_case) {
        return best_case;
      }

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
    }

    // If we got down here we couldn't find any way to intercept in time.
    return FAILURE_TRAJ;

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

    Collision detect_collisions_on_path(CShip* ship, CThing* thing, double time) {
        Collision collision;

        // TODO: Should check path from ship to target for:
        // - Other ships (enemy and friendly)
        // - Asteroids
        // - Account for object sizes/radii
        // - Use DetectCollisionCourse or similar method
        // Currently returns dummy collision that needs implementation
        collision.collision_thing = NULL;
        collision.collision_when = -1.0;
        collision.collision_where = CCoord(0, 0);

        return collision;
    }

} // End namespace Pathfinding
