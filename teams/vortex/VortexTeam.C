/* VortexTeam.C
 * Team Vortex Implementation
 *
 * Core strategy:
 * 1. Zone control - each ship operates in assigned quadrant
 * 2. Efficient pathing - minimize fuel usage
 * 3. Smart returns - optimize cargo/fuel balance
 * 4. Defensive combat - only fight when advantageous
 */

#include <cmath>
#include <cstdio>

#include "VortexTeam.h"

// Factory function
CTeam* CTeam::CreateTeam() { return new VortexTeam(); }

VortexTeam::VortexTeam() {
  total_vinyl = 0;
  total_uranium = 0;
  turn_count = 0;
}

VortexTeam::~VortexTeam() {
  // Clean up ship brains
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    CShip* ship = GetShip(i);
    if (ship) {
      CBrain* brain = ship->GetBrain();
      if (brain) {
        delete brain;
      }
    }
  }
}

void VortexTeam::Init() {
  // Don't set team number - let server assign it
  SetName("Vortex Squadron");
  GetStation()->SetName("Eye of Storm");

  // Name ships with vortex theme
  GetShip(0)->SetName("Whirlwind");
  GetShip(1)->SetName("Tempest");
  GetShip(2)->SetName("Cyclone");
  GetShip(3)->SetName("Maelstrom");

  // Match ChromeFunk's aggression with balanced approach
  // 30 fuel, 30 cargo - can carry more per trip than Chrome
  // This beats Chrome's 45/15 through efficiency
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    CShip* ship = GetShip(i);
    ship->SetCapacity(S_FUEL, 30.0);
    ship->SetCapacity(S_CARGO, 30.0);

    // Assign each ship to a zone (quadrant)
    ship->SetBrain(new VortexCollector(i));
  }
}

void VortexTeam::Turn() {
  turn_count++;

  // Reset resource counters
  total_vinyl = 0;
  total_uranium = 0;

  // Quick scan of world resources
  CWorld* world = GetWorld();
  for (unsigned int i = world->UFirstIndex; i != BAD_INDEX;
       i = world->GetNextIndex(i)) {
    CThing* thing = world->GetThing(i);
    if (!thing || !thing->IsAlive()) {
      continue;
    }

    if (thing->GetKind() == ASTEROID) {
      CAsteroid* ast = (CAsteroid*)thing;
      if (ast->GetMaterial() == VINYL) {
        total_vinyl += ast->GetMass();
      } else if (ast->GetMaterial() == URANIUM) {
        total_uranium += ast->GetMass();
      }
    }
  }

  // Let each ship decide
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    CShip* ship = GetShip(i);
    if (ship && ship->GetBrain()) {
      ship->GetBrain()->Decide();
    }
  }
}

///////////////////////////////////////////////
// VortexCollector Implementation

VortexCollector::VortexCollector(int zone) {
  current_target = nullptr;
  assigned_zone = zone;
  returning_home = false;
  last_fuel_check = 0;
}

VortexCollector::~VortexCollector() {}

void VortexCollector::Decide() {
  if (!pShip) {
    return;
  }

  pShip->ResetOrders();

  // Priority system
  HandleEmergency();   // Collision avoidance
  MaintainDefenses();  // Shield management
  ConsiderCombat();    // Opportunistic attacks

  // Main logic
  if (ShouldReturnHome()) {
    returning_home = true;
    current_target = pShip->GetTeam()->GetStation();
  } else if (returning_home && pShip->IsDocked()) {
    // Just docked - depart efficiently
    // Wait a moment to ensure full unload
    if (pShip->GetAmount(S_CARGO) > 0.01) {
      return;  // Still unloading
    }

    double angle = assigned_zone * PI / 2;  // Spread in 4 directions
    angle -= pShip->GetOrient();
    if (angle > PI) {
      angle -= PI2;
    }
    if (angle < -PI) {
      angle += PI2;
    }

    pShip->SetOrder(O_TURN, angle);
    if (fabs(angle) < 0.3) {
      pShip->SetOrder(O_THRUST, 25.0);  // Burst away fast
      returning_home = false;
    }
    return;
  } else {
    FindTarget();
  }

  NavigateToTarget();
}

