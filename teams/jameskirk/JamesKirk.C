/* James Kirk
 * Combat-focused team demonstrating engine exploits
 * MechMania IV: The Vinyl Frontier
 *
 * PURPOSE: This team demonstrates various exploits that existed in the
 * original MechMania IV codebase. These exploits show interesting
 * vulnerabilities in the game engine that teams could have discovered.
 *
 * NOTE: The game may need to be run with --legacy flag for exploits to work.
 * In the current implementation, only the laser power exploit is active,
 * which allows firing extremely high-powered lasers (2000 miles) while only
 * paying the fuel cost of the maximum validated laser (512 miles).
 *
 * The exploit works by directly manipulating the orders array after the
 * engine has already read the laser value but before it validates it,
 * creating a Time-Of-Check-Time-Of-Use (TOCTOU) vulnerability.
 */

#include "JamesKirk.h"
#include "GameConstants.h"
#include "KobayashiMaru.h"

// Tell the game to use our class
CTeam* CTeam::CreateTeam() { return new JamesKirk; }

//////////////////////////////////////////
// James Kirk class

JamesKirk::JamesKirk() {}

JamesKirk::~JamesKirk() {
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
  }
}

void JamesKirk::Init() {
  // Strategic initialization: Set up combat-focused team
  srand(time(NULL));
  SetTeamNumber(1 + (rand() % 16));
  SetName("James Kirk");
  GetStation()->SetName("Spacedock");

  // Famous Federation starships from Star Trek movies (≤13 chars)
  GetShip(0)->SetName("USS Enterprise");
  GetShip(1)->SetName("USS Excelsior");
  GetShip(2)->SetName("USS Reliant");
  GetShip(3)->SetName("USS Grissom");

  // Combat configuration: High fuel, zero cargo (pure combat role)
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 60.0);  // High fuel for sustained combat
    GetShip(i)->SetCapacity(S_CARGO, 0.0);  // No cargo - pure combat vessels
    GetShip(i)->SetBrain(new Shooter);  // All ships are shooters
  }
}

void JamesKirk::Turn() {
  // Simple combat AI: Undock if needed, then hunt enemies
  CShip* pSh;
  CBrain* pBr;

  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    pSh = GetShip(i);
    if (pSh == NULL) {
      continue;
    }

    pBr = pSh->GetBrain();

    // If docked, ensure we have Voyager brain to undock
    if (pSh->IsDocked()) {
      if (dynamic_cast<Voyager*>(pBr) == NULL) {
        // Not a Voyager brain, replace with one
        pSh->SetBrain(new Voyager(pBr));
        pBr = pSh->GetBrain();
      }
    } else {
      // Not docked, ensure we have Shooter brain
      if (dynamic_cast<Shooter*>(pBr) == NULL && dynamic_cast<Voyager*>(pBr) == NULL) {
        // Not a Shooter or Voyager, replace with Shooter
        pSh->SetBrain(new Shooter);
        pBr = pSh->GetBrain();
      }
    }

    // Execute tactical AI
    pBr->Decide();
  }
}

///////////////////////////////////////////////
// Voyager brain - handles station departure

Voyager::Voyager(CBrain* pLB) {
  pLastBrain = pLB;  // Store who we're replacing
  if (pLB->pShip != NULL) {
    pLB->pShip->SetBrain(this);  // Replace it
  }
}

Voyager::~Voyager() {
  if (pShip != NULL)
    pShip->SetBrain(pLastBrain);  // Put everything back
}

void Voyager::Decide() {
  if (pShip == NULL) {
    return;
  }
  if (!pShip->IsDocked()) {
    delete this;  // Don't need us anymore
    return;       // Back to combat!
  }

  // Undocking sequence - orient and thrust away from station
  double tang = (double)(pShip->GetShipNumber()) * PI / 2.0;

  tang -= pShip->GetOrient();
  if (tang < -PI) {
    tang += PI2;
  }
  if (tang > PI) {
    tang -= PI2;
  }
  pShip->SetOrder(O_TURN, tang);

  // Thrust when oriented (free while docked)
  if (fabs(tang) < 0.2)
    pShip->SetOrder(O_THRUST, g_game_max_speed);
}

//------------------------------------------
// Stalker brain - pursuit and interception

