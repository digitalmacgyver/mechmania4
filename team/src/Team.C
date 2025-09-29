/* Team.C
 * Definition of CTeam
 * Handles ships, gives orders
 * For use with MechMania IV
 * Misha Voloshin 6/1/98
 */

#include "Team.h"

///////////////////////////////////////////////////
// Construction/Destruction

CTeam::CTeam(unsigned int TNum, CWorld* pworld) {
  uWorldIndex = (unsigned int)-1;
  numShips = 0;
  pmyWorld = pworld;
  TeamNum = TNum;
  uImgSet = 0;
  memset(MsgText, 0, maxTextLen);  // Initialize message buffer to prevent garbled text
}

bool CTeam::Create(unsigned int numSh, unsigned int uCrd) {
  numShips = numSh;
  char namebuf[maxnamelen];
  unsigned int i = 0;

  // COORDINATE SYSTEM: +Y points down on screen, so:
  // fWYMin (-512) is top of screen, fWYMax (+512) is bottom of screen
  CCoord StPos;
  switch (uCrd) {
    case 0:
      StPos = CCoord(fWXMin / 2.0, fWYMin / 2.0);  // Top-left (-256, -256)
      break;
    case 1:
      StPos = CCoord(fWXMax / 2.0, fWYMax / 2.0);  // Bottom-right (256, 256)
      break;
    case 2:
      StPos = CCoord(fWXMin / 2.0, fWYMax / 2.0);  // Bottom-left (-256, 256)
      break;
    case 3:
      StPos = CCoord(fWXMax / 2.0, fWYMin / 2.0);  // Top-right (256, -256)
      break;
    default:
      StPos = CCoord(0.0, 0.0);
  }

  apShips = new CShip*[numShips];
  for (i = 0; i < numShips; ++i) {
    apShips[i] = new CShip(StPos, this, i);
    snprintf(namebuf, maxnamelen, "Ship-%d-of-%d", i, GetTeamNumber());
    apShips[i]->SetName(namebuf);
  }

  pStation = new CStation(StPos, this);
  snprintf(Name, maxTeamNameLen, "Team#%d", GetTeamNumber());
  pStation->SetName(Name);

  return true;
}

CTeam::~CTeam() {
  for (unsigned int i = 0; i < numShips; ++i) {
    delete apShips[i];
  }

  delete[] apShips;
  delete pStation;
}

//////////////////////////////////////////////////////
// Data access

unsigned int CTeam::GetShipCount() const { return numShips; }

unsigned int CTeam::GetTeamNumber() const { return TeamNum; }

CShip* CTeam::GetShip(unsigned int n) const {
  if (n >= numShips) {
    return NULL;
  }
  return apShips[n];
}

CStation* CTeam::GetStation() const { return pStation; }

double CTeam::GetScore() const { return (pStation->GetVinylStore()); }

CWorld* CTeam::GetWorld() const { return pmyWorld; }

unsigned int CTeam::GetWorldIndex() const { return uWorldIndex; }

char* CTeam::GetName() { return Name; }

CBrain* CTeam::GetBrain() { return pBrain; }

///////
// Incoming

CShip* CTeam::SetShip(unsigned int n, CShip* pSh) {
  if (n >= numShips) {
    return NULL;
  }
  CShip* pTmp = apShips[n];
  apShips[n] = pSh;

  if (pSh != NULL) {
    pSh->SetTeam(this);
  }
  if (pTmp != NULL) {
    pTmp->SetTeam(NULL);
  }
  return pTmp;
}

CStation* CTeam::SetStation(CStation* pSt) {
  CStation* pOldSt = GetStation();
  pStation = pSt;

  if (pSt != NULL) {
    pSt->SetTeam(this);
  }
  if (pOldSt != NULL) {
    pOldSt->SetTeam(NULL);
  }
  return pOldSt;
}

CWorld* CTeam::SetWorld(CWorld* pworld) {
  CWorld* ptmpw = GetWorld();
  pmyWorld = pworld;
  return ptmpw;
}

unsigned int CTeam::SetWorldIndex(unsigned int newInd) {
  unsigned int oldInd = GetWorldIndex();
  uWorldIndex = newInd;
  return oldInd;
}

unsigned int CTeam::SetTeamNumber(unsigned int newTN) {
  unsigned int oldTN = GetTeamNumber();
  TeamNum = newTN;
  return oldTN;
}

char* CTeam::SetName(const char* strname) {
  bool bGotZero = false;

  for (unsigned int i = 0; i < maxTeamNameLen; ++i) {
    if (bGotZero == true) {
      Name[i] = 0;
    }

    Name[i] = strname[i];
    if (Name[i] == '\n') {
      Name[i] = ' ';
    }
    if (Name[i] == 0) {
      bGotZero = true;
    }
  }

  Name[maxTeamNameLen - 1] = 0;
  return Name;
}

CBrain* CTeam::SetBrain(CBrain* pBr) {
  CBrain* pBrTmp = GetBrain();
  pBrain = pBr;
  if (pBrain != NULL) {
    pBrain->pTeam = this;
  }
  return pBrTmp;
}

void CTeam::Reset() {
  CShip* pSh;
  unsigned int nSh;

  memset(MsgText, 0, maxTextLen);

  for (nSh = 0; nSh < GetShipCount(); ++nSh) {
    pSh = GetShip(nSh);
    if (pSh == NULL) {
      continue;
    }
    pSh->ResetOrders();
  }
}

