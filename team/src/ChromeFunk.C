/* Chrome Funkadelic
 * Sample team
 * MechMania IV: The Vinyl Frontier
 * Misha Voloshin 9/26/98
 */

#include "ChromeFunk.h"
#include "GameConstants.h"

// Tell the game to use our class
CTeam* CTeam::CreateTeam() { return new ChromeFunk; }

//////////////////////////////////////////
// Chrome Funkadelic class

ChromeFunk::ChromeFunk() {}

ChromeFunk::~ChromeFunk() {
  CShip* pSh;
  CBrain* pBr;

  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    pSh = GetShip(i);
    if (pSh == NULL) {
      continue;  // Ship is dead
    }

    pBr = pSh->GetBrain();
    if (pBr != NULL) {
      delete pBr;
    }
    // Clean up after ourselves
  }
}

void ChromeFunk::Init() {
  // Strategic initialization: Set up team and assign default tactical contexts
  srand(time(NULL));
  SetTeamNumber(1 + (rand() % 16));
  SetName("Chrome Funkadelic");
  GetStation()->SetName("HeartLand");

  GetShip(0)->SetName("SS TurnTable");
  GetShip(1)->SetName("Bell Bottoms");
  GetShip(2)->SetName("DiscoInferno");
  GetShip(3)->SetName("PurpleVelvet");

  // Assign default tactical context: All ships start as resource gatherers
  // This demonstrates the basic Brain system - ships get focused AI behaviors
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 45.0);
    GetShip(i)->SetCapacity(S_CARGO, 15.0);  // Redundant, but be safe
    GetShip(i)->SetBrain(new Gatherer);  // Default context: resource collection
  }
}

void ChromeFunk::Turn() {
  // Strategic AI: Execute tactical behaviors for each ship
  // ChromeFunk uses a simple strategy: Let each ship's brain handle its own
  // context More advanced teams could analyze game state and switch brains
  // dynamically

  CShip* pSh;

  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    pSh = GetShip(i);
    if (pSh == NULL) {
      continue;
    }

    // Execute tactical AI: Each ship's brain handles its current context
    // Brains can switch contexts internally (e.g., Gatherer -> Voyager ->
    // Gatherer)
    pSh->GetBrain()->Decide();
  }
}

///////////////////////////////////////////////

Voyager::Voyager(CBrain* pLB) {
  pLastBrain = pLB;  // Store who we're replacing
  if (pLB->pShip != NULL) {
    pLB->pShip->SetBrain(this);  // Replace it
  }
}

Voyager::~Voyager() {
  if (pShip != NULL)
    pShip->SetBrain(pLastBrain);  // Put everything back
}

void Voyager::Decide() {
  if (pShip == NULL) {
    return;
  }
  if (!pShip->IsDocked()) {
    delete this;  // Don't need us anymore
    return;       // Let's blow this pop stand
  }

  // Otherwise, we're docked - time to depart!

  // Desired angle of station departure
  double tang = (double)(pShip->GetShipNumber()) * PI / 2.0;

  // Only one of O_TURN and O_THRUST can be active at a time. Here we set
  // O_TURN, but then if our desired turning angle is small (probably because
  // we did an O_TURN last turn), we then set an O_THRUST order, which will
  // clear the O_TURN order.
  tang -= pShip->GetOrient();
  if (tang < -PI) {
    tang += PI2;
  }
  if (tang > PI) {
    tang -= PI2;
  }
  pShip->SetOrder(O_TURN, tang);

  // O_THRUST and O_TURN orders while docked cost us no fuel, so we can go all
  // the way to maxspeed at no cost.
  if (fabs(tang) < 0.2)
    pShip->SetOrder(O_THRUST, g_game_max_speed);
}

//------------------------------------------

