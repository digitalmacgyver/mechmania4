/* World.C
 * Implementation of CWorld
 * Physics model and scoring mechanism
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include <sys/time.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <random>
#include <set>
#include <vector>

#include "ArgumentParser.h"
#include "Asteroid.h"
#include "CollisionTypes.h"
#include "GameConstants.h"
#include "ParserModern.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "World.h"

//////////////////////////////////////////////////
// Construction/Destruction

CWorld::CWorld(unsigned int nTm) {
  unsigned int i;
  numTeams = nTm;

  apTeams = new CTeam*[numTeams];
  atstamp = new double[numTeams];
  auClock = new double[numTeams];
  for (i = 0; i < numTeams; ++i) {
    apTeams[i] = NULL;
    atstamp[i] = 0.0;
    auClock[i] = 0.0;
  }

  gametime = 0.0;  // Start the clock
  currentTurn = 0;  // Start at turn 0
  bGameOver = false;
  memset(AnnouncerText, 0, maxAnnouncerTextLen);  // Initialize announcer buffer

  for (i = 0; i < MAX_THINGS; ++i) {
    apThings[i] = NULL;
    apTAddQueue[i] = NULL;
    aUNextInd[i] = (unsigned int)-1;
    aUPrevInd[i] = (unsigned int)-1;
  }

  UFirstIndex = (unsigned int)-1;
  ULastIndex = (unsigned int)-1;
  numNewThings = 0;
}

CWorld::~CWorld() {
  CThing* pTTmp;
  unsigned int i;

  for (i = 0; i < MAX_THINGS; ++i) {
    pTTmp = GetThing(i);
    if (pTTmp == NULL) {
      continue;
    }
    if (pTTmp->GetKind() == ASTEROID) {
      delete pTTmp;
    }
  }

  delete[] apTeams;
  delete[] atstamp;
  delete[] auClock;
}

CWorld* CWorld::CreateCopy() {
  CWorld* pWld;
  pWld = new CWorld(numTeams);

  unsigned int acsz, sz = GetSerialSize();
  char* buf = new char[sz];

  acsz = SerialPack(buf, sz);
  if (acsz != sz) {
    printf("ERROR: World assignment\n");
    return NULL;
  }

  pWld->SerialUnpack(buf, acsz);
  delete[] buf;
  return pWld;
}

//////////////////////////////////////////////////
// Data access to internal members

CTeam* CWorld::GetTeam(unsigned int nt) const {
  if (nt >= GetNumTeams()) {
    return NULL;
  }
  return apTeams[nt];
}

unsigned int CWorld::GetNumTeams() const { return numTeams; }

double CWorld::GetGameTime() const { return gametime; }

unsigned int CWorld::GetCurrentTurn() const { return currentTurn; }

void CWorld::IncrementTurn() { currentTurn++; }

void CWorld::AddAnnouncerMessage(const char* message) {
  if (message == NULL) return;

  size_t currentLen = strlen(AnnouncerText);
  size_t messageLen = strlen(message);

  // Ensure we have space for the message plus a newline and null terminator
  if (currentLen + messageLen + 2 < maxAnnouncerTextLen) {
    if (currentLen > 0) {
      strcat(AnnouncerText, "\n");  // Add newline between messages
    }
    strcat(AnnouncerText, message);

    // Log announcer messages to stdout for debugging
    extern CParser* g_pParser;
    if (g_pParser && g_pParser->verbose) {
      printf("[ANNOUNCER] %s\n", message);
    }
  }
}

CThing* CWorld::GetThing(unsigned int index) const {
  if (index >= MAX_THINGS) {
    return NULL;
  }
  return apThings[index];
}

unsigned int CWorld::GetNextIndex(unsigned int curindex) const {
  if (curindex >= MAX_THINGS) {
    return (unsigned int)-1;
  }
  return aUNextInd[curindex];
}

unsigned int CWorld::GetPrevIndex(unsigned int curindex) const {
  if (curindex >= MAX_THINGS) {
    return (unsigned int)-1;
  }
  return aUPrevInd[curindex];
}

/////////////////////////////////////////////////////
// Explicit functions

unsigned int CWorld::PhysicsModel(double dt, double turn_phase) {
  CThing* pThing;
  unsigned int i;

  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pThing = GetThing(i);
    pThing->Drift(dt, turn_phase);
  }

  CollisionEvaluation();
  AddNewThings();    // It's possible that new things are already dead
  KillDeadThings();  // So this comes after AddNewThings

  gametime += dt;
  return 0;
}

void CWorld::LaserModel() {
  // LASER PROCESSING - Dispatch to legacy or deterministic implementation
  extern CParser* g_pParser;

  if (g_pParser && !g_pParser->UseNewFeature("collision-handling")) {
    LaserModelOld();
  } else {
    LaserModelNew();
  }
}

void CWorld::LaserModelOld() {
  // LEGACY LASER PROCESSING - Bypass deterministic collision engine
  // LASER PROCESSING - OPTIONAL TOCTOU VULNERABILITY
  //
  // By default, laser power is validated BEFORE firing. When --legacy-laser-exploit
  // or --legacy-mode flags are set, the original 1998 vulnerability is re-enabled.

  extern CParser* g_pParser;

  unsigned int nteam, nship;
  CTeam* pTeam;
  CShip* pShip;
  CThing *pTarget, LasThing;
  CCoord LasPos, TmpPos;
  CTraj LasTraj, TarVel, TmpTraj;
  double dfuel, dLasPwr, dLasRng;

  /*
   * Laser delivery model:
   * ---------------------
   * Lasers are not persistent world objects. Instead, each turn we
   * synthesize a temporary CThing (LasThing) with kind GENTHING and
   * deliver it directly to the chosen target via Collide(). The
   * synthesized "laser thing" is placed one world-unit in front of the
   * target, along the beam direction, and its mass encodes the
   * remaining beam power at that point:
   *
   *   LasThing.mass = 30 * (L - D)
   *
   * where L is the requested beam length (clamped earlier), and D is
   * the distance from the shooter to a point one unit short of the
   * target along the beam line. By positioning just before the target
   * and using (L - D), the target's HandleCollision() sees the
   * remaining beam power when the beam reaches it, not the initial
   * requested length. Targets can then use the laser "mass" to decide
   * effects (e.g., asteroids break if mass >= 1000.0).
   */

  for (nteam = 0; nteam < GetNumTeams(); ++nteam) {
    pTeam = GetTeam(nteam);
    if (pTeam == NULL) {
      continue;
    }
    for (nship = 0; nship < pTeam->GetShipCount(); ++nship) {
      pShip = pTeam->GetShip(nship);
      if (pShip == NULL) {
        continue;
      }

      // Check for legacy exploit mode
      bool legacy_exploit = (g_pParser && g_pParser->UseNewFeature("laser-exploit"));

      if (legacy_exploit) {
        // LEGACY MODE: TOCTOU VULNERABILITY ENABLED
        // Reading raw client data BEFORE validation (exploitable)
        // This GetOrder() returns the unvalidated adOrders[O_LASER] value.
        // A malicious client can set this to any value by directly manipulating
        // the array, bypassing SetOrder() checks.
        //
        // Attack scenario:
        //   1. Client sets adOrders[O_LASER] = 999999 (bypassing SetOrder validation)
        //   2. Server reads raw value here and fires 999999-unit laser
        //   3. Server calls SetOrder() later which caps to fuel available (~500 for 10 fuel)
        //   4. Client gets 20x damage for same fuel cost
        dLasPwr = pShip->GetOrder(O_LASER);

        if (dLasPwr <= 0.0) {
          continue;
        }
      } else {
        // NEW MODE: VULNERABILITY PATCHED
        // Validate and cap laser power BEFORE using it for damage
        double oldFuel = pShip->GetAmount(S_FUEL);
        double requested_power = pShip->GetOrder(O_LASER);

        if (requested_power <= 0.0) {
          continue;
        }

        // Call SetOrder to validate/cap the power based on available fuel
        double fuel_cost = pShip->SetOrder(O_LASER, requested_power);

        // Use the VALIDATED power (read it back after SetOrder clamped it)
        dLasPwr = pShip->GetOrder(O_LASER);

        // If validated power is 0, skip laser firing
        if (dLasPwr <= 0.0) {
          continue;
        }

        // Deduct fuel immediately
        pShip->SetAmount(S_FUEL, oldFuel - fuel_cost);

        // Check if out of fuel
        if (oldFuel > 0.01 && (oldFuel - fuel_cost) <= 0.01) {
          printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", pShip->GetName(),
                 pShip->GetTeam() ? pShip->GetTeam()->GetName() : "Unknown");
        }
      }

      // Compute the nominal end-of-beam position from shooter
      LasPos = pShip->GetPos();
      LasTraj = CTraj(dLasPwr, pShip->GetOrient());
      LasPos += LasTraj.ConvertToCoord();
      LasThing.SetPos(LasPos);

      pTarget = pShip->LaserTarget();

      // LASER RANGE CHECK
      // This check determines if the laser can reach its target.
      // Two implementations exist due to a historical floating-point bug.
      bool use_legacy_rangecheck = (g_pParser && g_pParser->UseNewFeature("rangecheck-bug"));

      if (use_legacy_rangecheck) {
        // LEGACY: Buggy floating-point range check (original 1998 code)
        //
        // This compares dLasRng (distance from laser endpoint back to ship)
        // with dLasPwr (laser beam length). Since LasPos is computed as
        // ship_position + (dLasPwr * direction_vector), these two values
        // should theoretically always be equal.
        //
        // However, accumulated floating-point errors in the trigonometric
        // calculations can cause dLasRng to be slightly larger than dLasPwr,
        // incorrectly triggering this check and nullifying valid targets.
        //
        // This bug particularly manifests when laser power = 512.0 (exactly
        // half the world size), causing lasers to fail to hit targets they
        // should reach.
        dLasRng = LasPos.DistTo(pShip->GetPos());
        if (dLasRng > dLasPwr) {
          pTarget = NULL;
        }
      } else {
        // NEW: Correct range validation
        //
        // Check if the actual target is within laser range by measuring
        // the distance from the ship to the target's position, not the
        // distance from the ship to the computed laser endpoint.
        //
        // Note: Given how LaserTarget() works (it returns the nearest object
        // the ship is facing, regardless of range), this check should rarely
        // trigger. However, it's included for correctness and maintainability
        // in case LaserTarget() behavior changes in the future.
        if (pTarget != NULL) {
          double target_distance = pShip->GetPos().DistTo(pTarget->GetPos());
          if (target_distance > dLasPwr) {
            pTarget = NULL;  // Target out of laser range
          }
        }
      }

      if (pTarget != NULL) {
        TmpPos = pTarget->GetPos();
        // Move impact point to one unit in front of the target along the
        // beam. This ensures we measure remaining length (L - D) at impact.
        TmpTraj = pShip->GetPos().VectTo(TmpPos);
        TmpTraj.rho = 1.0;
        TmpPos -= (CCoord)TmpTraj;
        LasThing.SetPos(TmpPos);

        // Remaining beam power at impact: mass =
        // g_laser_mass_scale_per_remaining_unit * (L - D)
        dLasRng = TmpPos.DistTo(pShip->GetPos());
        LasThing.SetMass(g_laser_mass_scale_per_remaining_unit *
                         (dLasPwr - dLasRng));

        // Set laser velocity based on physics mode
        if (g_pParser && g_pParser->UseNewFeature("physics")) {
          // NEW PHYSICS: Photon momentum model
          // Photons travel at speed of light along beam direction
          // In our game: c = max_ship_speed (30 u/s)
          // Momentum transfer: p = E/c where E is laser "mass" (energy)
          // This gives momentum along beam, not target motion
          double beam_direction = pShip->GetOrient();
          double speed_of_light = g_game_max_speed;  // 30 u/s
          CTraj laser_velocity(speed_of_light, beam_direction);
          LasThing.SetVel(laser_velocity);
        } else {
          // LEGACY PHYSICS: Target-tracking laser
          // Give the laser thing a small velocity bias based on target motion
          TarVel = pTarget->GetVelocity();
          TarVel.rho += 1.0;
          LasThing.SetVel(TarVel);
        }

        // Log laser hit with verbose logging
        if (g_pParser && g_pParser->verbose) {
          const char* targetType = (pTarget->GetKind() == SHIP) ? "SHIP" :
                                   (pTarget->GetKind() == STATION) ? "STATION" :
                                   (pTarget->GetKind() == ASTEROID) ? "ASTEROID" : "UNKNOWN";
          const char* targetName = pTarget->GetName();
          const char* shooterTeam = pShip->GetTeam() ? pShip->GetTeam()->GetName() : "NoTeam";
          const char* targetTeam = pTarget->GetTeam() ? pTarget->GetTeam()->GetName() : "NoTeam";

          CCoord laser_pos = LasThing.GetPos();
          CCoord target_pos = pTarget->GetPos();
          CTraj laser_vel = LasThing.GetVelocity();

          printf("LASER_COLLISION: %s[%s] fires from pos=(%.1f,%.1f) power=%.1f range=%.1f -> %s[%s][%s] at pos=(%.1f,%.1f) laser_vel=(%.2f@%.1f°) mass=%.3f\n",
                 pShip->GetName(), shooterTeam,
                 pShip->GetPos().fX, pShip->GetPos().fY,
                 dLasPwr, dLasRng,
                 targetName ? targetName : "unnamed", targetType, targetTeam,
                 target_pos.fX, target_pos.fY,
                 laser_vel.rho, laser_vel.theta * 180.0 / PI,
                 LasThing.GetMass());
        }

        // Deliver the synthesized laser impact to the target
        pTarget->Collide(&LasThing, this);
      }

      // LEGACY MODE: Deduct fuel AFTER firing (TOCTOU vulnerability)
      if (legacy_exploit) {
        double oldFuel = pShip->GetAmount(S_FUEL);
        dfuel = oldFuel;
        // VULNERABILITY: Validation happens AFTER laser was already fired
        // SetOrder() validates and caps the laser power based on fuel available,
        // but the laser beam was already computed and fired using the raw dLasPwr
        // value above. Client only pays for the validated amount, not what they used.
        dfuel -= pShip->SetOrder(O_LASER, dLasPwr);
        pShip->SetAmount(S_FUEL, dfuel);

        // Check if out of fuel
        if (oldFuel > 0.01 && dfuel <= 0.01) {
          printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", pShip->GetName(),
                 pShip->GetTeam() ? pShip->GetTeam()->GetName() : "Unknown");
        }
      }
      // NEW MODE: Fuel already deducted before firing (lines 228-247)
    }
  }

  AddNewThings();
  KillDeadThings();
}

