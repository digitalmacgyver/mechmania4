#include "Asteroid.h"
#include "FuelTraj.h"
#include "GetVinyl.h"
#include "Groonew.h"
#include "MagicBag.h"
#include "LaserUtils.h"
#include "Pathfinding.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "Thing.h"
#include "World.h"
#include "ParserModern.h"
#include "GameConstants.h"

#include <algorithm>
#include <limits>
#include <cmath> // Required for fabs

// External reference to global parser instance
extern CParser* g_pParser;

GetVinyl::GetVinyl() {}

GetVinyl::~GetVinyl() {}

namespace {

using groonew::laser::BeamLengthForExactDamage;
using groonew::laser::ClampBeamToRange;
using groonew::laser::ComputeBeamLengthFromFuel;
using groonew::laser::FutureLineOfFire;
using groonew::laser::NormalizeAngle;

// Helper function to apply emergency orders
void ApplyEmergencyOrders(CShip* ship, const EmergencyOrders& orders) {
  if (orders.exclusive_order != O_ALL_ORDERS) {
    if (orders.exclusive_order == O_JETTISON) {
      // Assumes VINYL based on HandleImminentCollision logic.
      ship->SetJettison(VINYL, orders.exclusive_order_amount);
    } else {
      ship->SetOrder(static_cast<OrderKind>(orders.exclusive_order),
                     orders.exclusive_order_amount);
    }
  }

  if (orders.shield_order_amount > 0.0) {
    ship->SetOrder(O_SHIELD, orders.shield_order_amount);
  }
  if (orders.laser_order_amount > 0.0) {
    ship->SetOrder(O_LASER, orders.laser_order_amount);
  }
}
}  // namespace