void Stalker::Decide() {
  if (pTarget == NULL ||  // No valid target
      pShip == NULL ||    // No valid ship assigned to this AI
      *pShip == *pTarget) {
    return;  // Can't home in on ourselves!
  }

  // First of all, are we going to crash into them anyway?
  //
  // NOTE: Here we use a legacy and estimated collision detection to preserve
  // ChromeFunk's original behavior.
  //
  // NEW TEAMS: Should use *pShip->DetectCollisionCourse(*pTarget) instead,
  // which is inherited from CThing.
  double dt = LegacyDetectCollisionCourse(*pTarget);
  if (dt != NO_COLLIDE) {
    pShip->SetOrder(O_THRUST, 0.0);  // Yup. Cancel thrust orders, if any
    return;                          // Our work here is done
  }

  // First let's estimate how long interception will take
  // Most of these calculations are completely arbitrary...
  CTraj RelVel = pShip->RelativeVelocity(*pTarget);
  double dist = pShip->GetPos().DistTo(pTarget->GetPos());
  dt = sqrt(dist / RelVel.rho);
  dt += 1000.0 / dist;
  // dt isn't a very good estimate, since it doesn't take
  //  direction of velocity into account, but it's good
  //  enough for the Chrome Funkadelic.  It'll still
  //  intercept even if the time estimate isn't dead on,
  //  which it probably never will be.

  double dang = pShip->AngleToIntercept(*pTarget, dt);
  // This is how much we need to turn
  // dang is an angle between -PI and PI

  pShip->SetOrder(O_TURN, 1.2 * dang);
  // Let's set the turn order for now.
  // Multiply by 1.2 so we'll make sharper turns
  // If we end up deciding to thrust, thrusting
  //  will over-ride the turn order anyway

  double tol = 1.0;      // Angle tolerance, dependent on distance
  tol *= dist / 1000.0;  // Directly proportional, with an arbitrary constant

  if (fabs(dang) < tol) {             // We're facing our target's future posn
    pShip->SetOrder(O_THRUST, 10.0);  // Accelerate fairly quickly
  } else if (fabs(dang) > (PI - 0.15)) {  // We're oriented away from it
    pShip->SetOrder(O_THRUST, -10.0);     // Cheaper to blast backwards
  }
}

//------------------------------------
// Legacy collision detection function for ChromeFunk team Preserves original
// behavior under the old engine's collision detection estimation, which is
// imperfect.
//
// ChromeFunk's AI logic was designed around this approximation approach, so we
// maintain it to preserve their intended behavior.
//------------------------------------
double Stalker::LegacyDetectCollisionCourse(const CThing& OthThing) const {
  if (OthThing == *pShip) {
    return NO_COLLIDE;
  }

  CTraj VRel = pShip->RelativeVelocity(OthThing);  // Direction of vector
  if (VRel.rho <= 0.05) {
    return NO_COLLIDE;  // Never gonna hit if effectively not moving
  }

  double flyred = pShip->GetSize() +
                  OthThing.GetSize();  // Don't allow them to scrape each other
  double dist =
      pShip->GetPos().DistTo(OthThing.GetPos());  // Magnitude of vector
  if (dist < flyred) {
    return 0.0;  // They're already impacting
  }

  // LEGACY LOGIC: This approximation projects along relative velocity direction
  // for a distance equal to current separation looking for a collision.
  CTraj VHit(dist, VRel.theta);
  CCoord RelPos = OthThing.GetPos() - pShip->GetPos(),
         CHit(RelPos + VHit.ConvertToCoord());

  double flyby = CHit.DistTo(CCoord(0.0, 0.0));
  if (flyby > flyred) {
    return NO_COLLIDE;
  }

  // Pending collision
  double hittime = (dist - flyred) / VRel.rho;
  return hittime;
}

//------------------------------------

