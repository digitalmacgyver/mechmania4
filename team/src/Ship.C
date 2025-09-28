/* Ship.C
 * Ahh, the Motherload!!
 * Implementation of CShip class
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include "Brain.h"
#include "ParserModern.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "World.h"

extern CParser* g_pParser;


///////////////////////////////////////////
// Construction/Destruction

CShip::CShip(CCoord StPos, CTeam *pteam, UINT ShNum)
    : CThing(StPos.fX, StPos.fY) {
  TKind = SHIP;
  pmyTeam = pteam;
  myNum = ShNum;

  size = 12.0;
  mass = 40.0;
  orient = 0.0;
  uImgSet = 0;
  pBrain = NULL;

  bDockFlag = true;
  dDockDist = 30.0;
  dLaserDist = 0.0;
  omega = 0.0;

  for (UINT sh = (UINT)S_CARGO; sh < (UINT)S_ALL_STATS; sh++) {
    adStatMax[sh] = 30.0;
    adStatCur[sh] = 30.0;
    if (sh == (UINT)S_CARGO) {
      adStatCur[sh] = 0.0;
    }
  }

  adStatMax[(UINT)S_SHIELD] = 8000.0;  // Arbitrarily large value
  ResetOrders();
}

CShip::~CShip() {}

////////////////////////////////////////////
// Data access

UINT CShip::GetShipNumber() const { return myNum; }

bool CShip::IsDocked() const { return bDockFlag; }

double CShip::GetAmount(ShipStat st) const {
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  return adStatCur[(UINT)st];
}

double CShip::GetCapacity(ShipStat st) const {
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  return adStatMax[(UINT)st];
}

double CShip::GetOrder(OrderKind ord) const {
  if (ord >= O_ALL_ORDERS) {
    return 0.0;
  }
  return adOrders[(UINT)ord];
}

double CShip::GetMass() const {
  double sum = mass;
  sum += GetAmount(S_CARGO);
  sum += GetAmount(S_FUEL);
  return sum;
}

double CShip::GetLaserBeamDistance() { return dLaserDist; }

CBrain *CShip::GetBrain() { return pBrain; }

////////
// Incoming

double CShip::SetAmount(ShipStat st, double val) {
  if (val < 0.0) {
    val = 0.0;
  }
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  if (val >= GetCapacity(st)) {
    val = GetCapacity(st);
  }
  adStatCur[(UINT)st] = val;
  return GetAmount(st);
}

double CShip::SetCapacity(ShipStat st, double val) {
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  if (val < 0.0) {
    val = 0.0;
  }
  if (val > dMaxStatTot) {
    val = dMaxStatTot;
  }

  adStatMax[(UINT)st] = val;

  double tot = 0.0;
  tot += adStatMax[S_CARGO];
  tot += adStatMax[S_FUEL];

  if (tot > dMaxStatTot) {
    tot -= dMaxStatTot;
    if (st == S_CARGO) {
      adStatMax[(UINT)S_FUEL] -= tot;
    }
    if (st == S_FUEL) {
      adStatMax[(UINT)S_CARGO] -= tot;
    }
  }

  if (GetAmount(st) > GetCapacity(st)) {
    adStatCur[(UINT)st] = GetCapacity(st);
  }
  return GetCapacity(st);
}

CBrain *CShip::SetBrain(CBrain *pBr) {
  // Context switching: Replace current tactical behavior with new one
  // This enables dynamic AI behavior changes based on strategic context
  CBrain *pBrTmp = GetBrain();
  pBrain = pBr;
  if (pBrain != NULL) {
    pBrain->pShip = this;  // Link brain to this ship
  }
  return pBrTmp;  // Return previous brain for cleanup/restoration
}

////////////////////////////////////////////
// Ship control

void CShip::ResetOrders() {
  dLaserDist = 0.0;
  for (UINT ord = (UINT)O_SHIELD; ord < (UINT)O_ALL_ORDERS; ord++) {
    adOrders[ord] = 0.0;
  }
}

// SetOrder method used for computing fuel consumed for an order
double CShip::SetOrder(OrderKind ord, double value) {
  // NOTE: Use SetJettison() and GetJettison() helper functions instead of
  // calling SetOrder(O_JETTISON, ...) directly for better type safety and
  // readability
  double valtmp, fuelcon, maxfuel;
  CTraj AccVec;
  UINT oit;

  maxfuel = GetAmount(S_FUEL);
  if (IsDocked() == true) {
    maxfuel = GetCapacity(S_FUEL);
  }

  switch (ord) {
    case O_SHIELD:  // "value" is amt by which to boost shields
      if (value < 0.0) {
        value = 0.0;  // Can't lower shields
      }
      valtmp = value + GetAmount(S_SHIELD);
      if (valtmp > GetCapacity(S_SHIELD)) {
        value = GetCapacity(S_SHIELD) - GetAmount(S_SHIELD);
      }

      fuelcon = value;
      if (fuelcon > GetAmount(S_FUEL)) {  // Check for sufficient fuel
        fuelcon = GetAmount(S_FUEL);
        value = fuelcon;  // No, but here's how much we *can* do
      }

      adOrders[(UINT)O_SHIELD] = value;
      return fuelcon;  // Doesn't need a break since this returns

    case O_LASER:  // "value" is specified length of laser beam
      if (value < 0.0) {
        value = 0.0;
      }
      if (IsDocked()) {  // Can't shoot while docked
        value = 0.0;
        return 0.0;
      }
      if (value > (fWXMax - fWXMin) / 2.0) {
        value = (fWXMax - fWXMin) / 2.0;
      }
      if (value > (fWYMax - fWYMin) / 2.0) {
        value = (fWYMax - fWYMin) / 2.0;
      }

      fuelcon = value / 50.0;
      if (fuelcon > GetAmount(S_FUEL)) {  // Check for sufficient fuel
        fuelcon = GetAmount(S_FUEL);
        value = fuelcon * 50.0;  // No, but here's how much we *can* do
      }

      adOrders[(UINT)O_LASER] = value;
      return fuelcon;

    case O_THRUST:  // "value" is magnitude of acceleration vector
      // Initialize strategy on first use if needed
      if (!velocityStrategy) {
        InitializeVelocityStrategy();
      }
      return velocityStrategy->processThrustOrder(this, ord, value);

    case O_TURN:  // "value" is angle, in radians, to turn
      if (value == 0.0) {
        return 0.0;
      }
      adOrders[(UINT)O_THRUST] = 0.0;
      adOrders[(UINT)O_JETTISON] = 0.0;

      // 1 ton of fuel rotates naked ship full-circle six times
      fuelcon = fabs(value) * GetMass() / (6.0 * PI2 * mass);
      if (IsDocked() == true) {
        fuelcon = 0.0;
      }
      if (fuelcon > maxfuel) {
        fuelcon = maxfuel;
        valtmp = (mass * 6.0 * PI2 * fuelcon) / GetMass();
        if (value <= 0.0) {
          value = -valtmp;
        } else {
          value = valtmp;
        }
      }

      adOrders[(UINT)O_TURN] = value;
      return fuelcon;

    case O_JETTISON: {  // "value" is tonnage: positive for fuel, neg for cargo
      // NOTE: Use SetJettison() and GetJettison() helper functions instead of
      // calling SetOrder(O_JETTISON, ...) directly for better type safety
      double requestedAmount = fabs(value);

      // 1. Minimum mass threshold check
      if (requestedAmount < minmass) {
        adOrders[(UINT)O_JETTISON] = 0.0;
        return 0.0;
      }

      // 2. Cancel conflicting orders
      adOrders[(UINT)O_THRUST] = 0.0;
      adOrders[(UINT)O_TURN] = 0.0;

      // 3. Determine material and inventory stat
      ShipStat inventoryStat;
      bool isFuel = (value > 0.0);

      if (isFuel) {
        inventoryStat = S_FUEL;  // Uranium
      } else {
        inventoryStat = S_CARGO;  // Vinyl
      }

      // 4. Check available inventory and clamp the amount
      double availableAmount = GetAmount(inventoryStat);
      double actualAmount = requestedAmount;
      if (requestedAmount > availableAmount) {
        actualAmount = availableAmount;
      }

      // 5. Update the order, restoring the sign
      if (isFuel) {
        adOrders[(UINT)O_JETTISON] = actualAmount;
        return actualAmount;
      } else {
        adOrders[(UINT)O_JETTISON] = -actualAmount;
        return 0.0;
      }
    }

      /*
      // LEGACY CODE: This is the legacy implementation of O_JETTISON, which is
      // replaced above. In addition to being confusing due to using the sign to
      // determine the type of material jettisoned (- for vinyl, + for uranium),
      // there is a bug where it does not clamp vinyl jettison to the amount of
      // vinyl we have. E.g. if we have a command to jettison 10 vinyl and have
      // only 5 then:
      //  * value will be -10
      //  * maxfuel=GetAmount((ShipStat)oit); will be 5
      //  * if (maxfuel<value) is then if (5<-10) which is false, so we don't
      //    clamp the order to -5, and go ahead and add an O_JETTISON of -10.

      if (fabs(value)<minmass) {
          value=0.0;
          adOrders[(UINT)O_JETTISON]=0.0;
          return 0.0;  // Jettisoning costs no fuel
      }

      adOrders[(UINT)O_THRUST] = 0.0;
      adOrders[(UINT)O_TURN] = 0.0;

      AsMat=URANIUM;
      if (value<=0.0) AsMat=VINYL;
      oit = (UINT)AstToStat(AsMat);

      maxfuel=GetAmount((ShipStat)oit);  // Not necessarily fuel
      if (maxfuel<value) value=maxfuel;
      adOrders[(UINT)O_JETTISON] = value;

      if (AsMat==URANIUM) return value;  // We're spitting out this much fuel
      else return 0.0;       // Jettisoning itself takes no fuel
      */

      // LEGACY: Combined fuel calculation for O_SHIELD, O_LASER, and O_THRUST
      // orders (never used) This was intended as a "how much fuel will all my
      // orders take" convenience function but was never implemented or used
      // anywhere in the codebase.
      //
      // Issues with current implementation:
      // - Recursive SetOrder calls may have unintended side effects
      // - No clear interface or documentation for O_ALL_ORDERS usage
      // - Fuel calculation logic is unclear and potentially incorrect
      //
      // To make this useful, it would need:
      // - Clear documentation of intended behavior
      // - Non-recursive fuel calculation logic
      // - Proper handling of fuel conflicts between orders
      // - Better interface design for fuel budgeting
      /*
      default:
        valtmp=0.0;
        for (oit=(UINT)O_SHIELD; oit<=(UINT)O_THRUST; oit++)
           valtmp += SetOrder((OrderKind)oit,GetOrder((OrderKind)oit));
        return valtmp;
      */
  }

  return 0.0;  // Should never reach here, but prevents compiler warning
}

