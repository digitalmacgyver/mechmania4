// test_turn_cost_calculations.cpp
// Unit test for turn cost physics calculations
// Compares legacy linear model with new quadratic physical model

#include <iostream>
#include <iomanip>
#include <cmath>

// Constants (from GameConstants.h/C)
const double g_ship_turn_full_rotations_per_fuel = 6.0;
const double g_ship_turn_energy_per_fuel_ton = 648000.0;
const double PI = 3.14159265358979323846;
const double PI2 = 2.0 * PI;

// Hull mass for legacy calculation (default ship configuration)
const double hull_mass = 40.0;  // Default ship spawn mass

// Legacy turn cost formula (from Ship.C lines 285-286)
double CalcTurnCostLegacy(double angle_radians, double ship_mass) {
  return fabs(angle_radians) * ship_mass /
         (g_ship_turn_full_rotations_per_fuel * PI2 * hull_mass);
}

// New physical turn cost formula (from Ship.C lines 18-47)
double CalcTurnCostPhysical(double angle_radians, double ship_mass, double ship_radius) {
  const double T = 1.0;  // Turn duration in seconds
  const double T_squared = T * T;

  // Calculate rotational kinetic energy
  double KE_rot = ship_mass * ship_radius * ship_radius *
                  angle_radians * angle_radians / T_squared;

  // Convert energy to fuel cost
  return KE_rot / g_ship_turn_energy_per_fuel_ton;
}

void PrintComparison(const char* test_name, double angle_degrees,
                     double ship_mass, double ship_radius) {
  double angle_radians = angle_degrees * PI / 180.0;

  double legacy_cost = CalcTurnCostLegacy(angle_radians, ship_mass);
  double physical_cost = CalcTurnCostPhysical(angle_radians, ship_mass, ship_radius);

  double ratio = (legacy_cost > 0) ? (physical_cost / legacy_cost) : 0.0;
  double percent_change = ((physical_cost - legacy_cost) / legacy_cost) * 100.0;

  std::cout << std::setw(25) << test_name << ": "
            << std::setw(8) << angle_degrees << "° | "
            << "Legacy: " << std::setw(10) << legacy_cost << " tons | "
            << "Physical: " << std::setw(10) << physical_cost << " tons | "
            << "Ratio: " << std::setw(6) << std::setprecision(2) << ratio << "x | "
            << std::setw(7) << std::setprecision(1) << std::showpos << percent_change << "%"
            << std::noshowpos << std::endl;
}

int main() {
  std::cout << std::fixed << std::setprecision(6);

  std::cout << "\n=== Turn Cost Physics Comparison ===" << std::endl;
  std::cout << "\nConstants:" << std::endl;
  std::cout << "  Energy per fuel ton: " << g_ship_turn_energy_per_fuel_ton << std::endl;
  std::cout << "  Legacy rotations/fuel: " << g_ship_turn_full_rotations_per_fuel << std::endl;
  std::cout << "  Hull mass: " << hull_mass << " tons" << std::endl;

  // Test with default ship configuration (40 tons empty + 30 tons fuel = 70 tons)
  double ship_mass = 70.0;   // 40 tons hull + 30 tons fuel
  double ship_radius = 12.0;  // Default ship size

  std::cout << "\nTest Configuration:" << std::endl;
  std::cout << "  Ship mass: " << ship_mass << " tons" << std::endl;
  std::cout << "  Ship radius: " << ship_radius << " units" << std::endl;

  std::cout << "\n" << std::string(120, '-') << std::endl;
  std::cout << std::setw(25) << "Test Case" << "  "
            << std::setw(8) << "Angle" << "   "
            << std::setw(24) << "Legacy Cost" << "  "
            << std::setw(26) << "Physical Cost" << "  "
            << std::setw(14) << "Physical/Legacy" << "  "
            << "Change" << std::endl;
  std::cout << std::string(120, '-') << std::endl;

  // Test various turn angles
  PrintComparison("Tiny turn", 1.0, ship_mass, ship_radius);
  PrintComparison("Very small turn", 5.7, ship_mass, ship_radius);
  PrintComparison("Small turn", 10.0, ship_mass, ship_radius);
  PrintComparison("Medium-small turn", 30.0, ship_mass, ship_radius);
  PrintComparison("Medium turn", 45.0, ship_mass, ship_radius);
  PrintComparison("Right angle", 90.0, ship_mass, ship_radius);
  PrintComparison("Obtuse turn", 135.0, ship_mass, ship_radius);
  PrintComparison("Half rotation", 180.0, ship_mass, ship_radius);
  PrintComparison("3/4 rotation", 270.0, ship_mass, ship_radius);
  PrintComparison("Full rotation", 360.0, ship_mass, ship_radius);

  std::cout << std::string(120, '-') << std::endl;

  // Find crossover point (where costs are equal)
  std::cout << "\nCrossover Analysis:" << std::endl;

  for (double angle_deg = 0.1; angle_deg <= 360.0; angle_deg += 0.1) {
    double angle_rad = angle_deg * PI / 180.0;
    double legacy = CalcTurnCostLegacy(angle_rad, ship_mass);
    double physical = CalcTurnCostPhysical(angle_rad, ship_mass, ship_radius);

    if (fabs(physical - legacy) < 0.00001) {  // Found crossover
      std::cout << "  Costs are equal at " << angle_deg << "° ("
                << (angle_rad) << " rad)" << std::endl;
      std::cout << "  Cost: " << legacy << " tons" << std::endl;
      break;
    }
  }

  // Calculate theoretical crossover from formulas
  // Legacy: fuel = |θ| * M / (6 * 2π * 40)
  // Physical: fuel = M * R² * θ² / 648000
  // Equal when: |θ| * M / (480π) = M * R² * θ² / 648000
  // Simplify: |θ| / (480π) = R² * θ² / 648000
  // Solve for θ: θ = 648000 / (480π * R²) = 1350 / (πR²)

  double theoretical_crossover_rad = 648000.0 / (g_ship_turn_full_rotations_per_fuel * PI2 * hull_mass * ship_radius * ship_radius);
  double theoretical_crossover_deg = theoretical_crossover_rad * 180.0 / PI;

  std::cout << "  Theoretical crossover: " << theoretical_crossover_deg << "° ("
            << theoretical_crossover_rad << " rad)" << std::endl;

  std::cout << "\nKey Insights:" << std::endl;
  std::cout << "  - Physical model is CHEAPER for small turns (quadratic scaling)" << std::endl;
  std::cout << "  - Physical model is MORE EXPENSIVE for large turns" << std::endl;
  std::cout << "  - Crossover occurs at ~" << theoretical_crossover_deg << "°" << std::endl;
  std::cout << "  - Small adjustments become much more affordable" << std::endl;
  std::cout << "  - Large rotations require significantly more energy" << std::endl;

  std::cout << "\n=== Test Complete ===" << std::endl;

  return 0;
}