void CWorld::LaserModelNew() {
  // NEW DETERMINISTIC LASER PROCESSING
  // Uses snapshot/command pipeline instead of direct Collide() calls
  // This ensures laser collisions integrate properly with the deterministic collision engine

  extern CParser* g_pParser;

  unsigned int nteam, nship;
  CTeam* pTeam;
  CShip* pShip;
  CThing *pTarget, LasThing;
  CCoord LasPos, TmpPos;
  CTraj LasTraj, TarVel, TmpTraj;
  double dfuel, dLasPwr, dLasRng;

  for (nteam = 0; nteam < GetNumTeams(); ++nteam) {
    pTeam = GetTeam(nteam);
    if (pTeam == NULL) {
      continue;
    }
    for (nship = 0; nship < pTeam->GetShipCount(); ++nship) {
      pShip = pTeam->GetShip(nship);
      if (pShip == NULL) {
        continue;
      }

      // Laser power validation (always done BEFORE firing in new mode)
      double oldFuel = pShip->GetAmount(S_FUEL);
      double requested_power = pShip->GetOrder(O_LASER);

      if (requested_power <= 0.0) {
        continue;
      }

      // Call SetOrder to validate/cap the power based on available fuel
      double fuel_cost = pShip->SetOrder(O_LASER, requested_power);

      // Use the VALIDATED power (read it back after SetOrder clamped it)
      dLasPwr = pShip->GetOrder(O_LASER);

      // If validated power is 0, skip laser firing
      if (dLasPwr <= 0.0) {
        continue;
      }

      // Deduct fuel immediately
      pShip->SetAmount(S_FUEL, oldFuel - fuel_cost);

      // Check if out of fuel
      if (oldFuel > 0.01 && (oldFuel - fuel_cost) <= 0.01) {
        printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", pShip->GetName(),
               pShip->GetTeam() ? pShip->GetTeam()->GetName() : "Unknown");
      }

      // Compute the nominal end-of-beam position from shooter
      LasPos = pShip->GetPos();
      LasTraj = CTraj(dLasPwr, pShip->GetOrient());
      LasPos += LasTraj.ConvertToCoord();
      LasThing.SetPos(LasPos);

      pTarget = pShip->LaserTarget();

      // LASER RANGE CHECK (use corrected range check, not legacy buggy one)
      if (pTarget != NULL) {
        double target_distance = pShip->GetPos().DistTo(pTarget->GetPos());
        if (target_distance > dLasPwr) {
          pTarget = NULL;  // Target out of laser range
        }
      }

      if (pTarget != NULL) {
        TmpPos = pTarget->GetPos();
        // Move impact point to one unit in front of the target along the beam
        TmpTraj = pShip->GetPos().VectTo(TmpPos);
        TmpTraj.rho = 1.0;
        TmpPos -= (CCoord)TmpTraj;
        LasThing.SetPos(TmpPos);

        // Remaining beam power at impact
        dLasRng = TmpPos.DistTo(pShip->GetPos());
        LasThing.SetMass(g_laser_mass_scale_per_remaining_unit * (dLasPwr - dLasRng));

        // Set laser velocity (photon momentum model in new physics)
        double beam_direction = pShip->GetOrient();
        double speed_of_light = g_game_max_speed;  // 30 u/s
        CTraj laser_velocity(speed_of_light, beam_direction);
        LasThing.SetVel(laser_velocity);

        // Log laser hit with verbose logging
        if (g_pParser && g_pParser->verbose) {
          const char* targetType = (pTarget->GetKind() == SHIP) ? "SHIP" :
                                   (pTarget->GetKind() == STATION) ? "STATION" :
                                   (pTarget->GetKind() == ASTEROID) ? "ASTEROID" : "UNKNOWN";
          const char* targetName = pTarget->GetName();
          const char* shooterTeam = pShip->GetTeam() ? pShip->GetTeam()->GetName() : "NoTeam";
          const char* targetTeam = pTarget->GetTeam() ? pTarget->GetTeam()->GetName() : "NoTeam";

          CCoord laser_pos = LasThing.GetPos();
          CCoord target_pos = pTarget->GetPos();
          CTraj laser_vel = LasThing.GetVelocity();

          printf("LASER_COLLISION: %s[%s] fires from pos=(%.1f,%.1f) power=%.1f range=%.1f -> %s[%s][%s] at pos=(%.1f,%.1f) laser_vel=(%.2f@%.1f°) mass=%.3f\n",
                 pShip->GetName(), shooterTeam,
                 pShip->GetPos().fX, pShip->GetPos().fY,
                 dLasPwr, dLasRng,
                 targetName ? targetName : "unnamed", targetType, targetTeam,
                 target_pos.fX, target_pos.fY,
                 laser_vel.rho, laser_vel.theta * 180.0 / PI,
                 LasThing.GetMass());
        }

        // === DETERMINISTIC LASER COLLISION PROCESSING ===
        // Create snapshots for laser and target
        CollisionState laser_state = LasThing.MakeCollisionState();
        CollisionState target_state = pTarget->MakeCollisionState();

        // Build collision context with feature flags
        bool use_new_physics = g_pParser ? g_pParser->UseNewFeature("physics") : true;
        bool disable_eat_damage = g_pParser ? g_pParser->UseNewFeature("asteroid-eat-damage") : true;
        bool use_docking_fix = g_pParser ? g_pParser->UseNewFeature("docking") : true;

        CollisionContext ctx(this, &target_state, &laser_state, 1.0,
                             use_new_physics, disable_eat_damage, use_docking_fix);

        // Generate commands from target's perspective (target being hit by laser)
        CollisionOutcome outcome = pTarget->GenerateCollisionCommands(ctx);

        // Apply commands immediately
        CollisionContext apply_ctx(this, NULL, NULL, 1.0, use_new_physics, disable_eat_damage, use_docking_fix);

        for (int i = 0; i < outcome.command_count; ++i) {
          const CollisionCommand& cmd = outcome.commands[i];

          // Handle announcer messages
          if (cmd.type == CollisionCommandType::kAnnounceMessage) {
            if (cmd.message_buffer[0] != '\0') {
              AddAnnouncerMessage(cmd.message_buffer);
            }
            continue;
          }

          // Skip if target is null or already dead
          if (!cmd.target || !cmd.target->IsAlive()) continue;

          // Apply the command
          cmd.target->ApplyCollisionCommand(cmd, apply_ctx);
        }

        // Process spawn requests (asteroid fragments)
        for (int i = 0; i < outcome.spawn_count; ++i) {
          const SpawnRequest& spawn = outcome.spawns[i];

          if (spawn.kind == ASTEROID) {
            CAsteroid* fragment = new CAsteroid(spawn.mass, spawn.material);
            CCoord pos = spawn.position;
            CTraj vel = spawn.velocity;
            fragment->SetPos(pos);
            fragment->SetVel(vel);
            AddThingToWorld(fragment);
          }
        }
      }
    }
  }

  AddNewThings();
  KillDeadThings();
}

