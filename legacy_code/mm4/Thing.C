/* Thing.C
 * Definition of class CThing
 * For use with MechMania IV
 * 4/29/98 by Misha Voloshin
*/ 

#include "Coord.h"
#include "Traj.h"
#include "Thing.h"
#include "World.h"
#include "Team.h"

/////////////////////////////////////////////
// Construction/destruction

CThing::CThing (double fx0, double fy0)
{
  TKind=GENTHING;
  for (UINT i=0; i<maxnamelen; i++)
     Name[i]=0;  // Initialize Name

  sprintf (Name,"Generic Thing");
  ulIDCookie=rand();
  DeadFlag=FALSE;
  bIsColliding=NO_DAMAGE;
  bIsGettingShot=NO_DAMAGE;

  pmyTeam=NULL;
  pmyWorld=NULL;
  uWldIndex=(UINT)-1;

  Pos=CCoord(fx0,fy0);
  Vel=CTraj(0.0,0.0);
  orient=0.0;
  omega=0.0;
  uImgSet=0;

  mass=1.0;
  size=1.0;
}

CThing::CThing (const CThing& OthThing)
{
  *this = OthThing;
}

CThing::~CThing()
{

}

////////////////////////////////////////////////
// Data Access functions

/////
// Outgoing data

const CCoord& CThing::GetPos() const
{
  return Pos;
}

ThingKind CThing::GetKind() const
{
  return TKind;
}

double CThing::GetMass() const
{
  return mass;
}

double CThing::GetSize() const
{
  return size;
}

double CThing::GetOrient() const
{
  return orient;
}

const CTraj& CThing::GetVelocity() const
{
  return Vel;
}

CTraj CThing::GetMomentum() const
{
  return Vel*GetMass();
}

CTeam *CThing::GetTeam() const
{
  return pmyTeam;
}

BOOL CThing::IsAlive() const
{
  if (DeadFlag==TRUE) return FALSE;
  return TRUE;
}

UINT CThing::GetImage() const
{
  return uImgSet;
}

const char *CThing::GetName() const
{
  return Name;
}

///////
// Incoming data

void CThing::SetName (const char *strsrc)
{
  BOOL bGotZero=FALSE;

  for (UINT i=0; i<maxnamelen-1; i++) {
    if (bGotZero==TRUE) Name[i]=0;
    Name[i]=strsrc[i];
    if (Name[i]=='\n') Name[i]=' ';
    if (Name[i]==0) bGotZero=TRUE;
  }

  Name[maxnamelen-1]=0;
}

void CThing::KillThing()
{
  DeadFlag=TRUE;
}

////////////////////////////////////////////////
// Explicit methods

void CThing::Drift (double dt)
{
  bIsColliding=NO_DAMAGE;
  bIsGettingShot=NO_DAMAGE;
  if (Vel.rho > maxspeed) Vel.rho=maxspeed;

  Pos += (Vel*dt).ConvertToCoord();
  orient += omega*dt;

  if (orient<-PI || orient>PI) {
    CTraj VTmp(1.0,orient);
    VTmp.Normalize();
    orient = VTmp.theta;
  }
}

BOOL CThing::Collide (CThing* pOthThing, CWorld *pWorld)
{
  if (pOthThing==NULL) {
    printf ("Colliding with NULL!\n");
    return FALSE;  // How did THAT happen!!??
  }
  if (*pOthThing==*this) return FALSE;  // Can't collide with yourself!

  if (Overlaps(*pOthThing)==FALSE) return FALSE;

  double dAng = GetPos().AngleTo(pOthThing->GetPos());
  if (pOthThing->GetKind()==GENTHING) bIsGettingShot=dAng;
  else bIsColliding=dAng;

  HandleCollision(pOthThing,pWorld);
  return TRUE;
}

BOOL CThing::Overlaps (const CThing& OthThing) const
{
  if (OthThing==*this) return FALSE;  // Overlap yourself? :P

  double dtmprad,ddist;
    dtmprad = size + OthThing.GetSize();
  ddist = Pos.DistTo(OthThing.GetPos());

  if (ddist<dtmprad) return TRUE; 
  return FALSE;
}

////////////////////////////////////////////////
// Helper functions

