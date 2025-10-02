/* World.C
 * Implementation of CWorld
 * Physics model and scoring mechanism
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include <sys/time.h>

#include <ctime>

#include "Asteroid.h"
#include "GameConstants.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "World.h"

//////////////////////////////////////////////////
// Construction/Destruction

CWorld::CWorld(unsigned int nTm) {
  unsigned int i;
  numTeams = nTm;

  apTeams = new CTeam*[numTeams];
  atstamp = new double[numTeams];
  auClock = new double[numTeams];
  for (i = 0; i < numTeams; ++i) {
    apTeams[i] = NULL;
    atstamp[i] = 0.0;
    auClock[i] = 0.0;
  }

  gametime = 0.0;  // Start the clock
  bGameOver = false;
  memset(AnnouncerText, 0, maxAnnouncerTextLen);  // Initialize announcer buffer

  for (i = 0; i < MAX_THINGS; ++i) {
    apThings[i] = NULL;
    apTAddQueue[i] = NULL;
    aUNextInd[i] = (unsigned int)-1;
    aUPrevInd[i] = (unsigned int)-1;
  }

  UFirstIndex = (unsigned int)-1;
  ULastIndex = (unsigned int)-1;
  numNewThings = 0;
}

CWorld::~CWorld() {
  CThing* pTTmp;
  unsigned int i;

  for (i = 0; i < MAX_THINGS; ++i) {
    pTTmp = GetThing(i);
    if (pTTmp == NULL) {
      continue;
    }
    if (pTTmp->GetKind() == ASTEROID) {
      delete pTTmp;
    }
  }

  delete[] apTeams;
  delete[] atstamp;
  delete[] auClock;
}

CWorld* CWorld::CreateCopy() {
  CWorld* pWld;
  pWld = new CWorld(numTeams);

  unsigned int acsz, sz = GetSerialSize();
  char* buf = new char[sz];

  acsz = SerialPack(buf, sz);
  if (acsz != sz) {
    printf("ERROR: World assignment\n");
    return NULL;
  }

  pWld->SerialUnpack(buf, acsz);
  delete[] buf;
  return pWld;
}

//////////////////////////////////////////////////
// Data access to internal members

CTeam* CWorld::GetTeam(unsigned int nt) const {
  if (nt >= GetNumTeams()) {
    return NULL;
  }
  return apTeams[nt];
}

unsigned int CWorld::GetNumTeams() const { return numTeams; }

double CWorld::GetGameTime() const { return gametime; }

void CWorld::AddAnnouncerMessage(const char* message) {
  if (message == NULL) return;

  size_t currentLen = strlen(AnnouncerText);
  size_t messageLen = strlen(message);

  // Ensure we have space for the message plus a newline and null terminator
  if (currentLen + messageLen + 2 < maxAnnouncerTextLen) {
    if (currentLen > 0) {
      strcat(AnnouncerText, "\n");  // Add newline between messages
    }
    strcat(AnnouncerText, message);
  }
}

CThing* CWorld::GetThing(unsigned int index) const {
  if (index >= MAX_THINGS) {
    return NULL;
  }
  return apThings[index];
}

unsigned int CWorld::GetNextIndex(unsigned int curindex) const {
  if (curindex >= MAX_THINGS) {
    return (unsigned int)-1;
  }
  return aUNextInd[curindex];
}

unsigned int CWorld::GetPrevIndex(unsigned int curindex) const {
  if (curindex >= MAX_THINGS) {
    return (unsigned int)-1;
  }
  return aUPrevInd[curindex];
}

/////////////////////////////////////////////////////
// Explicit functions

unsigned int CWorld::PhysicsModel(double dt) {
  CThing* pThing;
  unsigned int i;

  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pThing = GetThing(i);
    pThing->Drift(dt);
  }

  CollisionEvaluation();
  AddNewThings();    // It's possible that new things are already dead
  KillDeadThings();  // So this comes after AddNewThings

  gametime += dt;
  return 0;
}

void CWorld::LaserModel() {
  // TODO: SECURITY VULNERABILITY - Time-of-check to time-of-use (TOCTOU) bug
  // This function uses GetOrder(O_LASER) to determine laser power/damage BEFORE
  // calling SetOrder(O_LASER) to validate and cap the value. A malicious client
  // can bypass SetOrder() validation by directly manipulating the adOrders array
  // (e.g., via KobayashiMaru exploit using C-style casts to access protected members).
  // The exploit allows firing a massive laser while only paying fuel for the capped value.
  //
  // Attack scenario:
  //   1. Client sets adOrders[O_LASER] = 999999 (bypassing SetOrder validation)
  //   2. Server reads raw value via GetOrder() and fires 999999-unit laser
  //   3. Server calls SetOrder() which caps to fuel available (~500 for 10 fuel)
  //   4. Client gets 20x damage for same fuel cost
  //
  // Fix would be: Call SetOrder() FIRST to validate/cap, THEN use GetOrder() for damage.
  // However, this is preserved as a historical 1998-era vulnerability for educational purposes.

  // TODO: Accuracy of fuel consumption. In our game loop effectively this
  // happens:
  // 1. Shileds order is processed.
  // 2. The first physics step is done.
  // 3. The laser order is processed - using GetOrder to determine magnitude. In
  //    addition to the exploit above, this can also cause us to spend more fuel
  //    than we have - the fix to both is to make LaserModel respect the
  //    SetOrder guardrails.

  unsigned int nteam, nship;
  CTeam* pTeam;
  CShip* pShip;
  CThing *pTarget, LasThing;
  CCoord LasPos, TmpPos;
  CTraj LasTraj, TarVel, TmpTraj;
  double dfuel, dLasPwr, dLasRng;

  /*
   * Laser delivery model:
   * ---------------------
   * Lasers are not persistent world objects. Instead, each turn we
   * synthesize a temporary CThing (LasThing) with kind GENTHING and
   * deliver it directly to the chosen target via Collide(). The
   * synthesized "laser thing" is placed one world-unit in front of the
   * target, along the beam direction, and its mass encodes the
   * remaining beam power at that point:
   *
   *   LasThing.mass = 30 * (L - D)
   *
   * where L is the requested beam length (clamped earlier), and D is
   * the distance from the shooter to a point one unit short of the
   * target along the beam line. By positioning just before the target
   * and using (L - D), the target's HandleCollision() sees the
   * remaining beam power when the beam reaches it, not the initial
   * requested length. Targets can then use the laser "mass" to decide
   * effects (e.g., asteroids break if mass >= 1000.0).
   */

  for (nteam = 0; nteam < GetNumTeams(); ++nteam) {
    pTeam = GetTeam(nteam);
    if (pTeam == NULL) {
      continue;
    }
    for (nship = 0; nship < pTeam->GetShipCount(); ++nship) {
      pShip = pTeam->GetShip(nship);
      if (pShip == NULL) {
        continue;
      }
      // TODO: VULNERABILITY - Reading raw client data before validation
      // This GetOrder() returns the unvalidated adOrders[O_LASER] value that
      // the client sent. A malicious client can set this to any value by
      // directly manipulating the array, bypassing SetOrder() checks.
      dLasPwr = pShip->GetOrder(O_LASER);
      if (dLasPwr <= 0.0) {
        continue;
      }

      // Compute the nominal end-of-beam position from shooter
      LasPos = pShip->GetPos();
      LasTraj = CTraj(dLasPwr, pShip->GetOrient());
      LasPos += LasTraj.ConvertToCoord();
      LasThing.SetPos(LasPos);

      pTarget = pShip->LaserTarget();
      dLasRng = LasPos.DistTo(pShip->GetPos());
      if (dLasRng > dLasPwr) {
        pTarget = NULL;
      }
      if (pTarget != NULL) {
        TmpPos = pTarget->GetPos();
        // Move impact point to one unit in front of the target along the
        // beam. This ensures we measure remaining length (L - D) at impact.
        TmpTraj = pShip->GetPos().VectTo(TmpPos);
        TmpTraj.rho = 1.0;
        TmpPos -= (CCoord)TmpTraj;
        LasThing.SetPos(TmpPos);

        // Remaining beam power at impact: mass =
        // g_laser_mass_scale_per_remaining_unit * (L - D)
        dLasRng = TmpPos.DistTo(pShip->GetPos());
        LasThing.SetMass(g_laser_mass_scale_per_remaining_unit *
                         (dLasPwr - dLasRng));
        // Give the laser thing a small velocity bias based on target motion
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

        printf("[LASER HIT] %s %s (%s) shot %s %s", pShip->GetTeam()->GetName(),
               pShip->GetName(), pShip->GetTeam()->GetName(), targetType,
               targetName ? targetName : "");

        // Add team info for ships/stations
        if (pTarget->GetKind() == SHIP && pTarget->GetTeam()) {
          printf(" (%s)", pTarget->GetTeam()->GetName());
        } else if (pTarget->GetKind() == STATION && pTarget->GetTeam()) {
          printf(" (%s)", pTarget->GetTeam()->GetName());
        }
        printf("\n");

        // Deliver the synthesized laser impact to the target
        pTarget->Collide(&LasThing, this);
      }

      double oldFuel = pShip->GetAmount(S_FUEL);
      dfuel = oldFuel;
      // TODO: VULNERABILITY - Validation happens AFTER laser was already fired
      // SetOrder() validates and caps the laser power based on fuel available,
      // but the laser beam was already computed and fired using the raw dLasPwr
      // value above. Client only pays for the validated amount, not what they used.
      // This should be called BEFORE using dLasPwr for damage calculations.
      dfuel -= pShip->SetOrder(O_LASER, dLasPwr);
      pShip->SetAmount(S_FUEL, dfuel);

      // Check if out of fuel
      if (oldFuel > 0.01 && dfuel <= 0.01) {
        printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", pShip->GetName(),
               pShip->GetTeam() ? pShip->GetTeam()->GetName() : "Unknown");
      }
    }
  }

  AddNewThings();
  KillDeadThings();
}