void CWorld::AddThingToWorld(CThing* pNewThing) {
  if (pNewThing == NULL || numNewThings >= MAX_THINGS) {
    return;
  }
  apTAddQueue[numNewThings] = pNewThing;
  numNewThings++;
}

void CWorld::ResolvePendingOperations(bool resetTransientState) {
  AddNewThings();
  KillDeadThings();

  if (!resetTransientState) {
    return;
  }

  for (unsigned int idx = UFirstIndex; idx != (unsigned int)-1; idx = GetNextIndex(idx)) {
    CThing* thing = GetThing(idx);
    if (thing == NULL) {
      continue;
    }
    thing->ResetTransientState();
  }
}

void CWorld::CreateAsteroids(AsteroidKind mat, unsigned int numast, double mass) {
  CAsteroid* pAst;
  unsigned int i;

  for (i = 0; i < numast; ++i) {
    pAst = new CAsteroid(mass, mat);
    AddThingToWorld(pAst);
  }
}

CTeam* CWorld::SetTeam(unsigned int n, CTeam* pTm) {
  if (n >= GetNumTeams()) {
    return NULL;
  }

  CTeam* oldteam = apTeams[n];
  CTeam* tmown;
  CThing* delth;
  ThingKind delkind;
  unsigned int i, numsh;

  if (oldteam == pTm) {
    return oldteam;
  }
  if (oldteam != NULL) {
    for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
      delth = GetThing(i);
      delkind = delth->GetKind();
      if (delkind == SHIP || delkind == STATION) {
        if (delkind == SHIP) {
          tmown = ((CShip*)delth)->GetTeam();
        } else {
          tmown = ((CStation*)delth)->GetTeam();
        }

        if (tmown == oldteam) {
          RemoveIndex(i);
        }
      }
    }
  }

  apTeams[n] = pTm;
  pTm->SetWorldIndex(n);
  pTm->SetWorld(this);
  AddThingToWorld(pTm->GetStation());
  for (numsh = 0; numsh < pTm->GetShipCount(); ++numsh) {
    AddThingToWorld(pTm->GetShip(numsh));
  }

  return oldteam;
}

