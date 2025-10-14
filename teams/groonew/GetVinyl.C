#include "Asteroid.h"
#include "FuelTraj.h"
#include "GetVinyl.h"
#include "Groonew.h"
#include "MagicBag.h"
#include "Pathfinding.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "Thing.h"
#include "ParserModern.h"

#include <limits>

// External reference to global parser instance
extern CParser* g_pParser;

GetVinyl::GetVinyl() {}

GetVinyl::~GetVinyl() {}

namespace {
struct FacingTargets {
  CStation* station = NULL;
  double station_dist = std::numeric_limits<double>::max();
  CShip* ship = NULL;
  double ship_dist = std::numeric_limits<double>::max();
};

FacingTargets FindEnemyFacingTargets(CShip* ship) {
  FacingTargets targets;
  if (ship == NULL) {
    return targets;
  }
  CTeam* team = ship->GetTeam();
  if (team == NULL) {
    return targets;
  }
  CWorld* world = team->GetWorld();
  if (world == NULL) {
    return targets;
  }

  for (unsigned int idx = world->UFirstIndex; idx != (unsigned int)-1;
       idx = world->GetNextIndex(idx)) {
    CThing* thing = world->GetThing(idx);
    if (thing == NULL || thing == ship || !(thing->IsAlive())) {
      continue;
    }

    ThingKind kind = thing->GetKind();
    if (kind != STATION && kind != SHIP) {
      continue;
    }

    CTeam* thing_team = thing->GetTeam();
    if (thing_team == NULL) {
      continue;
    }
    if (thing_team->GetTeamNumber() == team->GetTeamNumber()) {
      continue;
    }

    if (!ship->IsFacing(*thing)) {
      continue;
    }

    double dist = ship->GetPos().DistTo(thing->GetPos());
    if (kind == STATION) {
      if (dist < targets.station_dist) {
        targets.station = static_cast<CStation*>(thing);
        targets.station_dist = dist;
      }
    } else {
      if (dist < targets.ship_dist) {
        targets.ship = static_cast<CShip*>(thing);
        targets.ship_dist = dist;
      }
    }
  }

  return targets;
}

double ComputeLaserFuelCost(double beam_length) {
  return beam_length / g_laser_range_per_fuel_unit;
}

double ComputeLaserDamage(double beam_length, double target_distance) {
  double extra_length = beam_length - target_distance;
  if (extra_length <= g_fp_error_epsilon) {
    return 0.0;
  }
  return extra_length *
         (g_laser_mass_scale_per_remaining_unit / g_laser_damage_mass_divisor);
}

void LogPotshotDecision(const CShip* shooter,
                        const CThing* target,
                        double distance,
                        double beam_length,
                        double expected_damage,
                        double fuel_cost,
                        double efficiency,
                        const char* reason) {
  if (!(g_pParser && g_pParser->verbose)) {
    return;
  }

  const CCoord shooter_pos = shooter->GetPos();
  const CCoord target_pos = target->GetPos();
  const char* target_kind =
      (target->GetKind() == STATION) ? "Station"
                                     : (target->GetKind() == SHIP) ? "Ship"
                                                                   : "Thing";

  printf("\t[Potshot] %s -> %s '%s'\n",
         shooter->GetName(),
         target_kind,
         target->GetName());
  printf("\t  shooter_pos(%.1f, %.1f) target_pos(%.1f, %.1f)\n",
         shooter_pos.fX, shooter_pos.fY,
         target_pos.fX, target_pos.fY);
  printf("\t  dist=%.1f beam=%.1f dmg=%.2f fuel=%.2f eff=%.2f : %s\n",
         distance,
         beam_length,
         expected_damage,
         fuel_cost,
         efficiency,
         reason);
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
  double cur_cargo = pShip->GetAmount(S_CARGO);
  double max_fuel = pShip->GetCapacity(S_FUEL);
  double max_cargo = pShip->GetCapacity(S_CARGO);

  // The approximate amount of shields needed to fend off one 
  // really bad collision and one maximum powered laser blast.
  double wanted_shields = 20.66;
  // We don't issue orders that would deplete this below 5
  // so we have enough to get home / get more fuel.
  double fuel_reserve = 5.0;

  // The stuff we're going to collide with in the next 1, 2, or 3 turns.
  std::vector<CThing *> t1_collisions;
  std::vector<CThing *> t2_collisions;
  std::vector<CThing *> t3_collisions;

  // Collision Handling. We can collide with multiple things in a turn however
  // we can only do one of: turn, thrust, or jettison once per turn
  //
  // We can shoot or manage shields every turn.
  if (!pShip->IsDocked()) {
    for (unsigned int thing_i = pmyWorld->UFirstIndex; thing_i <= pmyWorld->ULastIndex;
         thing_i = pmyWorld->GetNextIndex(thing_i)) {
      CThing *athing = pmyWorld->GetThing(thing_i);

      // Skip dead things,generic things, and things in the past, and ourself.
      if (athing == NULL || !(athing->IsAlive()) || athing == pShip) {
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
    emergency_orders = HandleImminentCollision(t1_collisions, 1, emergency_orders);
    emergency_orders = HandleImminentCollision(t2_collisions, 2, emergency_orders);
    emergency_orders = HandleImminentCollision(t3_collisions, 3, emergency_orders);

    if (emergency_orders.exclusive_order != O_ALL_ORDERS) {
      if (emergency_orders.exclusive_order == O_JETTISON) {
        pShip->SetJettison(VINYL, emergency_orders.exclusive_order_amount);
      }
    } else {
        pShip->SetOrder((OrderKind)emergency_orders.exclusive_order, emergency_orders.exclusive_order_amount);
    }

    if (emergency_orders.shield_order_amount > 0.0) {
      pShip->SetOrder(O_SHIELD, emergency_orders.shield_order_amount);
    }
    if (emergency_orders.laser_order_amount > 0.0) {
      pShip->SetOrder(O_LASER, emergency_orders.laser_order_amount);
    }
  }

  // TODO: Take potshots at enemy ships and stations.
  if (pShip->GetOrder(O_LASER) == 0.0) {
    const double laser_damage_per_unit =
        g_laser_mass_scale_per_remaining_unit / g_laser_damage_mass_divisor;

    FacingTargets facing_targets = FindEnemyFacingTargets(pShip);

    double available_fuel = pShip->GetAmount(S_FUEL) - fuel_reserve;

    if (available_fuel > g_fp_error_epsilon) {
      double max_beam_length =
          min(512.0, available_fuel * g_laser_range_per_fuel_unit);
      if (max_beam_length > g_fp_error_epsilon) {
        // Station potshots: simplified logic
        if (facing_targets.station != NULL) {
          double distance_to_target = facing_targets.station_dist;
          if (distance_to_target <= max_beam_length + g_fp_error_epsilon) {
            CStation *enemy_station = facing_targets.station;
            double station_vinyl = enemy_station->GetVinylStore();
            double max_extra = max_beam_length - distance_to_target;
            double max_damage = max_extra * laser_damage_per_unit;

            if (station_vinyl > g_fp_error_epsilon && max_extra > g_fp_error_epsilon) {
              // A. Can we destroy all vinyl?
              bool can_destroy_all = (max_damage >= station_vinyl);
              if (can_destroy_all) {
                double beam_length = distance_to_target + (station_vinyl / laser_damage_per_unit);
                double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                double fuel_cost = ComputeLaserFuelCost(beam_length);
                double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                LogPotshotDecision(pShip, enemy_station, distance_to_target, beam_length,
                                  expected_damage, fuel_cost, efficiency, "fire (destroy all vinyl)");
                pShip->SetOrder(O_LASER, beam_length);
              }
              // B. Can we damage vinyl with good efficiency (beam >= 3x distance)?
              else {
                double beam_length = max_beam_length;
                bool good_efficiency = (beam_length >= 3.0 * distance_to_target);
                if (good_efficiency) {
                  double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                  double fuel_cost = ComputeLaserFuelCost(beam_length);
                  double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                  LogPotshotDecision(pShip, enemy_station, distance_to_target, beam_length,
                                    expected_damage, fuel_cost, efficiency, "fire (partial damage)");
                  pShip->SetOrder(O_LASER, beam_length);
                } else {
                  double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                  double fuel_cost = ComputeLaserFuelCost(beam_length);
                  double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                  LogPotshotDecision(pShip, enemy_station, distance_to_target, beam_length,
                                    expected_damage, fuel_cost, efficiency, "skip (poor efficiency)");
                }
              }
            }
          }
        }

        // Ship potshots: simplified logic
        if (pShip->GetOrder(O_LASER) == 0.0 &&
            facing_targets.ship != NULL) {
          double distance_to_target = facing_targets.ship_dist;
          if (distance_to_target + g_fp_error_epsilon < max_beam_length) {
            CShip *enemy_ship = facing_targets.ship;
            double enemy_shield = enemy_ship->GetAmount(S_SHIELD);
            double max_extra = max_beam_length - distance_to_target;
            double max_damage = max_extra * laser_damage_per_unit;

            if (max_damage > g_fp_error_epsilon) {
              const double kill_margin = 0.01;

              // A. Can we kill them?
              bool can_kill = (max_damage >= enemy_shield + kill_margin);
              if (can_kill) {
                double damage_to_kill = enemy_shield + kill_margin;
                double beam_length = distance_to_target + (damage_to_kill / laser_damage_per_unit);
                double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                double fuel_cost = ComputeLaserFuelCost(beam_length);
                double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                LogPotshotDecision(pShip, enemy_ship, distance_to_target, beam_length,
                                  expected_damage, fuel_cost, efficiency, "fire (kill)");
                pShip->SetOrder(O_LASER, beam_length);
              }
              // B. Can we get them below 6.0 with good efficiency (beam >= 3x distance)?
              else if (enemy_shield > 6.0) {
                double min_damage_to_cross = enemy_shield - 6.0 + kill_margin;
                bool can_cross_threshold = (max_damage >= min_damage_to_cross);
                if (can_cross_threshold) {
                  // Deal max damage (not just min to cross 6.0)
                  double beam_length = max_beam_length;
                  bool good_efficiency = (beam_length >= 3.0 * distance_to_target);
                  if (good_efficiency) {
                    double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                    double fuel_cost = ComputeLaserFuelCost(beam_length);
                    double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                    LogPotshotDecision(pShip, enemy_ship, distance_to_target, beam_length,
                                      expected_damage, fuel_cost, efficiency, "fire (force dock)");
                    pShip->SetOrder(O_LASER, beam_length);
                  } else {
                    double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                    double fuel_cost = ComputeLaserFuelCost(beam_length);
                    double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                    LogPotshotDecision(pShip, enemy_ship, distance_to_target, beam_length,
                                      expected_damage, fuel_cost, efficiency, "skip (poor efficiency)");
                  }
                } else {
                  // Can't even get them below 6.0
                  double beam_length = max_beam_length;
                  double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                  double fuel_cost = ComputeLaserFuelCost(beam_length);
                  double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                  LogPotshotDecision(pShip, enemy_ship, distance_to_target, beam_length,
                                    expected_damage, fuel_cost, efficiency, "skip (insufficient damage)");
                }
              } else {
                // Enemy already below 6.0, can't kill
                double beam_length = max_beam_length;
                double expected_damage = ComputeLaserDamage(beam_length, distance_to_target);
                double fuel_cost = ComputeLaserFuelCost(beam_length);
                double efficiency = (fuel_cost > g_fp_error_epsilon) ? (expected_damage / fuel_cost) : 0.0;
                LogPotshotDecision(pShip, enemy_ship, distance_to_target, beam_length,
                                  expected_damage, fuel_cost, efficiency, "skip (already vulnerable)");
              }
            }
          }
        }
      }
    }
  }

  if (pShip->GetOrder(O_SHIELD) == 0.0) {
    // PHASE 3: SHIELD MAINTENANCE
    // Calculate total fuel that will be used this turn
    double fuel_used = pShip->GetOrder(O_SHIELD) + pShip->GetOrder(O_LASER) +
                       pShip->GetOrder(O_THRUST) + pShip->GetOrder(O_TURN) +
                       pShip->GetOrder(O_JETTISON);
    cur_fuel -= fuel_used;

    // Maintain minimum shield buffer of 11 units
    if (cur_shields < wanted_shields) {
      cur_fuel -= fuel_reserve;  // Reserve some emergency fuel
      double shields_order = wanted_shields - cur_shields;
      // Add shields up to desired level or available fuel
      pShip->SetOrder(O_SHIELD,
                      (shields_order < cur_fuel) ? shields_order : cur_fuel);
    }
  }

  // END OF DECISION LOGIC
}


// The idiom here is that we never overwrite orders that are already set - if they
// are set they pertain to something more critical or something happening sooner.
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

    double fuel_allowed = pShip->GetAmount(S_FUEL) - 5.0;
    if (fuel_allowed < 0.0) {
      fuel_allowed = 0.0;
    }
    
    // Asteroids have NULL team number and aren't enemies.
    bool order_allowed = (emergency_orders.exclusive_order == O_ALL_ORDERS);
    bool shield_allowed = (emergency_orders.shield_order_amount == 0.0);
    bool laser_allowed = ((fuel_allowed > 0.0) && (emergency_orders.laser_order_amount == 0.0));
    bool is_asteroid = (athing->GetKind() == ASTEROID);
    bool is_vinyl = (is_asteroid && (((CAsteroid*)athing)->GetMaterial() == VINYL));
    bool is_uranium = (is_asteroid && (((CAsteroid*)athing)->GetMaterial() == URANIUM));
    bool is_station = (athing->GetKind() == STATION);
    bool is_ship = (athing->GetKind() == SHIP);
    bool is_enemy = (!is_asteroid && ((athing)->GetTeam()->GetTeamNumber() != pmyTeam->GetTeamNumber()));

    bool enemy_cargo = (
      (is_enemy && is_ship && (((CShip*)athing)->GetAmount(S_CARGO) > 0.01))
      || (is_enemy && is_station && (((CStation*)athing)->GetVinylStore() > 0.01))
    );
    double enemy_cargo_amount = 0.0;
    if (enemy_cargo) {
      if (is_ship) {
        enemy_cargo_amount = ((CShip*)athing)->GetAmount(S_CARGO);
      } else if (is_station) {
        enemy_cargo_amount = ((CStation*)athing)->GetVinylStore();
      }
    }

    double asteroid_mass = (is_asteroid ? athing->GetMass() : 0.0);


    // You can't jettison less than the minimum asteroid size.
    bool have_cargo = (pShip->GetAmount(S_CARGO) >= g_thing_minmass);


    // Handle enemy stations.
    if (is_enemy && is_station) {
      if (have_cargo && order_allowed) {
        if (turns == 1) {
          // Dump cargo.
          double cur_cargo = pShip->GetAmount(S_CARGO);
          char shipmsg[256];
          snprintf(shipmsg, sizeof(shipmsg), "%s: Jabba will not take kindly to this!\n", pShip->GetName());
          strncat(pShip->GetTeam()->MsgText, shipmsg, 
                  maxTextLen - strlen(pShip->GetTeam()->MsgText) - 1);
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ Jettisoning %.1f vinyl near enemy station\n", cur_cargo);
          }
          pShip->SetJettison(VINYL, cur_cargo);
          emergency_orders.exclusive_order = O_JETTISON;
          emergency_orders.exclusive_order_amount = cur_cargo;
          order_allowed = false;
        } else {
          // Face opposite of the station for dumping cargo in a second.
          double intercept_angle = pShip->GetPos().AngleTo(athing->GetPos());
          double turn_angle = intercept_angle - pShip->GetOrient();
          emergency_orders.exclusive_order = O_TURN;
          emergency_orders.exclusive_order_amount = turn_angle;
          order_allowed = false;
        }
      }

      // DEBUG - We should check if we're going to turn before shooting so we don't waste
      // bullets - but for now leave this as is and see how it works.
      if (enemy_cargo && laser_allowed) {
        // Check if we'll hit it.
        if (pShip->IsFacing(*athing)) {
          CTraj laser_dist = pShip->GetPos().VectTo(athing->GetPos());

          // We can't set a beam length more than min(512, fuel_allows*50)
          
          // We'll destroy an amount of cargo roughly equal to:
          // 30*(beam length - laser_dist.rho) / 1000
          double max_useful_beam_length = laser_dist.rho + (enemy_cargo_amount * 1000.0 / 30.0);

          double laser_order = min(512.0, fuel_allowed * g_laser_range_per_fuel_unit);
          laser_order = min(laser_order, max_useful_beam_length);
          emergency_orders.laser_order_amount = laser_order;
          laser_allowed = false;
        }
      }
    }

    // Handle enemy ships.
    // TODO: For now we'll handle this in a more general potshot taking 
    // logic not tied to emergency maneuvers.

    // Handle Uranium asteroids.
    if (is_uranium) {

      if (asteroid_mass <= max_fuel) {
        if (shield_allowed) {
          // uranium less than max fuel
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ Using shields to absorb %.1f uranium\n",
                   athing->GetMass() - (max_fuel - cur_fuel));
          }
          double shield_order = asteroid_mass - (max_fuel - cur_fuel);
          emergency_orders.shield_order_amount = shield_order;
          shield_allowed = false;
        }
      } else {
        // uranium greater than max fuel
        if (g_pParser && g_pParser->verbose) {
          printf("\t→ CONSIDERING Shooting %.1f uranium\n", asteroid_mass);
        }
        if (laser_allowed) {
          if (g_pParser && g_pParser->verbose) {
            printf("\t→ CONSIDERING-LASER ALLOWED Shooting %.1f uranium\n", asteroid_mass);
          }
          if (pShip->IsFacing(*athing)) {
            if (g_pParser && g_pParser->verbose) {
              printf("\t→ CONSIDERING-LASER ALLOWED-FACING Shooting %.1f uranium\n", asteroid_mass);
            }
            CTraj laser_dist = pShip->GetPos().VectTo(athing->GetPos());

            // We can't set a beam length more than min(512, fuel_allows*50)
          
            // We need 30*(beam length - laser_dist.rho) to be > 1000.0.
            // We'll make it ~= 1060.0.
            double desired_beam_length = laser_dist.rho + (1060.0 / 30.0);

            if ((desired_beam_length <= 512.0) && ((fuel_allowed * g_laser_range_per_fuel_unit) >= desired_beam_length)) {
              emergency_orders.laser_order_amount = desired_beam_length;
              laser_allowed = false;
              if (g_pParser && g_pParser->verbose) {
                printf("\t→ Shooting %.1f uranium\n", asteroid_mass);
              }
            }
          }
       }
      }
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
