/* Thing.C
 * Definition of class CThing
 * For use with MechMania IV
 * 4/29/98 by Misha Voloshin
 */

#include <cmath>  // For sqrt()

#include "Coord.h"
#include "GameConstants.h"
#include "ParserModern.h"
#include "Team.h"
#include "Thing.h"
#include "Traj.h"
#include "World.h"
#include "CollisionTypes.h"  // For deterministic collision engine

/////////////////////////////////////////////
// Construction/destruction

CThing::CThing(double fx0, double fy0) {
  TKind = GENTHING;
  for (unsigned int i = 0; i < maxnamelen; ++i) {
    Name[i] = 0;  // Initialize Name
  }

  snprintf(Name, maxnamelen, "Generic Thing");
  ulIDCookie = rand();
  DeadFlag = false;
  bIsColliding = g_no_damage_sentinel;
  bIsGettingShot = g_no_damage_sentinel;

  pmyTeam = NULL;
  pmyWorld = NULL;
  uWldIndex = (unsigned int)-1;

  Pos = CCoord(fx0, fy0);
  Vel = CTraj(0.0, 0.0);
  orient = 0.0;
  omega = 0.0;
  uImgSet = 0;

  mass = 1.0;
  size = 1.0;
}

CThing::CThing(const CThing& OthThing) { *this = OthThing; }

CThing::~CThing() {}

////////////////////////////////////////////////
// Data Access functions

/////
// Outgoing data

const CCoord& CThing::GetPos() const { return Pos; }

ThingKind CThing::GetKind() const { return TKind; }

double CThing::GetMass() const { return mass; }

double CThing::GetSize() const { return size; }

double CThing::GetOrient() const { return orient; }

const CTraj& CThing::GetVelocity() const { return Vel; }

CTraj CThing::GetMomentum() const { return Vel * GetMass(); }

CTeam* CThing::GetTeam() const { return pmyTeam; }

bool CThing::IsAlive() const {
  if (DeadFlag == true) {
    return false;
  }
  return true;
}

unsigned int CThing::GetImage() const { return uImgSet; }

const char* CThing::GetName() const { return Name; }

///////
// Incoming data

void CThing::SetName(const char* strsrc) {
  bool bGotZero = false;

  for (unsigned int i = 0; i < maxnamelen - 1; ++i) {
    if (bGotZero == true) {
      Name[i] = 0;
    }
    Name[i] = strsrc[i];
    if (Name[i] == '\n') {
      Name[i] = ' ';
    }
    if (Name[i] == 0) {
      bGotZero = true;
    }
  }

  Name[maxnamelen - 1] = 0;
}

void CThing::KillThing() { DeadFlag = true; }

////////////////////////////////////////////////
// Explicit methods

void CThing::Drift(double dt, double turn_phase) {
  // Base class ignores turn_phase parameter (only used by ships)
  bIsColliding = g_no_damage_sentinel;
  bIsGettingShot = g_no_damage_sentinel;
  if (Vel.rho > g_game_max_speed) {
    Vel.rho = g_game_max_speed;
  }

  Pos += (Vel * dt).ConvertToCoord();
  orient += omega * dt;

  if (orient < -PI || orient > PI) {
    CTraj VTmp(1.0, orient);
    VTmp.Normalize();
    orient = VTmp.theta;
  }
}

bool CThing::Collide(CThing* pOthThing, CWorld* pWorld) {
  extern CParser* g_pParser;

  if (g_pParser && !g_pParser->UseNewFeature("collision-handling")) {
    return CollideOld(pOthThing, pWorld);
  } else {
    return CollideNew(pOthThing, pWorld);
  }
}

