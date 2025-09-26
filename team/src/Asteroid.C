/* Asteroid.C
 * Implementation of generic Asteroid class
 * For use with MechMania IV
 * Misha Voloshin and (a little of) Jason Govig
 * 5/26/98
 */

#include "Asteroid.h"
#include "World.h"
#include "Ship.h"

/////////////////////////////////////////////////////////
// Construction/Destruction

CAsteroid::CAsteroid (double dm, AsteroidKind mat) : CThing(0.0,0.0)
{
  mass=dm;
  if (mass<minmass) mass=minmass;
  if (mass==0.0) {
    mass = 1.0 + ((double)rand()/(double)RAND_MAX)*10.0;
  }

  TKind = ASTEROID;
  material = mat;

  if (mass>=40.0) uImgSet=0;
  else if (mass>=10.0) uImgSet=1;
  else uImgSet=2; 

  if (uImgSet>2) uImgSet=0;
  if (material==URANIUM) uImgSet+=3;

  switch (material) {
    case VINYL: snprintf(Name, maxnamelen, "Vinyl %.1f",mass);  break;
    case URANIUM: snprintf(Name, maxnamelen, "Urnm %.1f",mass); break;
    default: snprintf (Name, maxnamelen, "Astrd %.1f",mass);
  }
  
  Pos=CCoord(0,0);
  orient=0.0;
  omega=1.0;
  size = 3.0+1.6*sqrt(mass);
  pThEat=NULL;
  
  double vt = (((double)rand()/(double)RAND_MAX) * PI2) - PI;
  double vr = (1.0 - ((double)rand()/(double)RAND_MAX)) * maxspeed;
  Vel=CTraj(vr,vt);
}

CAsteroid::~CAsteroid()
{

}

///////////////////////////////////////////////////
// Data access methods

AsteroidKind CAsteroid::GetMaterial() const
{
  return material;
}

CThing* CAsteroid::EatenBy() const
{
  return pThEat;
}

///////////////////////////////////////////////////
// Virtual methods

CAsteroid* CAsteroid::MakeChildAsteroid(double dm)
{
  CAsteroid *pChildAst=new CAsteroid(dm,GetMaterial());
  return pChildAst;
}

void CAsteroid::HandleCollision(CThing* pOthThing, CWorld *pWorld)
{
  ThingKind OthKind = pOthThing->GetKind();

  bIsColliding=NO_DAMAGE;
  bIsGettingShot=NO_DAMAGE;

  // Asteroid-to-asteroid interactions are not simulated in the engine.
  // World::CollisionEvaluation only pairs team-controlled things (ships/stations)
  // with other world things (e.g., asteroids). As a defensive guard, ignore
  // asteroid-asteroid "collisions" if ever invoked.
  if (OthKind==ASTEROID) return;

  if (OthKind==STATION) {
    // ASTEROID-STATION COLLISION PHYSICS
    // This implements a perfectly elastic collision between an asteroid (point mass)
    // and a stationary station (immovable circular object with infinite mass).

    // Step 1: Calculate the surface normal at the collision point
    // This is the direction from station to asteroid (impact direction)
    double angbo = pOthThing->GetPos().AngleTo(this->GetPos());

    // Step 2: Implement specular reflection physics
    // For elastic collisions: reflection_angle = 2 * surface_normal - incident_angle + PI
    angbo *= 2.0;           // Double the surface normal angle
    angbo -= Vel.theta;     // Subtract the incident velocity direction
    angbo -= PI;            // Flip direction (add PI) to make it a proper reflection

    // Step 3: Set the asteroid's new velocity direction to the calculated reflection
    Vel.theta = angbo;
    Vel.Normalize();        // Ensure angle is in proper range [-PI, PI]

    // Step 4: Position the asteroid outside the station to prevent overlap
    // Recalculate the surface normal for positioning (not velocity)
    angbo = pOthThing->GetPos().AngleTo(this->GetPos());
    CTraj TMove (size+pOthThing->GetSize()+1, angbo);
    Pos = pOthThing->GetPos();
    Pos += TMove.ConvertToCoord();

    // Step 5: Set collision angle for visual damage effects
    // The graphics system uses this angle to render damage sprites at the impact point
    pOthThing->bIsColliding=angbo;  // How convenient

    return;
  }

  // Handle laser-blast:
  // --------------------
  // Lasers are delivered as a temporary GENTHING synthesized by
  // CWorld::LaserModel. That temporary "laser thing" is positioned
  // one unit shy of the target along the beam, and its mass encodes
  // the remaining beam power at impact:
  //   mass = 30 * (L - D)
  // where L is the requested beam length and D is the shooter-to-
  // impact distance. We require at least 1000 units of laser mass to
  // shatter the asteroid. If below threshold, the beam glances off.
  if (OthKind==GENTHING && pOthThing->GetMass()<1000.0) return;
  
  DeadFlag=true;
  if (OthKind==SHIP) {
    // ASTEROID-SHIP COLLISION PHYSICS
    // Implements momentum exchange between asteroid and ship.
    // Ships can absorb asteroids if they're small enough or take damage from larger ones.
    pThEat = pOthThing;
    if (((CShip*)pOthThing)->AsteroidFits(this))
      return;  // Don't make child asteroids if this one got eaten
  }

  // Make child asteroids
  UINT i, numNew = 3;
  double angstep=(PI2)/((double)numNew);
  double nMass=GetMass()/((double)numNew);
  if (nMass<minmass) return;  // Space-dust

  CTraj VCh = RelativeVelocity(*pOthThing);
  if (OthKind==GENTHING) VCh.rho = pOthThing->GetMass()/(3.0*GetMass());
  if (VCh.rho>maxspeed) VCh.rho = maxspeed;
  CAsteroid *pChildAst;

  for (i=0; i<numNew; i++) {
    pChildAst = MakeChildAsteroid(nMass);
    pChildAst->Vel = VCh;
    pChildAst->Pos = Pos;

    VCh.Rotate(angstep);
    pWorld->AddThingToWorld(pChildAst);
  }
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CAsteroid::GetSerialSize() const
{
  UINT totsize=0;

  totsize += CThing::GetSerialSize();

  UINT umat = (UINT)material;
  totsize += BufWrite(NULL, umat);

  return totsize;
}

unsigned CAsteroid::SerialPack (char *buf, unsigned buflen) const
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += (CThing::SerialPack(buf,buflen));

  UINT umat = (UINT)material;
  vpb += BufWrite(vpb, umat);

  return (vpb-buf);
}
  
unsigned CAsteroid::SerialUnpack (char *buf, unsigned buflen)
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += (CThing::SerialUnpack(buf,buflen));

  UINT umat;
  vpb += BufRead(vpb, umat);
  material = (AsteroidKind) umat;

  return (vpb-buf);
}
