/* Asteroid.C
 * Implementation of generic Asteroid class
 * For use with MechMania IV
 * Misha Voloshin and (a little of) Jason Govig
 * 5/26/98
 */

#include "Asteroid.h"
#include "ArgumentParser.h"
#include "GameConstants.h"
#include "Ship.h"
#include "World.h"

extern ArgumentParser* g_pParser;

/////////////////////////////////////////////////////////
// Construction/Destruction

CAsteroid::CAsteroid(double dm, AsteroidKind mat) : CThing(0.0, 0.0) {
  mass = dm;
  if (mass < g_thing_minmass) {
    mass = g_thing_minmass;
  }
  if (mass == 0.0) {
    mass = g_asteroid_random_mass_offset +
           ((double)rand() / (double)RAND_MAX) * g_asteroid_random_mass_range;
  }

  TKind = ASTEROID;
  material = mat;

  if (mass >= g_asteroid_large_mass_threshold) {
    uImgSet = 0;
  } else if (mass >= g_asteroid_medium_mass_threshold) {
    uImgSet = 1;
  } else {
    uImgSet = 2;
  }

  if (uImgSet > 2) {
    uImgSet = 0;
  }
  if (material == URANIUM) {
    uImgSet += 3;
  }

  switch (material) {
    case VINYL:
      snprintf(Name, maxnamelen, "Vinyl %.1f", mass);
      break;
    case URANIUM:
      snprintf(Name, maxnamelen, "Urnm %.1f", mass);
      break;
    default:
      snprintf(Name, maxnamelen, "Astrd %.1f", mass);
  }

  Pos = CCoord(0, 0);
  orient = 0.0;
  omega = 1.0;
  size = g_asteroid_size_base +
         g_asteroid_size_mass_scale * sqrt(mass);
  pThEat = NULL;

  double vt = (((double)rand() / (double)RAND_MAX) * PI2) - PI;
  double vr = (1.0 - ((double)rand() / (double)RAND_MAX)) * g_game_max_speed;
  Vel = CTraj(vr, vt);
}

CAsteroid::~CAsteroid() {}

///////////////////////////////////////////////////
// Data access methods

AsteroidKind CAsteroid::GetMaterial() const { return material; }

CThing* CAsteroid::EatenBy() const { return pThEat; }

///////////////////////////////////////////////////
// Virtual methods

CAsteroid* CAsteroid::MakeChildAsteroid(double dm) {
  CAsteroid* pChildAst = new CAsteroid(dm, GetMaterial());
  return pChildAst;
}

void CAsteroid::HandleCollision(CThing* pOthThing, CWorld* pWorld) {
  ThingKind OthKind = pOthThing->GetKind();

  bIsColliding = g_no_damage_sentinel;
  bIsGettingShot = g_no_damage_sentinel;

  // Asteroid-to-asteroid interactions are not simulated in the engine.
  // World::CollisionEvaluation only pairs team-controlled things
  // (ships/stations) with other world things (e.g., asteroids). As a defensive
  // guard, ignore asteroid-asteroid "collisions" if ever invoked.
  if (OthKind == ASTEROID) {
    return;
  }

  if (OthKind == STATION) {
    // ASTEROID-STATION COLLISION PHYSICS
    // This implements a perfectly elastic collision between an asteroid (point
    // mass) and a stationary station (immovable circular object with infinite
    // mass).
    //
    // PHYSICS NOTE: This is a correct elastic collision with an infinite mass.
    // Momentum is NOT conserved (station has infinite mass and doesn't move),
    // but kinetic energy IS conserved (asteroid speed unchanged, only direction
    // changes via specular reflection).

    // Step 1: Calculate the surface normal at the collision point
    // This is the direction from station to asteroid (impact direction)
    double angbo = pOthThing->GetPos().AngleTo(this->GetPos());

    // Step 2: Implement specular reflection physics
    // For elastic collisions: reflection_angle = 2 * surface_normal -
    // incident_angle - PI
    angbo *= 2.0;        // Double the surface normal angle
    angbo -= Vel.theta;  // Subtract the incident velocity direction
    angbo -= PI;  // Account for velocity vector vs approach vector convention

    // Step 3: Set the asteroid's new velocity direction to the calculated
    // reflection
    Vel.theta = angbo;
    Vel.Normalize();  // Ensure angle is in proper range [-PI, PI]

    // Step 4: Position the asteroid outside the station to prevent overlap
    // Recalculate the surface normal for positioning (not velocity)
    angbo = pOthThing->GetPos().AngleTo(this->GetPos());
    CTraj TMove(size + pOthThing->GetSize() + 1, angbo);
    Pos = pOthThing->GetPos();
    Pos += TMove.ConvertToCoord();

    // Step 5: Set collision angle for visual damage effects
    // The graphics system uses this angle to render damage sprites at the
    // impact point
    pOthThing->bIsColliding = angbo;  // How convenient

    return;
  }

  // Handle laser-blast:
  // --------------------
  // Lasers are delivered as a temporary GENTHING synthesized by
  // CWorld::LaserModel. That temporary "laser thing" is positioned
  // one unit shy of the target along the beam, and its mass encodes
  // the remaining beam power at impact:
  //   mass = g_laser_mass_scale_per_remaining_unit * (L - D)
  // where L is the requested beam length and D is the shooter-to-
  // impact distance. We require at least g_asteroid_laser_shatter_threshold
  // units of laser mass to
  // shatter the asteroid. If below threshold, the beam glances off.
  if (OthKind == GENTHING &&
      pOthThing->GetMass() < g_asteroid_laser_shatter_threshold) {
    return;
  }

  DeadFlag = true;
  if (OthKind == SHIP) {
    // ASTEROID-SHIP COLLISION PHYSICS
    // Implements momentum exchange between asteroid and ship.
    // Ships can absorb asteroids if they're small enough or take damage from
    // larger ones.
    pThEat = pOthThing;
    if (((CShip*)pOthThing)->AsteroidFits(this)) {
      return;  // Don't make child asteroids if this one got eaten
    }
  }

  // Dispatch to appropriate fragmentation implementation
  if (g_pParser && !g_pParser->UseNewFeature("physics")) {
    CreateFragmentsOld(pOthThing, pWorld, OthKind);
  } else {
    CreateFragmentsNew(pOthThing, pWorld, OthKind);
  }
}