bool CThing::CollideOld(CThing* pOthThing, CWorld* pWorld) {
  // LEGACY COLLISION BEHAVIOR
  // Preserves original MechMania IV collision processing.
  extern CParser* g_pParser;

  if (pOthThing == NULL) {
    printf("Colliding with NULL!\n");
    return false;  // How did THAT happen!!??
  }
  if (*pOthThing == *this) {
    return false;  // Can't collide with yourself!
  }

  if (Overlaps(*pOthThing) == false) {
    return false;
  }

  double dAng = GetPos().AngleTo(pOthThing->GetPos());
  if (pOthThing->GetKind() == GENTHING) {
    bIsGettingShot = dAng;
  } else {
    bIsColliding = dAng;
  }

  // Verbose logging for collision detection
  if (g_pParser && g_pParser->verbose) {
    // Check if we should skip logging to reduce noise from docked ships
    bool skip_logging = false;

    // Skip ship-station collisions if ship was previously docked
    if ((GetKind() == SHIP && pOthThing->GetKind() == STATION) ||
        (GetKind() == STATION && pOthThing->GetKind() == SHIP)) {
      CShip* ship = (GetKind() == SHIP) ? dynamic_cast<CShip*>(this) : dynamic_cast<CShip*>(pOthThing);
      if (ship && ship->WasDocked()) {
        skip_logging = true;
      }
    }

    // Skip ship-ship collisions if either ship is currently docked
    if (GetKind() == SHIP && pOthThing->GetKind() == SHIP) {
      CShip* ship1 = dynamic_cast<CShip*>(this);
      CShip* ship2 = dynamic_cast<CShip*>(pOthThing);
      if ((ship1 && ship1->IsDocked()) || (ship2 && ship2->IsDocked())) {
        skip_logging = true;
      }
    }

    if (!skip_logging) {
      CCoord pos1 = GetPos();
      CCoord pos2 = pOthThing->GetPos();
      double dist = sqrt((pos1.fX - pos2.fX) * (pos1.fX - pos2.fX) +
                         (pos1.fY - pos2.fY) * (pos1.fY - pos2.fY));
      double combined_size = GetSize() + pOthThing->GetSize();
      double overlap = combined_size - dist;

      CTraj vel1 = GetVelocity();
      CTraj vel2 = pOthThing->GetVelocity();

      const char* kind1 = (GetKind() == SHIP) ? "SHIP" : (GetKind() == STATION) ? "STATION" : (GetKind() == ASTEROID) ? "ASTEROID" : "GENTHING";
      const char* kind2 = (pOthThing->GetKind() == SHIP) ? "SHIP" : (pOthThing->GetKind() == STATION) ? "STATION" : (pOthThing->GetKind() == ASTEROID) ? "ASTEROID" : "GENTHING";

      unsigned int turn = (pWorld != NULL) ? pWorld->GetCurrentTurn() : 0;

      printf("COLLISION_DETECTED: Turn %u: %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f <-> %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f | dist=%.3f overlap=%.3f\n",
             turn,
             GetName(), kind1, pos1.fX, pos1.fY, vel1.rho, vel1.theta * 180.0 / PI, GetSize(),
             pOthThing->GetName(), kind2, pos2.fX, pos2.fY, vel2.rho, vel2.theta * 180.0 / PI, pOthThing->GetSize(),
             dist, overlap);
    }
  }

  HandleCollision(pOthThing, pWorld);
  return true;
}