//////////////////////////////////////////////
// Assistant Methods

void CWorld::RemoveIndex(unsigned int index) {
  if (index >= MAX_THINGS) {
    return;
  }

  unsigned int Prev, Next;
  Prev = aUPrevInd[index];
  Next = aUNextInd[index];

  if (Prev < MAX_THINGS) {
    aUNextInd[Prev] = Next;  // Work him out of the sequence
  }

  if (Next < MAX_THINGS) {
    aUPrevInd[Next] = Prev;
  }

  aUPrevInd[index] = (unsigned int)-1;  // Reset his indices
  aUNextInd[index] = (unsigned int)-1;

  apThings[index] = NULL;  // And kiss 'im goodbye

  if (index == UFirstIndex) {
    UFirstIndex = Next;
  }
  if (index == ULastIndex) {
    ULastIndex = Prev;
  }
}

unsigned int CWorld::CollisionEvaluation() {
  extern CParser* g_pParser;

  if (g_pParser && !g_pParser->UseNewFeature("collision-handling")) {
    return CollisionEvaluationOld();
  } else {
    return CollisionEvaluationNew();
  }
}

unsigned int CWorld::CollisionEvaluationOld() {
  // LEGACY COLLISION PROCESSING
  // This implementation preserves the original behavior, including known bugs:
  // - Asteroids can be hit multiple times in same frame (multi-fragmentation)
  // - Ship-ship collisions processed multiple times (double damage)
  // - Dead objects continue processing collisions within same frame

  CThing *pTItr, *pTTm;
  unsigned int i, j, iteam, iship, numtmth, URes = 0;
  CTeam* pTeam;
  static CThing* apTTmTh[MAX_THINGS];  // List of team-controlled (i.e.
                                       // non-asteroid) objects static saves on
                                       // reallocation time btwn calls
  numtmth = 0;
  for (iteam = 0; iteam < GetNumTeams(); ++iteam) {
    pTeam = GetTeam(iteam);
    if (pTeam == NULL) {
      continue;
    }
    pTTm = pTeam->GetStation();  // Put station into list
    apTTmTh[numtmth] = pTTm;
    numtmth++;

    if (bGameOver == true) {
      continue;  // Ships invisible after game ends
    }

    for (iship = 0; iship < pTeam->GetShipCount(); ++iship) {
      pTTm = pTeam->GetShip(iship);
      if (pTTm == NULL) {
        continue;
      }
      apTTmTh[numtmth] = pTTm;
      numtmth++;
    }
  }

  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pTItr = GetThing(i);
    if ((pTItr->IsAlive()) == false) {
      continue;
    }
    if (pTItr == NULL) {
      continue;
    }

    for (j = 0; j < numtmth; ++j) {
      pTTm = apTTmTh[j];
      if (pTTm == NULL) {
        continue;
      }

      pTItr->Collide(pTTm, this);  // Asteroid(?) shattered by ship
      if (pTTm->Collide(pTItr, this) ==  true) {  // Ship deflected by asteroid(?)
        URes++;
      }
    }
  }

  return URes;
}

// Collision pair data structure for physics-ordered collision processing
struct CollisionPair {
  CThing* object1;
  CThing* object2;
  double overlap_distance;  // (r1 + r2) - center_distance

  // Constructor for convenience
  CollisionPair(CThing* obj1, CThing* obj2, double overlap)
      : object1(obj1), object2(obj2), overlap_distance(overlap) {}
};