void CWorld::AddThingToWorld(CThing* pNewThing) {
  if (pNewThing == NULL || numNewThings >= MAX_THINGS) {
    return;
  }
  apTAddQueue[numNewThings] = pNewThing;
  numNewThings++;
}

void CWorld::CreateAsteroids(AsteroidKind mat, unsigned int numast, double mass) {
  CAsteroid* pAst;
  unsigned int i;

  for (i = 0; i < numast; ++i) {
    pAst = new CAsteroid(mass, mat);
    AddThingToWorld(pAst);
  }
}

CTeam* CWorld::SetTeam(unsigned int n, CTeam* pTm) {
  if (n >= GetNumTeams()) {
    return NULL;
  }

  CTeam* oldteam = apTeams[n];
  CTeam* tmown;
  CThing* delth;
  ThingKind delkind;
  unsigned int i, numsh;

  if (oldteam == pTm) {
    return oldteam;
  }
  if (oldteam != NULL) {
    for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
      delth = GetThing(i);
      delkind = delth->GetKind();
      if (delkind == SHIP || delkind == STATION) {
        if (delkind == SHIP) {
          tmown = ((CShip*)delth)->GetTeam();
        } else {
          tmown = ((CStation*)delth)->GetTeam();
        }

        if (tmown == oldteam) {
          RemoveIndex(i);
        }
      }
    }
  }

  apTeams[n] = pTm;
  pTm->SetWorldIndex(n);
  pTm->SetWorld(this);
  AddThingToWorld(pTm->GetStation());
  for (numsh = 0; numsh < pTm->GetShipCount(); ++numsh) {
    AddThingToWorld(pTm->GetShip(numsh));
  }

  return oldteam;
}