void CShip::SetJettison(AsteroidKind Mat, double amt) {
  switch (Mat) {
    case URANIUM:
      SetOrder(O_JETTISON, amt);
      return;
    case VINYL:
      SetOrder(O_JETTISON, -amt);
      return;
    default:
      SetOrder(O_JETTISON, 0.0);
      return;
  }
}

double CShip::GetJettison(AsteroidKind Mat) {
  double amt = GetOrder(O_JETTISON);
  if (amt > 0.0 && Mat == URANIUM) {
    return amt;
  }
  if (amt < 0.0 && Mat == VINYL) {
    return -amt;
  }
  return 0.0;
}

////////////////////////////////////////////
// Inherited methods

void CShip::Drift(double dt) {
  if (GetTeam()->GetWorld()->bGameOver == true) {
    CThing::Drift(0.0);  // Ships don't move when game is over
    return;
  }

  bIsColliding = NO_DAMAGE;
  bIsGettingShot = NO_DAMAGE;

  // Check for velocity clamping before applying it
  if (Vel.rho > maxspeed) {
    double originalSpeed = Vel.rho;
    Vel.rho = maxspeed;

    // Announce when velocity gets clamped (if enabled)
    if (g_pParser && g_pParser->UseNewFeature("announcer-velocity-clamping")) {
      CWorld* pWorld = GetTeam()->GetWorld();
      if (pWorld) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s velocity clamped %.1f -> %.1f",
                 GetName(), originalSpeed, maxspeed);
        pWorld->AddAnnouncerMessage(msg);
      }
    }
  }
  // From CThing::Drift

  double thrustamt = GetOrder(O_THRUST);
  double turnamt = GetOrder(O_TURN);
  double shieldamt = GetOrder(O_SHIELD);
  double fuelcons;

  uImgSet = 0;  // Assume it's just drifting for now

  // Jettisonning, then movement stuff
  HandleJettison();

  // First handle shields
  if (shieldamt > 0.0) {
    fuelcons = SetOrder(O_SHIELD, shieldamt);
    double oldFuel = GetAmount(S_FUEL);
    double newFuel = oldFuel - fuelcons;
    SetAmount(S_FUEL, newFuel);
    SetAmount(S_SHIELD, GetAmount(S_SHIELD) + shieldamt);
    SetOrder(O_SHIELD, 0.0);  // Shield set, ignore it now

    // Check if out of fuel
    if (oldFuel > 0.01 && newFuel <= 0.01) {
      printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown");
    }
  }

  // Now handle turning
  omega = 0.0;
  if (turnamt != 0.0) {
    fuelcons = SetOrder(O_TURN, turnamt);
    double oldFuel = GetAmount(S_FUEL);
    double newFuel = oldFuel - fuelcons * dt;
    SetAmount(S_FUEL, newFuel);
    omega = turnamt;

    // Check if out of fuel
    if (oldFuel > 0.01 && newFuel <= 0.01) {
      printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown");
    }

    if (turnamt < 0.0) {
      uImgSet = 3;
    } else {
      uImgSet = 4;
    }
  }

  // Thrusting time
  if (thrustamt != 0.0) {
    // Initialize strategy on first use if needed
    if (!velocityStrategy) {
      InitializeVelocityStrategy();
    }
    velocityStrategy->processThrustDrift(this, thrustamt, dt);
  }

  // Also from CThing::Drift
  Pos += (Vel * dt).ConvertToCoord();
  orient += omega * dt;
  if (orient < -PI || orient > PI) {
    CTraj VTmp(1.0, orient);
    VTmp.Normalize();
    orient = VTmp.theta;
  }

  omega = 0.0;       // Just for good measure
  dLaserDist = 0.0;  // Don't want lasers left on
}

