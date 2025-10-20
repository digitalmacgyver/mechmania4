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