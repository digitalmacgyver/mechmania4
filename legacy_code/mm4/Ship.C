/* Ship.C
 * Ahh, the Motherload!!
 * Implementation of CShip class
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include "Ship.h"
#include "Station.h"
#include "World.h"
#include "Team.h"
#include "Brain.h"

///////////////////////////////////////////
// Construction/Destruction

CShip::CShip (CCoord StPos, CTeam* pteam, UINT ShNum)
  : CThing(StPos.fX,StPos.fY)
{
  TKind=SHIP;
  pmyTeam = pteam;
  myNum = ShNum;

  size=12.0;
  mass=40.0;
  orient=0.0;
  uImgSet=0;
  pBrain=NULL;

  bDockFlag=TRUE;
  dDockDist=30.0;
  dLaserDist=0.0;
  omega=0.0;

  for (UINT sh=(UINT)S_CARGO; sh<(UINT)S_ALL_STATS; sh++) {
    adStatMax[sh]=30.0;
    adStatCur[sh]=30.0;
    if (sh==(UINT)S_CARGO) adStatCur[sh]=0.0;
  }

  adStatMax[(UINT)S_SHIELD]=8000.0;  // Arbitrarily large value
  ResetOrders();
}

CShip::~CShip()
{

}

////////////////////////////////////////////
// Data access

UINT CShip::GetShipNumber() const
{
  return myNum;
}

BOOL CShip::IsDocked() const
{
  return bDockFlag;
}

double CShip::GetAmount(ShipStat st) const
{
  if (st>=S_ALL_STATS) return 0.0;
  return adStatCur[(UINT)st];
}

double CShip::GetCapacity(ShipStat st) const
{
  if (st>=S_ALL_STATS) return 0.0;
  return adStatMax[(UINT)st];
}

double CShip::GetOrder(OrderKind ord) const
{
  if (ord>=O_ALL_ORDERS) return 0.0;
  return adOrders[(UINT)ord];
}

double CShip::GetMass() const
{
  double sum= mass;
  sum += GetAmount(S_CARGO);
  sum += GetAmount(S_FUEL);
  return sum;
}

double CShip::GetLaserBeamDistance()
{
  return dLaserDist;
}

CBrain* CShip::GetBrain()
{
  return pBrain;
}

////////
// Incoming

double CShip::SetAmount(ShipStat st, double val)
{
  if (val<0.0) val=0.0;
  if (st>=S_ALL_STATS) return 0.0;
  if (val>=GetCapacity(st)) val=GetCapacity(st);
  adStatCur[(UINT)st] = val;
  return GetAmount(st);
}

double CShip::SetCapacity(ShipStat st, double val)
{
  if (st>=S_ALL_STATS) return 0.0;
  if (val<0.0) val=0.0;
  if (val>dMaxStatTot) val = dMaxStatTot;

  adStatMax[(UINT)st] = val;

  double tot=0.0;
  tot += adStatMax[S_CARGO];
  tot += adStatMax[S_FUEL];

  if (tot>dMaxStatTot) {
    tot -= dMaxStatTot;
    if (st==S_CARGO) adStatMax[(UINT)S_FUEL] -= tot;
    if (st==S_FUEL) adStatMax[(UINT)S_CARGO] -= tot;
  }

  if (GetAmount(st)>GetCapacity(st))
    adStatCur[(UINT)st] = GetCapacity(st);
  return GetCapacity(st);
}

CBrain* CShip::SetBrain(CBrain* pBr)
{
  CBrain *pBrTmp = GetBrain();
  pBrain = pBr;
  if (pBrain!=NULL) pBrain->pShip = this;
  return pBrTmp;
}

////////////////////////////////////////////
// Ship control

void CShip::ResetOrders()
{
  dLaserDist=0.0;
  for (UINT ord=(UINT)O_SHIELD; ord<(UINT)O_ALL_ORDERS; ord++)
    adOrders[ord]=0.0;
}

// SetOrder method used for computing fuel consumed for an order
double CShip::SetOrder(OrderKind ord, double value)
{
  double valtmp, fuelcon, maxfuel;
  CTraj AccVec;
  UINT oit;
  AsteroidKind AsMat;

  maxfuel=GetAmount(S_FUEL);
  if (IsDocked()==TRUE) maxfuel=GetCapacity(S_FUEL);

  switch(ord) {
    case O_SHIELD:      // "value" is amt by which to boost shields
      if (value<0.0) value=0.0;  // Can't lower shields
      valtmp = value + GetAmount(S_SHIELD);
      if (valtmp>GetCapacity(S_SHIELD))
	value = GetCapacity(S_SHIELD)-GetAmount(S_SHIELD);
      
      fuelcon = value;
      if (fuelcon>GetAmount(S_FUEL)) {          // Check for sufficient fuel
	fuelcon = GetAmount(S_FUEL);
	value = fuelcon;              // No, but here's how much we *can* do
      }

      adOrders[(UINT)O_SHIELD]=value;
      return fuelcon;   // Doesn't need a break since this returns

    case O_LASER:       // "value" is specified length of laser beam
      if (value<0.0) value=0.0;
      if (IsDocked()) {    // Can't shoot while docked
	value=0.0;
	return 0.0;
      }
      if (value>(fWXMax-fWXMin)/2.0) value = (fWXMax-fWXMin)/2.0;
      if (value>(fWYMax-fWYMin)/2.0) value = (fWYMax-fWYMin)/2.0;

      fuelcon = value / 50.0;
      if (fuelcon>GetAmount(S_FUEL)) {       // Check for sufficient fuel
	fuelcon = GetAmount(S_FUEL);
	value = fuelcon*50.0;             // No, but here's how much we *can* do
      }
      
      adOrders[(UINT)O_LASER]=value;
      return fuelcon;
    
    case O_THRUST:   // "value" is magnitude of acceleration vector
      if (value==0.0) return 0.0;
      adOrders[(UINT)O_TURN] = 0.0;
      adOrders[(UINT)O_JETTISON] = 0.0;
      
      AccVec=CTraj(value,GetOrient());
      AccVec += GetVelocity();
      if (AccVec.rho>maxspeed) AccVec.rho=maxspeed;
      AccVec = AccVec - GetVelocity();       // Should = what it was before, in most cases
      if (value<=0.0) value=-AccVec.rho;
      else value=AccVec.rho;

      // 1 ton of fuel accelerates a naked ship from zero to 6.0*maxspeed
      fuelcon = fabs(value) * GetMass() / (6.0*maxspeed*mass);
      if (fuelcon>maxfuel && IsDocked()==FALSE) {
	fuelcon = maxfuel;
	valtmp = fuelcon*6.0*maxspeed*mass/GetMass();
	if (value<=0.0) value=-valtmp;
	else value=valtmp;
      }
      if (IsDocked()==TRUE) fuelcon=0.0;
      
      adOrders[(UINT)O_THRUST] = value;
      return fuelcon;

    case O_TURN:    // "value" is angle, in radians, to turn
      if (value==0.0) return 0.0;
      adOrders[(UINT)O_THRUST] = 0.0;
      adOrders[(UINT)O_JETTISON] = 0.0;
      
      // 1 ton of fuel rotates naked ship full-circle six times
      fuelcon = fabs(value) * GetMass() / (6.0*PI2 * mass);
      if (IsDocked()==TRUE) fuelcon=0.0;
      if (fuelcon>maxfuel) {
	fuelcon=maxfuel;
	valtmp = (mass * 6.0 * PI2 * fuelcon) / GetMass();
	if (value<=0.0) value=-valtmp;
	else value=valtmp;
      }

      adOrders[(UINT)O_TURN] = value;
      return fuelcon;

    case O_JETTISON:  // "value" is tonnage: positive for fuel, neg for cargo
      if (fabs(value)<minmass) {
	value=0.0;
	adOrders[(UINT)O_JETTISON]=0.0;
	return 0.0;  // Jettisonning costs no fuel
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
      else return 0.0;       // Jettisonning itself takes no fuel

    default:
      valtmp=0.0;
      for (oit=(UINT)O_SHIELD; oit<=(UINT)O_THRUST; oit++)
         valtmp += SetOrder((OrderKind)oit,GetOrder((OrderKind)oit));
      return valtmp;
  }
}

void CShip::SetJettison(AsteroidKind Mat, double amt)
{
  switch (Mat) {
    case URANIUM: SetOrder(O_JETTISON,amt); return;
    case VINYL: SetOrder(O_JETTISON, -amt); return;
    default: SetOrder(O_JETTISON, 0.0); return;
  }
}

double CShip::GetJettison(AsteroidKind Mat)
{
  double amt=GetOrder(O_JETTISON);
  if (amt>0.0 && Mat==URANIUM) return amt;
  if (amt<0.0 && Mat==VINYL) return -amt;
  return 0.0;
}

////////////////////////////////////////////
// Inherited methods

void CShip::Drift(double dt)
{
  if (GetTeam()->GetWorld()->bGameOver==TRUE) {
    CThing::Drift(0.0);  // Ships don't move when game is over
    return;
  }

  bIsColliding=NO_DAMAGE;
  bIsGettingShot=NO_DAMAGE;
  if (Vel.rho > maxspeed) Vel.rho=maxspeed;
  // From CThing::Drift

  double thrustamt = GetOrder(O_THRUST);
  double turnamt = GetOrder(O_TURN);
  double shieldamt = GetOrder(O_SHIELD);
  double fuelcons;

  uImgSet=0;  // Assume it's just drifting for now

  // Jettisonning, then movement stuff
  HandleJettison();

  // First handle shields
  if (shieldamt>0.0) {
    fuelcons = SetOrder(O_SHIELD,shieldamt);
    SetAmount(S_FUEL, GetAmount(S_FUEL)-fuelcons);
    SetAmount(S_SHIELD, GetAmount(S_SHIELD)+shieldamt);
    SetOrder(O_SHIELD,0.0);  // Shield set, ignore it now
  }

  // Now handle turning
  omega=0.0;
  if (turnamt!=0.0) {
    fuelcons = SetOrder(O_TURN,turnamt);
    SetAmount(S_FUEL, GetAmount(S_FUEL)-fuelcons*dt);
    omega = turnamt;

    if (turnamt<0.0) uImgSet=3;
    else uImgSet=4;
  }

  // Thrusting time
  if (thrustamt!=0.0) {
    fuelcons = SetOrder(O_THRUST,thrustamt);
    SetAmount(S_FUEL,GetAmount(S_FUEL)-fuelcons);
    
    CTraj Accel(thrustamt,GetOrient());
    Vel += (Accel * dt);
    if (Vel.rho>maxspeed) Vel.rho = maxspeed;
    
    if (IsDocked()==TRUE) {
      CTraj VOff(dDockDist+5.0,GetOrient());
      if (GetOrder(O_THRUST)>0.0) Pos += VOff.ConvertToCoord();
      else Pos -= VOff.ConvertToCoord();
      Vel = Accel;  // Leave station at full speed
      bDockFlag=FALSE;
    }

    if (thrustamt<0.0) uImgSet=2;
    else uImgSet=1;
  }

  // Also from CThing::Drift
  Pos += (Vel*dt).ConvertToCoord();
  orient += omega*dt;
  if (orient<-PI || orient>PI) {
    CTraj VTmp(1.0,orient);
    VTmp.Normalize();
    orient = VTmp.theta;
  }

  omega=0.0;  // Just for good measure
  dLaserDist=0.0;  // Don't want lasers left on
}

BOOL CShip::AsteroidFits(const CAsteroid* pAst)
{ 
  double othmass = pAst->GetMass();
  switch (pAst->GetMaterial()) {
    case VINYL:
      if ((othmass+GetAmount(S_CARGO))>GetCapacity(S_CARGO)) return FALSE;
      return TRUE;
    case URANIUM:
      if ((othmass+GetAmount(S_FUEL))>GetCapacity(S_FUEL)) return FALSE;
      return TRUE;
    default: return FALSE;
  }
}

////////////////////////////////////////////
// Battle assistants

CThing* CShip::LaserTarget()
{
  if (pmyTeam==NULL) {
    return NULL;
  }

  CWorld *pWorld = pmyTeam->GetWorld();
  if (pWorld==NULL) {
    return NULL;
  }

  CThing *pTCur,*pTRes=NULL;
  double dist,mindist=-1.0;  // Start with something invalid
  UINT i;

  dLaserDist=0.0;

  for (i=pWorld->UFirstIndex; i!=(UINT)-1; i=pWorld->GetNextIndex(i)) {
    pTCur = pWorld->GetThing(i);
    if (IsFacing(*pTCur)==FALSE) continue;  // Nevermind, we're not facing it

    dist = GetPos().DistTo(pTCur->GetPos());
    if (dist<mindist || mindist==-1.0) {
      mindist=dist;
      pTRes = pTCur;
    }
  }

  dLaserDist=mindist;
  double dlaspwr = GetOrder(O_LASER);
  if (mindist>dlaspwr) dLaserDist=dlaspwr;
  return pTRes;
}

double CShip::AngleToIntercept(const CThing& OthThing, double dtime)
{
  CCoord myPos,hisPos;
  myPos = PredictPosition(dtime);
  hisPos = OthThing.PredictPosition(dtime);

  double ang = myPos.AngleTo(hisPos),
    face = GetOrient(),
    turn = ang-face;

  if (turn<-PI || turn>PI) {
    CTraj VTmp(1.0,turn);
    VTmp.Normalize();
    turn = VTmp.theta;
  }

  return turn;
}


ShipStat CShip::AstToStat(AsteroidKind AsMat) const
{
  switch (AsMat) {
    case URANIUM: return S_FUEL; 
    case VINYL: return S_CARGO;
    default: return S_ALL_STATS;
  }
}

AsteroidKind CShip::StatToAst(ShipStat ShStat) const
{
  switch (ShStat) {
    case S_FUEL: return URANIUM;
    case S_CARGO: return VINYL;
    default: return GENAST;
  }
}


////////////////////////////////////////////
// Protected methods

void CShip::HandleCollision (CThing* pOthThing, CWorld *pWorld)
{
  if (*pOthThing==*this ||  // Can't collide with yourself!
      IsDocked()==TRUE) {    // Nothing can hurt you at a station
    bIsColliding=NO_DAMAGE;
    return;
  }
  if (pWorld==NULL);  // Okay for world to be NULL, this suppresses warning

  ThingKind OthKind = pOthThing->GetKind();

  if (OthKind==STATION) {
    dDockDist=Pos.DistTo(pOthThing->GetPos());
    bIsColliding=NO_DAMAGE;

    Pos = pOthThing->GetPos();
    Vel = CTraj(0.0,0.0);
    SetOrder(O_THRUST,0.0);

    ((CStation*)pOthThing)->AddVinyl(GetAmount(S_CARGO));
    adStatCur[(UINT)S_CARGO]=0.0;

    bDockFlag=TRUE;
    return;
  }

  double msh, dshield = GetAmount(S_SHIELD);
  if (OthKind==GENTHING) {   // Laser object
    msh = (pOthThing->GetMass());
    dshield -= (msh/1000.0);
    
    SetAmount(S_SHIELD, dshield);
    if (dshield<0.0) KillThing();
    return;
  }

  dshield -= ((RelativeMomentum(*pOthThing).rho)/1000.0);
  SetAmount(S_SHIELD,dshield);
  if (dshield<0.0) KillThing();
  
  if (OthKind==ASTEROID) {
    CThing *pEat = ((CAsteroid*)pOthThing)->EatenBy();
    if (pEat!=NULL && !(*pEat==*this)) return;
    // Already taken by another ship

    CTraj MomTot = GetMomentum() + pOthThing->GetMomentum();
    double othmass = pOthThing->GetMass();
    double masstot = GetMass() + othmass;
    Vel = MomTot/masstot;
    if (Vel.rho>maxspeed) Vel.rho=maxspeed;

    if (AsteroidFits((CAsteroid*)pOthThing)) {
      switch (((CAsteroid*)pOthThing)->GetMaterial()) {
        case VINYL:  adStatCur[(UINT)S_CARGO] += othmass;    break;
        case URANIUM:  adStatCur[(UINT)S_FUEL] += othmass;   break;
        default:  break;
      }
    }
  }

  if (OthKind==SHIP && pOthThing->GetTeam()!=NULL) {
    CTeam *pTmpTm = pmyTeam;
    pmyTeam=NULL;  // Prevents an infinite recursive call.
    pOthThing->Collide(this,pWorld);
    pmyTeam=pTmpTm;
  }

  double dang = pOthThing->GetPos().AngleTo(GetPos());
  double dsmov = pOthThing->GetSize() + 3.0;
  CTraj MovVec(dsmov,dang);
  CCoord MovCoord(MovVec);
  Pos += MovCoord;

  double dmassrat = pOthThing->GetMass()/GetMass();
  MovVec= MovVec * dmassrat;
  Vel += MovVec;
  if (Vel.rho>maxspeed) Vel.rho=maxspeed;
}

void CShip::HandleJettison()
{
  AsteroidKind AsMat;
  double dMass;

  if (GetTeam()==NULL) return;
  CWorld *pWld = GetTeam()->GetWorld();
  if (pWld==NULL) return;

  if (IsDocked()) return;

  AsMat=URANIUM;
  dMass = GetOrder(O_JETTISON);
  if (fabs(dMass)<minmass) return;
  if (dMass<0.0) {
    dMass *= -1;
    AsMat = VINYL;
  }

  CAsteroid *pAst = new CAsteroid(dMass,AsMat);
  CCoord AstPos(Pos);
  CTraj AstVel(Vel);

  CTraj MovVec(Vel);
  double totsize = GetSize() + pAst->GetSize();
  MovVec.rho = totsize * 1.15;
  MovVec.theta = GetOrient();
  //Pos -= MovVec.ConvertToCoord();
  AstPos += MovVec.ConvertToCoord();

  // Set the asteroid's stats and add it to the world
  AstVel.theta = GetOrient();
  pAst->SetPos(AstPos);
  pAst->SetVel(AstVel);
  pWld->AddThingToWorld(pAst);

  // Set your own stats to accomodate
  double dnewmass = GetMass()-dMass;
  MovVec = GetMomentum();
  MovVec -= (pAst->GetMomentum() * 2.0);  // Give it some extra Kick   
  MovVec = MovVec / dnewmass;

  Vel = MovVec;
  if (Vel.rho>maxspeed) Vel.rho=maxspeed;
  SetOrder(O_JETTISON,0.0);

  double matamt = GetAmount(AstToStat(AsMat));
  matamt -= dMass;
  SetAmount (AstToStat(AsMat),matamt);
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CShip::GetSerialSize() const
{
  UINT totsize=0;

  totsize += CThing::GetSerialSize();
  totsize += BufWrite(NULL, myNum);
  totsize += BufWrite(NULL, bDockFlag);
  totsize += BufWrite(NULL, dDockDist);
  totsize += BufWrite(NULL, dLaserDist);

  UINT i;
  for (i=0; i<(UINT)O_ALL_ORDERS; i++)
    totsize += BufWrite (NULL, adOrders[i]);

  for (i=0; i<(UINT)S_ALL_STATS; i++) {
    totsize += BufWrite (NULL, adStatCur[i]);
    totsize += BufWrite (NULL, adStatMax[i]);
  }

  return totsize;
}

unsigned CShip::SerialPack (char *buf, unsigned buflen) const
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += (CThing::SerialPack(buf,buflen));
  vpb += BufWrite(vpb, myNum);
  vpb += BufWrite(vpb, bDockFlag);
  vpb += BufWrite(vpb, dDockDist);
  vpb += BufWrite(vpb, dLaserDist);

  UINT i;
  for (i=0; i<(UINT)O_ALL_ORDERS; i++)
    vpb += BufWrite (vpb, adOrders[i]);

  for (i=0; i<(UINT)S_ALL_STATS; i++) {
    vpb += BufWrite (vpb, adStatCur[i]);
    vpb += BufWrite (vpb, adStatMax[i]);
  }

  return (vpb-buf);
}
  
unsigned CShip::SerialUnpack (char *buf, unsigned buflen)
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += (CThing::SerialUnpack(buf,buflen));
  vpb += BufRead(vpb, myNum);
  vpb += BufRead(vpb, bDockFlag);
  vpb += BufRead(vpb, dDockDist);
  vpb += BufRead(vpb, dLaserDist);

  UINT i;
  for (i=0; i<(UINT)O_ALL_ORDERS; i++)
    vpb += BufRead (vpb, adOrders[i]);

  for (i=0; i<(UINT)S_ALL_STATS; i++) {
    vpb += BufRead (vpb, adStatCur[i]);
    vpb += BufRead (vpb, adStatMax[i]);
  }

  return (vpb-buf);
}
