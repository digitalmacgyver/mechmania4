/* Team.C
 * Definition of CTeam
 * Handles ships, gives orders
 * For use with MechMania IV
 * Misha Voloshin 6/1/98
 */

#include "Team.h"

///////////////////////////////////////////////////
// Construction/Destruction

CTeam::CTeam(UINT TNum, CWorld *pworld)
{
  uWorldIndex=(UINT)-1;
  numShips=0;
  pmyWorld = pworld;
  TeamNum=TNum;  
  uImgSet=0;
}

BOOL CTeam::Create (UINT numSh, UINT uCrd)
{
  numShips = numSh;
  char namebuf[maxnamelen];
  UINT i=0;
  
  CCoord StPos;
  switch (uCrd) {
    case 0: StPos=CCoord(fWXMin/2.0,fWYMin/2.0);  break;
    case 1: StPos=CCoord(fWXMax/2.0,fWYMax/2.0);  break;
    case 2: StPos=CCoord(fWXMin/2.0,fWYMax/2.0);  break;
    case 3: StPos=CCoord(fWXMax/2.0,fWYMin/2.0);  break;
    default: StPos=CCoord(0.0,0.0);
  }

  apShips = new CShip*[numShips];
  for (i=0; i<numShips; i++) {
    apShips[i] = new CShip(StPos,this,i);
    sprintf (namebuf,"Ship-%d-of-%d",i,GetTeamNumber());
    apShips[i]->SetName(namebuf);
  }
  
  pStation = new CStation(StPos,this);
  sprintf(Name,"Team#%d",GetTeamNumber());
  pStation->SetName(Name);

  return TRUE;
}

CTeam::~CTeam()
{
  for (UINT i=0; i<numShips; i++) {
    delete apShips[i];
  }
  
  delete [] apShips;
  delete pStation;
}

//////////////////////////////////////////////////////
// Data access

UINT CTeam::GetShipCount() const
{
  return numShips;
}

UINT CTeam::GetTeamNumber() const
{
  return TeamNum;
}

CShip* CTeam::GetShip(UINT n) const
{
  if (n>=numShips) return NULL;
  return apShips[n];
}

CStation* CTeam::GetStation() const
{
  return pStation;
}

double CTeam::GetScore() const
{
  return (pStation->GetVinylStore());
}

CWorld* CTeam::GetWorld() const
{
  return pmyWorld;
}

UINT CTeam::GetWorldIndex() const
{
  return uWorldIndex;
}

char *CTeam::GetName()
{
  return Name;
}

CBrain* CTeam::GetBrain()
{
  return pBrain;
}

///////
// Incoming

CShip *CTeam::SetShip(UINT n, CShip* pSh)
{
  if (n>=numShips) return NULL;
  CShip *pTmp = apShips[n];
  apShips[n] = pSh;

  if (pSh!=NULL) pSh->SetTeam(this);
  if (pTmp!=NULL) pTmp->SetTeam(NULL);
  return pTmp;
}

CStation *CTeam::SetStation(CStation *pSt)
{
  CStation *pOldSt = GetStation();
  pStation = pSt;

  if (pSt!=NULL) pSt->SetTeam(this);
  if (pOldSt!=NULL) pOldSt->SetTeam(NULL);
  return pOldSt;
}

CWorld* CTeam::SetWorld (CWorld* pworld)
{
  CWorld *ptmpw = GetWorld();
  pmyWorld = pworld;
  return ptmpw;
}

UINT CTeam::SetWorldIndex(UINT newInd)
{
  UINT oldInd=GetWorldIndex();
  uWorldIndex=newInd;
  return oldInd;
}

UINT CTeam::SetTeamNumber(UINT newTN)
{
  UINT oldTN=GetTeamNumber();
  TeamNum = newTN;
  return oldTN;
}

char *CTeam::SetName(char *strname)
{
  BOOL bGotZero=FALSE;

  for (UINT i=0; i<maxTeamNameLen; i++) {
    if (bGotZero==TRUE) Name[i]=0;

    Name[i]=strname[i];
    if (Name[i]=='\n') Name[i]=' ';
    if (Name[i]==0) bGotZero=TRUE;
  }

  Name[maxTeamNameLen-1]=0;
  return Name;
}

CBrain* CTeam::SetBrain(CBrain* pBr)
{
  CBrain *pBrTmp = GetBrain();
  pBrain = pBr;
  if (pBrain!=NULL) pBrain->pTeam = this;
  return pBrTmp;
}

void CTeam::Reset()
{
  CShip *pSh;
  UINT nSh;

  memset(MsgText,0, maxTextLen);

  for (nSh=0; nSh<GetShipCount(); nSh++) {
    pSh=GetShip(nSh);
    if (pSh==NULL) continue;
    pSh->ResetOrders();
  }
}