void GetVinyl::Decide() {
  // Strategic planning has already been done in Groonew::AssignShipOrders()
  // Only override orders if we locked them due to collision handling above We
  // rely on these properties of SetOrder/SetJettison: they clear incompatible
  // thurst/turn/jettison orders. We rely on the fact that shooting and shields
  // can happen in parallel with navigational orders.

  // can't fire and drive cause of alcohol breath
  CTeam *pmyTeam = pShip->GetTeam();
  CWorld *pmyWorld = pmyTeam->GetWorld();

  // Verbose logging header
  if (g_pParser && g_pParser->verbose) {
    printf("t=%.1f\t%s:\n", pmyWorld->GetGameTime(), pShip->GetName());
  }

  double cur_shields = pShip->GetAmount(S_SHIELD);
  double cur_fuel = pShip->GetAmount(S_FUEL);
  // double cur_cargo = pShip->GetAmount(S_CARGO); // Unused in Decide, used in HandleImminentCollision
  // double max_fuel = pShip->GetCapacity(S_FUEL); // Unused
  // double max_cargo = pShip->GetCapacity(S_CARGO); // Unused

  // Check resource availability for shield strategy
  Groonew* groonew_team = static_cast<Groonew*>(pmyTeam);
  bool no_vinyl_free = (fabs(groonew_team->vinyl_left) <= g_fp_error_epsilon);
  bool no_resources_free = (fabs(groonew_team->uranium_left) <= g_fp_error_epsilon &&
                            fabs(groonew_team->vinyl_left) <= g_fp_error_epsilon);

  // Shield maintenance strategy based on game phase:
  // - Normal (resources available): 20.66 shields (protect against collisions + laser)
  // - Mid-game (no vinyl): 12.5 shields (lighter protection, more fuel for combat)
  // - End-game (no resources): 0.0 shields (all fuel to weapons)
  double wanted_shields = 20.66;
  if (no_resources_free) {
    wanted_shields = 0.0;
  } else if (no_vinyl_free) {
    wanted_shields = 12.5;
  }
  // We don't issue orders that would deplete this below 5
  // so we have enough to get home / get more fuel.
  double fuel_reserve = groonew::constants::FUEL_RESERVE;

  // The stuff we're going to collide with in the next 1, 2, or 3 turns.
  std::vector<CThing *> t1_collisions;
  std::vector<CThing *> t2_collisions;
  std::vector<CThing *> t3_collisions;

  // Collision Handling. We can collide with multiple things in a turn however
  // we can only do one of: turn, thrust, or jettison once per turn
  //
  // We can shoot or manage shields every turn.
  if (!pShip->IsDocked()) {
    for (unsigned int idx = pmyWorld->UFirstIndex; 
        idx != BAD_INDEX; 
        idx = pmyWorld->GetNextIndex(idx)) {
    
      CThing* athing = pmyWorld->GetThing(idx);
    
      // Always check both null and alive
      if (!athing || !athing->IsAlive()) {
        continue;
      }

      // Skip ourself.
      if (athing == pShip) {
        continue;
      }

      ThingKind kind = athing->GetKind();
      if (kind == GENTHING) {
        continue;
      }

      double turns = pShip->DetectCollisionCourse(*athing);
      // -1 indicates no collision detected.
      if (turns < 0.0) {
        continue;
      }

      if (turns < 1.0) {
        t1_collisions.push_back(athing);
      } else if (turns < 2.0) {
        t2_collisions.push_back(athing);
      } else if (turns < 3.0) {
        t3_collisions.push_back(athing);
      }
    }

    EmergencyOrders emergency_orders;
    emergency_orders =
        HandleImminentCollision(t1_collisions, 1, emergency_orders);
    emergency_orders =
        HandleImminentCollision(t2_collisions, 2, emergency_orders);
    emergency_orders =
        HandleImminentCollision(t3_collisions, 3, emergency_orders);

    ApplyEmergencyOrders(pShip, emergency_orders);
  }

  // TODO: Take potshots at enemy ships and stations.
  // PHASE 2: Opportunistic Firing (Potshots)
  // Refactored: Uses centralized utilities from TrenchRun.
  if (pShip->GetOrder(O_LASER) == 0.0) {
    // Calculate available laser resources
    groonew::laser::LaserResources laser =
        groonew::laser::ComputeLaserResources(pShip, fuel_reserve);
        
    if (laser.max_beam_length > g_fp_error_epsilon) {
      // Find targets in the line of fire using the centralized utility
      TrenchRun::FacingTargets facing_targets = TrenchRun::FindEnemyFacingTargets(pShip);

      // Prioritize station targets
      if (facing_targets.station != NULL) {
        // Use the centralized tactical decision maker
        TrenchRun::TryStationPotshot(laser,
                                     pShip,
                                     facing_targets.station,
                                     facing_targets.station_dist);
      }

      // If still haven't fired, try ship targets
      if (pShip->GetOrder(O_LASER) == 0.0 && facing_targets.ship != NULL) {
        TrenchRun::TryShipPotshot(laser,
                                  pShip,
                                  facing_targets.ship,
                                  facing_targets.ship_dist);
      }
    }
  }

  // PHASE 3: SHIELD MAINTENANCE
  if (pShip->GetOrder(O_SHIELD) == 0.0) {
    // Calculate total fuel that will be used this turn

    double fuel_used = 0.0;

    // Check existing orders and estimate their fuel consumption using SetOrder.
    if (pShip->GetOrder(O_SHIELD) > 0.0 + g_fp_error_epsilon) {
      fuel_used += pShip->SetOrder(O_SHIELD, pShip->GetOrder(O_SHIELD));    
    }
    if (pShip->GetOrder(O_LASER) > 0.0 + g_fp_error_epsilon) {
      fuel_used += pShip->SetOrder(O_LASER, pShip->GetOrder(O_LASER));
    }
    if (fabs(pShip->GetOrder(O_THRUST)) > 0.0 + g_fp_error_epsilon) {
      fuel_used += pShip->SetOrder(O_THRUST, pShip->GetOrder(O_THRUST));
    }
    if (fabs(pShip->GetOrder(O_TURN)) > 0.0 + g_fp_error_epsilon) {
      fuel_used += pShip->SetOrder(O_TURN, pShip->GetOrder(O_TURN));
    }

    // Calculate remaining fuel after accounting for orders already set
    double remaining_fuel = cur_fuel - fuel_used;

    // Maintain minimum shield buffer
    if (cur_shields < wanted_shields) {
      double available_fuel = remaining_fuel - fuel_reserve;  // Reserve emergency fuel
      if (available_fuel > 0) {
          double shields_order = wanted_shields - cur_shields;
          // Add shields up to desired level or available fuel
          pShip->SetOrder(O_SHIELD, std::min(shields_order, available_fuel));
      }
    }
  }

  // END OF DECISION LOGIC
}


