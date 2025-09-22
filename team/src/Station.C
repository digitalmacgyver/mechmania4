/* Station.C
 * Implementation of the CStation class
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include "Station.h"
#include "Ship.h"
#include "Team.h"

///////////////////////////////////////////
// Construction/Destruction

CStation::CStation (CCoord StPos, CTeam* pTeam) 
  : CThing(StPos.fX,StPos.fY)
{
  TKind=STATION;
  pmyTeam = pTeam;

  if (pmyTeam!=NULL) {
    snprintf(Name, maxnamelen, "Station #%d", pmyTeam->GetTeamNumber());
    uImgSet=pTeam->uImgSet;
  }
  else {
    snprintf(Name, maxnamelen, "Station");
    uImgSet=0;
  }

  size=30.0;
  mass=99999.9;
  orient=0.0;
  omega=0.9;
  dCargo=0.0;
}

CStation::~CStation()
{

}

////////////////////////////////////////////
// Data access

double CStation::GetVinylStore() const
{
  return dCargo;
}

/////////////////////////////////////////////
// Explicit functions

double CStation::AddVinyl(double dvtons)
{
  dCargo += dvtons;
  return dCargo;
}

/////////////////////////////////////////////
// Protected methods

void CStation::HandleCollision (CThing* pOthThing, CWorld *pWorld)
{
  ThingKind OthKind = pOthThing->GetKind();
  CTraj myVel(GetVelocity());
  CCoord myPos(GetPos());

  if (OthKind==SHIP) {
    bIsColliding=NO_DAMAGE;
    return;
  }

  if (OthKind!=GENTHING) return;  // Only gets lased
  if (pWorld==NULL) return;  // Colliding in a vaccuum?

  double dDmg = pOthThing->GetMass();   // Laser object, whuppage is mass
  dDmg /= 1000.0;   // Takes a kilowhup
  double oldCargo = dCargo;
  dCargo -= dDmg;
  if (dCargo<0.0) dCargo=0.0;

  // Log station damage
  if (dDmg > 0.01) {
    printf("[STATION DAMAGE] Station %s (%s) lost %.2f vinyl from laser (%.2f -> %.2f)\n",
           GetName(), GetTeam() ? GetTeam()->GetName() : "Unknown", dDmg, oldCargo, dCargo);
  }
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CStation::GetSerialSize() const
{
  UINT totsize=0;

  totsize += CThing::GetSerialSize();
  totsize += BufWrite(NULL, dCargo);

  return totsize;
}

unsigned CStation::SerialPack (char *buf, unsigned buflen) const
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += (CThing::SerialPack(buf,buflen));
  vpb += BufWrite(vpb, dCargo);

  return (vpb-buf);
}
  
unsigned CStation::SerialUnpack (char *buf, unsigned buflen)
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += (CThing::SerialUnpack(buf,buflen));
  vpb += BufRead(vpb, dCargo);

  return (vpb-buf);
}
