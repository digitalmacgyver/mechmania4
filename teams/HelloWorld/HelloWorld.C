/* HelloWorld.C - Implementation */
#include "HelloWorld.h"
#include "GameConstants.h"
#include <cmath>

// Factory function - required by game
CTeam* CTeam::CreateTeam() {
    return new HelloWorld();
}

void HelloWorld::Init() {
    // Set team identity
    SetTeamNumber(1);
    SetName("Hello World");

    // Configure each ship with balanced fuel/cargo
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);

        // 35 fuel, 25 cargo - balanced configuration
        ship->SetCapacity(S_FUEL, 35.0);
        ship->SetCapacity(S_CARGO, 25.0);

        // Assign simple AI
        ship->SetBrain(new SimpleCollector());
    }
}

void HelloWorld::Turn() {
    // Each ship's brain decides independently
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (ship && ship->GetBrain()) {
            ship->GetBrain()->Decide();
        }
    }
}

void SimpleCollector::Decide() {
    // Safety checks
    if (!pShip) return;

    // Clear previous orders
    pShip->ResetOrders();

    // Priority system:
    // 1. Maintain shields
    MaintainShields();

    // 2. Avoid imminent collisions
    AvoidCollision();

    // 3. Find and navigate to target
    FindTarget();
    NavigateToTarget();
}

void SimpleCollector::FindTarget() {
    CWorld* world = pShip->GetWorld();
    CTeam* team = pShip->GetTeam();

    // If carrying cargo, go home
    if (pShip->GetAmount(S_CARGO) > 5.0) {
        target = team->GetStation();
        return;
    }

    // Find nearest collectible asteroid
    CThing* best = nullptr;
    double best_dist = 99999;

    // Scan all objects
    for (unsigned int i = world->UFirstIndex;
         i != BAD_INDEX;
         i = world->GetNextIndex(i)) {

        CThing* thing = world->GetThing(i);
        if (!thing || !thing->IsAlive()) continue;

        // Only want asteroids
        if (thing->GetKind() != ASTEROID) continue;

        CAsteroid* ast = (CAsteroid*)thing;

        // Prefer fuel if low
        bool need_fuel = (pShip->GetAmount(S_FUEL) < 15.0);
        if (need_fuel && ast->GetMaterial() != URANIUM) continue;

        // Check if it fits in our hold
        if (!pShip->AsteroidFits(ast)) continue;

        // Find closest
        double dist = pShip->GetPos().DistTo(thing->GetPos());
        if (dist < best_dist) {
            best_dist = dist;
            best = thing;
        }
    }

    target = best;
}

void SimpleCollector::NavigateToTarget() {
    if (!target) return;

    // Check if we'll collide anyway
    double impact_time = pShip->DetectCollisionCourse(*target);
    if (impact_time != g_no_collide_sentinel && impact_time < 10.0) {
        // Already on collision course - just drift
        return;
    }

    // Calculate intercept angle
    double time_estimate = 5.0;  // Simple estimate
    double angle = pShip->AngleToIntercept(*target, time_estimate);

    // Turn toward target
    if (fabs(angle) > 0.1) {
        pShip->SetOrder(O_TURN, angle);
    }

    // Thrust if facing target
    if (fabs(angle) < 0.5) {
        pShip->SetOrder(O_THRUST, 10.0);
    }
}

void SimpleCollector::AvoidCollision() {
    CWorld* world = pShip->GetWorld();

    // Check all objects for collisions
    for (unsigned int i = world->UFirstIndex;
         i != BAD_INDEX;
         i = world->GetNextIndex(i)) {

        CThing* thing = world->GetThing(i);
        if (!thing || thing == target) continue;

        // Check collision time
        double impact = pShip->DetectCollisionCourse(*thing);
        if (impact == g_no_collide_sentinel || impact > 3.0) continue;

        // Emergency evasion!
        // If it's an enemy ship or large asteroid, dodge
        if (thing->GetKind() == SHIP ||
            (thing->GetKind() == ASTEROID &&
             !pShip->AsteroidFits((CAsteroid*)thing))) {

            // Reverse thrust
            pShip->SetOrder(O_THRUST, -15.0);
            return;  // Handle only one emergency at a time
        }
    }
}

void SimpleCollector::MaintainShields() {
    double shields = pShip->GetAmount(S_SHIELD);
    double fuel = pShip->GetAmount(S_FUEL);

    // Keep 20 units of shields if possible
    if (shields < 20.0 && fuel > 10.0) {
        double needed = 20.0 - shields;
        double available = fuel - 10.0;  // Keep reserve

        pShip->SetOrder(O_SHIELD, (needed < available) ? needed : available);
    }
}