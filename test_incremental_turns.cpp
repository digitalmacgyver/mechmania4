// test_incremental_turns.cpp
// Test whether one large turn or multiple small turns is cheaper
// This matters because the engine processes turns in sub-game-turns

#include <iostream>
#include <iomanip>
#include <cmath>

// Constants
const double g_ship_turn_energy_per_fuel_ton = 648000.0;
const double PI = 3.14159265358979323846;

// New physical turn cost formula (quadratic in angle)
double CalcTurnCostPhysical(double angle_radians, double ship_mass, double ship_radius) {
  const double T = 1.0;  // Turn duration in seconds
  const double T_squared = T * T;

  // Calculate rotational kinetic energy
  double KE_rot = ship_mass * ship_radius * ship_radius *
                  angle_radians * angle_radians / T_squared;

  // Convert energy to fuel cost
  return KE_rot / g_ship_turn_energy_per_fuel_ton;
}

void CompareIncrementalTurns(double total_angle_deg, int num_increments,
                             double ship_mass, double ship_radius) {
  double total_angle_rad = total_angle_deg * PI / 180.0;
  double increment_angle_rad = total_angle_rad / num_increments;
  double increment_angle_deg = total_angle_deg / num_increments;

  // Cost of one large turn
  double large_turn_cost = CalcTurnCostPhysical(total_angle_rad, ship_mass, ship_radius);

  // Cost of multiple small turns
  double small_turns_cost = 0.0;
  for (int i = 0; i < num_increments; i++) {
    small_turns_cost += CalcTurnCostPhysical(increment_angle_rad, ship_mass, ship_radius);
  }

  double ratio = small_turns_cost / large_turn_cost;
  double savings_percent = ((large_turn_cost - small_turns_cost) / large_turn_cost) * 100.0;

  std::cout << "\n" << std::string(80, '-') << std::endl;
  std::cout << "Turning " << total_angle_deg << "° total:" << std::endl;
  std::cout << std::string(80, '-') << std::endl;
  std::cout << "  One turn of " << total_angle_deg << "°:" << std::endl;
  std::cout << "    Cost: " << std::setw(10) << large_turn_cost << " tons" << std::endl;
  std::cout << "\n  " << num_increments << " turns of " << increment_angle_deg << "° each:" << std::endl;
  std::cout << "    Cost per turn: " << std::setw(10) << (small_turns_cost / num_increments) << " tons" << std::endl;
  std::cout << "    Total cost:    " << std::setw(10) << small_turns_cost << " tons" << std::endl;
  std::cout << "\n  Ratio (incremental/large): " << std::setw(6) << std::setprecision(4) << ratio << std::endl;

  if (ratio < 1.0) {
    std::cout << "  CHEAPER to use " << num_increments << " small turns: "
              << std::setw(6) << std::setprecision(2) << savings_percent << "% savings" << std::endl;
  } else {
    std::cout << "  MORE EXPENSIVE to use " << num_increments << " small turns: "
              << std::setw(6) << std::setprecision(2) << -savings_percent << "% more costly" << std::endl;
  }
}

void AnalyzeQuadraticScaling(int num_increments) {
  std::cout << "\nMathematical Analysis:" << std::endl;
  std::cout << "  Physical cost formula: fuel = M * R² * θ² / E" << std::endl;
  std::cout << "  For quadratic scaling: cost ∝ θ²" << std::endl;
  std::cout << "\n  One turn of angle θ:     cost = k * θ²" << std::endl;
  std::cout << "  " << num_increments << " turns of θ/" << num_increments << ":   cost = "
            << num_increments << " * k * (θ/" << num_increments << ")²" << std::endl;
  std::cout << "                            = " << num_increments << " * k * θ² / "
            << (num_increments * num_increments) << std::endl;
  std::cout << "                            = k * θ² / " << num_increments << std::endl;
  std::cout << "\n  Ratio: (k*θ²/" << num_increments << ") / (k*θ²) = 1/" << num_increments << std::endl;
  std::cout << "  Multiple small turns are " << num_increments << "x CHEAPER!" << std::endl;
}

int main() {
  std::cout << std::fixed << std::setprecision(6);

  std::cout << "\n=== Incremental Turn Cost Analysis ===" << std::endl;
  std::cout << "\nPhysical model uses quadratic scaling: cost ∝ θ²" << std::endl;
  std::cout << "This means small turns are MUCH more efficient than large turns." << std::endl;

  double ship_mass = 70.0;
  double ship_radius = 12.0;

  std::cout << "\nTest Configuration:" << std::endl;
  std::cout << "  Ship mass: " << ship_mass << " tons" << std::endl;
  std::cout << "  Ship radius: " << ship_radius << " units" << std::endl;

  // Test: 100° in one turn vs 5x 20° turns
  CompareIncrementalTurns(100.0, 5, ship_mass, ship_radius);

  // Test: 180° in one turn vs 5x 36° turns
  CompareIncrementalTurns(180.0, 5, ship_mass, ship_radius);

  // Test: 360° in one turn vs 5x 72° turns
  CompareIncrementalTurns(360.0, 5, ship_mass, ship_radius);

  // Test with different numbers of increments
  std::cout << "\n\n" << std::string(80, '=') << std::endl;
  std::cout << "Effect of Number of Increments (360° total turn):" << std::endl;
  std::cout << std::string(80, '=') << std::endl;

  for (int n : {2, 3, 5, 10, 20}) {
    CompareIncrementalTurns(360.0, n, ship_mass, ship_radius);
  }

  // Mathematical explanation
  std::cout << "\n\n" << std::string(80, '=') << std::endl;
  AnalyzeQuadraticScaling(5);
  std::cout << std::string(80, '=') << std::endl;

  std::cout << "\n=== Key Findings ===" << std::endl;
  std::cout << "1. Due to quadratic scaling (cost ∝ θ²), multiple small turns are CHEAPER" << std::endl;
  std::cout << "2. Splitting a turn into N parts makes it N times cheaper" << std::endl;
  std::cout << "3. The engine processes O_TURN orders over 5 sub-game-turns (dt=0.2)" << std::endl;
  std::cout << "4. This means actual fuel cost is ~1/5 of SetOrder return value!" << std::endl;
  std::cout << "5. SetOrder returns an UPPER BOUND on fuel costs" << std::endl;

  std::cout << "\n=== Test Complete ===" << std::endl;

  return 0;
}