void Stalker::Decide() {
  if (pTarget == NULL ||  // No valid target
      pShip == NULL ||    // No valid ship assigned to this AI
      *pShip == *pTarget) {
    return;  // Can't home in on ourselves!
  }

  // Check for pending collision
  double dt = LegacyDetectCollisionCourse(*pTarget);
  if (dt != g_no_collide_sentinel) {
    pShip->SetOrder(O_THRUST, 0.0);  // Cancel thrust, we're on intercept
    return;
  }

  // Estimate interception time
  CTraj RelVel = pShip->RelativeVelocity(*pTarget);
  double dist = pShip->GetPos().DistTo(pTarget->GetPos());
  dt = sqrt(dist / RelVel.rho);
  dt += 1000.0 / dist;

  double dang = pShip->AngleToIntercept(*pTarget, dt);

  pShip->SetOrder(O_TURN, 1.2 * dang);  // Sharp turns

  // Thrust if fairly well oriented
  if (fabs(dang) < 0.15) {
    pShip->SetOrder(O_THRUST, 10.0);  // Accelerate toward target
  } else if (fabs(dang) > (PI - 0.15)) {
    pShip->SetOrder(O_THRUST, -10.0);  // Reverse thrust if pointing away
  }
}

// Legacy collision detection from ChromeFunk
double Stalker::LegacyDetectCollisionCourse(const CThing& OthThing) const {
  if (OthThing == *pShip) {
    return g_no_collide_sentinel;
  }

  CTraj VRel = pShip->RelativeVelocity(OthThing);
  if (VRel.rho <= 0.05) {
    return g_no_collide_sentinel;
  }

  double flyred = pShip->GetSize() + OthThing.GetSize();
  double dist = pShip->GetPos().DistTo(OthThing.GetPos());
  if (dist < flyred) {
    return 0.0;  // Already impacting
  }

  // LEGACY LOGIC: Approximation-based collision detection
  CTraj VHit(dist, VRel.theta);
  CCoord RelPos = OthThing.GetPos() - pShip->GetPos(),
         CHit(RelPos + VHit.ConvertToCoord());

  double flyby = CHit.DistTo(CCoord(0.0, 0.0));
  if (flyby > flyred) {
    return g_no_collide_sentinel;
  }

  // Pending collision
  double hittime = (dist - flyred) / VRel.rho;
  return hittime;
}

//------------------------------------
// Shooter brain - combat with laser exploit

unsigned int Shooter::SelectTarget() {
  CTeam* pmyTeam = pShip->GetTeam();
  CWorld* pmyWorld = pShip->GetWorld();

  if (pmyWorld == NULL || pmyTeam == NULL) {
    return BAD_INDEX;
  }

  unsigned int index;
  CThing* pTh;
  ThingKind ThKind;
  AsteroidKind AsMat;

  // If critically low on fuel, seek fuel asteroids
  if (pShip->GetAmount(S_FUEL) < 15.0) {
    unsigned int indbest = BAD_INDEX;
    double dist, dbest = -1.0;

    for (index = pmyWorld->UFirstIndex;
         index <= pmyWorld->ULastIndex;
         index = pmyWorld->GetNextIndex(index)) {
      pTh = pmyWorld->GetThing(index);

      if (pTh->GetKind() != ASTEROID) {
        continue;
      }

      AsMat = ((CAsteroid*)pTh)->GetMaterial();
      if (AsMat == VINYL) {
        continue;  // We need fuel, not vinyl
      }

      // Found a fuel asteroid
      dist = pShip->GetPos().DistTo(pTh->GetPos());

      if (indbest == BAD_INDEX || dist < dbest) {
        indbest = index;
        dbest = dist;
      }
    }

    if (indbest != BAD_INDEX) {
      return indbest;  // Go get fuel
    }
  }

  // Look for enemy targets - track closest enemy ship
  unsigned int closestShipIndex = BAD_INDEX;
  double closestShipDist = -1.0;
  unsigned int stationIndex = BAD_INDEX;

  for (index = pmyWorld->UFirstIndex;
       index <= pmyWorld->ULastIndex;
       index = pmyWorld->GetNextIndex(index)) {
    pTh = pmyWorld->GetThing(index);

    // Check if target is alive before proceeding
    if (!pTh->IsAlive()) {
      continue;  // Dead, skip
    }

    ThKind = pTh->GetKind();

    // Friend-or-foe check: Only target enemies
    if (pTh->GetTeam() == pmyTeam) {
      continue;  // Friendly, skip
    }

    // Priority 1: Enemy ships (track closest)
    if (ThKind == SHIP) {
      double dist = pShip->GetPos().DistTo(pTh->GetPos());
      if (closestShipIndex == BAD_INDEX || dist < closestShipDist) {
        closestShipIndex = index;
        closestShipDist = dist;
      }
    }

    // Priority 2: Enemy stations with vinyl (remember first one found)
    if (ThKind == STATION && stationIndex == BAD_INDEX) {
      CStation* pStation = (CStation*)pTh;
      if (pStation->GetVinylStore() > 0.0) {
        stationIndex = index;
      }
    }
  }

  // Return closest enemy ship if found, otherwise station with vinyl
  if (closestShipIndex != BAD_INDEX) {
    return closestShipIndex;
  }
  if (stationIndex != BAD_INDEX) {
    return stationIndex;
  }

  return BAD_INDEX;  // No valid targets
}