// HandleImminentCollision implementation remains functionally unchanged, with comments restored.
EmergencyOrders GetVinyl::HandleImminentCollision(std::vector<CThing *> collisions, unsigned int turns, EmergencyOrders emergency_orders) {

  CTeam *pmyTeam = pShip->GetTeam();

  double cur_shields = pShip->GetAmount(S_SHIELD);
  double cur_fuel = pShip->GetAmount(S_FUEL);
  double cur_cargo = pShip->GetAmount(S_CARGO);
  double max_fuel = pShip->GetCapacity(S_FUEL);
  double max_cargo = pShip->GetCapacity(S_CARGO);


  for (CThing *athing : collisions) {

    ThingKind kind = athing->GetKind();
    if (g_pParser && g_pParser->verbose) {
      printf("\tCollision in %d turns with ", turns);
      if (kind == SHIP) {
        printf("ship '%s'\n", ((CShip*)athing)->GetName());
      } else if (kind == STATION) {
        printf("station '%s'\n", ((CStation*)athing)->GetName());
      } else if (kind == ASTEROID) {
        CAsteroid* ast = (CAsteroid*)athing;
        printf("asteroid %s %.1f tons\n",
               (ast->GetMaterial() == VINYL) ? "vinyl" : "uranium",
               ast->GetMass());
      } else {
        printf("object kind %d\n", kind);
      }
    }

    double fuel_allowed =
        pShip->GetAmount(S_FUEL) - groonew::constants::FUEL_RESERVE;
    if (fuel_allowed < 0.0) {
      fuel_allowed = 0.0;
    }
    
    // Check if exclusive orders (Turn/Thrust/Jettison) are already set
    // The idiom here is that we never overwrite orders that are already set (priority to sooner collisions)
    bool order_allowed = (emergency_orders.exclusive_order == O_ALL_ORDERS);
    bool shield_allowed = (emergency_orders.shield_order_amount == 0.0);
    bool laser_allowed = ((fuel_allowed > 0.0) && (emergency_orders.laser_order_amount == 0.0));
    
    // Categorize the colliding object
    bool is_asteroid = (athing->GetKind() == ASTEROID);
    bool is_vinyl = (is_asteroid && (((CAsteroid*)athing)->GetMaterial() == VINYL));
    bool is_uranium = (is_asteroid && (((CAsteroid*)athing)->GetMaterial() == URANIUM));
    bool is_station = (athing->GetKind() == STATION);
    bool is_ship = (athing->GetKind() == SHIP);
    
    // Check if it's an enemy (Asteroids have NULL team)
    bool is_enemy = false;
    if (!is_asteroid && athing->GetTeam() != NULL) {
        is_enemy = ((athing)->GetTeam()->GetTeamNumber() != pmyTeam->GetTeamNumber());
    }

    // Check if the enemy has cargo/vinyl
    bool enemy_cargo = false;
    double enemy_cargo_amount = 0.0;
    if (is_enemy) {
        if (is_ship) {
            enemy_cargo_amount = ((CShip*)athing)->GetAmount(S_CARGO);
        } else if (is_station) {
            enemy_cargo_amount = ((CStation*)athing)->GetVinylStore();
        }
        // Original check used 0.01 threshold
        enemy_cargo = (enemy_cargo_amount > 0.01);
    }

    // Check resource availability for shield strategy
    Groonew* groonew_team = static_cast<Groonew*>(pmyTeam);
    bool world_has_vinyl = (fabs(groonew_team->vinyl_left) > g_fp_error_epsilon);
    bool world_has_uranium = (fabs(groonew_team->uranium_left) > g_fp_error_epsilon);

    double asteroid_mass = (is_asteroid ? athing->GetMass() : 0.0);

    // You can't jettison less than the minimum asteroid size.
    bool have_cargo = (pShip->GetAmount(S_CARGO) >= g_thing_minmass);

    // Handle enemy stations.
    if (is_enemy && is_station) {
      if (have_cargo && order_allowed) {
        if (turns == 1) {
          // Dump cargo.
          double cur_cargo_now = pShip->GetAmount(S_CARGO);
          char shipmsg[256];
          snprintf(shipmsg, sizeof(shipmsg), "%s: Jabba will not take kindly to this!\n", pShip->GetName());
          strncat(pShip->GetTeam()->MsgText, shipmsg, 
                  maxTextLen - strlen(pShip->GetTeam()->MsgText) - 1);
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ Jettisoning %.1f vinyl near enemy station\n", cur_cargo_now);
          }
          pShip->SetJettison(VINYL, cur_cargo_now);
          emergency_orders.exclusive_order = O_JETTISON;
          emergency_orders.exclusive_order_amount = cur_cargo_now;
          order_allowed = false;
        } else {
          // Face opposite of the station for dumping cargo in a second.
          double intercept_angle = pShip->GetPos().AngleTo(athing->GetPos());
          double turn_angle = intercept_angle - pShip->GetOrient();
          turn_angle = NormalizeAngle(turn_angle);
          emergency_orders.exclusive_order = O_TURN;
          emergency_orders.exclusive_order_amount = turn_angle;
          order_allowed = false;
        }
      }

      if (enemy_cargo && laser_allowed) {
        double future_distance = 0.0;  // Updated by FutureLineOfFire on success.
        if (FutureLineOfFire(pShip, athing, &future_distance)) {
          double max_useful_beam_length =
              BeamLengthForExactDamage(future_distance, enemy_cargo_amount);

          double laser_order = ClampBeamToRange(
              ComputeBeamLengthFromFuel(fuel_allowed));
          laser_order = min(laser_order, max_useful_beam_length);
          emergency_orders.laser_order_amount = laser_order;
          laser_allowed = false;
        }
      }
    }

    // We handle shooting enemy ships in the general potshot taking logic,
    // not here.

    // Handle Uranium asteroids.
    if (is_uranium) {

      if (asteroid_mass <= max_fuel) {
        // Note - when there is no vinyl free in the game world we choose
        // not to boost our shields to completely consume breakable, eatable
        // asteroids, prefering to split them by ramming so we keep that
        // extra uranium in the game world for fuel and lasers.
        bool endgame_shield_management = (!world_has_vinyl && ((asteroid_mass / g_asteroid_split_child_count) >= g_thing_minmass));
        if (shield_allowed && !endgame_shield_management) {
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ Using shields to absorb %.1f uranium\n",
                   athing->GetMass() - (max_fuel - cur_fuel));
          }
          double shield_order = asteroid_mass - (max_fuel - cur_fuel);
          emergency_orders.shield_order_amount = shield_order;
          shield_allowed = false;
        }
      } 
      /* We used to have logic here to shoot asteroids to break them up,
      however the expected fuel cost to break up the asteroid is around 
      2.2, and the expected damage to shields is around 0.6. In nearly 
      the worst case scenario we'd take around 3 damage from a collision.
      So it's not worth the fuel to break up the asteroid.
      else {
        // uranium greater than max fuel
        if (g_pParser && g_pParser->verbose) {
          printf("\t→ CONSIDERING Shooting %.1f uranium\n", asteroid_mass);
        }
        if (laser_allowed) {
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ CONSIDERING-LASER ALLOWED Shooting %.1f uranium\n", asteroid_mass);
          }
          double future_distance = 0.0;  // Updated by FutureLineOfFire on success.
          if (FutureLineOfFire(pShip, athing, &future_distance)) {
            if (g_pParser && g_pParser->verbose) {
              printf("\t→ CONSIDERING-LASER ALLOWED-FACING Shooting %.1f uranium\n", asteroid_mass);
            }

            // We need 30*(beam length - distance) to be > 1000.0.
            // We'll make it ~= 1060.0.
            double desired_beam_length = future_distance + (1060.0 / 30.0);

            if ((desired_beam_length <= 512.0) &&
                ((fuel_allowed * g_laser_range_per_fuel_unit) >= desired_beam_length)) {
              emergency_orders.laser_order_amount = desired_beam_length;
              laser_allowed = false;
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ Shooting %.1f uranium\n", asteroid_mass);
              }
            }
          }
        }
      }*/
    }

    // TODO: For us this isn't a big deal as we can hold max size asteroid, and tend
    // to return home soon enough. Implementation here would be more important for
    // smaller vinyl capacities.
    // Handle Vinyl asteroids.
    if (is_vinyl) {
      if ((athing->GetMass() <= max_cargo)) {
        if (athing->GetMass() >= (max_cargo - cur_cargo)) {
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
  }

  return emergency_orders;
}
