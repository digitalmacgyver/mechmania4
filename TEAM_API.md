# MechMania IV Team API Reference

This document provides the programming interface for implementing team strategies in MechMania IV. For game rules and mechanics, see CONTEST_RULES.md.

## Table of Contents
1. [Team Setup](#team-setup)
2. [Ship Configuration](#ship-configuration)
3. [Ship Orders](#ship-orders)
4. [World Information](#world-information)
5. [Navigation and Physics](#navigation-and-physics)
6. [Combat and Defense](#combat-and-defense)
7. [Resource Collection](#resource-collection)
8. [Utility Functions](#utility-functions)
9. [Hello World Example](#hello-world-example)

## Team Setup

### Creating Your Team Class

Your team must inherit from `CTeam` and implement required methods:

```cpp
class MyTeam : public CTeam {
public:
    MyTeam();
    ~MyTeam();

    void Init();  // Called once at game start
    void Turn();  // Called every simulation step
};

// Required factory function
CTeam* CTeam::CreateTeam() {
    return new MyTeam();
}
```

### Team Initialization

```cpp
void MyTeam::Init() {
    // Set team identity (1-16)
    SetTeamNumber(1);

    // Name your team and station
    SetName("Team Name");
    GetStation()->SetName("Base Name");

    // Name your ships
    GetShip(0)->SetName("Ship Alpha");
    GetShip(1)->SetName("Ship Beta");
    GetShip(2)->SetName("Ship Gamma");
    GetShip(3)->SetName("Ship Delta");

    // Configure ships (see Ship Configuration)
}
```

## Ship Configuration

### Capacity Allocation

Ships have 60 tons total capacity split between fuel and cargo:

```cpp
// Default: 30 fuel, 30 cargo
// Custom example: High fuel configuration
GetShip(0)->SetCapacity(S_FUEL, 45.0);   // 45 tons fuel
GetShip(0)->SetCapacity(S_CARGO, 15.0);  // 15 tons cargo (automatic)

// High cargo configuration
GetShip(1)->SetCapacity(S_CARGO, 40.0);  // 40 tons cargo
GetShip(1)->SetCapacity(S_FUEL, 20.0);   // 20 tons fuel (automatic)
```

### AI Brain Assignment

Ships need a brain (AI controller) to make decisions:

```cpp
// Assign AI to ship
GetShip(0)->SetBrain(new MyShipAI());

// Create custom AI by inheriting CBrain
class MyShipAI : public CBrain {
public:
    void Decide();  // Called each turn for this ship
};

void MyShipAI::Decide() {
    // Access ship via pShip member variable
    pShip->SetOrder(O_THRUST, 10.0);
}
```

## Ship Orders

### Order Types

Ships can issue these orders each turn (all can be combined):

```cpp
// Movement orders
pShip->SetOrder(O_THRUST, value);   // -30 to +30 units/sec
pShip->SetOrder(O_TURN, radians);   // -2π to +2π radians

// Combat orders
pShip->SetOrder(O_LASER, distance); // Fire laser beam
pShip->SetOrder(O_SHIELD, amount);  // Add to shields

// Resource management
pShip->SetJettison(VINYL, tons);    // Eject cargo
pShip->SetJettison(URANIUM, tons);  // Eject fuel
```

### Order Examples

```cpp
// Accelerate forward
pShip->SetOrder(O_THRUST, 10.0);

// Reverse thrust
pShip->SetOrder(O_THRUST, -10.0);

// Turn 90 degrees left
pShip->SetOrder(O_TURN, PI/2);

// Fire 200-mile laser
pShip->SetOrder(O_LASER, 200.0);

// Add 5 shield units
pShip->SetOrder(O_SHIELD, 5.0);

// Dump 3 tons of cargo
pShip->SetJettison(VINYL, 3.0);
```

### Fuel Costs

Each order returns fuel consumed:

```cpp
double fuel_used = pShip->SetOrder(O_THRUST, 10.0);
// Check if we had enough fuel
if (fuel_used < 10.0) {
    // Insufficient fuel - order was reduced
}
```

## World Information

### Accessing World Objects

```cpp
CWorld* world = GetWorld();

// Iterate through all objects
for (UINT i = world->UFirstIndex;
     i != BAD_INDEX;
     i = world->GetNextIndex(i)) {

    CThing* thing = world->GetThing(i);
    if (!thing || !thing->IsAlive()) continue;

    // Check object type
    ThingKind kind = thing->GetKind();
    switch (kind) {
        case SHIP:     // Enemy or friendly ship
        case STATION:  // Station
        case ASTEROID: // Resource asteroid
        case GENTHING: // Generic object (laser beam)
    }
}
```

### Ship Information

```cpp
// Your ship's resources
double fuel = pShip->GetAmount(S_FUEL);
double cargo = pShip->GetAmount(S_CARGO);
double shields = pShip->GetAmount(S_SHIELD);

// Capacities
double max_fuel = pShip->GetCapacity(S_FUEL);
double max_cargo = pShip->GetCapacity(S_CARGO);

// Status
bool docked = pShip->IsDocked();
UINT ship_num = pShip->GetShipNumber();  // 0-3

// Physics
CCoord position = pShip->GetPos();       // x,y coordinates
CTraj velocity = pShip->GetVelocity();   // rho,theta polar
double orientation = pShip->GetOrient(); // radians
double mass = pShip->GetMass();         // includes cargo/fuel
```

### Team Information

```cpp
// Your team
CTeam* team = pShip->GetTeam();
double score = team->GetScore();        // Vinyl at station
CStation* station = team->GetStation();
CShip* teammate = team->GetShip(2);     // Get ship by index

// Check if object belongs to your team
if (thing->GetTeam() == team) {
    // Friendly object
}
```

### Asteroid Information

```cpp
CAsteroid* asteroid = (CAsteroid*)thing;

// Material type
AsteroidKind material = asteroid->GetMaterial();
if (material == VINYL) {
    // Yellow - goes in cargo hold
} else if (material == URANIUM) {
    // Green - goes in fuel tank
}

// Size
double mass = asteroid->GetMass();

// Check if it fits
if (pShip->AsteroidFits(asteroid)) {
    // Will be collected on collision
}
```

## Navigation and Physics

### Position and Distance

```cpp
// Get positions
CCoord my_pos = pShip->GetPos();
CCoord target_pos = thing->GetPos();

// Calculate distance
double distance = my_pos.DistTo(target_pos);

// Get vector to target
CTraj vector_to_target = my_pos.VectTo(target_pos);
// vector_to_target.rho = distance
// vector_to_target.theta = angle to target
```

### Collision Detection

```cpp
// Check if on collision course
double time_to_impact = pShip->DetectCollisionCourse(*thing);
if (time_to_impact == NO_COLLIDE) {
    // No collision predicted
} else if (time_to_impact < 5.0) {
    // Collision in less than 5 seconds!
}
```

### Interception Calculation

```cpp
// Calculate angle to intercept moving target
double intercept_time = 10.0;  // Estimated time
double angle = pShip->AngleToIntercept(*target, intercept_time);

// Turn toward intercept point
pShip->SetOrder(O_TURN, angle);
```

### Predictive Movement

```cpp
// Predict future positions
CCoord future_pos = thing->PredictPosition(5.0);  // 5 seconds ahead

// Get relative velocity
CTraj rel_vel = pShip->RelativeVelocity(*thing);
```

## Combat and Defense

### Firing Lasers

```cpp
// Find what laser will hit
CThing* target = pShip->LaserTarget();
if (target && target->GetTeam() != team) {
    // Calculate required power
    double range = pShip->GetPos().DistTo(target->GetPos());

    // Fire with extra power to ensure hit
    pShip->SetOrder(O_LASER, range + 100.0);
}
```

### Shield Management

```cpp
// Check shields
double shields = pShip->GetAmount(S_SHIELD);

// Maintain minimum shields
if (shields < 30.0) {
    double needed = 30.0 - shields;
    double fuel = pShip->GetAmount(S_FUEL);

    // Add shields (limited by available fuel)
    pShip->SetOrder(O_SHIELD, min(needed, fuel));
}
```

### Damage Calculation

```cpp
// Collision damage = relative_momentum / 1000
// Laser damage = beam_length / 1000

// Check if ship will survive collision
CTraj rel_momentum = pShip->RelativeMomentum(*thing);
double damage = rel_momentum.rho / 1000.0;
if (damage > shields) {
    // Ship will be destroyed!
}
```

## Resource Collection

### Finding Resources

```cpp
// Find nearest vinyl asteroid
CThing* best_target = nullptr;
double best_dist = 999999;

for (UINT i = world->UFirstIndex; i != BAD_INDEX; i = world->GetNextIndex(i)) {
    CThing* thing = world->GetThing(i);
    if (!thing || thing->GetKind() != ASTEROID) continue;

    CAsteroid* ast = (CAsteroid*)thing;

    // Check material type
    if (ast->GetMaterial() != VINYL) continue;

    // Check if it fits
    if (!pShip->AsteroidFits(ast)) continue;

    double dist = pShip->GetPos().DistTo(ast->GetPos());
    if (dist < best_dist) {
        best_dist = dist;
        best_target = ast;
    }
}
```

### Station Docking

```cpp
// Check if carrying cargo
if (pShip->GetAmount(S_CARGO) > 0) {
    // Head to station
    CStation* station = team->GetStation();

    // Navigate to station
    // Collision with station automatically:
    // - Docks ship
    // - Deposits all cargo
    // - Enables free thrust/turn (no fuel cost while docked)
}

// Departing station
if (pShip->IsDocked()) {
    // Choose departure angle
    double angle = ship_number * PI/2;  // Spread out
    pShip->SetOrder(O_TURN, angle - pShip->GetOrient());

    // Thrust away (costs no fuel while docked)
    pShip->SetOrder(O_THRUST, 20.0);
}
```

## Utility Functions

### Message Logging

```cpp
// Print team messages (visible to observer)
char msg[256];
sprintf(msg, "Ship %s collecting asteroid at (%.0f,%.0f)\n",
        pShip->GetName(), target->GetPos().fX, target->GetPos().fY);
strcat(team->MsgText, msg);  // Append to team message buffer
```

### Coordinate Wrapping

World coordinates wrap at edges (-512 to 512):

```cpp
// Positions automatically wrap
CCoord pos(600, 300);  // Automatically wraps to (88, 300)

// Check for wrapped distance
double dist1 = pos1.DistTo(pos2);  // Direct distance
// DistTo automatically handles wrapping for shortest path
```

### Constants

```cpp
// World boundaries
const double fWXMin = -512.0, fWXMax = 512.0;
const double fWYMin = -512.0, fWYMax = 512.0;

// Physics
const double maxspeed = 30.0;     // Maximum ship velocity
const double PI = 3.14159265359;
const double PI2 = 2 * PI;

// Special values
const double NO_COLLIDE = -1.0;   // No collision detected
const UINT BAD_INDEX = -1;        // Invalid object index
```

## Hello World Example

Here's a complete, minimal team implementation with clear documentation:

```cpp
/* HelloWorld.h - Minimal team implementation */
#ifndef HELLO_WORLD_H
#define HELLO_WORLD_H

#include "Team.h"
#include "Brain.h"

// Main team class
class HelloWorld : public CTeam {
public:
    HelloWorld() {}
    ~HelloWorld() {}

    void Init();  // Setup ships
    void Turn();  // Run each ship's AI
};

// Simple collector AI
class SimpleCollector : public CBrain {
public:
    CThing* target;  // Current target

    SimpleCollector() : target(nullptr) {}
    void Decide();   // Make decisions

private:
    void FindTarget();      // Select asteroid or station
    void NavigateToTarget(); // Move toward target
    void AvoidCollision();   // Emergency collision avoidance
    void MaintainShields();  // Keep shields charged
};

#endif

/* HelloWorld.cpp - Implementation */
#include "HelloWorld.h"

// Factory function - required by game
CTeam* CTeam::CreateTeam() {
    return new HelloWorld();
}

void HelloWorld::Init() {
    // Set team identity
    SetTeamNumber(1);
    SetName("Hello World");

    // Configure each ship with balanced fuel/cargo
    for (UINT i = 0; i < GetShipCount(); i++) {
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
    for (UINT i = 0; i < GetShipCount(); i++) {
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
    for (UINT i = world->UFirstIndex;
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
    if (impact_time != NO_COLLIDE && impact_time < 10.0) {
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
    for (UINT i = world->UFirstIndex;
         i != BAD_INDEX;
         i = world->GetNextIndex(i)) {

        CThing* thing = world->GetThing(i);
        if (!thing || thing == target) continue;

        // Check collision time
        double impact = pShip->DetectCollisionCourse(*thing);
        if (impact == NO_COLLIDE || impact > 3.0) continue;

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

        pShip->SetOrder(O_SHIELD, min(needed, available));
    }
}
```

This Hello World example demonstrates:
- Clean, modular design with separate concerns
- Basic resource collection strategy
- Collision avoidance
- Shield management
- Clear documentation
- Minimal but functional implementation

Teams can extend this foundation by adding:
- Combat capabilities (laser targeting)
- Team coordination
- Advanced pathfinding
- Strategic decision making
- Enemy tracking and avoidance