unsigned int CWorld::CollisionEvaluationNew() {
  // NEW COLLISION ENGINE - SNAPSHOT/COMMAND PIPELINE
  //
  // This implementation uses immutable snapshots and command emission to eliminate
  // order-dependent bugs like asteroid multi-fragmentation and ship double-damage.
  //
  // Pipeline stages:
  // 1. Create snapshots of all objects (read-only phase)
  // 2. Detect all collision pairs using existing overlap logic
  // 3. Generate commands from both collision participants
  // 4. Sort commands by priority (kills first, then position, velocity, resources)
  // 5. Apply commands deterministically
  // 6. Process spawn requests (create new objects)

  extern CParser* g_pParser;

  if (g_pParser && g_pParser->verbose) {
    printf("[COLLISION-ENGINE] Starting collision evaluation\n");
  }

  // Stage 1: Create snapshots of all objects
  std::map<CThing*, CollisionState> snapshots;

  for (unsigned int idx = UFirstIndex; idx != (unsigned int)-1; idx = GetNextIndex(idx)) {
    CThing* thing = GetThing(idx);
    if (thing && thing->IsAlive()) {
      snapshots[thing] = thing->MakeCollisionState();
    }
  }

  // Maintain a mutable copy of snapshots that we update as collision commands are emitted.
  // This lets later collisions observe resource changes (fuel/cargo/shield) computed earlier.
  std::map<CThing*, CollisionState> current_states = snapshots;

  // Build list of team-controlled objects for collision detection
  CThing* team_objects[MAX_THINGS];
  unsigned int num_team_objects = 0;

  for (unsigned int team_idx = 0; team_idx < GetNumTeams(); ++team_idx) {
    CTeam* team = GetTeam(team_idx);
    if (team == NULL) continue;

    // Add station
    CThing* station = team->GetStation();
    team_objects[num_team_objects++] = station;

    // Skip ships if game over
    if (bGameOver) continue;

    // Add ships
    for (unsigned int ship_idx = 0; ship_idx < team->GetShipCount(); ++ship_idx) {
      CThing* ship = team->GetShip(ship_idx);
      if (ship) {
        team_objects[num_team_objects++] = ship;
      }
    }
  }

  // Stage 2: Detect all collision pairs
  std::vector<CollisionPair> collisions;

  // CRITICAL FIX: Deduplicate collision pairs
  // The nested loop creates both (A,B) and (B,A) for each overlap.
  // We use a set to track processed pairs and skip duplicates.
  std::set<std::pair<CThing*, CThing*>> processed_pairs;

  for (unsigned int world_idx = UFirstIndex; world_idx != (unsigned int)-1;
       world_idx = GetNextIndex(world_idx)) {
    CThing* world_object = GetThing(world_idx);
    if (!world_object || !world_object->IsAlive()) continue;

    for (unsigned int team_obj_idx = 0; team_obj_idx < num_team_objects; ++team_obj_idx) {
      CThing* team_object = team_objects[team_obj_idx];
      if (!team_object) continue;

      // Skip self-collision
      if (world_object == team_object) continue;

      // Deduplicate: Skip if we've already processed this pair
      // Canonicalize pair: always (lower_index, higher_index) order for consistent lookup
      // Use world index instead of pointer comparison for determinism across platforms
      CThing* obj1 = world_object;
      CThing* obj2 = team_object;
      if (obj1->GetWorldIndex() > obj2->GetWorldIndex()) {
        CThing* temp = obj1;
        obj1 = obj2;
        obj2 = temp;
      }
      std::pair<CThing*, CThing*> pair_key(obj1, obj2);

      if (processed_pairs.count(pair_key) > 0) {
        continue;  // Already processed this collision pair
      }

      // Game collision rules:
      // 1. Asteroids don't collide with other asteroids
      // 2. Docked ships don't collide with ANYTHING
      // 3. Everything else collides

      ThingKind kind1 = world_object->GetKind();
      ThingKind kind2 = team_object->GetKind();

      // Rule 1: Skip asteroid-asteroid collisions
      if (kind1 == ASTEROID && kind2 == ASTEROID) {
        continue;
      }

      // ============================================================================
      // DOCKING STATE MACHINE - Part 1: Collision Detection Filter
      // ============================================================================
      // This is the FIRST checkpoint in a two-phase docking system. Ships can be in
      // three distinct docking states during collision processing:
      //
      // STATE 1: ALREADY_DOCKED (from previous turn)
      //   - Condition: IsDocked() && WasDocked()
      //   - Where detected: HERE (collision detection phase)
      //   - Behavior: Skip collision detection entirely for non-station objects
      //   - Rationale: Ships docked at start of turn are intangible to everything except their station
      //
      // STATE 2: JUST_DOCKED (docking this turn)
      //   - Condition: IsDocked() && !WasDocked()
      //   - Where detected: Later in command generation (line 1063)
      //   - Behavior: Allow initial ship-station collision to process docking, then become intangible
      //   - Rationale: Ship must collide with station ONCE to trigger docking, then ignore other collisions
      //
      // STATE 3: PENDING_DOCK (docking queued but not applied)
      //   - Condition: pending_docks.count(ship) > 0
      //   - Where detected: Later in command generation (line 1063)
      //   - Behavior: Skip remaining collisions this turn
      //   - Rationale: Once ship has kSetDocked command queued, it shouldn't collide with anything else
      //
      // This checkpoint (Part 1) only filters STATE 1 (already docked from previous turn).
      // States 2 and 3 are filtered later in command generation (see lines 1049-1081).
      //
      // Rule 2: Skip collisions involving ships that were ALREADY docked
      // Exception: Docked ships CAN collide with their own station (undocking mechanics)
      // Ships that dock THIS TURN need their ship-station collision processed first.
      // Use WasDocked() to distinguish: IsDocked() && WasDocked() = was already docked
      if (kind1 == SHIP && kind2 != STATION) {
        CShip* ship = (CShip*)world_object;
        if (ship->IsDocked() && ship->WasDocked()) {
          if (g_pParser && g_pParser->verbose) {
            printf("[DEBUG] Skipping collision for already-docked ship: %s (not with station)\n", ship->GetName());
          }
          continue;  // Already-docked ship doesn't collide with non-stations
        }
      }
      if (kind2 == SHIP && kind1 != STATION) {
        CShip* ship = (CShip*)team_object;
        if (ship->IsDocked() && ship->WasDocked()) {
          if (g_pParser && g_pParser->verbose) {
            printf("[DEBUG] Skipping collision for already-docked ship: %s (not with station)\n", ship->GetName());
          }
          continue;  // Already-docked ship doesn't collide with non-stations
        }
      }

      double radius1 = world_object->GetSize();
      double radius2 = team_object->GetSize();
      double center_distance = world_object->GetPos().DistTo(team_object->GetPos());
      double overlap = (radius1 + radius2) - center_distance;

      // Use >= to include "just touching" collisions (overlap == 0)
      // This matches legacy behavior where dist == (r1 + r2) triggers collision
      if (overlap >= 0.0) {
        // Mark this pair as processed
        processed_pairs.insert(pair_key);

        collisions.push_back(CollisionPair(world_object, team_object, overlap));

        // Print collision detection message (for test harness and debugging)
        // NOTE: We print ONCE now (not twice) since we deduplicated pairs
        if (g_pParser && g_pParser->verbose) {
          CCoord pos1 = world_object->GetPos();
          CCoord pos2 = team_object->GetPos();
          CTraj vel1 = world_object->GetVelocity();
          CTraj vel2 = team_object->GetVelocity();
          unsigned int turn = GetCurrentTurn();

          const char* kind1_str = (kind1 == SHIP) ? "SHIP" : (kind1 == STATION) ? "STATION" : (kind1 == ASTEROID) ? "ASTEROID" : "LASER";
          const char* kind2_str = (kind2 == SHIP) ? "SHIP" : (kind2 == STATION) ? "STATION" : (kind2 == ASTEROID) ? "ASTEROID" : "LASER";

          // Check for "ship just docked" case for special logging
          const char* docking_status = "";
          if ((kind1 == SHIP && kind2 == STATION) || (kind1 == STATION && kind2 == SHIP)) {
            CShip* ship = (kind1 == SHIP) ? (CShip*)world_object : (CShip*)team_object;
            if (ship->IsDocked() && !ship->WasDocked()) {
              docking_status = " [SHIP-JUST-DOCKED]";
            } else if (ship->IsDocked() && ship->WasDocked()) {
              docking_status = " [SHIP-ALREADY-DOCKED]";
            }
          }

          // Print collision (single entry now that pairs are deduplicated)
          printf("COLLISION_DETECTED: Turn %u: %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f <-> %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f | dist=%.3f overlap=%.3f%s\n",
                 turn,
                 world_object->GetName(), kind1_str, pos1.fX, pos1.fY, vel1.rho, vel1.theta * 180.0 / PI, world_object->GetSize(),
                 team_object->GetName(), kind2_str, pos2.fX, pos2.fY, vel2.rho, vel2.theta * 180.0 / PI, team_object->GetSize(),
                 center_distance, overlap, docking_status);
        }
      }
    }
  }

  if (g_pParser && g_pParser->verbose) {
    printf("[COLLISION-ENGINE] Total collisions detected: %zu\n", collisions.size());
  }

  // Stage 2.5: Sort collisions by overlap distance with randomized tie-breaking
  // This ensures the most severe collisions are processed first, while collisions
  // with equal (or nearly equal) overlap are randomized to prevent systematic bias.
  //
  // Algorithm:
  // 1. Sort by overlap distance (highest first = most critical)
  // 2. Group collisions with equal overlap (within epsilon tolerance)
  // 3. Shuffle each group randomly
  //
  // This prevents Team 0 advantage and handles edge cases like multiple ships
  // colliding with the same asteroid at the exact same distance.

  const double overlap_epsilon = 0.001;  // Collisions within 1mm are "equal"

  // First pass: Sort by overlap (highest first)
  std::sort(collisions.begin(), collisions.end(),
            [](const CollisionPair& a, const CollisionPair& b) {
              return a.overlap_distance > b.overlap_distance;
            });

  // Second pass: Randomize within equal-overlap groups
  std::random_device rd_sort;
  std::mt19937 rng_sort(rd_sort());

  size_t group_start = 0;
  while (group_start < collisions.size()) {
    // Find the end of this group (all collisions with same overlap within epsilon)
    double group_overlap = collisions[group_start].overlap_distance;
    size_t group_end = group_start + 1;

    while (group_end < collisions.size() &&
           fabs(collisions[group_end].overlap_distance - group_overlap) < overlap_epsilon) {
      group_end++;
    }

    // Shuffle this group if it has more than one element
    if (group_end - group_start > 1) {
      std::shuffle(collisions.begin() + group_start,
                   collisions.begin() + group_end,
                   rng_sort);
    }

    group_start = group_end;
  }

  if (g_pParser && g_pParser->verbose && collisions.size() > 0) {
    printf("[COLLISION-SORT] Sorted %zu collisions by overlap (highest first, randomized ties):\n",
           collisions.size());
    for (size_t i = 0; i < std::min(collisions.size(), size_t(5)); ++i) {
      printf("  #%zu: %s <-> %s overlap=%.3f\n", i,
             collisions[i].object1->GetName(),
             collisions[i].object2->GetName(),
             collisions[i].overlap_distance);
    }
    if (collisions.size() > 5) {
      printf("  ... and %zu more\n", collisions.size() - 5);
    }
  }

  // Stage 3: Generate commands from all collision pairs
  std::vector<CollisionCommand> all_commands;
  std::vector<SpawnRequest> all_spawns;

  // CRITICAL FIX: Track pending kills to prevent multi-hit bugs
  // Example: Asteroid collides with 2 ships → generates 2 kill commands + 6 fragment spawns
  // Without this tracking, both spawn batches execute → 6 fragments instead of 3
  std::set<CThing*> pending_kills;

  // CRITICAL FIX: Track pending docking to prevent spurious collisions
  // When a ship collides with a station, it docks and can't collide with anything else this turn.
  // We track ships with pending kSetDocked commands and skip their remaining collisions.
  std::set<CThing*> pending_docks;

  // Build collision context with feature flags
  bool use_new_physics = g_pParser ? g_pParser->UseNewFeature("physics") : true;
  bool disable_eat_damage = g_pParser ? g_pParser->UseNewFeature("asteroid-eat-damage") : true;
  bool use_docking_fix = g_pParser ? g_pParser->UseNewFeature("docking") : true;

  // Random number generator for ship-ship same-position collisions
  std::random_device rd_angle;
  std::mt19937 rng_angle(rd_angle());
  std::uniform_real_distribution<double> angle_dist(-PI, PI);

  auto apply_command_to_state = [&](const CollisionCommand& cmd) {
    if (cmd.target == NULL) {
      return;
    }

    auto it = current_states.find(cmd.target);
    if (it == current_states.end()) {
      return;
    }

    CollisionState& state = it->second;

    switch (cmd.type) {
      case CollisionCommandType::kAdjustCargo: {
        if (state.kind == SHIP) {
          double max_cargo = state.ship_cargo_capacity;
          double previous_cargo = state.ship_cargo;
          double hull_mass = state.mass - previous_cargo - state.ship_fuel;
          if (hull_mass < 0.0) {
            hull_mass = 0.0;
          }

          state.ship_cargo += cmd.scalar;
          if (state.ship_cargo < 0.0) {
            state.ship_cargo = 0.0;
          }
          if (max_cargo > 0.0 && state.ship_cargo > max_cargo) {
            state.ship_cargo = max_cargo;
          }

          state.mass = hull_mass + state.ship_cargo + state.ship_fuel;
        } else if (state.kind == STATION) {
          state.station_cargo += cmd.scalar;
          if (state.station_cargo < 0.0) {
            state.station_cargo = 0.0;
          }
        }
        break;
      }
      case CollisionCommandType::kAdjustFuel: {
        if (state.kind == SHIP) {
          double max_fuel = state.ship_fuel_capacity;
          double previous_fuel = state.ship_fuel;
          double hull_mass = state.mass - state.ship_cargo - previous_fuel;
          if (hull_mass < 0.0) {
            hull_mass = 0.0;
          }

          state.ship_fuel += cmd.scalar;
          if (state.ship_fuel < 0.0) {
            state.ship_fuel = 0.0;
          }
          if (max_fuel > 0.0 && state.ship_fuel > max_fuel) {
            state.ship_fuel = max_fuel;
          }

          state.mass = hull_mass + state.ship_cargo + state.ship_fuel;
        }
        break;
      }
      case CollisionCommandType::kAdjustShield: {
        if (state.kind == SHIP) {
          double max_shield = state.ship_shield_capacity;
          state.ship_shield += cmd.scalar;
          if (state.ship_shield < 0.0) {
            state.ship_shield = 0.0;
          }
          if (max_shield > 0.0 && state.ship_shield > max_shield) {
            state.ship_shield = max_shield;
          }
        }
        break;
      }
      case CollisionCommandType::kSetDocked:
        if (state.kind == SHIP) {
          state.is_docked = cmd.bool_flag;
        }
        break;
      case CollisionCommandType::kKillSelf:
        state.is_alive = false;
        break;
      case CollisionCommandType::kSetVelocity:
        state.velocity = cmd.velocity;
        break;
      case CollisionCommandType::kSetPosition:
        state.position = cmd.position;
        break;
      default:
        break;
    }
  };

  for (size_t i = 0; i < collisions.size(); ++i) {
    CThing* obj1 = collisions[i].object1;
    CThing* obj2 = collisions[i].object2;

    // Skip if either object is already dead (killed by earlier command)
    if (!obj1->IsAlive() || !obj2->IsAlive()) continue;

    // Skip if either object has a pending kill command (multi-hit prevention)
    // Once an asteroid/ship is marked for death, it can't participate in further collisions
    if (pending_kills.count(obj1) > 0 || pending_kills.count(obj2) > 0) {
      if (g_pParser && g_pParser->verbose) {
        const char* reason = pending_kills.count(obj1) > 0 ? obj1->GetName() : obj2->GetName();
        printf("[COLLISION-SKIP] Skipping collision %s <-> %s: %s has pending kill\n",
               obj1->GetName(), obj2->GetName(), reason);
      }
      continue;
    }

    // ============================================================================
    // DOCKING STATE MACHINE - Part 2: Command Generation Filter
    // ============================================================================
    // This is the SECOND checkpoint in the docking system. At this point, collision pairs
    // have already been detected and we're generating collision commands.
    //
    // We need to filter out ships in STATES 2 and 3 (ships docking this turn):
    //
    // STATE 2: JUST_DOCKED (ship-station collision processed earlier this loop)
    //   - Ship has IsDocked() = true (set by earlier collision command)
    //   - This state wasn't filtered in Part 1 because WasDocked() = false
    //   - Must filter here to prevent ship from colliding with other objects after docking
    //
    // STATE 3: PENDING_DOCK (ship has kSetDocked command queued)
    //   - Ship has pending_docks.count(ship) > 0
    //   - Command not yet applied, but ship should be treated as intangible
    //   - Prevents ship from generating multiple collision outcomes this turn
    //
    // Combined check: Ship is "docked" if IsDocked() OR in pending_docks set
    //
    // Exception: Ship-station collisions are ALWAYS processed (undocking mechanics)
    // Docked ships are intangible to non-stations (game rule).
    // This applies to:
    // 1. Ships already docked from previous turn/tick (IsDocked() == true, filtered in Part 1)
    // 2. Ships that just docked this turn (IsDocked() == true, filtered HERE)
    // 3. Ships with pending docking command from earlier collision this turn (filtered HERE)

    // Check if this is a ship-station collision (special case)
    bool is_ship_station_collision = (obj1->GetKind() == SHIP && obj2->GetKind() == STATION) ||
                                     (obj1->GetKind() == STATION && obj2->GetKind() == SHIP);

    if (!is_ship_station_collision) {
      // For non-ship-station collisions, apply docking rules
      bool obj1_docked = (obj1->GetKind() == SHIP) &&
                         (((CShip*)obj1)->IsDocked() || pending_docks.count(obj1) > 0);
      bool obj2_docked = (obj2->GetKind() == SHIP) &&
                         (((CShip*)obj2)->IsDocked() || pending_docks.count(obj2) > 0);

      if (obj1_docked || obj2_docked) {
        if (g_pParser && g_pParser->verbose) {
          const char* docker = obj1_docked ? obj1->GetName() : obj2->GetName();
          const char* reason = "";
          if (obj1_docked && ((CShip*)obj1)->IsDocked()) reason = "already docked";
          else if (obj2_docked && ((CShip*)obj2)->IsDocked()) reason = "already docked";
          else if (obj1_docked && pending_docks.count(obj1) > 0) reason = "docking this turn";
          else if (obj2_docked && pending_docks.count(obj2) > 0) reason = "docking this turn";

          printf("[COLLISION-SKIP] Skipping collision %s <-> %s: %s is %s\n",
                 obj1->GetName(), obj2->GetName(), docker, reason);
        }
        continue;
      }
    }

    // Generate random separation angle for this collision pair
    // (used only for ship-ship collisions at same position with same velocity)
    double random_angle = angle_dist(rng_angle);

    // Build contexts for both directions (both get same random angle)
    CollisionContext ctx1(this, &current_states[obj1], &current_states[obj2], 1.0,
                          use_new_physics, disable_eat_damage, use_docking_fix, random_angle);
    CollisionContext ctx2(this, &current_states[obj2], &current_states[obj1], 1.0,
                          use_new_physics, disable_eat_damage, use_docking_fix, random_angle);

    // Generate commands from both perspectives
    CollisionOutcome out1 = obj1->GenerateCollisionCommands(ctx1);
    CollisionOutcome out2 = obj2->GenerateCollisionCommands(ctx2);

    // Collect commands and track kills and docking
    for (int j = 0; j < out1.command_count; ++j) {
      const CollisionCommand& cmd = out1.commands[j];
      if (cmd.type == CollisionCommandType::kKillSelf) {
        pending_kills.insert(cmd.target);
      }
      // Track ships with pending docking commands - they can't collide with anything else
      if (cmd.type == CollisionCommandType::kSetDocked) {
        pending_docks.insert(cmd.target);
      }
      apply_command_to_state(cmd);
      all_commands.push_back(cmd);
    }
    for (int j = 0; j < out2.command_count; ++j) {
      const CollisionCommand& cmd = out2.commands[j];
      if (cmd.type == CollisionCommandType::kKillSelf) {
        pending_kills.insert(cmd.target);
      }
      // Track ships with pending docking commands - they can't collide with anything else
      if (cmd.type == CollisionCommandType::kSetDocked) {
        pending_docks.insert(cmd.target);
      }
      apply_command_to_state(cmd);
      all_commands.push_back(cmd);
    }

    // Collect spawn requests
    for (int j = 0; j < out1.spawn_count; ++j) {
      all_spawns.push_back(out1.spawns[j]);
    }
    for (int j = 0; j < out2.spawn_count; ++j) {
      all_spawns.push_back(out2.spawns[j]);
    }
  }

  // Stage 4: Sort commands by priority
  std::sort(all_commands.begin(), all_commands.end(),
            [](const CollisionCommand& a, const CollisionCommand& b) {
              return GetCommandTypePriority(a.type) < GetCommandTypePriority(b.type);
            });

  // Stage 5: Apply commands in deterministic order
  CollisionContext apply_ctx(this, NULL, NULL, 1.0, use_new_physics, disable_eat_damage, use_docking_fix);

  for (size_t i = 0; i < all_commands.size(); ++i) {
    const CollisionCommand& cmd = all_commands[i];

    // Handle announcer messages
    if (cmd.type == CollisionCommandType::kAnnounceMessage) {
      if (cmd.message_buffer[0] != '\0') {
        AddAnnouncerMessage(cmd.message_buffer);
      }
      continue;
    }

    // Skip if target is null or already dead
    // Exception: Metadata commands (kRecordEatenBy, kAnnounceMessage) can run on dead objects
    // This allows ownership tracking even after asteroids are destroyed (kKillSelf runs first)
    if (!cmd.target || !cmd.target->IsAlive()) {
      bool is_metadata_command = (cmd.type == CollisionCommandType::kRecordEatenBy ||
                                  cmd.type == CollisionCommandType::kAnnounceMessage);
      if (!is_metadata_command) {
        continue;  // Skip physical commands on dead objects
      }
    }

    // Apply the command
    cmd.target->ApplyCollisionCommand(cmd, apply_ctx);
  }

  // Stage 5.5: Log final separation distances after all commands applied (verbose mode)
  if (g_pParser && g_pParser->verbose && collisions.size() > 0) {
    printf("\n[COLLISION-FINAL] After applying all %zu collision commands:\n", all_commands.size());

    // Track which pairs we've already logged (to avoid duplicate A-B and B-A logs)
    std::set<std::pair<CThing*, CThing*>> logged_pairs;

    for (size_t i = 0; i < collisions.size(); ++i) {
      CThing* obj1 = collisions[i].object1;
      CThing* obj2 = collisions[i].object2;

      // Canonicalize pair: Use world index for deterministic ordering
      if (obj1->GetWorldIndex() > obj2->GetWorldIndex()) std::swap(obj1, obj2);
      std::pair<CThing*, CThing*> pair_key(obj1, obj2);

      if (logged_pairs.count(pair_key) > 0) {
        continue;  // Already logged this pair
      }
      logged_pairs.insert(pair_key);

      // Only log ship-ship collisions for clarity
      if (obj1->GetKind() == SHIP && obj2->GetKind() == SHIP) {
        double final_dist = obj1->GetPos().DistTo(obj2->GetPos());
        double collision_threshold = obj1->GetSize() + obj2->GetSize();  // 24 for ships

        printf("  %s <-> %s: dist=%.3f (threshold=%.1f) %s\n",
               obj1->GetName(), obj2->GetName(),
               final_dist, collision_threshold,
               (final_dist > collision_threshold) ? "CLEAR" : "STILL OVERLAPPING!");
      }
    }
    printf("\n");
  }

  // Stage 6: Process spawn requests (create new objects)
  for (size_t i = 0; i < all_spawns.size(); ++i) {
    const SpawnRequest& spawn = all_spawns[i];

    if (spawn.kind == ASTEROID) {
      CAsteroid* fragment = new CAsteroid(spawn.mass, spawn.material);
      CCoord pos = spawn.position;  // Copy to non-const for SetPos
      CTraj vel = spawn.velocity;   // Copy to non-const for SetVel
      fragment->SetPos(pos);
      fragment->SetVel(vel);
      AddThingToWorld(fragment);
    }
    // Other spawn types can be added here if needed
  }

  return collisions.size();
}

