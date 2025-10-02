/* Asteroid.C
 * Implementation of generic Asteroid class
 * For use with MechMania IV
 * Misha Voloshin and (a little of) Jason Govig
 * 5/26/98
 */

#include "Asteroid.h"
#include "GameConstants.h"
#include "Ship.h"
#include "World.h"

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

    // Step 1: Calculate the surface normal at the collision point
    // This is the direction from station to asteroid (impact direction)
    double angbo = pOthThing->GetPos().AngleTo(this->GetPos());

    // Step 2: Implement specular reflection physics
    // For elastic collisions: reflection_angle = 2 * surface_normal -
    // incident_angle + PI
    angbo *= 2.0;        // Double the surface normal angle
    angbo -= Vel.theta;  // Subtract the incident velocity direction
    angbo -= PI;  // Flip direction (add PI) to make it a proper reflection

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

  // Make child asteroids
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
