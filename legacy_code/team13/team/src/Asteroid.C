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
    case VINYL: sprintf(Name,"Vinyl %.1f",mass);  break;
    case URANIUM: sprintf(Name,"Urnm %.1f",mass); break;
    default: sprintf (Name,"Astrd %.1f",mass);
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

  if (OthKind==STATION) {
    double angbo = pOthThing->GetPos().AngleTo(this->GetPos());
    angbo *= 2.0;
    angbo -= Vel.theta;
    angbo -= PI;
    Vel.theta = angbo;
    Vel.Normalize();
    
    angbo = pOthThing->GetPos().AngleTo(this->GetPos());
    CTraj TMove (size+pOthThing->GetSize()+1, angbo);
    Pos = pOthThing->GetPos();
    Pos += TMove.ConvertToCoord();
    pOthThing->bIsColliding=angbo;  // How convenient

    return;
  }

  // Handle laser-blast; needs 1kWh to smash asteroid
  if (OthKind==GENTHING && pOthThing->GetMass()<1000.0) return;
  
  DeadFlag=TRUE;
  if (OthKind==SHIP) {
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