CCoord CThing::PredictPosition (double dt) const
{
  CCoord PosRes = GetPos();
  CTraj TmpVel(GetVelocity());
  if (TmpVel.rho==0.0) return PosRes;

  PosRes += (CCoord)(GetVelocity()*dt);
  return PosRes;
}

CTraj CThing::RelativeVelocity (const CThing& OthThing) const
{
  return (OthThing.Vel-Vel);
}

CTraj CThing::RelativeMomentum (const CThing& OthThing) const
{
  return (RelativeVelocity(OthThing)*OthThing.GetMass());
}

BOOL CThing::IsFacing (const CThing& OthThing) const
{
  if (*this==OthThing) return FALSE;  // Won't laser-fire yourself

  CCoord cOrg(0.0,0.0), cOth(OthThing.GetPos()-GetPos());
  if (cOrg==cOth) return TRUE;

  double ddist = cOrg.DistTo(cOth);

  CTraj tGo(1.0,GetOrient());
  tGo = tGo * ddist;

  CCoord cGo(cOrg);
  cGo += tGo.ConvertToCoord();

  double dhit = cGo.DistTo(cOth);
  if (dhit <= OthThing.GetSize()) return TRUE;
  return FALSE;
}

double CThing::DetectCollisionCourse(const CThing& OthThing) const
{
  if (OthThing==*this) return NO_COLLIDE;

  CTraj VRel = RelativeVelocity(OthThing);  // Direction of vector
  if (VRel.rho<=0.05) return NO_COLLIDE;  // Never gonna hit if effectively not moving

  double flyred = GetSize() + OthThing.GetSize();   // Don't allow them to scrape each other
  double dist = GetPos().DistTo(OthThing.GetPos());  // Magnitude of vector
  if (dist<flyred) return 0.0;   // They're already impacting

  CTraj VHit (dist,VRel.theta);
  CCoord RelPos = OthThing.GetPos()-GetPos(),
    CHit(RelPos+VHit.ConvertToCoord());

  double flyby = CHit.DistTo(CCoord(0.0,0.0));
  if (flyby>flyred) return NO_COLLIDE;
  
  // Pending collision
  double hittime = (dist-flyred) / VRel.rho;
  return hittime;
}

////////////////////////////////////////////////
// Operators

CThing& CThing::operator= (const CThing& OthThing)
{
  char *buf;
  UINT pksz, sz=OthThing.GetSerialSize();

  buf = new char[sz];
  pksz = OthThing.SerialPack(buf,sz);

  if (pksz!=sz) {
    printf ("ERROR: Assignment operator failure\n");
    return *this;
  }

  SerialUnpack(buf,pksz);
  delete buf;
  return *this;
}

BOOL CThing::operator== (const CThing& OthThing) const
{
  if (ulIDCookie!=OthThing.ulIDCookie) return FALSE;

  return TRUE;
}

BOOL CThing::operator!= (const CThing& OthThing) const
{
  return (!(*this==OthThing));
}

//////////////////////////////////////////////////////////////
// Protected methods

void CThing::HandleCollision(CThing* pOthThing, CWorld *pWorld)
{
  if (pOthThing==NULL) return;
  if (pWorld==NULL) return;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CThing::GetSerialSize() const
{
  UINT totsize=0;

  UINT uTK = (UINT) TKind;
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

unsigned CThing::SerialPack (char *buf, unsigned buflen) const
{
  UINT needlen = GetSerialSize();
  if (buflen<needlen) return 0;
  char *vpb = buf;

  UINT uTK = (UINT) TKind;
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

  vpb += Pos.SerialPack(vpb,Pos.GetSerialSize());
  vpb += Vel.SerialPack(vpb,Vel.GetSerialSize());

  return (vpb-buf);
}
  
unsigned CThing::SerialUnpack (char *buf, unsigned buflen)
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  UINT uTK;
  vpb += BufRead(vpb, uTK);
  TKind = (ThingKind) uTK;

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

  vpb += Pos.SerialUnpack((char*)vpb,Pos.GetSerialSize());
  vpb += Vel.SerialUnpack((char*)vpb,Vel.GetSerialSize());

  return (vpb-buf);
}