double CTeam::GetWallClock()
{
  if (pmyWorld==NULL) return 0.0;
  return pmyWorld->auClock[GetWorldIndex()];
}

/////////////////////////////////////////////////////////
// Virtual methods

void CTeam::Init()
{

}

void CTeam::Turn()
{

}

/////////////////////////////////////////////////////////////////
// Serialization routines

unsigned CTeam::GetSerInitSize() const
{
  unsigned totsize=0;

  totsize += BufWrite (NULL, TeamNum);
  totsize += maxTeamNameLen;
  totsize += maxnamelen;
  
  double carcap=0.0,fuelcap=0.0;
  char name[maxnamelen];

  for (UINT sh=0; sh<GetShipCount(); sh++) {
    totsize += BufWrite (NULL, carcap);
    totsize += BufWrite (NULL, fuelcap);
    totsize += BufWrite (NULL, name, maxnamelen);
  }
  
  return totsize;
}

unsigned CTeam::SerPackInitData(char *buf, unsigned len) const
{
  if (len<GetSerInitSize()) return 0;
  char *vpb = buf;

  vpb += BufWrite (vpb, TeamNum);
  vpb += BufWrite (vpb, Name, maxTeamNameLen);
  vpb += BufWrite (vpb, GetStation()->GetName(), maxnamelen);

  double carcap,fuelcap;
  char name[maxnamelen];
  CShip *pSh;

  for (UINT sh=0; sh<GetShipCount(); sh++) {
    carcap=0.0;
    fuelcap=0.0;
    pSh = GetShip(sh);
    if (pSh!=NULL) {
      carcap = pSh->GetCapacity(S_CARGO);
      fuelcap = pSh->GetCapacity(S_FUEL);
      memcpy(name,pSh->GetName(), maxnamelen);
    }

    vpb += BufWrite (vpb, carcap);
    vpb += BufWrite (vpb, fuelcap);
    vpb += BufWrite (vpb, name, maxnamelen);
  }

  return (vpb-buf);
}

unsigned CTeam::SerUnpackInitData (char *buf, unsigned len)
{
  if (len<GetSerInitSize()) return 0;
  char *vpb = buf;

  vpb += BufRead (vpb, TeamNum);
  SetName(vpb);
  vpb += maxTeamNameLen;
  
  GetStation()->SetName(vpb);
  vpb += maxnamelen;

  double carcap,fuelcap;
  char name[maxnamelen];
  CShip *pSh;

  for (UINT sh=0; sh<GetShipCount(); sh++) {
    vpb += BufRead (vpb, carcap);
    vpb += BufRead (vpb, fuelcap);
    vpb += BufRead (vpb, name, maxnamelen);

    pSh = GetShip(sh);
    if (pSh!=NULL) {
      pSh->SetCapacity(S_CARGO,carcap);
      fuelcap = pSh->SetCapacity(S_FUEL,fuelcap);

      pSh->SetAmount(S_FUEL,fuelcap);
      pSh->SetAmount(S_SHIELD,15.0);
      pSh->SetName(name);
    }
  }

  return (vpb-buf);
}


unsigned CTeam::GetSerialSize() const
{
  unsigned totsize=0;
 
  totsize += BufWrite (NULL, MsgText, maxTextLen);

  UINT shNum, ordnum;
  double ordval=0.0;

  for (shNum=0; shNum<GetShipCount(); shNum++) {
    for (ordnum=0; ordnum<(UINT)O_ALL_ORDERS; ordnum++) {
      totsize += BufWrite (NULL, ordval);
    }
  }

  return totsize;
}

unsigned CTeam::SerialPack (char *buf, unsigned len) const
{
  if (len<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += BufWrite (vpb, MsgText, maxTextLen);

  UINT shNum, ordnum;
  double ordval;
  CShip *pSh;

  for (shNum=0; shNum<GetShipCount(); shNum++) {
    pSh = GetShip(shNum);
    for (ordnum=0; ordnum<(UINT)O_ALL_ORDERS; ordnum++) {
      ordval=0.0;
      if (pSh!=NULL)
	ordval = pSh->GetOrder((OrderKind)ordnum);
      
      vpb += BufWrite (vpb, ordval);
    }
  }

  return (vpb-buf);
}


unsigned CTeam::SerialUnpack (char *buf, unsigned len)
{
  if (len<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += BufRead (vpb, MsgText, maxTextLen);

  UINT shNum, ordnum;
  double ordval;
  CShip *pSh;

  for (shNum=0; shNum<GetShipCount(); shNum++) {
    pSh = GetShip(shNum);
    if (pSh!=NULL) pSh->ResetOrders();
    for (ordnum=0; ordnum<(UINT)O_ALL_ORDERS; ordnum++) {
      vpb += BufRead (vpb, ordval);
      if (pSh!=NULL)
	pSh->SetOrder ((OrderKind)ordnum, ordval);
    }
  }

  return (vpb-buf);
}