bool CThing::CollideNew(CThing* pOthThing, CWorld* pWorld) {
  // NEW COLLISION BEHAVIOR
  // Currently identical to legacy - will be updated to fix multi-processing issues.
  extern CParser* g_pParser;

  if (pOthThing == NULL) {
    printf("Colliding with NULL!\n");
    return false;  // How did THAT happen!!??
  }
  if (*pOthThing == *this) {
    return false;  // Can't collide with yourself!
  }

  if (Overlaps(*pOthThing) == false) {
    return false;
  }

  double dAng = GetPos().AngleTo(pOthThing->GetPos());
  if (pOthThing->GetKind() == GENTHING) {
    bIsGettingShot = dAng;
  } else {
    bIsColliding = dAng;
  }

  // Verbose logging for collision detection
  if (g_pParser && g_pParser->verbose) {
    // Check if we should skip logging to reduce noise from docked ships
    bool skip_logging = false;

    // Skip ship-station collisions if ship was previously docked
    if ((GetKind() == SHIP && pOthThing->GetKind() == STATION) ||
        (GetKind() == STATION && pOthThing->GetKind() == SHIP)) {
      CShip* ship = (GetKind() == SHIP) ? dynamic_cast<CShip*>(this) : dynamic_cast<CShip*>(pOthThing);
      if (ship && ship->WasDocked()) {
        skip_logging = true;
      }
    }

    // Skip ship-ship collisions if either ship is currently docked
    if (GetKind() == SHIP && pOthThing->GetKind() == SHIP) {
      CShip* ship1 = dynamic_cast<CShip*>(this);
      CShip* ship2 = dynamic_cast<CShip*>(pOthThing);
      if ((ship1 && ship1->IsDocked()) || (ship2 && ship2->IsDocked())) {
        skip_logging = true;
      }
    }

    if (!skip_logging) {
      CCoord pos1 = GetPos();
      CCoord pos2 = pOthThing->GetPos();
      double dist = sqrt((pos1.fX - pos2.fX) * (pos1.fX - pos2.fX) +
                         (pos1.fY - pos2.fY) * (pos1.fY - pos2.fY));
      double combined_size = GetSize() + pOthThing->GetSize();
      double overlap = combined_size - dist;

      CTraj vel1 = GetVelocity();
      CTraj vel2 = pOthThing->GetVelocity();

      const char* kind1 = (GetKind() == SHIP) ? "SHIP" : (GetKind() == STATION) ? "STATION" : (GetKind() == ASTEROID) ? "ASTEROID" : "GENTHING";
      const char* kind2 = (pOthThing->GetKind() == SHIP) ? "SHIP" : (pOthThing->GetKind() == STATION) ? "STATION" : (pOthThing->GetKind() == ASTEROID) ? "ASTEROID" : "GENTHING";

      unsigned int turn = (pWorld != NULL) ? pWorld->GetCurrentTurn() : 0;

      printf("COLLISION_DETECTED: Turn %u: %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f <-> %s[%s] pos=(%.1f,%.1f) vel=(%.2f@%.1f°) rad=%.1f | dist=%.3f overlap=%.3f\n",
             turn,
             GetName(), kind1, pos1.fX, pos1.fY, vel1.rho, vel1.theta * 180.0 / PI, GetSize(),
             pOthThing->GetName(), kind2, pos2.fX, pos2.fY, vel2.rho, vel2.theta * 180.0 / PI, pOthThing->GetSize(),
             dist, overlap);
    }
  }

  HandleCollision(pOthThing, pWorld);
  return true;
}

bool CThing::Overlaps(const CThing& OthThing) const {
  if (OthThing == *this) {
    return false;  // Overlap yourself? :P
  }

  double dtmprad, ddist;
  dtmprad = size + OthThing.GetSize();
  ddist = Pos.DistTo(OthThing.GetPos());

  if (ddist < dtmprad) {
    return true;
  }
  return false;
}

// Deterministic Collision Engine - Create Immutable Snapshot
// This method captures the complete state of this object at the moment
// it's called. The returned snapshot is used by the deterministic collision
// engine to ensure both collision participants see the same state.
CollisionState CThing::MakeCollisionState() const {
  CollisionState state;

  // Identity
  state.thing = const_cast<CThing*>(this);  // Pointer for identity only
  state.kind = TKind;
  state.world_index = uWldIndex;

  // Physics state
  state.position = Pos;
  state.velocity = Vel;
  state.mass = mass;
  state.size = size;
  state.orient = orient;
  state.omega = omega;

  // Ownership and status
  state.team = pmyTeam;
  state.is_alive = !DeadFlag;

  // Default values for derived-class-specific state
  // (Derived classes should override this method if they have additional state)
  state.is_docked = false;
  state.ship_shield = 0.0;
  state.ship_cargo = 0.0;
  state.ship_fuel = 0.0;
  state.asteroid_material = GENAST;
  state.station_cargo = 0.0;

  return state;
}

// Deterministic Collision Engine - Apply Command
// This method applies a collision command to this object. Commands are generated
// during collision processing and applied in deterministic order by the world.
void CThing::ApplyCollisionCommand(const CollisionCommand& cmd, const CollisionContext& ctx) {
  // Validate that this command targets this object
  if (cmd.target != this) {
    return;  // Command doesn't target us, ignore
  }

  // Apply command based on type
  switch (cmd.type) {
    case CollisionCommandType::kNoOp:
      // Do nothing
      break;

    case CollisionCommandType::kKillSelf:
      // Mark this object as dead
      DeadFlag = true;
      break;

    case CollisionCommandType::kSetVelocity:
      // Set velocity (used for momentum transfer)
      Vel = cmd.velocity;
      break;

    case CollisionCommandType::kSetPosition:
      // Set position (used for separation or docking)
      Pos = cmd.position;
      break;

    case CollisionCommandType::kAdjustShield:
    case CollisionCommandType::kAdjustCargo:
    case CollisionCommandType::kAdjustFuel:
    case CollisionCommandType::kSetDocked:
    case CollisionCommandType::kRecordEatenBy:
      // These are derived-class-specific, delegate to virtual method
      ApplyCollisionCommandDerived(cmd, ctx);
      break;

    case CollisionCommandType::kAnnounceMessage:
      // Announcer messages are handled by the world, not by individual objects
      break;
  }
}