void Shooter::Decide() {
  if (pTarget == NULL ||  // No valid target
      pShip == NULL ||    // No valid ship assigned to this AI
      *pShip == *pTarget) {
    return;  // Can't attack ourselves!
  }

  // Guage laser range
  double drange = pShip->GetPos().DistTo(pTarget->GetPos());

  if (drange > 350.0) {  // Too far away, will cost too much fuel
    Stalker::Decide();   // Home in on our prey
    return;              // That's all we'll do for now
  }

  drange += 100.0;  // We want another 100 miles left on the beam
  // when it hits our poor helpless target

  CCoord MyPos, TargPos;
  MyPos = pShip->PredictPosition(g_game_turn_duration);
  TargPos = pTarget->PredictPosition(g_game_turn_duration);
  // We're shooting one game turn from now, since the physics
  //  model computes movement before lasers

  CTraj TurnVec = MyPos.VectTo(TargPos);
  TurnVec.theta -= pShip->GetOrient();
  TurnVec.Normalize();
  double dang = TurnVec.theta;

  // LEGACY BUG: The engine won't let us turn and thrust in the same turn, so
  // the THRUST below never happens (the THRUST command is overridden by the
  // TURN command). We leave the bug in place for historic interest.
  pShip->SetOrder(O_THRUST, 0.0);  // Stabilize, get a decent shot

  pShip->SetOrder(O_TURN, dang);             // Turn to face him
  pShip->SetOrder(O_LASER, drange + 100.0);  // Fry the sucker!
  // Our lasers will fire 1 second from now.  Hence, by the time the
  // turn order is complete, we'll be looking right at him.
  // Unless, of course, he's thrusted or hit something.
}

//-------------------------------------

Gatherer::Gatherer() { pTarget = NULL; }

Gatherer::~Gatherer() {}

unsigned int Gatherer::SelectTarget() {
  CTeam* pmyTeam = pShip->GetTeam();
  CWorld* pmyWorld = pShip->GetWorld();
  char shipmsg[128];  // Ship message

  if (pShip->GetAmount(S_CARGO) > 0.0) {  // We have cargo, let's go home
    if (pTarget != pmyTeam->GetStation()) {
      snprintf(shipmsg, sizeof(shipmsg),
               "%s gets %.1f tons of vinyl and goes home\n", pShip->GetName(),
               pShip->GetAmount(S_CARGO));
      strncat(pmyTeam->MsgText, shipmsg,
              maxTextLen - strlen(pmyTeam->MsgText) - 1);
    }
    return pmyTeam->GetStation()->GetWorldIndex();
  }

  unsigned int index, indbest = BAD_INDEX;
  CThing* pTh;
  ThingKind ThKind;
  AsteroidKind AsMat;
  double dist, dbest = -1.0;  // initialize dbest with some useless value

  for (index = pmyWorld->UFirstIndex;          // Let's iterate through the
       index <= pmyWorld->ULastIndex;          // things in the world, seeking
       index = pmyWorld->GetNextIndex(index))  // stuff to take
  {
    pTh = pmyWorld->GetThing(index);  // Get ptr to CThing object
    ThKind = pTh->GetKind();          // What are you?

    // If we find an enemy ship, we make that the target.
    if (ThKind == SHIP && pTh->GetTeam() != pShip->GetTeam()) {
      return index;
    }

    if (ThKind != ASTEROID) {
      continue;  // We're only looking for asteroids, so
    }
    // let's go on with the next cycle of the loop

    AsMat = ((CAsteroid*)pTh)->GetMaterial();
    if (pShip->GetAmount(S_FUEL) < 20.0) {  // Are we low on fuel?
      if (AsMat == VINYL) {
        continue;  // If we are, only look for fuel asteroids
      }
    }

    // If we've made it this far into the looping block,
    // then this asteroid must be something we want
    dist = pShip->GetPos().DistTo(pTh->GetPos());  // Distance to this Thing

    if (dbest < dist  // If this is better than all previous potential targets
        || indbest == BAD_INDEX) {  // Or if this is our first potential target
      indbest = index;  // Remember this is our best target candidate
      dbest = dist;     // Remember the best calculated distance
    }
  }

  return indbest;  // Best target asteroid found
}