///////////////////////////////////////////////////
// Fragmentation implementations

void CAsteroid::CreateFragmentsOld(CThing* pOthThing, CWorld* pWorld,
                                   ThingKind OthKind) {
  // LEGACY FRAGMENTATION PHYSICS
  // This is the original MechMania IV behavior preserved for backward
  // compatibility with --legacy-physics flag.

  unsigned int i;
  unsigned int numNew = g_asteroid_split_child_count;
  double angstep = (PI2) / static_cast<double>(numNew);
  double nMass = GetMass() / static_cast<double>(numNew);
  if (nMass < g_thing_minmass) {
    return;  // Space-dust
  }

  CTraj VCh = RelativeVelocity(*pOthThing);
  if (OthKind == GENTHING) {
    VCh.rho = pOthThing->GetMass() /
              (g_asteroid_laser_impulse_divisor * GetMass());
  }
  if (VCh.rho > g_game_max_speed) {
    VCh.rho = g_game_max_speed;
  }
  CAsteroid* pChildAst;

  for (i = 0; i < numNew; ++i) {
    pChildAst = MakeChildAsteroid(nMass);
    pChildAst->Vel = VCh;
    pChildAst->Pos = Pos;

    VCh.Rotate(angstep);
    pWorld->AddThingToWorld(pChildAst);
  }
}

