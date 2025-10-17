/* TurnTest.C
 * Implementation of turn physics test team
 * Tests different turn angles to verify fuel consumption
 */

#include "TurnTest.h"
#include "Ship.h"
#include <cmath>
#include <iostream>
#include <iomanip>

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() {
  return new TurnTest;
}

TurnTest::TurnTest() : CTeam(), turn_count(0) {
}

TurnTest::~TurnTest() {
}

void TurnTest::Init() {
  // Initialize ships with default configuration
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 30.0);   // 30 tons fuel
    GetShip(i)->SetCapacity(S_CARGO, 30.0);  // 30 tons cargo
  }
}

void TurnTest::Turn() {
  turn_count++;

  // Undock all ships on turn 1 by issuing thrust orders
  if (turn_count == 1) {
    std::cout << "\n=== Undocking ships ===" << std::endl;
    for (unsigned int i = 0; i < GetShipCount(); i++) {
      CShip* ship = GetShip(i);
      if (ship && ship->IsDocked()) {
        // Issue a small thrust to undock (1.0 units/s)
        ship->SetOrder(O_THRUST, 1.0);
        std::cout << "Undocking ship " << i << " with thrust" << std::endl;
      }
    }
    return;
  }

  // Only issue turn orders on second turn (after undocking)
  if (turn_count != 2) {
    return;
  }

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "\n=== Turn Physics Test (Turn " << turn_count << ") ===" << std::endl;

  // Test different turn magnitudes on different ships
  for (unsigned int i = 0; i < GetShipCount(); i++) {
    CShip* ship = GetShip(i);
    if (!ship) continue;

    double turn_angle = 0.0;
    const char* description = "";

    switch(i % 4) {
      case 0:
        // Small turn: 10 degrees = 0.1745 radians
        turn_angle = 0.1745;
        description = "Small turn (10°, 0.1745 rad)";
        break;
      case 1:
        // Medium turn: 90 degrees = π/2 radians
        turn_angle = M_PI / 2.0;
        description = "Medium turn (90°, π/2 rad)";
        break;
      case 2:
        // Large turn: 180 degrees = π radians
        turn_angle = M_PI;
        description = "Large turn (180°, π rad)";
        break;
      case 3:
        // Full rotation: 360 degrees = 2π radians
        turn_angle = 2.0 * M_PI;
        description = "Full rotation (360°, 2π rad)";
        break;
    }

    // Get fuel before turn
    double fuel_before = ship->GetAmount(S_FUEL);

    // Issue the turn order
    double fuel_consumed = ship->SetOrder(O_TURN, turn_angle);

    // Get fuel after turn (should match fuel_consumed)
    double fuel_after = ship->GetAmount(S_FUEL);
    double actual_consumed = fuel_before - fuel_after;

    std::cout << "\nShip " << i << " (" << ship->GetName() << "):" << std::endl;
    std::cout << "  Test: " << description << std::endl;
    std::cout << "  Mass: " << ship->GetMass() << " tons" << std::endl;
    std::cout << "  Size: " << ship->GetSize() << " units" << std::endl;
    std::cout << "  Fuel before: " << fuel_before << " tons" << std::endl;
    std::cout << "  Fuel after: " << fuel_after << " tons" << std::endl;
    std::cout << "  Fuel consumed (SetOrder): " << fuel_consumed << " tons" << std::endl;
    std::cout << "  Fuel consumed (actual): " << actual_consumed << " tons" << std::endl;
  }

  std::cout << "\n=== Test Complete ===" << std::endl;
}

void TurnTest::SelectShipNames() {
  // Use default ship names
}

void TurnTest::SelectTeamName() {
  SetName("TurnTest");
}