void Gatherer::AvoidCollide() {
  unsigned int index;
  CThing* pTh;
  double dsec;
  char shipmsg[128];  // Ship might print a message
  CTraj RelMom;

  CTeam* pmyTeam = pShip->GetTeam();
  CWorld* pmyWorld = pShip->GetWorld();

  for (index = pmyWorld->UFirstIndex;          // Let's iterate through the
       index <= pmyWorld->ULastIndex;          // things in the world, seeking
       index = pmyWorld->GetNextIndex(index))  // stuff to take
  {
    pTh = pmyWorld->GetThing(index);  // Get ptr to CThing object
    // if (pTarget == pTh) continue;  // Okay to collide with target
    if (pTh == pShip) {
      continue;  // Won't collide with yourself
    }

    // Use legacy collision detection to preserve ChromeFunk's behavior
    dsec = LegacyDetectCollisionCourse(*pTh);
    if (dsec == NO_COLLIDE) {
      continue;  // No collision pending
    }
    if (dsec > 15.0) {
      continue;  // Collision won't happen for a while
    }

    // If we made it this far into this block of code,
    // we need to take evasive action

    // First, though, are we already accelerating anyway?
    // NOTE: Use GetJettison() convenience methods instead of
    // GetOrder(O_JETTISON) directly for better type safety and readability
    if (pShip->GetOrder(O_THRUST) != 0.0 || pShip->GetJettison(VINYL) != 0.0 ||
        pShip->GetJettison(URANIUM) != 0.0) {
      continue;
    }
    // We're either thrusting or ejecting something, so we'll probably move out
    // of the way anyway due to change of trajectory

    // Nope, we need to dodge an impact
    // Do we have enough time to get away?
    if (dsec > 15.0) {
      // This can be done much better than it's being done here,
      //  but this is merely a sample client
      pShip->SetOrder(O_THRUST, -15.0);  // Accelerate
      snprintf(shipmsg, sizeof(shipmsg), "%s brakes for %s\n", pShip->GetName(),
               pTh->GetName());
      strncat(pmyTeam->MsgText, shipmsg,
              maxTextLen - strlen(pmyTeam->MsgText) - 1);
      return;  // We already know we need to move.
    } else {   // No time to get out of the way
      // NOTE: In this simple client we shoot at anything we'll collide with -
      // including friendly ships or our own station!
      pTarget = pTh;
      Shooter::Decide();  // Let's just shoot it
      return;             // That's all we can handle for this turn
    }
  }

  // Loop finishes without any impending impacts detected
}

void Gatherer::Decide() {
  // Context switching: Handle station departure with temporary brain
  if (pShip->IsDocked()) {
    new Voyager(this);  // Switch to departure context temporarily
    return;  // Voyager will handle departure, then restore this brain
  }

  CTeam* pmyTeam = pShip->GetTeam();
  CWorld* pmyWorld = pmyTeam->GetWorld();

  unsigned int TargIndex = SelectTarget();
  if (TargIndex != BAD_INDEX) {
    pTarget = pmyWorld->GetThing(TargIndex);
  } else {
    return;
  }

  Stalker::Decide();  // Set sail for the target!

  if (pTarget->GetKind() == ASTEROID  // If the target's an asteroid
      && !(pShip->AsteroidFits((CAsteroid*)pTarget))) {
    // And we can't eat it...
    // Let's blast it!
    Shooter::Decide();  // Blast it if we can
  }

  if (pShip->GetAmount(S_FUEL) < 5.0         // Fuel is dangerously low!!!
      && pShip->GetAmount(S_CARGO) > 5.0) {  // Cargo's weighing us down
    // NOTE: Use SetJettison() convenience method instead of
    // SetOrder(O_JETTISON, ...) directly for better type safety and readability
    pShip->SetJettison(VINYL, 5.0);
    // Eject cargo so we can maneuver more easily
  }

  // Last but not least, let's keep ourselves from dying
  if (pShip->GetAmount(S_SHIELD) < 30.0)
    pShip->SetOrder(O_SHIELD, 3.0);
  if (pTarget->GetKind() != STATION) {  // If we're not going home
    AvoidCollide();  // Worry excessively about bumping into stuff
  }
}