void CAsteroid::CreateFragmentsNew(CThing* pOthThing, CWorld* pWorld,
                                   ThingKind OthKind) {
  // NEW FRAGMENTATION PHYSICS
  // Properly models conservation of momentum when an asteroid shatters.
  // Fragment velocity = center_of_mass_velocity + spread_velocity

  unsigned int i;
  unsigned int numNew = g_asteroid_split_child_count;
  double angstep = (PI2) / static_cast<double>(numNew);
  double nMass = GetMass() / static_cast<double>(numNew);
  if (nMass < g_thing_minmass) {
    return;  // Space-dust
  }

  // Calculate asteroid's center-of-mass velocity after collision
  CCoord v_center_of_mass;

  if (OthKind == SHIP) {
    // SHIP COLLISION: Perfect elastic collision
    // Formula: v_ast' = v_ast - (2*m_ship/(m_ast+m_ship)) *
    //                   ((v_ast-v_ship)·(x_ast-x_ship))/|x_ast-x_ship|² *
    //                   (x_ast-x_ship)

    // Get positions and velocities
    CCoord pos_ast = GetPos();
    CCoord pos_ship = pOthThing->GetPos();
    CCoord vel_ast = Vel.ConvertToCoord();
    CCoord vel_ship = pOthThing->GetVelocity().ConvertToCoord();

    // Calculate position difference (x_ast - x_ship)
    CCoord dx;
    dx.fX = pos_ast.fX - pos_ship.fX;
    dx.fY = pos_ast.fY - pos_ship.fY;

    // Calculate squared distance |x_ast - x_ship|²
    double dx_squared = dx.fX * dx.fX + dx.fY * dx.fY;

    if (dx_squared > g_fp_error_epsilon) {
      // Normal case: objects at different positions

      // Calculate velocity difference (v_ast - v_ship)
      CCoord dv;
      dv.fX = vel_ast.fX - vel_ship.fX;
      dv.fY = vel_ast.fY - vel_ship.fY;

      // Calculate dot product (v_ast - v_ship) · (x_ast - x_ship)
      double dot_product = dv.fX * dx.fX + dv.fY * dx.fY;

      // Calculate mass factor
      double m_ship = pOthThing->GetMass();
      double m_ast = GetMass();
      double mass_factor = (2.0 * m_ship) / (m_ast + m_ship);
      double scale = mass_factor * dot_product / dx_squared;

      // Calculate new asteroid velocity
      v_center_of_mass.fX = vel_ast.fX - scale * dx.fX;
      v_center_of_mass.fY = vel_ast.fY - scale * dx.fY;
    } else {
      // Edge case: same position (rare for asteroids)
      v_center_of_mass = vel_ast;
    }
  } else if (OthKind == GENTHING) {
    // LASER COLLISION: Perfect inelastic collision
    // The laser's mass and momentum are absorbed into the asteroid
    // Conservation of momentum: m_ast * v_ast + m_laser * v_laser =
    //                           (m_ast + m_laser) * v_final
    // Simplifying: v_final = (m_ast * v_ast + m_laser * v_laser) / (m_ast +
    // m_laser)

    double m_ast = GetMass();
    double m_laser = pOthThing->GetMass();
    double total_mass = m_ast + m_laser;

    CCoord vel_ast = Vel.ConvertToCoord();
    CCoord vel_laser = pOthThing->GetVelocity().ConvertToCoord();

    // Calculate momentum-weighted average velocity
    v_center_of_mass.fX = (m_ast * vel_ast.fX + m_laser * vel_laser.fX) / total_mass;
    v_center_of_mass.fY = (m_ast * vel_ast.fY + m_laser * vel_laser.fY) / total_mass;
  } else {
    // Other collision types: use current asteroid velocity
    v_center_of_mass = Vel.ConvertToCoord();
  }

  // Calculate spread velocity pattern (symmetric explosion from center)
  // Use relative velocity as the base spread direction
  CTraj v_spread = RelativeVelocity(*pOthThing);

  if (OthKind == GENTHING) {
    // For lasers: scale spread magnitude by laser energy
    // This creates the "explosion" effect when laser shatters an asteroid
    v_spread.rho = pOthThing->GetMass() /
                   (g_asteroid_laser_impulse_divisor * GetMass());
  }

  // Create fragments with center-of-mass motion + 120° spread pattern
  CAsteroid* pChildAst;
  for (i = 0; i < numNew; ++i) {
    pChildAst = MakeChildAsteroid(nMass);

    // Fragment velocity = center_of_mass + spread (rotated by 120° * i)
    CCoord v_fragment = v_center_of_mass + v_spread.ConvertToCoord();
    CTraj v_final(v_fragment);

    // Enforce maximum speed
    if (v_final.rho > g_game_max_speed) {
      v_final.rho = g_game_max_speed;
    }

    pChildAst->Vel = v_final;
    pChildAst->Pos = Pos;

    // Rotate spread vector for next fragment
    v_spread.Rotate(angstep);
    pWorld->AddThingToWorld(pChildAst);
  }
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CAsteroid::GetSerialSize() const {
  unsigned int totsize = 0;

  totsize += CThing::GetSerialSize();

  unsigned int umat = (unsigned int)material;
  totsize += BufWrite(NULL, umat);

  return totsize;
}

unsigned CAsteroid::SerialPack(char* buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += (CThing::SerialPack(buf, buflen));

  unsigned int umat = (unsigned int)material;
  vpb += BufWrite(vpb, umat);

  return (vpb - buf);
}

unsigned CAsteroid::SerialUnpack(char* buf, unsigned buflen) {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += (CThing::SerialUnpack(buf, buflen));

  unsigned int umat;
  vpb += BufRead(vpb, umat);
  material = (AsteroidKind)umat;

  return (vpb - buf);
}