//////////////////////////////////////////////
// Assistant Methods

void CWorld::RemoveIndex(unsigned int index) {
  if (index >= MAX_THINGS) {
    return;
  }

  unsigned int Prev, Next;
  Prev = aUPrevInd[index];
  Next = aUNextInd[index];

  if (Prev < MAX_THINGS) {
    aUNextInd[Prev] = Next;  // Work him out of the sequence
  }

  if (Next < MAX_THINGS) {
    aUPrevInd[Next] = Prev;
  }

  aUPrevInd[index] = (unsigned int)-1;  // Reset his indices
  aUNextInd[index] = (unsigned int)-1;

  apThings[index] = NULL;  // And kiss 'im goodbye

  if (index == UFirstIndex) {
    UFirstIndex = Next;
  }
  if (index == ULastIndex) {
    ULastIndex = Prev;
  }
}

unsigned int CWorld::CollisionEvaluation() {
  CThing *pTItr, *pTTm;
  unsigned int i, j, iteam, iship, numtmth, URes = 0;
  CTeam* pTeam;
  static CThing* apTTmTh[MAX_THINGS];  // List of team-controlled (i.e.
                                       // non-asteroid) objects static saves on
                                       // reallocation time btwn calls
  numtmth = 0;
  for (iteam = 0; iteam < GetNumTeams(); ++iteam) {
    pTeam = GetTeam(iteam);
    if (pTeam == NULL) {
      continue;
    }
    pTTm = pTeam->GetStation();  // Put station into list
    apTTmTh[numtmth] = pTTm;
    numtmth++;

    if (bGameOver == true) {
      continue;  // Ships invisible after game ends
    }

    for (iship = 0; iship < pTeam->GetShipCount(); ++iship) {
      pTTm = pTeam->GetShip(iship);
      if (pTTm == NULL) {
        continue;
      }
      apTTmTh[numtmth] = pTTm;
      numtmth++;
    }
  }

  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pTItr = GetThing(i);
    if ((pTItr->IsAlive()) == false) {
      continue;
    }
    if (pTItr == NULL) {
      continue;
    }

    for (j = 0; j < numtmth; ++j) {
      pTTm = apTTmTh[j];
      if (pTTm == NULL) {
        continue;
      }

      pTItr->Collide(pTTm, this);  // Asteroid(?) shattered by ship
      if (pTTm->Collide(pTItr, this) ==
          true) {  // Ship deflected by asteroid(?)
        URes++;
      }
    }
  }

  return URes;
}