// Base class implementation does nothing for derived-specific commands
// Derived classes override this to handle their specific command types
void CThing::ApplyCollisionCommandDerived(const CollisionCommand& cmd, const CollisionContext& ctx) {
  // Base CThing has no derived-specific state to modify
  // Ships, Asteroids, and Stations will override this
  (void)cmd;  // Suppress unused parameter warning
  (void)ctx;  // Suppress unused parameter warning
}

// Deterministic collision engine - generate collision commands from snapshots
// Base implementation returns empty outcome (no commands)
// Derived classes override this to emit collision-specific commands
CollisionOutcome CThing::GenerateCollisionCommands(const CollisionContext& ctx) {
  (void)ctx;  // Suppress unused parameter warning
  return CollisionOutcome();  // Empty outcome - base things don't participate in collisions
}

////////////////////////////////////////////////
// Helper functions

CCoord CThing::PredictPosition(double dt) const {
  CCoord PosRes = GetPos();
  CTraj TmpVel(GetVelocity());
  if (TmpVel.rho == 0.0) {
    return PosRes;
  }

  PosRes += (CCoord)(GetVelocity() * dt);
  return PosRes;
}

CTraj CThing::RelativeVelocity(const CThing& OthThing) const {
  return (OthThing.Vel - Vel);
}

CTraj CThing::RelativeMomentum(const CThing& OthThing) const {
  return (RelativeVelocity(OthThing) * OthThing.GetMass());
}

bool CThing::IsFacing(const CThing& OthThing) const {
  if (*this == OthThing) {
    return false;  // Won't laser-fire yourself
  }

  // Work in relative coordinate system where 'this' object is at origin (0,0)
  // cOrg = this object's position in relative coords, cOth = other object's
  // relative position
  CCoord cOrg(0.0, 0.0), cOth(OthThing.GetPos() - GetPos());
  if (cOrg == cOth) {
    return true;
  }

  double ddist = cOrg.DistTo(cOth);

  CTraj tGo(1.0, GetOrient());
  tGo = tGo * ddist;

  CCoord cGo(cOrg);
  cGo += tGo.ConvertToCoord();

  double dhit = cGo.DistTo(cOth);
  if (dhit <= OthThing.GetSize()) {
    return true;
  }
  return false;
}

// Global parser instance - will be set by main programs
extern CParser* g_pParser;

double CThing::DetectCollisionCourse(const CThing& OthThing) const {
  // Use ArgumentParser to determine which collision detection to use
  // Default to new behavior unless explicitly set to old
  if (g_pParser && !g_pParser->UseNewFeature("collision-detection")) {
    return DetectCollisionCourseOld(OthThing);
  } else {
    return DetectCollisionCourseNew(OthThing);
  }
}

double CThing::DetectCollisionCourseOld(const CThing& OthThing) const {
  // LEGACY COLLISION DETECTION (retained for backward compatibility)
  // This uses an approximation that projects along relative velocity direction
  // for a distance equal to current separation. Works in some cases but fails
  // for perpendicular approaches and complex trajectories.

  if (OthThing == *this) {
    return g_no_collide_sentinel;
  }

  CTraj VRel = OthThing.Vel - Vel;  // Direction of vector
  if (VRel.rho <= 0.05) {
    return g_no_collide_sentinel;  // Never gonna hit if effectively not moving
  }

  double flyred =
      size + OthThing.size;  // Don't allow them to scrape each other
  double dist = Pos.DistTo(OthThing.Pos);  // Magnitude of vector
  if (dist < flyred) {
    return 0.0;  // They're already impacting
  }

  CTraj VHit(dist, VRel.theta);
  CCoord RelPos = OthThing.Pos - Pos,
         CHit(RelPos + VHit.ConvertToCoord());

  double flyby = CHit.DistTo(CCoord(0.0, 0.0));
  if (flyby > flyred) {
    return g_no_collide_sentinel;
  }

  // Pending collision
  double hittime = (dist - flyred) / VRel.rho;
  return hittime;
}