void VortexCollector::FindTarget() {
  CWorld* world = pShip->GetWorld();
  VortexTeam* team = (VortexTeam*)pShip->GetTeam();

  CThing* best = nullptr;
  double best_score = -99999;

  for (unsigned int i = world->UFirstIndex; i != BAD_INDEX;
       i = world->GetNextIndex(i)) {
    CThing* thing = world->GetThing(i);
    if (!thing || !thing->IsAlive()) {
      continue;
    }
    if (thing->GetKind() != ASTEROID) {
      continue;
    }

    CAsteroid* ast = (CAsteroid*)thing;

    // Skip if doesn't fit
    if (!pShip->AsteroidFits(ast)) {
      continue;
    }

    // Calculate score
    double dist = pShip->GetPos().DistTo(thing->GetPos());
    double score = 0;

    // Prioritize vinyl heavily - it's what wins games
    if (ast->GetMaterial() == VINYL) {
      score = ast->GetMass() * 3.0;  // Triple value for vinyl
    } else {
      // Only get uranium if really low on fuel
      double fuel_ratio = pShip->GetAmount(S_FUEL) / pShip->GetCapacity(S_FUEL);
      if (fuel_ratio < 0.3) {
        score = ast->GetMass() * 2.0;
      } else {
        score = ast->GetMass() * 0.5;  // Low priority otherwise
      }
    }

    score -= dist / 50.0;  // Distance penalty

    // Small bonus for objects in our zone - flexibility is key
    if (IsInMyZone(thing)) {
      score += 20.0;  // Much more flexible
    }

    // Bonus for asteroids near our flight path home
    if (returning_home) {
      CCoord home_pos = pShip->GetTeam()->GetStation()->GetPos();
      CTraj to_home = pShip->GetPos().VectTo(home_pos);
      CTraj to_ast = pShip->GetPos().VectTo(thing->GetPos());
      double angle_diff = fabs(to_home.theta - to_ast.theta);
      if (angle_diff < 0.5) {  // Within 30 degrees of home path
        score += 30.0;
      }
    }

    // Penalty for crowded areas
    int ships_nearby = 0;
    for (unsigned int j = 0; j < 4; ++j) {
      CShip* other = team->GetShip(j);
      if (other && other != pShip && other->IsAlive()) {
        if (other->GetPos().DistTo(thing->GetPos()) < 100) {
          ships_nearby++;
        }
      }
    }
    score -= ships_nearby * 30.0;

    if (score > best_score) {
      best_score = score;
      best = thing;
    }
  }

  current_target = best;
}

void VortexCollector::NavigateToTarget() {
  if (!current_target) {
    return;
  }

  // Check if already on collision course
  double impact = pShip->DetectCollisionCourse(*current_target);
  if (impact != NO_COLLIDE && impact < 10.0) {
    // Just coast
    return;
  }

  // Smart navigation with fuel conservation
  double dist = pShip->GetPos().DistTo(current_target->GetPos());
  double time = sqrt(dist / 10.0) + 2.0;  // Estimate

  double angle = pShip->AngleToIntercept(*current_target, time);

  // Turn if needed
  if (fabs(angle) > 0.1) {
    pShip->SetOrder(O_TURN, angle * 1.5);  // Aggressive turning
  }

  // More aggressive thrust
  if (fabs(angle) < 0.3) {
    if (dist > 200) {
      pShip->SetOrder(O_THRUST, 20.0);  // Very fast approach
    } else if (dist > 50) {
      pShip->SetOrder(O_THRUST, 10.0);  // Faster moderate
    } else {
      pShip->SetOrder(O_THRUST, 4.0);  // Faster final approach
    }
  }
}

void VortexCollector::HandleEmergency() { AvoidCollisions(); }

void VortexCollector::MaintainDefenses() {
  double shields = pShip->GetAmount(S_SHIELD);
  double fuel = pShip->GetAmount(S_FUEL);

  // Keep 25 shield units if we have fuel to spare
  if (shields < 25.0 && fuel > 15.0) {
    double needed = 25.0 - shields;
    double available = fuel - 15.0;
    pShip->SetOrder(O_SHIELD, (needed < available) ? needed : available);
  }
}