double CTeam::GetWallClock() {
  if (pmyWorld == NULL) {
    return 0.0;
  }
  return pmyWorld->auClock[GetWorldIndex()];
}

/////////////////////////////////////////////////////////
// Virtual methods

void CTeam::Init() {}

void CTeam::Turn() {
  // Strategic AI implementation
  // Teams should analyze game state and assign appropriate brains to ships
  // Example strategic decisions:
  // - Early game: Focus on resource gathering
  // - Late game: Balance resources and combat
  // - Low fuel: Prioritize fuel collection
  // - Enemy nearby: Switch to combat behaviors
}

/////////////////////////////////////////////////////////////////
// Serialization routines

unsigned CTeam::GetSerInitSize() const {
  unsigned totsize = 0;

  totsize += BufWrite(NULL, TeamNum);
  totsize += maxTeamNameLen;
  totsize += maxnamelen;

  double carcap = 0.0, fuelcap = 0.0;
  char name[maxnamelen];

  for (unsigned int sh = 0; sh < GetShipCount(); ++sh) {
    totsize += BufWrite(NULL, carcap);
    totsize += BufWrite(NULL, fuelcap);
    totsize += BufWrite(NULL, name, maxnamelen);
  }

  return totsize;
}

unsigned CTeam::SerPackInitData(char* buf, unsigned len) const {
  if (len < GetSerInitSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufWrite(vpb, TeamNum);
  vpb += BufWrite(vpb, Name, maxTeamNameLen);
  vpb += BufWrite(vpb, GetStation()->GetName(), maxnamelen);

  double carcap, fuelcap;
  char name[maxnamelen];
  CShip* pSh;

  for (unsigned int sh = 0; sh < GetShipCount(); ++sh) {
    carcap = 0.0;
    fuelcap = 0.0;
    pSh = GetShip(sh);
    if (pSh != NULL) {
      carcap = pSh->GetCapacity(S_CARGO);
      fuelcap = pSh->GetCapacity(S_FUEL);
      memcpy(name, pSh->GetName(), maxnamelen);
    }

    vpb += BufWrite(vpb, carcap);
    vpb += BufWrite(vpb, fuelcap);
    vpb += BufWrite(vpb, name, maxnamelen);
  }

  return (vpb - buf);
}

unsigned CTeam::SerUnpackInitData(char* buf, unsigned len) {
  if (len < GetSerInitSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufRead(vpb, TeamNum);
  SetName(vpb);
  vpb += maxTeamNameLen;

  GetStation()->SetName(vpb);
  vpb += maxnamelen;

  double carcap, fuelcap;
  char name[maxnamelen];
  CShip* pSh;

  for (unsigned int sh = 0; sh < GetShipCount(); ++sh) {
    vpb += BufRead(vpb, carcap);
    vpb += BufRead(vpb, fuelcap);
    vpb += BufRead(vpb, name, maxnamelen);

    pSh = GetShip(sh);
    if (pSh != NULL) {
      pSh->SetCapacity(S_CARGO, carcap);
      fuelcap = pSh->SetCapacity(S_FUEL, fuelcap);

      pSh->SetAmount(S_FUEL, fuelcap);
      pSh->SetAmount(S_SHIELD, 15.0);
      pSh->SetName(name);
    }
  }

  return (vpb - buf);
}

unsigned CTeam::GetSerialSize() const {
  unsigned totsize = 0;

  totsize += BufWrite(NULL, MsgText, maxTextLen);

  unsigned int shNum, ordnum;
  double ordval = 0.0;

  for (shNum = 0; shNum < GetShipCount(); ++shNum) {
    for (ordnum = 0; ordnum < (unsigned int)O_ALL_ORDERS; ++ordnum) {
      totsize += BufWrite(NULL, ordval);
    }
  }

  return totsize;
}

unsigned CTeam::SerialPack(char* buf, unsigned len) const {
  if (len < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufWrite(vpb, MsgText, maxTextLen);

  unsigned int shNum, ordnum;
  double ordval;
  CShip* pSh;

  for (shNum = 0; shNum < GetShipCount(); ++shNum) {
    pSh = GetShip(shNum);
    for (ordnum = 0; ordnum < (unsigned int)O_ALL_ORDERS; ++ordnum) {
      ordval = 0.0;
      if (pSh != NULL) {
        ordval = pSh->GetOrder((OrderKind)ordnum);
      }

      vpb += BufWrite(vpb, ordval);
    }
  }

  return (vpb - buf);
}

unsigned CTeam::SerialUnpack(char* buf, unsigned len) {
  if (len < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufRead(vpb, MsgText, maxTextLen);

  unsigned int shNum, ordnum;
  double ordval;
  CShip* pSh;

  for (shNum = 0; shNum < GetShipCount(); ++shNum) {
    pSh = GetShip(shNum);
    if (pSh != NULL) {
      pSh->ResetOrders();
    }
    for (ordnum = 0; ordnum < (unsigned int)O_ALL_ORDERS; ++ordnum) {
      vpb += BufRead(vpb, ordval);
      if (pSh != NULL) {
        pSh->SetOrder((OrderKind)ordnum, ordval);
      }
    }
  }

  return (vpb - buf);
}