unsigned int CWorld::AddNewThings() {
  unsigned int URes, UInd;

  if (numNewThings == 0) {
    return 0;  // Duh.
  }

  for (URes = 0; URes < numNewThings; ++URes) {
    UInd = ULastIndex + 1;
    if (ULastIndex == (unsigned int)-1) {
      UInd = 0;  // Might as well make it explicit
    }
    if (URes >= MAX_THINGS) {
      break;  // Can't hold anymore!!
    }

    apThings[UInd] = apTAddQueue[URes];
    apThings[UInd]->SetWorld(this);
    apThings[UInd]->SetWorldIndex(UInd);

    aUPrevInd[UInd] = ULastIndex;

    if (ULastIndex == (unsigned int)-1) {
      UFirstIndex = UInd;
    } else {
      aUNextInd[ULastIndex] = UInd;
    }

    ULastIndex = UInd;
  }

  numNewThings = 0;
  return URes;
}

unsigned int CWorld::KillDeadThings() {
  CThing* pTTry;
  unsigned int URes = 0, index, ShNum;
  CTeam* pTm;
  ThingKind KTry;

  for (index = UFirstIndex; index != (unsigned int)-1; index = GetNextIndex(index)) {
    pTTry = GetThing(index);

    if ((pTTry->IsAlive()) != true) {
      RemoveIndex(index);
      URes++;

      KTry = pTTry->GetKind();
      if (KTry == SHIP) {
        pTm = ((CShip*)pTTry)->GetTeam();
        if (pTm != NULL) {
          ShNum = ((CShip*)pTTry)->GetShipNumber();
          pTm->SetShip(ShNum, NULL);
        }
      }

      delete pTTry;
      continue;
    }
  }

  return URes;
}

void CWorld::ReLinkList() {
  unsigned int i, ilast = (unsigned int)-1;
  CThing* pTh;

  for (i = 0; i < MAX_THINGS; ++i) {
    pTh = apThings[i];
    if (pTh == NULL) {
      continue;
    }

    aUPrevInd[i] = ilast;
    if (ilast != (unsigned int)-1) {
      aUNextInd[ilast] = i;
    } else {
      UFirstIndex = i;
    }

    ilast = i;
  }

  ULastIndex = ilast;
}

double CWorld::GetTimeStamp() {
  struct timeval tp;
  gettimeofday(&tp, NULL);

  double res = (double)(tp.tv_sec);         // Seconds
  res += (double)(tp.tv_usec) / 1000000.0;  // microseconds
  return res;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CWorld::GetSerialSize() const {
  unsigned int totsize = 0;
  CThing* pTh;

  totsize += BufWrite(NULL, UFirstIndex);
  totsize += BufWrite(NULL, ULastIndex);
  totsize += BufWrite(NULL, gametime);
  totsize += BufWrite(NULL, AnnouncerText, maxAnnouncerTextLen);

  unsigned int i, inext, sz, iTm, crc = 666, uTK = 0;

  for (i = 0; i < numTeams; ++i) {
    totsize += BufWrite(NULL, auClock[i]);
    totsize += GetTeam(i)->GetSerialSize();
  }

  unsigned int tk = 0;
  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pTh = GetThing(i);
    tk++;
    sz = pTh->GetSerialSize();
    inext = GetNextIndex(i);

    iTm = 0;
    totsize += BufWrite(NULL, crc);
    totsize += BufWrite(NULL, inext);
    totsize += BufWrite(NULL, sz);

    totsize += BufWrite(NULL, uTK);
    totsize += BufWrite(NULL, iTm);

    totsize += pTh->GetSerialSize();
  }

  return totsize;
}