bool CShip::AsteroidFits(const CAsteroid *pAst) {
  double othmass = pAst->GetMass();
  switch (pAst->GetMaterial()) {
    case VINYL:
      if ((othmass + GetAmount(S_CARGO)) > GetCapacity(S_CARGO)) {
        return false;
      }
      return true;
    case URANIUM:
      if ((othmass + GetAmount(S_FUEL)) > GetCapacity(S_FUEL)) {
        return false;
      }
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////
// Battle assistants

CThing *CShip::LaserTarget() {
  if (pmyTeam == NULL) {
    return NULL;
  }

  CWorld *pWorld = pmyTeam->GetWorld();
  if (pWorld == NULL) {
    return NULL;
  }

  CThing *pTCur, *pTRes = NULL;
  double dist, mindist = -1.0;  // Start with something invalid
  UINT i;

  dLaserDist = 0.0;

  for (i = pWorld->UFirstIndex; i != (UINT)-1; i = pWorld->GetNextIndex(i)) {
    pTCur = pWorld->GetThing(i);
    if (IsFacing(*pTCur) == false) {
      continue;  // Nevermind, we're not facing it
    }

    dist = GetPos().DistTo(pTCur->GetPos());
    if (dist < mindist || mindist == -1.0) {
      mindist = dist;
      pTRes = pTCur;
    }
  }

  dLaserDist = mindist;
  double dlaspwr = GetOrder(O_LASER);
  if (mindist > dlaspwr) {
    dLaserDist = dlaspwr;
  }
  return pTRes;
}

double CShip::AngleToIntercept(const CThing &OthThing, double dtime) {
  // COORDINATE SYSTEM NOTE: In our display, +Y points downward on screen
  // Angle orientation: 0 = right (+X), PI/2 = down (+Y), PI = left (-X), -PI/2 = up (-Y)
  CCoord myPos, hisPos;
  myPos = PredictPosition(dtime);
  hisPos = OthThing.PredictPosition(dtime);

  double ang = myPos.AngleTo(hisPos), face = GetOrient(), turn = ang - face;

  if (turn < -PI || turn > PI) {
    CTraj VTmp(1.0, turn);
    VTmp.Normalize();
    turn = VTmp.theta;
  }

  return turn;
}

ShipStat CShip::AstToStat(AsteroidKind AsMat) const {
  switch (AsMat) {
    case URANIUM:
      return S_FUEL;
    case VINYL:
      return S_CARGO;
    default:
      return S_ALL_STATS;
  }
}

AsteroidKind CShip::StatToAst(ShipStat ShStat) const {
  switch (ShStat) {
    case S_FUEL:
      return URANIUM;
    case S_CARGO:
      return VINYL;
    default:
      return GENAST;
  }
}

////////////////////////////////////////////
// Protected methods

void CShip::HandleCollision(CThing *pOthThing, CWorld *pWorld) {
  if (*pOthThing == *this ||  // Can't collide with yourself!
      IsDocked() == true) {   // Nothing can hurt you at a station
    bIsColliding = NO_DAMAGE;
    return;
  }
  if (pWorld == NULL) {
    ;  // Okay for world to be NULL, this suppresses warning
  }

  ThingKind OthKind = pOthThing->GetKind();

  if (OthKind == STATION) {
    dDockDist = Pos.DistTo(pOthThing->GetPos());
    bIsColliding = NO_DAMAGE;

    Pos = pOthThing->GetPos();
    Vel = CTraj(0.0, 0.0);
    SetOrder(O_THRUST, 0.0);

    // Log vinyl delivery
    double vinylDelivered = GetAmount(S_CARGO);
    if (vinylDelivered > 0.01) {
      CStation *pStation = (CStation *)pOthThing;
      if (pStation->GetTeam() == this->GetTeam()) {
        printf("[DELIVERY] Ship %s delivered %.2f vinyl to HOME base (%s)\n",
               GetName(), vinylDelivered, GetTeam()->GetName());
        if (pWorld) {
          char msg[256];
          snprintf(msg, sizeof(msg), "%s delivered %.1f vinyl to %s",
                   GetName(), vinylDelivered, pStation->GetName());
          pWorld->AddAnnouncerMessage(msg);
        }
      } else {
        printf(
            "[ENEMY DELIVERY] Ship %s delivered %.2f vinyl to ENEMY base (%s "
            "to %s)\n",
            GetName(), vinylDelivered, GetTeam()->GetName(),
            pStation->GetTeam()->GetName());
      }
    }
    ((CStation *)pOthThing)->AddVinyl(vinylDelivered);
    adStatCur[(UINT)S_CARGO] = 0.0;

    bDockFlag = true;
    return;
  }

  double msh, dshield = GetAmount(S_SHIELD);
  if (OthKind == GENTHING) {  // Laser object
    msh = (pOthThing->GetMass());
    dshield -= (msh / 1000.0);

    SetAmount(S_SHIELD, dshield);
    if (dshield < 0.0) {
      printf("[DESTROYED] Ship %s (%s) destroyed by laser\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown");
      if (pWorld) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s destroyed by laser", GetName());
        pWorld->AddAnnouncerMessage(msg);
      }
      KillThing();
    }
    return;
  }

  double damage = (RelativeMomentum(*pOthThing).rho) / 1000.0;
  dshield -= damage;
  SetAmount(S_SHIELD, dshield);

  // Announce collision even if ship survives
  if (pWorld && damage > 0.1) {  // Only announce significant collisions
    char msg[256];
    const char *targetName = "unknown";
    if (pOthThing->GetKind() == SHIP) {
      targetName = pOthThing->GetName();
    } else if (pOthThing->GetKind() == ASTEROID) {
      targetName = "asteroid";
    }
    snprintf(msg, sizeof(msg), "%s hit %s, %.1f damage",
             GetName(), targetName, damage);
    pWorld->AddAnnouncerMessage(msg);
  }
  if (dshield < 0.0) {
    const char *causeType = "unknown";
    if (pOthThing->GetKind() == SHIP) {
      causeType = "ship collision";
    } else if (pOthThing->GetKind() == ASTEROID) {
      causeType = "asteroid collision";
    }
    printf("[DESTROYED] Ship %s (%s) destroyed by %s\n", GetName(),
           GetTeam() ? GetTeam()->GetName() : "Unknown", causeType);
    if (pWorld) {
      char msg[256];
      const char *shortCause = (pOthThing->GetKind() == SHIP) ? "ship" : "asteroid";
      snprintf(msg, sizeof(msg), "%s destroyed by %s", GetName(), shortCause);
      pWorld->AddAnnouncerMessage(msg);
    }
    KillThing();
  }

  if (OthKind == ASTEROID) {
    CThing *pEat = ((CAsteroid *)pOthThing)->EatenBy();
    if (pEat != NULL && !(*pEat == *this)) {
      return;
    }
    // Already taken by another ship

    // Update ship velocity with conservation of linear momentum for a perfectly
    // inelastic collision, clamped by maxspeed.
    CTraj MomTot = GetMomentum() + pOthThing->GetMomentum();
    double othmass = pOthThing->GetMass();
    double masstot = GetMass() + othmass;
    Vel = MomTot / masstot;
    if (Vel.rho > maxspeed) {
      Vel.rho = maxspeed;
    }

    if (AsteroidFits((CAsteroid *)pOthThing)) {
      switch (((CAsteroid *)pOthThing)->GetMaterial()) {
        case VINYL:
          adStatCur[(UINT)S_CARGO] += othmass;
          break;
        case URANIUM:
          adStatCur[(UINT)S_FUEL] += othmass;
          break;
        default:
          break;
      }
    }
  }

  if (OthKind == SHIP && pOthThing->GetTeam() != NULL) {
    CTeam *pTmpTm = pmyTeam;
    pmyTeam = NULL;  // Prevents an infinite recursive call.
    pOthThing->Collide(this, pWorld);
    pmyTeam = pTmpTm;
  }

  // Apply a separation impulse to a ship to bump it clear of other ships or
  // asteroid bits that may have been created as a result of collision.
  double dang = pOthThing->GetPos().AngleTo(GetPos());
  double dsmov = pOthThing->GetSize() + 3.0;
  CTraj MovVec(dsmov, dang);
  CCoord MovCoord(MovVec);
  Pos += MovCoord;

  double dmassrat = pOthThing->GetMass() / GetMass();
  MovVec = MovVec * dmassrat;
  Vel += MovVec;
  if (Vel.rho > maxspeed) {
    Vel.rho = maxspeed;
  }
}

void CShip::HandleJettison() {
  AsteroidKind AsMat;
  double dMass;

  if (GetTeam() == NULL) {
    return;
  }
  CWorld *pWld = GetTeam()->GetWorld();
  if (pWld == NULL) {
    return;
  }

  if (IsDocked()) {
    return;
  }

  AsMat = URANIUM;
  dMass = GetOrder(O_JETTISON);
  if (fabs(dMass) < minmass) {
    return;
  }
  if (dMass < 0.0) {
    dMass *= -1;
    AsMat = VINYL;
  }

  CAsteroid *pAst = new CAsteroid(dMass, AsMat);
  CCoord AstPos(Pos);
  CTraj AstVel(Vel);

  // Place the asteroid at a distance from the ship to avoid overlap.
  CTraj MovVec(Vel);
  double totsize = GetSize() + pAst->GetSize();
  MovVec.rho = totsize * 1.15;
  MovVec.theta = GetOrient();
  // Pos -= MovVec.ConvertToCoord();
  AstPos += MovVec.ConvertToCoord();

  // Set the asteroid's stats and add it to the world
  AstVel.theta = GetOrient();
  pAst->SetPos(AstPos);
  pAst->SetVel(AstVel);
  pWld->AddThingToWorld(pAst);

  // Set your own stats to accomodate
  double dnewmass = GetMass() - dMass;
  MovVec = GetMomentum();
  MovVec -= (pAst->GetMomentum() * 2.0);  // Give it some extra Kick
  MovVec = MovVec / dnewmass;

  Vel = MovVec;
  if (Vel.rho > maxspeed) {
    Vel.rho = maxspeed;
  }
  SetOrder(O_JETTISON, 0.0);

  double matamt = GetAmount(AstToStat(AsMat));
  matamt -= dMass;
  SetAmount(AstToStat(AsMat), matamt);
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CShip::GetSerialSize() const {
  UINT totsize = 0;

  totsize += CThing::GetSerialSize();
  totsize += BufWrite(NULL, myNum);
  totsize += BufWrite(NULL, bDockFlag);
  totsize += BufWrite(NULL, dDockDist);
  totsize += BufWrite(NULL, dLaserDist);

  UINT i;
  for (i = 0; i < (UINT)O_ALL_ORDERS; i++) {
    totsize += BufWrite(NULL, adOrders[i]);
  }

  for (i = 0; i < (UINT)S_ALL_STATS; i++) {
    totsize += BufWrite(NULL, adStatCur[i]);
    totsize += BufWrite(NULL, adStatMax[i]);
  }

  return totsize;
}

unsigned CShip::SerialPack(char *buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char *vpb = buf;

  vpb += (CThing::SerialPack(buf, buflen));
  vpb += BufWrite(vpb, myNum);
  vpb += BufWrite(vpb, bDockFlag);
  vpb += BufWrite(vpb, dDockDist);
  vpb += BufWrite(vpb, dLaserDist);

  UINT i;
  for (i = 0; i < (UINT)O_ALL_ORDERS; i++) {
    vpb += BufWrite(vpb, adOrders[i]);
  }

  for (i = 0; i < (UINT)S_ALL_STATS; i++) {
    vpb += BufWrite(vpb, adStatCur[i]);
    vpb += BufWrite(vpb, adStatMax[i]);
  }

  return (vpb - buf);
}

unsigned CShip::SerialUnpack(char *buf, unsigned buflen) {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char *vpb = buf;

  vpb += (CThing::SerialUnpack(buf, buflen));
  vpb += BufRead(vpb, myNum);
  vpb += BufRead(vpb, bDockFlag);
  vpb += BufRead(vpb, dDockDist);
  vpb += BufRead(vpb, dLaserDist);

  UINT i;
  for (i = 0; i < (UINT)O_ALL_ORDERS; i++) {
    vpb += BufRead(vpb, adOrders[i]);
  }

  for (i = 0; i < (UINT)S_ALL_STATS; i++) {
    vpb += BufRead(vpb, adStatCur[i]);
    vpb += BufRead(vpb, adStatMax[i]);
  }

  return (vpb - buf);
}

///////////////////////////////////////////
// Strategy Pattern Implementation for Velocity/Acceleration Processing

// Anonymous namespace for strategy implementations
namespace {

// Legacy velocity/acceleration strategy
class LegacyVelocityStrategy : public CShip::VelocityStrategy {
public:
  double processThrustOrder(CShip* ship, OrderKind ord, double value) override {
    // Legacy thrust order processing - contains the current SetOrder O_THRUST logic
    double valtmp, fuelcon, maxfuel;
    CTraj AccVec;

    maxfuel = ship->GetAmount(S_FUEL);
    if (ship->IsDocked() == true) {
      maxfuel = ship->GetCapacity(S_FUEL);
    }

    if (value == 0.0) {
      return 0.0;
    }
    ship->ClearTurnJettisonOrders();

    AccVec = CTraj(value, ship->GetOrient());
    AccVec += ship->GetVelocity();
    if (AccVec.rho > maxspeed) {
      AccVec.rho = maxspeed;
    }
    AccVec = AccVec - ship->GetVelocity();  // Should = what it was before, in most cases
    if (value <= 0.0) {
      value = -AccVec.rho;
    } else {
      value = AccVec.rho;
    }

    // NOTE: The departure thrust of a ship is limited by its maximum fuel
    // capacity, even though that thrust is free (will only be relevant for
    // ships with very small fuel tanks).
    //
    // 1 ton of fuel accelerates a naked ship from zero to 6.0*maxspeed
    fuelcon = fabs(value) * ship->GetMass() / (6.0 * maxspeed * ship->GetBaseMass());
    if (fuelcon > maxfuel && ship->IsDocked() == false) {
      fuelcon = maxfuel;
      valtmp = fuelcon * 6.0 * maxspeed * ship->GetBaseMass() / ship->GetMass();
      // If our original requested thrust was negative, make our clamped value
      // negative as well.
      if (value <= 0.0) {
        value = -valtmp;
      } else {
        value = valtmp;
      }
    }
    if (ship->IsDocked() == true) {
      fuelcon = 0.0;
    }

    ship->SetThrustOrder(value);
    return fuelcon;
  }

  void processThrustDrift(CShip* ship, double thrustamt, double dt) override {
    // Legacy thrust drift processing - contains the current Drift thrusting logic
    double fuelcons;

    // Calculate fuel consumption directly using legacy method
    fuelcons = processThrustOrder(ship, O_THRUST, thrustamt);
    double oldFuel = ship->GetAmount(S_FUEL);
    double newFuel = oldFuel - fuelcons;
    ship->SetAmount(S_FUEL, newFuel);

    // Check if out of fuel
    if (oldFuel > 0.01 && newFuel <= 0.01) {
      printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", ship->GetName(),
             ship->GetTeam() ? ship->GetTeam()->GetName() : "Unknown");
    }

    CTraj Accel(thrustamt, ship->GetOrient());
    CTraj currentVel = ship->GetVelocity();
    currentVel += (Accel * dt);
    if (currentVel.rho > maxspeed) {
      double originalSpeed = currentVel.rho;
      currentVel.rho = maxspeed;

      // Announce when thrusting causes velocity clamping (if enabled)
      if (g_pParser && g_pParser->UseNewFeature("announcer-velocity-clamping")) {
        CWorld* pWorld = ship->GetTeam()->GetWorld();
        if (pWorld) {
          char msg[256];
          snprintf(msg, sizeof(msg), "%s thrust clamped %.1f -> %.1f",
                   ship->GetName(), originalSpeed, maxspeed);
          pWorld->AddAnnouncerMessage(msg);
        }
      }
    }
    ship->ApplyVelocityChange(currentVel);

    if (ship->IsDocked() == true) {
      CTraj VOff(ship->GetDockDistance() + 5.0, ship->GetOrient());
      CCoord newPos = ship->GetPos();
      if (ship->GetOrder(O_THRUST) > 0.0) {
        newPos += VOff.ConvertToCoord();
      } else {
        newPos -= VOff.ConvertToCoord();
      }
      ship->ApplyPositionChange(newPos);
      ship->ApplyVelocityChange(Accel);  // Leave station at full speed
      ship->SetDockFlag(false);
    }

    if (thrustamt < 0.0) {
      ship->SetImageSet(2);
    } else {
      ship->SetImageSet(1);
    }
  }
};

// New/improved velocity/acceleration strategy
class ImprovedVelocityStrategy : public CShip::VelocityStrategy {
public:
  double processThrustOrder(CShip* ship, OrderKind ord, double value) override {
    // For now, use the same logic as legacy until we implement improvements
    // This is where future improvements to velocity/acceleration limits would go
    LegacyVelocityStrategy legacy;
    return legacy.processThrustOrder(ship, ord, value);
  }

  void processThrustDrift(CShip* ship, double thrustamt, double dt) override {
    // For now, use the same logic as legacy until we implement improvements
    // This is where future improvements to drift behavior would go
    LegacyVelocityStrategy legacy;
    legacy.processThrustDrift(ship, thrustamt, dt);
  }
};

} // anonymous namespace

// Initialize velocity strategy based on configuration
void CShip::InitializeVelocityStrategy() const {
  extern CParser* g_pParser;
  if (g_pParser && !g_pParser->UseNewFeature("velocity-limits")) {
    velocityStrategy = std::make_unique<LegacyVelocityStrategy>();
  } else {
    velocityStrategy = std::make_unique<ImprovedVelocityStrategy>();
  }
}
