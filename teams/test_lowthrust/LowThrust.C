/* LowThrust.C
 * Test team to demonstrate launch re-docking bug
 * Uses very low thrust (1.0 units/sec) to trigger re-docking behavior
 *
 * Expected behavior with bug (legacy mode):
 *   Turn 1: Ship launches, re-docks (dDockDist→~35)
 *   Turn 2: Ship launches, re-docks (dDockDist→~40)
 *   Turn 3: Ship launches, successfully escapes (distance ~45)
 *
 * Expected behavior with fix:
 *   Turn 1: Ship launches to safe distance (48 units), escapes immediately
 */

#include "LowThrust.h"
#include "GameConstants.h"
#include "ParserModern.h"

extern CParser* g_pParser;

// Factory function
CTeam* CTeam::CreateTeam() { return new LowThrust; }

LowThrust::LowThrust() {}

LowThrust::~LowThrust() {
  CShip* pSh;
  CBrain* pBr;

  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    pSh = GetShip(i);
    if (pSh == NULL) continue;

    pBr = pSh->GetBrain();
    if (pBr != NULL) {
      delete pBr;
    }
  }
}

void LowThrust::Init() {
  srand(time(NULL));
  SetTeamNumber(1 + (rand() % 16));
  SetName("Low Thrust Test");
  GetStation()->SetName("Test Station");

  // Name ships
  GetShip(0)->SetName("Test Ship 1");
  GetShip(1)->SetName("Test Ship 2");
  GetShip(2)->SetName("Test Ship 3");
  GetShip(3)->SetName("Test Ship 4");

  // Standard configuration
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 60.0);
    GetShip(i)->SetCapacity(S_CARGO, 30.0);
    GetShip(i)->SetBrain(new LowThrustBrain);
  }

  printf("=== LOW THRUST TEST TEAM INITIALIZED ===\n");
  printf("This team uses O_THRUST=1.0 to test launch re-docking bug\n");
  printf("Expected with bug: Ships re-dock turns 1&2, escape turn 3\n");
  printf("Expected with fix: Ships escape turn 1\n");
  printf("==========================================\n\n");
}

void LowThrust::Turn() {
  CShip* pSh;
  CBrain* pBr;

  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    pSh = GetShip(i);
    if (pSh == NULL) continue;

    pBr = pSh->GetBrain();
    if (pBr != NULL) {
      pBr->Decide();
    }
  }
}

void LowThrustBrain::Decide() {
  if (pShip == NULL) return;

  CWorld* pWorld = pShip->GetTeam()->GetWorld();
  double game_time = pWorld->GetGameTime();

  // Log ship state at start of each turn (first 5 turns only)
  if (game_time <= 5.0) {
    printf("[TURN %.0f] %s: docked=%d pos=(%.1f,%.1f) vel=(%.2f,%.1f°) orient=%.2f\n",
           game_time,
           pShip->GetName(),
           pShip->IsDocked(),
           pShip->GetPos().fX,
           pShip->GetPos().fY,
           pShip->GetVelocity().rho,
           pShip->GetVelocity().theta * 180.0 / PI,
           pShip->GetOrient());
  }

  // If docked, launch with very low thrust
  if (pShip->IsDocked()) {
    // Orient to unique direction for each ship (0, 90, 180, 270 degrees)
    double target_orient = (double)(pShip->GetShipNumber()) * PI / 2.0;
    double turn_amount = target_orient - pShip->GetOrient();

    // Normalize turn to [-PI, PI]
    if (turn_amount < -PI) turn_amount += PI2;
    if (turn_amount > PI) turn_amount -= PI2;

    pShip->SetOrder(O_TURN, turn_amount);

    // Only thrust when oriented correctly
    if (fabs(turn_amount) < 0.2) {
      // CRITICAL: Use very low thrust to trigger re-docking bug
      pShip->SetOrder(O_THRUST, 1.0);

      if (g_pParser && g_pParser->verbose) {
        printf("  → %s issuing launch thrust O_THRUST=1.0\n", pShip->GetName());
      }
    }
  } else {
    // Once undocked, continue with low thrust to maintain velocity
    pShip->SetOrder(O_THRUST, 1.0);
  }
}