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

// External reference to global parser instance
extern CParser* g_pParser;

GetVinyl::GetVinyl() {}

GetVinyl::~GetVinyl() {}

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
        // NOTE: Actually this is a pretty good heursitc for old groogroo and
        // even groonew. Usually we travel "forward" and we want to eject, dock,
        // and then relaunch right into it so we want to hit the station back
        // first probably.
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