unsigned int CWorld::AddNewThings() {
  unsigned int URes, UInd;

  if (numNewThings == 0) {
    return 0;  // Duh.
  }

  for (URes = 0; URes < numNewThings; ++URes) {
    UInd = ULastIndex + 1;
    if (ULastIndex == (unsigned int)-1) {
      UInd = 0;  // Might as well make it explicit
    }
    if (URes >= MAX_THINGS) {
      break;  // Can't hold anymore!!
    }

    apThings[UInd] = apTAddQueue[URes];
    apThings[UInd]->SetWorld(this);
    apThings[UInd]->SetWorldIndex(UInd);

    aUPrevInd[UInd] = ULastIndex;

    if (ULastIndex == (unsigned int)-1) {
      UFirstIndex = UInd;
    } else {
      aUNextInd[ULastIndex] = UInd;
    }

    ULastIndex = UInd;
  }

  numNewThings = 0;
  return URes;
}

unsigned int CWorld::KillDeadThings() {
  CThing* pTTry;
  unsigned int URes = 0, index, ShNum;
  CTeam* pTm;
  ThingKind KTry;

  for (index = UFirstIndex; index != (unsigned int)-1; index = GetNextIndex(index)) {
    pTTry = GetThing(index);

    if ((pTTry->IsAlive()) != true) {
      RemoveIndex(index);
      URes++;

      KTry = pTTry->GetKind();
      if (KTry == SHIP) {
        pTm = ((CShip*)pTTry)->GetTeam();
        if (pTm != NULL) {
          ShNum = ((CShip*)pTTry)->GetShipNumber();
          pTm->SetShip(ShNum, NULL);
        }
      }

      delete pTTry;
      continue;
    }
  }

  return URes;
}

