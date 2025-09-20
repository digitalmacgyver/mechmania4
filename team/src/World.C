/* World.C
 * Implementation of CWorld
 * Physics model and scoring mechanism
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include "World.h"
#include "Asteroid.h"
#include "Station.h"
#include "Ship.h"
#include "Team.h"

#include <sys/time.h>
#include <ctime>

//////////////////////////////////////////////////
// Construction/Destruction

CWorld::CWorld(UINT nTm)
{
  UINT i;
  numTeams=nTm;

  apTeams = new CTeam*[numTeams];
  atstamp = new double[numTeams];
  auClock = new double[numTeams];
  for (i=0; i<numTeams; i++) {
    apTeams[i]=NULL;
    atstamp[i]=0.0;
    auClock[i]=0.0;
  }

  gametime=0.0;   // Start the clock
  bGameOver=false;

  for (i=0; i<MAX_THINGS; i++) {
    apThings[i]=NULL;
    apTAddQueue[i]=NULL;
    aUNextInd[i]=(UINT)-1;
    aUPrevInd[i]=(UINT)-1;
  }

  UFirstIndex = (UINT)-1;
  ULastIndex = (UINT)-1;
  numNewThings = 0;
}

CWorld::~CWorld()
{
  CThing *pTTmp;
  UINT i;

  for (i=0; i<MAX_THINGS; i++) {
    pTTmp = GetThing(i);
    if (pTTmp==NULL) continue;
    if (pTTmp->GetKind()==ASTEROID)
      delete pTTmp;
  }

  delete [] apTeams;
  delete [] atstamp;
  delete [] auClock;
}

CWorld* CWorld::CreateCopy()
{
  CWorld *pWld;
  pWld = new CWorld(numTeams);

  UINT acsz, sz=GetSerialSize();
  char *buf = new char[sz];

  acsz = SerialPack(buf,sz);
  if (acsz!=sz) {
    printf ("ERROR: World assignment\n");
    return NULL;
  }

  pWld->SerialUnpack(buf,acsz);
  delete buf;
  return pWld;
}

//////////////////////////////////////////////////
// Data access to internal members

CTeam* CWorld::GetTeam(UINT nt) const
{
  if (nt>=GetNumTeams()) return NULL;
  return apTeams[nt];
}

UINT CWorld::GetNumTeams() const
{
  return numTeams;
}

double CWorld::GetGameTime() const
{
  return gametime;
}

CThing* CWorld::GetThing(UINT index) const
{
  if (index>=MAX_THINGS) return NULL;
  return apThings[index];
}

UINT CWorld::GetNextIndex(UINT curindex) const
{
  if (curindex>=MAX_THINGS) return (UINT)-1;
  return aUNextInd[curindex];
}

UINT CWorld::GetPrevIndex(UINT curindex) const
{
  if (curindex>=MAX_THINGS) return (UINT)-1;
  return aUPrevInd[curindex];
}

/////////////////////////////////////////////////////
// Explicit functions

UINT CWorld::PhysicsModel (double dt)
{
  CThing* pThing;
  UINT i;

  for (i=UFirstIndex; i!=(UINT)-1; i=GetNextIndex(i)) {
    pThing = GetThing(i);
    pThing->Drift(dt);
  }

  CollisionEvaluation();
  AddNewThings();    // It's possible that new things are already dead
  KillDeadThings();   // So this comes after AddNewThings

  gametime+=dt;
  return 0;
}

void CWorld::LaserModel ()
{
  UINT nteam,nship;
  CTeam *pTeam;
  CShip *pShip;
  CThing *pTarget, LasThing;
  CCoord LasPos, TmpPos;
  CTraj LasTraj, TarVel, TmpTraj;
  double dfuel,dLasPwr, dLasRng;

  for (nteam=0; nteam<GetNumTeams(); nteam++) {
    pTeam = GetTeam(nteam);
    if (pTeam==NULL) continue;
    for (nship=0; nship<pTeam->GetShipCount(); nship++) {
      pShip = pTeam->GetShip(nship);
      if (pShip==NULL) continue;
      dLasPwr = pShip->GetOrder(O_LASER);
      if (dLasPwr<=0.0) continue;

      LasPos = pShip->GetPos();
      LasTraj = CTraj(dLasPwr,pShip->GetOrient());
      LasPos += LasTraj.ConvertToCoord();
      LasThing.SetPos(LasPos);

      pTarget = pShip->LaserTarget();
      dLasRng = LasPos.DistTo(pShip->GetPos());
      if (dLasRng>dLasPwr) pTarget=NULL;
      if (pTarget!=NULL) {
	TmpPos=pTarget->GetPos();
	TmpTraj = pShip->GetPos().VectTo(TmpPos);
	TmpTraj.rho=1.0;
	TmpPos -= (CCoord)TmpTraj;
	LasThing.SetPos(TmpPos);

	dLasRng = TmpPos.DistTo(pShip->GetPos());
	LasThing.SetMass(30.0*(dLasPwr-dLasRng));
	TarVel = pTarget->GetVelocity();
	TarVel.rho += 1.0;
	LasThing.SetVel(TarVel);

	// Log laser hit
	const char* targetType = "unknown";
	const char* targetName = pTarget->GetName();
	if (pTarget->GetKind() == SHIP) {
	  targetType = "Ship";
	} else if (pTarget->GetKind() == STATION) {
	  targetType = "Station";
	} else if (pTarget->GetKind() == ASTEROID) {
	  targetType = "Asteroid";
	}

	printf("[LASER HIT] %s %s (Team %d) shot %s %s",
	       pShip->GetTeam()->GetName(),
	       pShip->GetName(),
	       pShip->GetTeam()->GetTeamNumber(),
	       targetType,
	       targetName ? targetName : "");

	// Add team info for ships/stations
	if (pTarget->GetKind() == SHIP && pTarget->GetTeam()) {
	  printf(" (Team %d)", pTarget->GetTeam()->GetTeamNumber());
	} else if (pTarget->GetKind() == STATION && pTarget->GetTeam()) {
	  printf(" (Team %d)", pTarget->GetTeam()->GetTeamNumber());
	}
	printf("\n");

	pTarget->Collide(&LasThing, this);
      }

      double oldFuel = pShip->GetAmount(S_FUEL);
      dfuel = oldFuel;
      dfuel -= pShip->SetOrder(O_LASER,dLasPwr);
      pShip->SetAmount(S_FUEL,dfuel);

      // Check if out of fuel
      if (oldFuel > 0.01 && dfuel <= 0.01) {
        printf("[OUT OF FUEL] Ship %s (Team %d) ran out of fuel\n",
               pShip->GetName(), pShip->GetTeam() ? pShip->GetTeam()->GetTeamNumber() : -1);
      }
    }
  }

  AddNewThings();
  KillDeadThings();
}

void CWorld::AddThingToWorld(CThing* pNewThing)
{
  if (pNewThing==NULL || numNewThings>=MAX_THINGS) return;
  apTAddQueue[numNewThings] = pNewThing;
  numNewThings++;
}

void CWorld::CreateAsteroids(AsteroidKind mat, UINT numast, double mass)
{
  CAsteroid *pAst;
  UINT i;

  for (i=0; i<numast; i++) {
    pAst = new CAsteroid (mass,mat);
    AddThingToWorld(pAst);
  }
}

CTeam* CWorld::SetTeam (UINT n, CTeam* pTm)
{
  if (n>=GetNumTeams()) return NULL;

  CTeam* oldteam = apTeams[n];
  CTeam* tmown;
  CThing* delth;
  ThingKind delkind;
  UINT i, numsh;

  if (oldteam==pTm) return oldteam;
  if (oldteam!=NULL) {
    for (i=UFirstIndex; i!=(UINT)-1; i=GetNextIndex(i)) {
      delth=GetThing(i);
      delkind=delth->GetKind();
      if (delkind==SHIP || delkind==STATION) {
	if (delkind==SHIP) tmown = ((CShip*)delth)->GetTeam();
	else tmown = ((CStation*)delth)->GetTeam();

	if (tmown==oldteam) RemoveIndex(i);
      }
    }
  }

  apTeams[n]=pTm;
  pTm->SetWorldIndex(n);
  pTm->SetWorld(this);
  AddThingToWorld(pTm->GetStation());
  for (numsh=0; numsh<pTm->GetShipCount(); numsh++) {
    AddThingToWorld(pTm->GetShip(numsh));
  }

  return oldteam;
}

//////////////////////////////////////////////
// Assistant Methods

void CWorld::RemoveIndex(UINT index)
{
  if (index>=MAX_THINGS) return;

  UINT Prev,Next;
  Prev=aUPrevInd[index];
  Next=aUNextInd[index];

  if (Prev<MAX_THINGS) {
    aUNextInd[Prev] = Next;  // Work him out of the sequence
  }
  
  if (Next<MAX_THINGS) {
    aUPrevInd[Next] = Prev;
  }
      
  aUPrevInd[index]=(UINT)-1;                 // Reset his indices
  aUNextInd[index]=(UINT)-1; 
      
  apThings[index]=NULL;              // And kiss 'im goodbye

  if (index==UFirstIndex) UFirstIndex=Next;
  if (index==ULastIndex) ULastIndex=Prev;
}

UINT CWorld::CollisionEvaluation()
{
  CThing *pTItr,*pTTm;
  UINT i,j, iteam, iship, numtmth, URes=0;
  CTeam* pTeam;
  static CThing* apTTmTh[MAX_THINGS];  // List of team-controlled (i.e. non-asteroid) objects
                                       // static saves on reallocation time btwn calls
  numtmth=0;
  for (iteam=0; iteam<GetNumTeams(); iteam++) {
    pTeam = GetTeam(iteam);
    if (pTeam==NULL) continue;
    pTTm = pTeam->GetStation();  // Put station into list
    apTTmTh[numtmth] = pTTm;
    numtmth++;

    if (bGameOver==true) continue;  // Ships invisible after game ends

    for (iship=0; iship<pTeam->GetShipCount(); iship++) {
      pTTm = pTeam->GetShip(iship);
      if (pTTm==NULL) continue;
      apTTmTh[numtmth] = pTTm;
      numtmth++;
    }
  }
    
  for (i=UFirstIndex; i!=(UINT)-1; i=GetNextIndex(i)) {
    pTItr = GetThing(i);
    if ((pTItr->IsAlive())==false) continue;
    if (pTItr==NULL) continue;

    for (j=0; j<numtmth; j++) {
      pTTm = apTTmTh[j];
      if (pTTm==NULL) continue;

      pTItr->Collide(pTTm,this);   // Asteroid(?) shattered by ship
      if (pTTm->Collide(pTItr,this)==true)   // Ship deflected by asteroid(?)
	URes++;
    }
  }

  return URes;
}

UINT CWorld::AddNewThings()
{
  UINT URes,UInd;

  if (numNewThings==0) return 0;  // Duh.

  for (URes=0; URes<numNewThings; URes++) {
    UInd = ULastIndex + 1;
    if (ULastIndex==(UINT)-1) UInd=0;  // Might as well make it explicit
    if (URes>=MAX_THINGS) break;  // Can't hold anymore!!

    apThings[UInd] = apTAddQueue[URes];
    apThings[UInd]->SetWorld(this);
    apThings[UInd]->SetWorldIndex(UInd);

    aUPrevInd[UInd] = ULastIndex;

    if (ULastIndex==(UINT)-1) UFirstIndex=UInd;
    else aUNextInd[ULastIndex] = UInd;
    
    ULastIndex = UInd;
  }

  numNewThings=0;
  return URes;
}

UINT CWorld::KillDeadThings()
{
  CThing *pTTry;
  UINT URes=0,index,ShNum;
  CTeam* pTm;
  ThingKind KTry;

  for (index=UFirstIndex; index!=(UINT)-1; index=GetNextIndex(index)) {
    pTTry=GetThing(index);

    if ((pTTry->IsAlive())!=true) {
      RemoveIndex(index);
      URes++;

      KTry = pTTry->GetKind();
      if (KTry==SHIP) {
	pTm = ((CShip*)pTTry)->GetTeam();
	if (pTm!=NULL) {
	  ShNum = ((CShip*)pTTry)->GetShipNumber();
	  pTm->SetShip(ShNum,NULL);
	}
      }

      delete pTTry;
      continue;
    }
  }

  return URes;
}

void CWorld::ReLinkList()
{
  UINT i,ilast=(UINT)-1;
  CThing *pTh;

  for (i=0; i<MAX_THINGS; i++) {
    pTh = apThings[i];
    if (pTh==NULL) continue;

    aUPrevInd[i] = ilast;
    if (ilast!=(UINT)-1)
      aUNextInd[ilast] = i;
    else UFirstIndex=i;

    ilast=i;
  }

  ULastIndex=ilast;
}

double CWorld::GetTimeStamp()
{
  struct timeval tp;
  gettimeofday(&tp,NULL);

  double res = (double)(tp.tv_sec);    // Seconds
  res += (double)(tp.tv_usec)/1000000.0;  // microseconds
  return res;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CWorld::GetSerialSize() const
{
  UINT totsize=0;
  CThing* pTh;

  totsize += BufWrite (NULL, UFirstIndex);
  totsize += BufWrite (NULL, ULastIndex);
  totsize += BufWrite (NULL, gametime);

  UINT i,inext,sz,iTm, crc=666, uTK=0;

  for (i=0; i<numTeams; i++) {
    totsize += BufWrite (NULL, auClock[i]);
    totsize += GetTeam(i)->GetSerialSize();
  }

  UINT tk=0;
  for (i=UFirstIndex; i!=(UINT)-1; i=GetNextIndex(i)) {
    pTh=GetThing(i);
    tk++;
    sz=pTh->GetSerialSize();
    inext = GetNextIndex(i);

    iTm=0;
    totsize += BufWrite (NULL, crc);
    totsize += BufWrite (NULL, inext);
    totsize += BufWrite (NULL, sz);

    totsize += BufWrite (NULL, uTK);
    totsize += BufWrite (NULL, iTm);

    totsize += pTh->GetSerialSize();
  }

  return totsize;
}

unsigned CWorld::SerialPack (char *buf, unsigned buflen) const
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;
  CThing* pTh;

  vpb += BufWrite (vpb, UFirstIndex);
  vpb += BufWrite (vpb, ULastIndex);
  vpb += BufWrite (vpb, gametime);

  UINT i,inext,sz,iTm, crc=666, uTK;
  ThingKind TKind;
  CTeam *ptTeam;

  for (i=0; i<numTeams; i++) {
    vpb += BufWrite (vpb, auClock[i]);
    vpb += GetTeam(i)->SerialPack(vpb, buflen-(vpb-buf));
  }

  UINT tk=0;
  for (i=UFirstIndex; i!=(UINT)-1; i=GetNextIndex(i)) {
    pTh=GetThing(i);
    tk++;
    sz=pTh->GetSerialSize();
    TKind = pTh->GetKind();
    inext = GetNextIndex(i);

    iTm=0;
    if ((ptTeam=pTh->GetTeam())!=NULL) iTm=ptTeam->GetWorldIndex();
    if (TKind==SHIP) iTm |= (((CShip*)pTh)->GetShipNumber()) << 8; 
    if (TKind==ASTEROID) iTm = (UINT)((CAsteroid*)pTh)->GetMaterial();

    vpb += BufWrite (vpb, crc);
    vpb += BufWrite (vpb, inext);
    vpb += BufWrite (vpb, sz);

    uTK = (UINT) TKind;
    vpb += BufWrite (vpb, uTK);
    vpb += BufWrite (vpb, iTm);

    vpb += pTh->SerialPack((char*)vpb,sz);
  }

  return (vpb-buf);
}
  
unsigned CWorld::SerialUnpack (char *buf, unsigned buflen)
{
  char *vpb = buf;
  CThing* pTh;
  CAsteroid ATmp;

  UINT i,inext,ilast, crc, uTK;
  UINT sz,acsz,iTm;
  ThingKind TKind;
  UINT tk=0;

  vpb += BufRead (vpb, inext);
  vpb += BufRead (vpb, ilast);
  vpb += BufRead (vpb, gametime);

  for (i=0; i<numTeams; i++) {
    vpb += BufRead (vpb, auClock[i]);
    vpb += GetTeam(i)->SerialUnpack(vpb, buflen-(vpb-buf));
  }

  for (i=UFirstIndex; i<=ilast; i++) {
    pTh = GetThing(i);
    if (pTh!=NULL && i<inext) {
      pTh->KillThing();
    }

    if (i==inext) {
      tk++;
      vpb += BufRead (vpb, crc);
      if (crc!=666) printf ("Off-track!!, %d\n",crc);

      vpb += BufRead (vpb, inext);
      vpb += BufRead (vpb, sz);
      vpb += BufRead (vpb, uTK);
      TKind = (ThingKind) uTK;

      vpb += BufRead (vpb, iTm);

      if (pTh==NULL) { 
	pTh=CreateNewThing(TKind,iTm);
	apThings[i]=pTh;
      }
      acsz = pTh->SerialUnpack((char *)vpb, sz);
      if (acsz != sz) printf ("Serialization discrepancy, %d!=%d\n",acsz,sz);

      pTh->SetWorld(this);
      pTh->SetWorldIndex(i);

      vpb+=acsz;
      if (vpb >= buf+buflen) break;  // stooooooppppppp!!
      if (inext==(UINT)-1) break;
    }
  }

  if (ilast<ULastIndex) {     // Stuff died at the end of the list
    for (i=ilast+1; i<=ULastIndex; i++) {
      pTh=GetThing(i);
      if (pTh!=NULL)
	pTh->KillThing();
    }
  }

  KillDeadThings();
  ReLinkList();

  return (vpb-buf);
}

CThing* CWorld::CreateNewThing (ThingKind TKind, UINT iTm)
{
  CThing *pTh, *pThOld;
  CTeam* pTeam;
  UINT shnum=0;
  pThOld=NULL;

  shnum = iTm >> 8;
  iTm = iTm & 0xff;
  pTeam=GetTeam(iTm);
  
  switch (TKind) {
    case STATION: 
      pTh=new CStation(CCoord(0.0,0.0));  
      if (pTeam!=NULL) 
	pThOld = pTeam->SetStation((CStation*)pTh);
      break;

    case SHIP: 
      pTh=new CShip(CCoord(0.0,0.0)); 
      if (pTeam!=NULL) 
	pThOld = pTeam->SetShip(shnum,(CShip*)pTh);
      break;

    case ASTEROID:
      pTh=new CAsteroid();
      break;

    default: pTh=new CThing;
  }

  if (pThOld!=NULL) delete pThOld;
  return pTh;
}