double CThing::DetectCollisionCourseNew(const CThing& OthThing) const {
  // Robust collision detection using the Quadratic Formula approach.
  // We analyze the motion in a relative frame of reference where 'this' object
  // is stationary at the origin, and 'OthThing' moves relative to it.

  // NOTE ON OVERALL APPROACH BELOW:
  // Optimization: We operate on and compare squared distances (e.g. PMagSq <
  // RSq) instead of actual distances (sqrt(PMagSq) < R). This is much faster
  // because sqrt() is expensive. The result is mathematically identical since
  // distances are always positive.

  if (OthThing == *this) {
    return g_no_collide_sentinel;
  }

  // 1. Setup Relative Vectors in Cartesian Coordinates.

  // P: Relative Position (Vector from 'this' to 'OthThing').
  // CCoord operator- correctly handles toroidal wrap-around, providing the
  // shortest path vector.
  CCoord RelPos = OthThing.Pos - Pos;
  double Px = RelPos.fX;
  double Py = RelPos.fY;

  // V: Relative Velocity (Velocity of 'OthThing' minus Velocity of 'this').
  // We convert the result (CTraj) to Cartesian coordinates (CCoord) for the
  // vector math.
  CTraj VRel_Traj = OthThing.Vel - Vel;
  CCoord VRel = VRel_Traj.ConvertToCoord();
  double Vx = VRel.fX;
  double Vy = VRel.fY;

  // R: Collision Radius (Sum of the radii of both objects).
  double R = size + OthThing.size;
  double RSq = R * R;

  // 2. Check for immediate overlap.
  // PMagSq is the current distance squared.
  double PMagSq = Px * Px + Py * Py;
  if (PMagSq < RSq) {
    return 0.0;  // Already impacting
  }

  // 3. Setup the Quadratic Equation.
  // We want to find the time 't' (TTC) when the distance squared equals R^2.
  // |P + V*t|^2 = R^2
  // Expanding and rearranging into the standard form At^2 + Bt + C = 0:

  // A = V.V (Squared magnitude of relative velocity)
  double A = Vx * Vx + Vy * Vy;

  // B = 2 * (P.V) (Twice the dot product of relative position and velocity)
  double PdotV = Px * Vx + Py * Vy;
  double B = 2.0 * PdotV;

  // C = P.P - R^2 (Squared magnitude of position minus squared collision
  // radius)
  double C = PMagSq - RSq;

  // 4. Analyze coefficients for early exits.

  // Check for Zero Relative Velocity (Fixes Flaw 1: Division by zero/Incorrect
  // Theta usage) If A is near zero, the relative velocity is zero. Since we
  // already checked for overlap (C > 0), they will not collide.
  if (A < g_fp_error_epsilon) {
    return g_no_collide_sentinel;
  }

  // Check if objects are receding after a collision in the past (Fixes Flaw 2:
  // CPA in the past) PdotV indicates the rate of closure. If PdotV >= 0, the
  // distance is increasing or constant (moving apart or parallel).
  if (PdotV >= 0.0) {
    return g_no_collide_sentinel;
  }

  // 5. Calculate the Discriminant.
  // D = B^2 - 4AC.
  // If D < 0, the equation has no real solutions; the trajectories never
  // intersect.
  double Discriminant = B * B - 4.0 * A * C;

  if (Discriminant < 0.0) {
    return g_no_collide_sentinel;
  }

  // 6. Calculate the Time to Impact (TTC) (Fixes Flaw 3: Incorrect TTC
  // calculation). A collision will occur. We want the smallest positive root of
  // the equation: t = (-B +/- sqrt(D)) / 2A Since A > 0 and B < 0
  // (approaching), the smallest positive root is found using subtraction:
  double TTC = (-B - sqrt(Discriminant)) / (2.0 * A);

  // Final guard against floating point precision issues near the boundary.
  if (TTC < 0.0) {
    return 0.0;
  }

  return TTC;
}


////////////////////////////////////////////////
// Operators