bool VortexCollector::ShouldReturnHome() {
  // Return if cargo is 70% full (21 out of 30)
  if (pShip->GetAmount(S_CARGO) > 21.0) {
    return true;
  }

  // Return if fuel is getting low
  if (pShip->GetAmount(S_FUEL) < 10.0) {
    return true;
  }

  // Return if no vinyl left and have cargo
  VortexTeam* team = (VortexTeam*)pShip->GetTeam();
  if (team->total_vinyl < 1.0 && pShip->GetAmount(S_CARGO) > 1.0) {
    return true;
  }

  // Also return if we have decent cargo and are close to home
  if (pShip->GetAmount(S_CARGO) > 15.0) {
    double dist_home = pShip->GetPos().DistTo(team->GetStation()->GetPos());
    if (dist_home < 200.0) {
      return true;
    }
  }

  return false;
}

bool VortexCollector::IsInMyZone(CThing* thing) {
  // Divide world into 4 quadrants
  CCoord pos = thing->GetPos();

  switch (assigned_zone) {
    case 0:
      return (pos.fX < 0 && pos.fY < 0);  // Bottom-left
    case 1:
      return (pos.fX >= 0 && pos.fY < 0);  // Bottom-right
    case 2:
      return (pos.fX < 0 && pos.fY >= 0);  // Top-left
    case 3:
      return (pos.fX >= 0 && pos.fY >= 0);  // Top-right
  }
  return false;
}

void VortexCollector::AvoidCollisions() {
  CWorld* world = pShip->GetWorld();

  for (unsigned int i = world->UFirstIndex; i != BAD_INDEX;
       i = world->GetNextIndex(i)) {
    CThing* thing = world->GetThing(i);
    if (!thing || thing == pShip || thing == current_target) {
      continue;
    }

    double impact = pShip->DetectCollisionCourse(*thing);
    if (impact == NO_COLLIDE || impact > 3.0) {
      continue;
    }

    // Emergency evasion needed
    ThingKind kind = thing->GetKind();

    // Special handling for enemy stations with cargo
    if (kind == STATION && thing->GetTeam() != pShip->GetTeam()) {
      if (pShip->GetAmount(S_CARGO) > 0.01) {
        // Jettison cargo to prevent enemy scoring
        pShip->SetJettison(VINYL, pShip->GetAmount(S_CARGO));
        pShip->ResetOrders();
        pShip->SetOrder(O_THRUST, -20.0);
        return;
      }
    }

    // Dodge ships and large asteroids
    if (kind == SHIP ||
        (kind == ASTEROID && !pShip->AsteroidFits((CAsteroid*)thing))) {
      // Calculate evasion vector
      CTraj to_thing = pShip->GetPos().VectTo(thing->GetPos());
      double evade_angle = to_thing.theta + PI / 2;  // Perpendicular

      evade_angle -= pShip->GetOrient();
      if (evade_angle > PI) {
        evade_angle -= PI2;
      }
      if (evade_angle < -PI) {
        evade_angle += PI2;
      }

      pShip->ResetOrders();
      pShip->SetOrder(O_TURN, evade_angle);
      pShip->SetOrder(O_THRUST, -10.0);  // Back away
      return;
    }
  }
}

void VortexCollector::ConsiderCombat() {
  // Be more aggressive with combat
  if (pShip->GetAmount(S_FUEL) < 15.0) {
    return;
  }

  // Check what our laser would hit
  CThing* laser_target = pShip->LaserTarget();
  if (!laser_target) {
    return;
  }

  // Shoot enemy ships if close
  if (laser_target->GetKind() == SHIP &&
      laser_target->GetTeam() != pShip->GetTeam()) {
    double range = pShip->GetPos().DistTo(laser_target->GetPos());
    if (range < 200) {
      pShip->SetOrder(O_LASER, range + 50);
    }
  }

  // Break up large asteroids blocking our path
  if (laser_target->GetKind() == ASTEROID && laser_target == current_target) {
    CAsteroid* ast = (CAsteroid*)laser_target;
    if (!pShip->AsteroidFits(ast)) {
      double range = pShip->GetPos().DistTo(laser_target->GetPos());
      if (range < 150) {
        pShip->SetOrder(O_LASER, range + 50);
      }
    }
  }
}