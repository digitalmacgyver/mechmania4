#include "Asteroid.h"
#include "FuelTraj.h"
#include "GetVinyl.h"
#include "Groonew.h"
#include "MagicBag.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "Thing.h"
#include "ParserModern.h"

// External reference to global parser instance
extern CParser* g_pParser;

GetVinyl::GetVinyl() {}

GetVinyl::~GetVinyl() {}

void GetVinyl::Decide() {
  pShip->ResetOrders();

  // can't fire and drive cause of alcohol breath
  CTeam *pmyTeam = pShip->GetTeam();
  CWorld *pmyWorld = pmyTeam->GetWorld();

  // Verbose logging header
  if (g_pParser && g_pParser->verbose) {
    printf("t=%.1f\t%s:\n", pmyWorld->GetGameTime(), pShip->GetName());
  }

  unsigned int shipnum = pShip->GetShipNumber();
  MagicBag *mbp = ((Groonew *)pmyTeam)->mb;

  double cur_shields = pShip->GetAmount(S_SHIELD);
  double cur_fuel = pShip->GetAmount(S_FUEL);
  double cur_cargo = pShip->GetAmount(S_CARGO);
  double max_fuel = pShip->GetCapacity(S_FUEL);
  double max_cargo = pShip->GetCapacity(S_CARGO);

  bool lock_orders = false;

  // Collision Handling.
  for (unsigned int thing_i = pmyWorld->UFirstIndex; thing_i <= pmyWorld->ULastIndex;
       thing_i = pmyWorld->GetNextIndex(thing_i)) {
    CThing *athing = pmyWorld->GetThing(thing_i);
    if (athing == NULL || !(athing->IsAlive())) {
      continue;
    }

    ThingKind kind = athing->GetKind();
    if (kind == GENTHING) {
      continue;
    }

    double turns = pShip->DetectCollisionCourse(*athing);
    if (turns < 0.0) {
      continue;
    }

    // Verbose collision logging - only for imminent collisions and when not docked
    if (g_pParser && g_pParser->verbose && turns < 3.0 && !pShip->IsDocked()) {
      printf("\tCollision in %.1f turns with ", turns);
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

    // Collision next turn TODO: Improve this collision logic - we might collide
    // with many things, we probably only want to worry about the first one -
    // but as it is we'll lock orders and maybe break if we see a colllision out
    // 3 turns and there is a later object we'd collide with first but we
    // haven't evaluated it yet.
    if (turns < 2.0) {
      if (((kind == STATION) &&
           (((CStation *)athing)->GetTeam()->GetTeamNumber() !=
            pmyTeam->GetTeamNumber())) &&
          (pShip->GetAmount(S_CARGO) > 0.01)) {
        // if its enemy base and we have vinyl
        printf("Jabba will not take kindly to this!\n");
        if (g_pParser && g_pParser->verbose) {
          printf("\t→ Jettisoning %.1f vinyl near enemy station\n", cur_cargo);
        }
        pShip->SetJettison(VINYL, cur_cargo);
        lock_orders = true;
      } else if (kind == ASTEROID) {
        if ((((CAsteroid *)athing)->GetMaterial() == URANIUM)) {
          if (athing->GetMass() <= max_fuel) {
            // uranium less than max fuel
            if (g_pParser && g_pParser->verbose) {
              printf("\t→ Using shields to absorb %.1f uranium\n",
                     athing->GetMass() - (max_fuel - cur_fuel));
            }
            pShip->SetOrder(O_SHIELD,
                            athing->GetMass() - (max_fuel - cur_fuel));
            lock_orders = true;
          } else {
            // uranium greater than max fuel
            // WRITE this later! (shoot the asteroid)
          }
        } else {
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
      } else if (kind == SHIP) {
        if ((((CShip *)athing)->GetTeam()->GetTeamNumber() !=
             pmyTeam->GetTeamNumber())) {
          // enemy ship
          //  WRITE this later! (shoot it)
        } else {
          // our ship
          //  back away from the keyboard
          //	  pShip->SetOrder(O_THRUST, -1);
          //       lock_orders=true;
        }
      }
      break;
    } else if (turns < 3.0) {
      if (((kind == STATION) &&
           (((CStation *)athing)->GetTeam()->GetTeamNumber() !=
            pmyTeam->GetTeamNumber())) &&
          (pShip->GetAmount(S_CARGO) > 0.01)) {
        // if its enemy base and we have vinyl
        // turn away for jettison
        printf("Turning away from their base!\n");
        if (g_pParser && g_pParser->verbose) {
          printf("\t→ Turning away from enemy station (π radians)\n");
        }
        pShip->SetOrder(O_TURN, PI);  // should check where we are pointing
        lock_orders = true;
      } else if (kind == ASTEROID) {
        if ((((CAsteroid *)athing)->GetMaterial() == URANIUM)) {
          if (athing->GetMass() <= max_fuel) {
            // uranium less than max fuel, do nothing
          } else {
            // uranium greater than max fuel
            // SHOOT IT (later)
          }
        } else {
          if ((athing->GetMass() <= max_cargo)) {
            if (athing->GetMass() >= (max_cargo - cur_cargo)) {
              // fits in cargo hold but we're holding too much, do nothing
            } else {
              // vinyl fits, just ram it, do nothing
            }
          } else {
            // vinyl doesn't fit in cargo hold
            // SHOOT IT (later)
          }
        }
      } else if (kind == SHIP) {
        if ((((CShip *)athing)->GetTeam()->GetTeamNumber() !=
             pmyTeam->GetTeamNumber())) {
          // enemy ship
          //  SHOOT IT (later)
        } else {
          // our ship, do nothing
        }
      }
      break;
    }
  }

  // If we've not locked orders due to collision logic above, decide on orders.
  if (!lock_orders) {
    AsteroidKind prefered_asteroid = VINYL;
    // TODO: Add some logic for when neither is available - e.g. return to base
    // with cargo or attack enemy station/ship with cargo.
    bool uranium_available = (((Groonew *)pmyTeam)->uranium_left > 0.0);
    bool vinyl_available = (((Groonew *)pmyTeam)->vinyl_left > 0.0);
    if (!vinyl_available || (cur_fuel <= 5.0 && uranium_available)) {
      prefered_asteroid = URANIUM;
    }

    if ((pShip->GetAmount(S_CARGO) >= (2*40/3 - g_fp_error_epsilon)) ||
        (!vinyl_available && pShip->GetAmount(S_CARGO) > 0.01)) {
      // Return to base if we've got at least 2 medium sized asteroids in cargo,
      // or if there's no vinyl available and we've got any cargo.
      if (g_pParser && g_pParser->verbose) {
        printf("\t→ Returning to base (cargo=%.1f)\n", cur_cargo);
      }
      for (unsigned int j = 0; j < 50; ++j) {
        FuelTraj ft = ((Groonew *)pmyTeam)
                          ->determine_orders(pmyTeam->GetStation(), j, pShip);
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
    } else {
      // Harvest resources case.

      // Our best target.
      Entry *best_e = NULL;
    

      
      // TODO: Our basic deconflicting logic doesn't work, because it's
      // deconflicting on Entries, which aren't share anyway - it needs to
      // deconflict on targets.

      // Targets can only be uniquely identified by their operator==/!=, so we
      // need to save the targets we're trying to deconflict, or dynamically pull
      // them out of Entry->thing.

      unsigned int i = 0;
      for (Entry* e = mbp->getEntry(shipnum, 0); e != NULL;
           e = mbp->getEntry(shipnum, i), i++) {
        if (e->thing != NULL) {
          // TODO: This is the simplest of deconfliction - we just greedily hand
          // out claims. Next refinements: 
          // A. Give claim to ship with shortest time to intercept preferentially.
          // B. Do some kind of planning stability so ships don't switch targets
          // unless there is a clearly better (e.g. 1 turn sooner or better size
          // option).
          // DEBUG this doesn't work yet.
          bool already_claimed = false; //(e->claimed_by_mech == 1);
          bool is_asteroid = (e->thing->GetKind() == ASTEROID);
          // Note: Can't call GetMaterial on non-asteroids.
          bool is_prefered_asteroid = (is_asteroid && (static_cast<CAsteroid*>(e->thing)->GetMaterial() == prefered_asteroid));
 
          if (already_claimed || !is_asteroid || !is_prefered_asteroid) {
            continue;
          }

          if ((best_e == NULL) || (e->turns_total < best_e->turns_total)) {
            best_e = e;
          }
        }
      }

      if (best_e != NULL) {
        if (g_pParser && g_pParser->verbose) {
          CThing* target = best_e->thing;
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
                 best_e->turns_total,
                 ((best_e->fueltraj).order_kind == O_THRUST) ? "thrust" :
                 ((best_e->fueltraj).order_kind == O_TURN) ? "turn" : "other/none",
                 (best_e->fueltraj).order_mag);
        }

        pShip->SetOrder((best_e->fueltraj).order_kind,
                        (best_e->fueltraj).order_mag);

        // TODO: This is the simplest of deconfliction - we just greedily hand
        // out claims. Next refinements: 
        // A. Give claim to ship with shortest time to intercept preferentially.
        // B. Do some kind of planning stability so ships don't switch targets
        // unless there is a clearly better (e.g. 1 turn sooner or better size
        // option).
        // DEBUG this doesn't work yet.
        // best_e->claimed_by_mech = 1;
      }
    }
  }

  // PHASE 3: SHIELD MAINTENANCE
  // Calculate total fuel that will be used this turn
  double fuel_used = pShip->GetOrder(O_SHIELD) + pShip->GetOrder(O_LASER) +
                     pShip->GetOrder(O_THRUST) + pShip->GetOrder(O_TURN) +
                     pShip->GetOrder(O_JETTISON);
  cur_fuel -= fuel_used;

  // Maintain minimum shield buffer of 11 units
  if (cur_shields < 11.0) {
    cur_fuel -= 5.0;  // Reserve 5 tons as emergency fuel
    double wanted_shields = 11.0 - cur_shields;
    // Add shields up to desired level or available fuel
    pShip->SetOrder(O_SHIELD,
                    (wanted_shields < cur_fuel) ? wanted_shields : cur_fuel);
  }

  // END OF DECISION LOGIC
}