CThing& CThing::operator=(const CThing& OthThing) {
  char* buf;
  unsigned int pksz, sz = OthThing.GetSerialSize();

  buf = new char[sz];
  pksz = OthThing.SerialPack(buf, sz);

  if (pksz != sz) {
    printf("ERROR: Assignment operator failure\n");
    return *this;
  }

  SerialUnpack(buf, pksz);
  delete buf;
  return *this;
}

bool CThing::operator==(const CThing& OthThing) const {
  if (ulIDCookie != OthThing.ulIDCookie) {
    return false;
  }

  return true;
}

bool CThing::operator!=(const CThing& OthThing) const {
  return (!(*this == OthThing));
}

//////////////////////////////////////////////////////////////
// Protected methods

void CThing::HandleCollision(CThing* pOthThing, CWorld* pWorld) {
  extern CParser* g_pParser;

  if (g_pParser && !g_pParser->UseNewFeature("collision-handling")) {
    HandleCollisionOld(pOthThing, pWorld);
  } else {
    HandleCollisionNew(pOthThing, pWorld);
  }
}

void CThing::HandleCollisionOld(CThing* pOthThing, CWorld* pWorld) {
  // LEGACY COLLISION HANDLING
  // Base CThing does nothing on collision
  if (pOthThing == NULL) {
    return;
  }
  if (pWorld == NULL) {
    return;
  }
}

void CThing::HandleCollisionNew(CThing* pOthThing, CWorld* pWorld) {
  // NEW COLLISION HANDLING
  // Currently identical to legacy - will be updated for new collision system
  if (pOthThing == NULL) {
    return;
  }
  if (pWorld == NULL) {
    return;
  }
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CThing::GetSerialSize() const {
  unsigned int totsize = 0;

  unsigned int uTK = (unsigned int)TKind;
  totsize += BufWrite(NULL, uTK);

  totsize += BufWrite(NULL, ulIDCookie);
  totsize += BufWrite(NULL, uImgSet);

  totsize += BufWrite(NULL, orient);
  totsize += BufWrite(NULL, omega);
  totsize += BufWrite(NULL, mass);
  totsize += BufWrite(NULL, size);

  totsize += BufWrite(NULL, DeadFlag);
  totsize += BufWrite(NULL, bIsColliding);
  totsize += BufWrite(NULL, bIsGettingShot);
  totsize += maxnamelen;

  totsize += Pos.GetSerialSize();
  totsize += Vel.GetSerialSize();

  return totsize;
}

unsigned CThing::SerialPack(char* buf, unsigned buflen) const {
  unsigned int needlen = GetSerialSize();
  if (buflen < needlen) {
    return 0;
  }
  char* vpb = buf;

  unsigned int uTK = (unsigned int)TKind;
  vpb += BufWrite(vpb, uTK);

  vpb += BufWrite(vpb, ulIDCookie);
  vpb += BufWrite(vpb, uImgSet);

  vpb += BufWrite(vpb, orient);
  vpb += BufWrite(vpb, omega);
  vpb += BufWrite(vpb, mass);
  vpb += BufWrite(vpb, size);

  vpb += BufWrite(vpb, DeadFlag);
  vpb += BufWrite(vpb, bIsColliding);
  vpb += BufWrite(vpb, bIsGettingShot);
  vpb += BufWrite(vpb, Name, maxnamelen);

  vpb += Pos.SerialPack(vpb, Pos.GetSerialSize());
  vpb += Vel.SerialPack(vpb, Vel.GetSerialSize());

  return (vpb - buf);
}

unsigned CThing::SerialUnpack(char* buf, unsigned buflen) {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  unsigned int uTK;
  vpb += BufRead(vpb, uTK);
  TKind = (ThingKind)uTK;

  vpb += BufRead(vpb, ulIDCookie);
  vpb += BufRead(vpb, uImgSet);

  vpb += BufRead(vpb, orient);
  vpb += BufRead(vpb, omega);
  vpb += BufRead(vpb, mass);
  vpb += BufRead(vpb, size);

  vpb += BufRead(vpb, DeadFlag);
  vpb += BufRead(vpb, bIsColliding);
  vpb += BufRead(vpb, bIsGettingShot);
  vpb += BufRead(vpb, Name, maxnamelen);

  vpb += Pos.SerialUnpack((char*)vpb, Pos.GetSerialSize());
  vpb += Vel.SerialUnpack((char*)vpb, Vel.GetSerialSize());

  return (vpb - buf);
}