void CWorld::ReLinkList() {
  unsigned int i, ilast = (unsigned int)-1;
  CThing* pTh;

  for (i = 0; i < MAX_THINGS; ++i) {
    pTh = apThings[i];
    if (pTh == NULL) {
      continue;
    }

    aUPrevInd[i] = ilast;
    if (ilast != (unsigned int)-1) {
      aUNextInd[ilast] = i;
    } else {
      UFirstIndex = i;
    }

    ilast = i;
  }

  ULastIndex = ilast;
}

double CWorld::GetTimeStamp() {
  struct timeval tp;
  gettimeofday(&tp, NULL);

  double res = (double)(tp.tv_sec);         // Seconds
  res += (double)(tp.tv_usec) / 1000000.0;  // microseconds
  return res;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CWorld::GetSerialSize() const {
  unsigned int totsize = 0;
  CThing* pTh;

  totsize += BufWrite(NULL, UFirstIndex);
  totsize += BufWrite(NULL, ULastIndex);
  totsize += BufWrite(NULL, gametime);
  totsize += BufWrite(NULL, AnnouncerText, maxAnnouncerTextLen);

  unsigned int i, inext, sz, iTm, crc = 666, uTK = 0;

  for (i = 0; i < numTeams; ++i) {
    totsize += BufWrite(NULL, auClock[i]);
    totsize += GetTeam(i)->GetSerialSize();
  }

  unsigned int tk = 0;
  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pTh = GetThing(i);
    tk++;
    sz = pTh->GetSerialSize();
    inext = GetNextIndex(i);

    iTm = 0;
    totsize += BufWrite(NULL, crc);
    totsize += BufWrite(NULL, inext);
    totsize += BufWrite(NULL, sz);

    totsize += BufWrite(NULL, uTK);
    totsize += BufWrite(NULL, iTm);

    totsize += pTh->GetSerialSize();
  }

  return totsize;
}