void Shooter::Decide() {
  if (pShip == NULL) {
    return;
  }

  // Acquire target if we don't have one
  if (pTarget == NULL) {
    unsigned int targetIndex = SelectTarget();
    if (targetIndex != BAD_INDEX) {
      pTarget = pShip->GetWorld()->GetThing(targetIndex);
      // Ensure target is alive
      if (pTarget != NULL && !pTarget->IsAlive()) {
        pTarget = NULL;
      }
    }
  }

  // Re-validate target (may have been destroyed)
  if (pTarget == NULL || pTarget->GetKind() == GENTHING || !pTarget->IsAlive()) {
    pTarget = NULL;
    unsigned int targetIndex = SelectTarget();
    if (targetIndex != BAD_INDEX) {
      pTarget = pShip->GetWorld()->GetThing(targetIndex);
      // Ensure target is alive
      if (pTarget != NULL && !pTarget->IsAlive()) {
        pTarget = NULL;
      }
    }
  }

  if (pTarget == NULL || *pShip == *pTarget) {
    return;  // No valid target or targeting self
  }

  // CRITICAL: Verify target is not a friendly before proceeding
  CTeam* pmyTeam = pShip->GetTeam();
  if (pTarget->GetTeam() == pmyTeam) {
    pTarget = NULL;  // Clear the friendly target
    return;  // Never fire on friendlies
  }

  // Handle asteroids (for fuel collection)
  if (pTarget->GetKind() == ASTEROID) {
    // Just ram it - usually it will fit
    Stalker::Decide();
    return;
  }

  // Check range
  double drange = pShip->GetPos().DistTo(pTarget->GetPos());

  if (drange > 250.0) {  // Too far, close in first
    Stalker::Decide();   // Use pursuit logic
    return;
  }

  // Predict positions one turn ahead
  CCoord MyPos, TargPos;
  MyPos = pShip->PredictPosition(g_game_turn_duration);
  TargPos = pTarget->PredictPosition(g_game_turn_duration);

  CTraj TurnVec = MyPos.VectTo(TargPos);
  TurnVec.theta -= pShip->GetOrient();
  TurnVec.Normalize();
  double dang = TurnVec.theta;

  // Try to turn to face target
  // SetOrder modifies the order if we don't have enough fuel
  pShip->SetOrder(O_TURN, dang);

  // Check if turn was successful by comparing requested vs actual turn
  // GetOrder returns the actual turn that will be executed
  double actualTurn = pShip->GetOrder(O_TURN);
  bool turnSuccessful = (fabs(dang - actualTurn) < g_fp_error_epsilon);

  // Only shoot if we successfully turned to face the target
  // Note: With the exploit, we can shoot without fuel, but we need to be aimed correctly
  if (pTarget != NULL && pTarget->IsAlive() && turnSuccessful) {
    // Only show attack messages when shooting at ships or stations, not asteroids
    if (pTarget->GetKind() != ASTEROID) {
      // Random Star Trek phaser attack messages
      const char* attack_phrases[] = {
        "Engage!",
        "Fire phasers!",
        "Phasers - full spread!",
        "Fire at will, Lieutenant Worf!",
        "Target that explosion and fire!",
        "Lock phasers on target!",
        "Return fire!",
        "All weapons, fire!"
      };
      const int num_phrases = sizeof(attack_phrases) / sizeof(attack_phrases[0]);
      const char* phrase = attack_phrases[rand() % num_phrases];

      char shipmsg[128];
      snprintf(shipmsg, sizeof(shipmsg), "%s: %s\n", pShip->GetName(), phrase);
      strncat(pmyTeam->MsgText, shipmsg, maxTextLen - strlen(pmyTeam->MsgText) - 1);
    }

    // KOBAYASHI MARU EXPLOIT: Bypass SetOrder validation via direct array manipulation
    // This exploits the TOCTOU vulnerability in World::LaserModel()
    // The server reads GetOrder(O_LASER) before calling SetOrder(O_LASER) to validate
    // We set the raw array value to X, fire massive laser, but only pay for validated amount
    double* orders = KobayashiMaru::GetOrdersArray(pShip);
    orders[O_LASER] = 9999.0;  // Exploit: massive laser power (9999 miles!)

    // Note - it's important not to call SetOrder for Laser again after this on
    // this turn, or it will overwrite our exploit value.
  }
}
