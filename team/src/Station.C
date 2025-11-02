/* Station.C
 * Implementation of the CStation class
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include "ParserModern.h"
#include "GameConstants.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "CollisionTypes.h"  // For deterministic collision engine

extern CParser* g_pParser;

///////////////////////////////////////////
// Construction/Destruction

CStation::CStation(CCoord StPos, CTeam* pTeam) : CThing(StPos.fX, StPos.fY) {
  TKind = STATION;
  pmyTeam = pTeam;

  if (pmyTeam != NULL) {
    snprintf(Name, maxnamelen, "Station #%d", pmyTeam->GetTeamNumber());
    uImgSet = pTeam->uImgSet;
  } else {
    snprintf(Name, maxnamelen, "Station");
    uImgSet = 0;
  }

  size = g_station_spawn_size;
  mass = g_station_spawn_mass;
  orient = 0.0;
  omega = g_station_spawn_spin_rate;
  dCargo = 0.0;
}

CStation::~CStation() {}

////////////////////////////////////////////
// Data access

double CStation::GetVinylStore() const { return dCargo; }

// Deterministic collision engine - create snapshot with station-specific fields
CollisionState CStation::MakeCollisionState() const {
  // Start with base class snapshot
  CollisionState state = CThing::MakeCollisionState();

  // Populate station-specific fields
  state.station_cargo = dCargo;

  return state;
}

// Deterministic collision engine - apply station-specific commands
void CStation::ApplyCollisionCommandDerived(const CollisionCommand& cmd, const CollisionContext& ctx) {
  (void)ctx;
  // This method handles station-specific command types
  // Base class already handled kKillSelf, kSetVelocity, kSetPosition

  switch (cmd.type) {
    case CollisionCommandType::kAdjustCargo: {
      // Adjust vinyl cargo by delta (can be negative for laser damage)
      dCargo += cmd.scalar;

      // Clamp to minimum of 0 (stations have no maximum cargo capacity)
      if (dCargo < 0.0) {
        dCargo = 0.0;
      }
      break;
    }

    default:
      // Other command types not handled by stations
      break;
  }
}

// Deterministic collision engine - generate collision commands from snapshots
CollisionOutcome CStation::GenerateCollisionCommands(const CollisionContext& ctx) {
  // This method reads from immutable snapshots and emits commands
  // It does NOT mutate any object state

  CollisionOutcome outcome;

  // Get snapshots from context
  const CollisionState* self_state = ctx.self_state;
  const CollisionState* other_state = ctx.other_state;

  // Sanity checks
  if (self_state == NULL || other_state == NULL) {
    return outcome;  // Empty outcome
  }

  if (self_state->kind != STATION) {
    return outcome;  // Wrong kind, shouldn't happen
  }

  // Station collision logic
  ThingKind other_kind = other_state->kind;

  // Ship-station collision: Ships dock (handled by Ship's collision handler)
  if (other_kind == SHIP) {
    // Station does nothing, ship handles docking
    return outcome;
  }

  // Laser-station collision: Laser depletes vinyl cargo
  if (other_kind == GENTHING) {
    // Calculate damage from laser mass
    double laser_mass = other_state->mass;
    double damage = laser_mass / g_laser_damage_mass_divisor;

    // Emit command to reduce station cargo
    outcome.AddCommand(CollisionCommand::AdjustCargo(self_state->thing, -damage));

    // Emit announcement if significant damage
    if (damage > 0.01 && ctx.world) {
      char msg[256];
      snprintf(msg, sizeof(msg), "%s hit by laser, %.1f vinyl lost",
               self_state->thing->GetName(), damage);
      outcome.AddCommand(CollisionCommand::Announce(msg));
    }

    return outcome;
  }

  // Other collision types don't affect stations
  return outcome;
}

/////////////////////////////////////////////
// Explicit functions

double CStation::AddVinyl(double dvtons) {
  dCargo += dvtons;
  return dCargo;
}

/////////////////////////////////////////////
// Protected methods

void CStation::HandleCollision(CThing* pOthThing, CWorld* pWorld) {
  if (g_pParser && !g_pParser->UseNewFeature("collision-handling")) {
    HandleCollisionOld(pOthThing, pWorld);
  } else {
    HandleCollisionNew(pOthThing, pWorld);
  }
}

void CStation::HandleCollisionOld(CThing* pOthThing, CWorld* pWorld) {
  // LEGACY COLLISION HANDLING
  // Preserves original behavior for station collisions

  ThingKind OthKind = pOthThing->GetKind();
  CTraj myVel(GetVelocity());
  CCoord myPos(GetPos());

  if (OthKind == SHIP) {
    bIsColliding = g_no_damage_sentinel;
    return;
  }

  if (OthKind != GENTHING) {
    return;  // Only gets lased
  }
  if (pWorld == NULL) {
    return;  // Colliding in a vaccuum?
  }

  double dDmg = pOthThing->GetMass();  // Laser object, whuppage is mass
  dDmg /= g_laser_damage_mass_divisor;  // Takes a kilowhup
  double oldCargo = dCargo;
  dCargo -= dDmg;
  if (dCargo < 0.0) {
    dCargo = 0.0;
  }

  // Log station damage
  if (dDmg > 0.01) {
    printf(
        "[STATION DAMAGE] Station %s (%s) lost %.2f vinyl from laser (%.2f -> "
        "%.2f)\n",
        GetName(), GetTeam() ? GetTeam()->GetName() : "Unknown", dDmg, oldCargo,
        dCargo);
    if (pWorld) {
      char msg[256];
      snprintf(msg, sizeof(msg), "%s hit by laser, %.1f vinyl lost",
               GetName(), dDmg);
      pWorld->AddAnnouncerMessage(msg);
    }
  }
}

void CStation::HandleCollisionNew(CThing* pOthThing, CWorld* pWorld) {
  // NEW COLLISION HANDLING
  // Will be updated to remove duplicate collision processing and improve physics

  ThingKind OthKind = pOthThing->GetKind();
  CTraj myVel(GetVelocity());
  CCoord myPos(GetPos());

  if (OthKind == SHIP) {
    bIsColliding = g_no_damage_sentinel;
    return;
  }

  if (OthKind != GENTHING) {
    return;  // Only gets lased
  }
  if (pWorld == NULL) {
    return;  // Colliding in a vaccuum?
  }

  double dDmg = pOthThing->GetMass();  // Laser object, whuppage is mass
  dDmg /= g_laser_damage_mass_divisor;  // Takes a kilowhup
  double oldCargo = dCargo;
  dCargo -= dDmg;
  if (dCargo < 0.0) {
    dCargo = 0.0;
  }

  // Log station damage
  if (dDmg > 0.01) {
    printf(
        "[STATION DAMAGE] Station %s (%s) lost %.2f vinyl from laser (%.2f -> "
        "%.2f)\n",
        GetName(), GetTeam() ? GetTeam()->GetName() : "Unknown", dDmg, oldCargo,
        dCargo);
    if (pWorld) {
      char msg[256];
      snprintf(msg, sizeof(msg), "%s hit by laser, %.1f vinyl lost",
               GetName(), dDmg);
      pWorld->AddAnnouncerMessage(msg);
    }
  }
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CStation::GetSerialSize() const {
  unsigned int totsize = 0;

  totsize += CThing::GetSerialSize();
  totsize += BufWrite(NULL, dCargo);

  return totsize;
}

unsigned CStation::SerialPack(char* buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += (CThing::SerialPack(buf, buflen));
  vpb += BufWrite(vpb, dCargo);

  return (vpb - buf);
}

unsigned CStation::SerialUnpack(char* buf, unsigned buflen) {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += (CThing::SerialUnpack(buf, buflen));
  vpb += BufRead(vpb, dCargo);

  return (vpb - buf);
}