unsigned CWorld::SerialPack(char* buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;
  CThing* pTh;

  vpb += BufWrite(vpb, UFirstIndex);
  vpb += BufWrite(vpb, ULastIndex);
  vpb += BufWrite(vpb, gametime);
  vpb += BufWrite(vpb, AnnouncerText, maxAnnouncerTextLen);

  unsigned int i, inext, sz, iTm, crc = 666, uTK;
  ThingKind TKind;
  CTeam* ptTeam;

  for (i = 0; i < numTeams; ++i) {
    vpb += BufWrite(vpb, auClock[i]);
    vpb += GetTeam(i)->SerialPack(vpb, buflen - (vpb - buf));
  }

  unsigned int tk = 0;
  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pTh = GetThing(i);
    tk++;
    sz = pTh->GetSerialSize();
    TKind = pTh->GetKind();
    inext = GetNextIndex(i);

    iTm = 0;
    if ((ptTeam = pTh->GetTeam()) != NULL) {
      iTm = ptTeam->GetWorldIndex();
    }
    if (TKind == SHIP) {
      iTm |= (((CShip*)pTh)->GetShipNumber()) << 8;
    }
    if (TKind == ASTEROID) {
      iTm = (unsigned int)((CAsteroid*)pTh)->GetMaterial();
    }

    vpb += BufWrite(vpb, crc);
    vpb += BufWrite(vpb, inext);
    vpb += BufWrite(vpb, sz);

    uTK = (unsigned int)TKind;
    vpb += BufWrite(vpb, uTK);
    vpb += BufWrite(vpb, iTm);

    vpb += pTh->SerialPack((char*)vpb, sz);
  }

  return (vpb - buf);
}

unsigned CWorld::SerialUnpack(char* buf, unsigned buflen) {
  char* vpb = buf;
  CThing* pTh;
  CAsteroid ATmp;

  unsigned int i, inext, ilast, crc, uTK;
  unsigned int sz, acsz, iTm;
  ThingKind TKind;
  unsigned int tk = 0;

  vpb += BufRead(vpb, inext);
  vpb += BufRead(vpb, ilast);
  vpb += BufRead(vpb, gametime);
  vpb += BufRead(vpb, AnnouncerText, maxAnnouncerTextLen);

  // CRITICAL FIX: Ensure announcer text is null-terminated after deserialization
  // BufRead copies all maxAnnouncerTextLen bytes, which may include garbage after
  // the actual message. Force null termination at the last position.
  AnnouncerText[maxAnnouncerTextLen - 1] = '\0';

  for (i = 0; i < numTeams; ++i) {
    vpb += BufRead(vpb, auClock[i]);
    vpb += GetTeam(i)->SerialUnpack(vpb, buflen - (vpb - buf));
  }

  for (i = UFirstIndex; i <= ilast; ++i) {
    pTh = GetThing(i);
    if (pTh != NULL && i < inext) {
      pTh->KillThing();
    }

    if (i == inext) {
      tk++;
      vpb += BufRead(vpb, crc);
      if (crc != 666) {
        printf("Off-track!!, %d\n", crc);
      }

      vpb += BufRead(vpb, inext);
      vpb += BufRead(vpb, sz);
      vpb += BufRead(vpb, uTK);
      TKind = (ThingKind)uTK;

      vpb += BufRead(vpb, iTm);

      if (pTh == NULL) {
        pTh = CreateNewThing(TKind, iTm);
        apThings[i] = pTh;
      }
      acsz = pTh->SerialUnpack((char*)vpb, sz);
      if (acsz != sz) {
        printf("Serialization discrepancy, %d!=%d\n", acsz, sz);
      }

      pTh->SetWorld(this);
      pTh->SetWorldIndex(i);

      vpb += acsz;
      if (vpb >= buf + buflen) {
        break;  // stooooooppppppp!!
      }
      if (inext == (unsigned int)-1) {
        break;
      }
    }
  }

  if (ilast < ULastIndex) {  // Stuff died at the end of the list
    for (i = ilast + 1; i <= ULastIndex; ++i) {
      pTh = GetThing(i);
      if (pTh != NULL) {
        pTh->KillThing();
      }
    }
  }

  KillDeadThings();
  ReLinkList();

  return (vpb - buf);
}

CThing* CWorld::CreateNewThing(ThingKind TKind, unsigned int iTm) {
  CThing *pTh, *pThOld;
  CTeam* pTeam;
  unsigned int shnum = 0;
  pThOld = NULL;

  shnum = iTm >> 8;
  iTm = iTm & 0xff;
  pTeam = GetTeam(iTm);

  switch (TKind) {
    case STATION:
      pTh = new CStation(CCoord(0.0, 0.0));
      if (pTeam != NULL) {
        pThOld = pTeam->SetStation((CStation*)pTh);
      }
      break;

    case SHIP:
      pTh = new CShip(CCoord(0.0, 0.0));
      if (pTeam != NULL) {
        pThOld = pTeam->SetShip(shnum, (CShip*)pTh);
      }
      break;

    case ASTEROID:
      pTh = new CAsteroid();
      break;

    default:
      pTh = new CThing;
  }

  if (pThOld != NULL) {
    delete pThOld;
  }
  return pTh;
}
