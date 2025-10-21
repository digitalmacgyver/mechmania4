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

extern CParser* g_pParser;

//////////////////////////////////////////////////
// Construction/Destruction

CWorld::CWorld(unsigned int nTm)
    : collision_rng_(0x4D4D3434u),  // "MM44" in hex for deterministic seed
      ship_collision_angle_dist_(-PI, PI) {
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
  pWld->collision_rng_ = collision_rng_;
  pWld->ship_collision_angle_dist_ = ship_collision_angle_dist_;

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

// Safe announcer messaging interface
MessageResult CWorld::SetAnnouncerMessage(const char* message) {
  if (message == NULL) {
    ClearAnnouncerMessage();
    return MSG_SUCCESS;
  }

  size_t messageLen = strlen(message);

  // Check if message fits completely (including null terminator)
  if (messageLen < maxAnnouncerTextLen) {
    strncpy(AnnouncerText, message, maxAnnouncerTextLen - 1);
    AnnouncerText[maxAnnouncerTextLen - 1] = '\0';  // Always null-terminate

    // Log announcer messages to stdout for debugging
        if (g_pParser && g_pParser->verbose) {
      printf("[ANNOUNCER] %s\n", message);
    }

    return MSG_SUCCESS;
  } else {
    // Truncate to fit
    strncpy(AnnouncerText, message, maxAnnouncerTextLen - 1);
    AnnouncerText[maxAnnouncerTextLen - 1] = '\0';

    // Log announcer messages to stdout for debugging
        if (g_pParser && g_pParser->verbose) {
      printf("[ANNOUNCER] %s (TRUNCATED)\n", message);
    }

    return MSG_TRUNCATED;
  }
}

MessageResult CWorld::AppendAnnouncerMessage(const char* message) {
  if (message == NULL || message[0] == '\0') {
    return MSG_SUCCESS;
  }

  size_t currentLen = strlen(AnnouncerText);
  size_t messageLen = strlen(message);
  size_t availableSpace = maxAnnouncerTextLen - currentLen - 1;  // -1 for null terminator

  // No space available
  if (availableSpace == 0) {
    return MSG_NO_SPACE;
  }

  // Add newline if buffer is not empty
  if (currentLen > 0 && availableSpace > 1) {
    strcat(AnnouncerText, "\n");
    currentLen++;
    availableSpace--;
  }

  // Message fits completely
  if (messageLen <= availableSpace) {
    strncat(AnnouncerText, message, availableSpace);

    // Log announcer messages to stdout for debugging
        if (g_pParser && g_pParser->verbose) {
      printf("[ANNOUNCER] %s\n", message);
    }

    return MSG_SUCCESS;
  } else {
    // Truncate to fit available space
    strncat(AnnouncerText, message, availableSpace);
    AnnouncerText[maxAnnouncerTextLen - 1] = '\0';  // Ensure null termination

    // Log announcer messages to stdout for debugging
        if (g_pParser && g_pParser->verbose) {
      printf("[ANNOUNCER] %s (TRUNCATED)\n", message);
    }

    return MSG_TRUNCATED;
  }
}

void CWorld::ClearAnnouncerMessage() {
  AnnouncerText[0] = '\0';
}

// Legacy interface - wraps AppendAnnouncerMessage for backward compatibility
void CWorld::AddAnnouncerMessage(const char* message) {
  AppendAnnouncerMessage(message);
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
   * requested length. Targets can then use the laser "mass" (damage value) to decide
   * effects (e.g., asteroids shatter if damage >= 1000, ships lose shields/1000 units).
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

  
  unsigned int nteam, nship;
  CTeam* pTeam;
  CShip* pShip;
  CThing *pTarget, LasThing;
  CCoord LasPos, TmpPos;
  CTraj LasTraj, TarVel, TmpTraj;
  double dLasPwr, dLasRng;

  std::vector<CollisionCommand> all_commands;
  std::vector<SpawnRequest> all_spawns;
  std::map<CThing*, CollisionState> current_states;

  bool use_new_physics = g_pParser ? g_pParser->UseNewFeature("physics") : true;
  bool disable_eat_damage = g_pParser ? g_pParser->UseNewFeature("asteroid-eat-damage") : true;
  bool use_docking_fix = g_pParser ? g_pParser->UseNewFeature("docking") : true;

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

        // Use existing snapshot if target already processed this frame
        CollisionState* current_target_state = nullptr;
        auto current_it = current_states.find(pTarget);
        if (current_it == current_states.end()) {
          current_it = current_states.emplace(pTarget, target_state).first;
        }
        current_target_state = &current_it->second;

        // Skip if target already marked dead this frame
        if (!current_target_state->is_alive) {
          continue;
        }

        CollisionContext ctx(this, current_target_state, &laser_state, 1.0,
                             use_new_physics, disable_eat_damage, use_docking_fix);

        // Generate commands from target's perspective (target being hit by laser)
        CollisionOutcome outcome = pTarget->GenerateCollisionCommands(ctx);

        for (unsigned int i = 0; i < outcome.command_count; ++i) {
          const CollisionCommand& cmd = outcome.commands[i];

          ApplyCommandToSnapshot(cmd, current_states);
          all_commands.push_back(cmd);
        }

        // Process spawn requests (asteroid fragments)
        for (unsigned int i = 0; i < outcome.spawn_count; ++i) {
          const SpawnRequest& spawn = outcome.spawns[i];
          all_spawns.push_back(spawn);
        }
      }
    }
  }

  // Apply commands in deterministic order
  std::sort(all_commands.begin(), all_commands.end(),
            [](const CollisionCommand& a, const CollisionCommand& b) {
              return GetCommandTypePriority(a.type) < GetCommandTypePriority(b.type);
            });

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

    // Skip if target is null or already dead (unless metadata)
    bool is_metadata_command =
        (cmd.type == CollisionCommandType::kRecordEatenBy ||
         cmd.type == CollisionCommandType::kAnnounceMessage);

    if (!cmd.target || !cmd.target->IsAlive()) {
      if (!is_metadata_command) {
        continue;
      }
    }

    cmd.target->ApplyCollisionCommand(cmd, apply_ctx);
  }

  // Spawn new objects after all commands applied
  for (size_t i = 0; i < all_spawns.size(); ++i) {
    const SpawnRequest& spawn = all_spawns[i];
    if (spawn.kind == ASTEROID) {
      CAsteroid* fragment = new CAsteroid(spawn.mass, spawn.material);
      CCoord pos = spawn.position;
      CTraj vel = spawn.velocity;
      fragment->SetPos(pos);
      fragment->SetVel(vel);
      AddThingToWorld(fragment);
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

void CWorld::ApplyCommandToSnapshot(const CollisionCommand& cmd,
                                    std::map<CThing*, CollisionState>& states) {
  if (cmd.target == NULL) {
    return;
  }

  auto it = states.find(cmd.target);
  if (it == states.end()) {
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
}

void CWorld::CollectCollisionSnapshots(std::map<CThing*, CollisionState>& snapshots,
                                       std::map<CThing*, CollisionState>& current_states) const {
  snapshots.clear();
  current_states.clear();

  for (unsigned int idx = UFirstIndex; idx != (unsigned int)-1; idx = GetNextIndex(idx)) {
    CThing* thing = GetThing(idx);
    if (thing && thing->IsAlive()) {
      snapshots[thing] = thing->MakeCollisionState();
    }
  }

  current_states = snapshots;
}

void CWorld::CollectTeamObjects(CThing** team_objects, unsigned int& num_team_objects) const {
  num_team_objects = 0;

  for (unsigned int team_idx = 0; team_idx < GetNumTeams(); ++team_idx) {
    CTeam* team = GetTeam(team_idx);
    if (team == NULL) {
      continue;
    }

    team_objects[num_team_objects++] = team->GetStation();

    if (bGameOver) {
      continue;
    }

    for (unsigned int ship_idx = 0; ship_idx < team->GetShipCount(); ++ship_idx) {
      CThing* ship = team->GetShip(ship_idx);
      if (ship) {
        team_objects[num_team_objects++] = ship;
      }
    }
  }
}

std::vector<CollisionPair> CWorld::DetectCollisionPairs(
    const std::map<CThing*, CollisionState>& snapshots,
    CThing** team_objects,
    unsigned int num_team_objects) {
  std::vector<CollisionPair> collisions;
  std::set<std::pair<CThing*, CThing*>> processed_pairs;

  for (unsigned int world_idx = UFirstIndex; world_idx != (unsigned int)-1;
       world_idx = GetNextIndex(world_idx)) {
    CThing* world_object = GetThing(world_idx);
    if (!world_object || !world_object->IsAlive()) {
      continue;
    }

    for (unsigned int team_obj_idx = 0; team_obj_idx < num_team_objects; ++team_obj_idx) {
      CThing* team_object = team_objects[team_obj_idx];
      if (!team_object) {
        continue;
      }

      if (world_object == team_object) {
        continue;
      }

      CThing* obj1 = world_object;
      CThing* obj2 = team_object;
      if (obj1->GetWorldIndex() > obj2->GetWorldIndex()) {
        std::swap(obj1, obj2);
      }

      std::pair<CThing*, CThing*> pair_key(obj1, obj2);
      if (processed_pairs.count(pair_key) > 0) {
        continue;
      }

      ThingKind kind1 = world_object->GetKind();
      ThingKind kind2 = team_object->GetKind();

      const CollisionState* world_snapshot = nullptr;
      const CollisionState* team_snapshot = nullptr;

      auto world_snapshot_it = snapshots.find(world_object);
      if (world_snapshot_it != snapshots.end()) {
        world_snapshot = &world_snapshot_it->second;
      }
      auto team_snapshot_it = snapshots.find(team_object);
      if (team_snapshot_it != snapshots.end()) {
        team_snapshot = &team_snapshot_it->second;
      }

      if (kind1 == ASTEROID && kind2 == ASTEROID) {
        continue;
      }

      if (kind1 == SHIP && kind2 != STATION && world_snapshot != nullptr) {
        if (world_snapshot->is_docked && world_snapshot->was_docked) {
          continue;
        }
      }
      if (kind2 == SHIP && kind1 != STATION && team_snapshot != nullptr) {
        if (team_snapshot->is_docked && team_snapshot->was_docked) {
          continue;
        }
      }

      double radius1 = world_object->GetSize();
      double radius2 = team_object->GetSize();
      double center_distance = world_object->GetPos().DistTo(team_object->GetPos());
      double overlap = (radius1 + radius2) - center_distance;

      if (overlap >= 0.0) {
        processed_pairs.insert(pair_key);
        collisions.push_back({world_object, team_object, overlap});

        if (g_pParser && g_pParser->verbose) {
          CCoord pos1 = world_object->GetPos();
          CCoord pos2 = team_object->GetPos();
          CTraj vel1 = world_object->GetVelocity();
          CTraj vel2 = team_object->GetVelocity();
          unsigned int turn = GetCurrentTurn();

          const char* kind1_str = (kind1 == SHIP) ? "SHIP" :
                                   (kind1 == STATION) ? "STATION" :
                                   (kind1 == ASTEROID) ? "ASTEROID" : "LASER";
          const char* kind2_str = (kind2 == SHIP) ? "SHIP" :
                                   (kind2 == STATION) ? "STATION" :
                                   (kind2 == ASTEROID) ? "ASTEROID" : "LASER";

          const char* docking_status = "";
          if ((kind1 == SHIP && kind2 == STATION) || (kind1 == STATION && kind2 == SHIP)) {
            const CollisionState* ship_snapshot =
                (kind1 == SHIP) ? world_snapshot : team_snapshot;
            if (ship_snapshot != nullptr) {
              if (ship_snapshot->is_docked && !ship_snapshot->was_docked) {
                docking_status = " [SHIP-JUST-DOCKED]";
              } else if (ship_snapshot->is_docked && ship_snapshot->was_docked) {
                docking_status = " [SHIP-ALREADY-DOCKED]";
              }
            }
          }

          printf("COLLISION_DETECTED: Turn %u: %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f <-> %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f | dist=%.3f overlap=%.3f%s\n",
                 turn,
                 world_object->GetName(), kind1_str, pos1.fX, pos1.fY, vel1.rho, vel1.theta * 180.0 / PI, world_object->GetSize(),
                 team_object->GetName(), kind2_str, pos2.fX, pos2.fY, vel2.rho, vel2.theta * 180.0 / PI, team_object->GetSize(),
                 center_distance, overlap, docking_status);
        }
      }
    }
  }

  return collisions;
}

void CWorld::SortAndShuffleCollisions(std::vector<CollisionPair>& collisions) {
  const double overlap_epsilon = 0.001;

  std::sort(collisions.begin(), collisions.end(),
            [](const CollisionPair& a, const CollisionPair& b) {
              return a.overlap_distance > b.overlap_distance;
            });

  size_t group_start = 0;
  while (group_start < collisions.size()) {
    double group_overlap = collisions[group_start].overlap_distance;
    size_t group_end = group_start + 1;

    while (group_end < collisions.size() &&
           fabs(collisions[group_end].overlap_distance - group_overlap) < overlap_epsilon) {
      group_end++;
    }

    if (group_end - group_start > 1) {
      std::shuffle(collisions.begin() + group_start,
                   collisions.begin() + group_end,
                   collision_rng_);
    }

    group_start = group_end;
  }

  if (g_pParser && g_pParser->verbose && !collisions.empty()) {
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
}

void CWorld::GenerateCollisionOutputs(
    const std::vector<CollisionPair>& collisions,
    std::map<CThing*, CollisionState>& current_states,
    std::vector<CollisionCommand>& all_commands,
    std::vector<SpawnRequest>& all_spawns,
    bool use_new_physics,
    bool disable_eat_damage,
    bool use_docking_fix) {
  std::set<CThing*> pending_kills;
  std::set<CThing*> pending_docks;

  for (const CollisionPair& pair : collisions) {
    CThing* obj1 = pair.object1;
    CThing* obj2 = pair.object2;

    if (pending_kills.count(obj1) > 0 || pending_kills.count(obj2) > 0) {
      continue;
    }

    bool obj1_pending_dock = pending_docks.count(obj1) > 0;
    bool obj2_pending_dock = pending_docks.count(obj2) > 0;
    bool obj1_is_ship = (obj1->GetKind() == SHIP);
    bool obj2_is_ship = (obj2->GetKind() == SHIP);

    if ((obj1_pending_dock && obj1_is_ship && obj2->GetKind() != STATION) ||
        (obj2_pending_dock && obj2_is_ship && obj1->GetKind() != STATION)) {
      if (g_pParser && g_pParser->verbose) {
        const char* docker = obj1_pending_dock ? obj1->GetName() : obj2->GetName();
        const char* reason = obj1_pending_dock ? "docking this turn" : "docking this turn";
        printf("[COLLISION-SKIP] Skipping collision %s <-> %s: %s is %s\n",
               obj1->GetName(), obj2->GetName(), docker, reason);
      }
      continue;
    }

    auto it1 = current_states.find(obj1);
    auto it2 = current_states.find(obj2);
    if (it1 == current_states.end() || it2 == current_states.end()) {
      continue;
    }

    CollisionState& state1 = it1->second;
    CollisionState& state2 = it2->second;

    if (!state1.is_alive || !state2.is_alive) {
      continue;
    }

    double random_angle = ship_collision_angle_dist_(collision_rng_);

    CollisionContext ctx1(this, &state1, &state2, 1.0,
                          use_new_physics, disable_eat_damage, use_docking_fix, random_angle);
    CollisionContext ctx2(this, &state2, &state1, 1.0,
                          use_new_physics, disable_eat_damage, use_docking_fix, random_angle);

    CollisionOutcome out1 = state1.thing->GenerateCollisionCommands(ctx1);
    CollisionOutcome out2 = state2.thing->GenerateCollisionCommands(ctx2);

    for (unsigned int j = 0; j < out1.command_count; ++j) {
      const CollisionCommand& cmd = out1.commands[j];
      if (cmd.type == CollisionCommandType::kKillSelf) {
        pending_kills.insert(cmd.target);
      }
      if (cmd.type == CollisionCommandType::kSetDocked) {
        pending_docks.insert(cmd.target);
      }
      ApplyCommandToSnapshot(cmd, current_states);
      all_commands.push_back(cmd);
    }

    for (unsigned int j = 0; j < out2.command_count; ++j) {
      const CollisionCommand& cmd = out2.commands[j];
      if (cmd.type == CollisionCommandType::kKillSelf) {
        pending_kills.insert(cmd.target);
      }
      if (cmd.type == CollisionCommandType::kSetDocked) {
        pending_docks.insert(cmd.target);
      }
      ApplyCommandToSnapshot(cmd, current_states);
      all_commands.push_back(cmd);
    }

    for (unsigned int j = 0; j < out1.spawn_count; ++j) {
      all_spawns.push_back(out1.spawns[j]);
    }
    for (unsigned int j = 0; j < out2.spawn_count; ++j) {
      all_spawns.push_back(out2.spawns[j]);
    }
  }
}

void CWorld::ApplyCollisionResults(const std::vector<CollisionPair>& collisions,
                                   const std::vector<CollisionCommand>& all_commands,
                                   const std::vector<SpawnRequest>& all_spawns,
                                   bool use_new_physics,
                                   bool disable_eat_damage,
                                   bool use_docking_fix) {
  std::vector<CollisionCommand> sorted_commands = all_commands;
  std::sort(sorted_commands.begin(), sorted_commands.end(),
            [](const CollisionCommand& a, const CollisionCommand& b) {
              return GetCommandTypePriority(a.type) < GetCommandTypePriority(b.type);
            });

  CollisionContext apply_ctx(this, NULL, NULL, 1.0, use_new_physics, disable_eat_damage, use_docking_fix);

  for (const CollisionCommand& cmd : sorted_commands) {
    if (cmd.type == CollisionCommandType::kAnnounceMessage) {
      if (cmd.message_buffer[0] != '\0') {
        AddAnnouncerMessage(cmd.message_buffer);
      }
      continue;
    }

    bool is_metadata_command =
        (cmd.type == CollisionCommandType::kRecordEatenBy ||
         cmd.type == CollisionCommandType::kAnnounceMessage);

    if (!cmd.target || !cmd.target->IsAlive()) {
      if (!is_metadata_command) {
        continue;
      }
    }

    cmd.target->ApplyCollisionCommand(cmd, apply_ctx);
  }

  if (g_pParser && g_pParser->verbose && !collisions.empty()) {
    printf("\n[COLLISION-FINAL] After applying all %zu collision commands:\n", sorted_commands.size());

    std::set<std::pair<CThing*, CThing*>> logged_pairs;

    for (const CollisionPair& pair : collisions) {
      CThing* obj1 = pair.object1;
      CThing* obj2 = pair.object2;
      if (obj1->GetWorldIndex() > obj2->GetWorldIndex()) {
        std::swap(obj1, obj2);
      }
      std::pair<CThing*, CThing*> key(obj1, obj2);
      if (logged_pairs.count(key) > 0) {
        continue;
      }
      logged_pairs.insert(key);

      if (obj1->GetKind() == SHIP && obj2->GetKind() == SHIP) {
        double final_dist = obj1->GetPos().DistTo(obj2->GetPos());
        double collision_threshold = obj1->GetSize() + obj2->GetSize();

        printf("  %s <-> %s: dist=%.3f (threshold=%.1f) %s\n",
               obj1->GetName(), obj2->GetName(),
               final_dist, collision_threshold,
               (final_dist > collision_threshold) ? "CLEAR" : "STILL OVERLAPPING!");
      }
    }
    printf("\n");
  }

  for (const SpawnRequest& spawn : all_spawns) {
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


unsigned int CWorld::CollisionEvaluationNew() {
  
  if (g_pParser && g_pParser->verbose) {
    printf("[COLLISION-ENGINE] Starting collision evaluation\n");
  }

  std::map<CThing*, CollisionState> snapshots;
  std::map<CThing*, CollisionState> current_states;
  CollectCollisionSnapshots(snapshots, current_states);

  CThing* team_objects[MAX_THINGS];
  unsigned int num_team_objects = 0;
  CollectTeamObjects(team_objects, num_team_objects);

  std::vector<CollisionPair> collisions =
      DetectCollisionPairs(snapshots, team_objects, num_team_objects);
  SortAndShuffleCollisions(collisions);

  if (g_pParser && g_pParser->verbose) {
    printf("[COLLISION-ENGINE] Total collisions detected: %zu\n", collisions.size());
  }

  std::vector<CollisionCommand> all_commands;
  std::vector<SpawnRequest> all_spawns;

  bool use_new_physics = g_pParser ? g_pParser->UseNewFeature("physics") : true;
  bool disable_eat_damage = g_pParser ? g_pParser->UseNewFeature("asteroid-eat-damage") : true;
  bool use_docking_fix = g_pParser ? g_pParser->UseNewFeature("docking") : true;

  GenerateCollisionOutputs(collisions, current_states, all_commands, all_spawns,
                           use_new_physics, disable_eat_damage, use_docking_fix);

  ApplyCollisionResults(collisions, all_commands, all_spawns,
                        use_new_physics, disable_eat_damage, use_docking_fix);

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