unsigned CWorld::SerialPack(char* buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;
  CThing* pTh;

  vpb += BufWrite(vpb, UFirstIndex);
  vpb += BufWrite(vpb, ULastIndex);
  vpb += BufWrite(vpb, gametime);
  vpb += BufWrite(vpb, AnnouncerText, maxAnnouncerTextLen);

  unsigned int i, inext, sz, iTm, crc = 666, uTK;
  ThingKind TKind;
  CTeam* ptTeam;

  for (i = 0; i < numTeams; ++i) {
    vpb += BufWrite(vpb, auClock[i]);
    vpb += GetTeam(i)->SerialPack(vpb, buflen - (vpb - buf));
  }

  unsigned int tk = 0;
  for (i = UFirstIndex; i != (unsigned int)-1; i = GetNextIndex(i)) {
    pTh = GetThing(i);
    tk++;
    sz = pTh->GetSerialSize();
    TKind = pTh->GetKind();
    inext = GetNextIndex(i);

    iTm = 0;
    if ((ptTeam = pTh->GetTeam()) != NULL) {
      iTm = ptTeam->GetWorldIndex();
    }
    if (TKind == SHIP) {
      iTm |= (((CShip*)pTh)->GetShipNumber()) << 8;
    }
    if (TKind == ASTEROID) {
      iTm = (unsigned int)((CAsteroid*)pTh)->GetMaterial();
    }

    vpb += BufWrite(vpb, crc);
    vpb += BufWrite(vpb, inext);
    vpb += BufWrite(vpb, sz);

    uTK = (unsigned int)TKind;
    vpb += BufWrite(vpb, uTK);
    vpb += BufWrite(vpb, iTm);

    vpb += pTh->SerialPack((char*)vpb, sz);
  }

  return (vpb - buf);
}

unsigned CWorld::SerialUnpack(char* buf, unsigned buflen) {
  char* vpb = buf;
  CThing* pTh;
  CAsteroid ATmp;

  unsigned int i, inext, ilast, crc, uTK;
  unsigned int sz, acsz, iTm;
  ThingKind TKind;
  unsigned int tk = 0;

  vpb += BufRead(vpb, inext);
  vpb += BufRead(vpb, ilast);
  vpb += BufRead(vpb, gametime);
  vpb += BufRead(vpb, AnnouncerText, maxAnnouncerTextLen);

  for (i = 0; i < numTeams; ++i) {
    vpb += BufRead(vpb, auClock[i]);
    vpb += GetTeam(i)->SerialUnpack(vpb, buflen - (vpb - buf));
  }

  for (i = UFirstIndex; i <= ilast; ++i) {
    pTh = GetThing(i);
    if (pTh != NULL && i < inext) {
      pTh->KillThing();
    }

    if (i == inext) {
      tk++;
      vpb += BufRead(vpb, crc);
      if (crc != 666) {
        printf("Off-track!!, %d\n", crc);
      }

      vpb += BufRead(vpb, inext);
      vpb += BufRead(vpb, sz);
      vpb += BufRead(vpb, uTK);
      TKind = (ThingKind)uTK;

      vpb += BufRead(vpb, iTm);

      if (pTh == NULL) {
        pTh = CreateNewThing(TKind, iTm);
        apThings[i] = pTh;
      }
      acsz = pTh->SerialUnpack((char*)vpb, sz);
      if (acsz != sz) {
        printf("Serialization discrepancy, %d!=%d\n", acsz, sz);
      }

      pTh->SetWorld(this);
      pTh->SetWorldIndex(i);

      vpb += acsz;
      if (vpb >= buf + buflen) {
        break;  // stooooooppppppp!!
      }
      if (inext == (unsigned int)-1) {
        break;
      }
    }
  }

  if (ilast < ULastIndex) {  // Stuff died at the end of the list
    for (i = ilast + 1; i <= ULastIndex; ++i) {
      pTh = GetThing(i);
      if (pTh != NULL) {
        pTh->KillThing();
      }
    }
  }

  KillDeadThings();
  ReLinkList();

  return (vpb - buf);
}

CThing* CWorld::CreateNewThing(ThingKind TKind, unsigned int iTm) {
  CThing *pTh, *pThOld;
  CTeam* pTeam;
  unsigned int shnum = 0;
  pThOld = NULL;

  shnum = iTm >> 8;
  iTm = iTm & 0xff;
  pTeam = GetTeam(iTm);

  switch (TKind) {
    case STATION:
      pTh = new CStation(CCoord(0.0, 0.0));
      if (pTeam != NULL) {
        pThOld = pTeam->SetStation((CStation*)pTh);
      }
      break;

    case SHIP:
      pTh = new CShip(CCoord(0.0, 0.0));
      if (pTeam != NULL) {
        pThOld = pTeam->SetShip(shnum, (CShip*)pTh);
      }
      break;

    case ASTEROID:
      pTh = new CAsteroid();
      break;

    default:
      pTh = new CThing;
  }

  if (pThOld != NULL) {
    delete pThOld;
  }
  return pTh;
}
