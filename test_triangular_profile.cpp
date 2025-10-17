// test_triangular_profile.cpp
// Verify triangular velocity profile implementation for turn physics

#include <iostream>
#include <iomanip>
#include <cmath>

// Constants (from GameConstants)
const double g_ship_turn_energy_per_fuel_ton = 648000.0;
const double g_game_turn_duration = 1.0;
const double PI = 3.14159265358979323846;

// Helper function to get angular velocity at a phase
double GetTriangularOmega(double phase, double omega_max) {
  if (phase <= 0.5) {
    return 2.0 * omega_max * phase;
  } else {
    return 2.0 * omega_max * (1.0 - phase);
  }
}

// Calculate SetOrder fuel cost (what the player sees)
double CalcSetOrderCost(double angle_radians, double ship_mass, double ship_radius) {
  const double T = 1.0;
  const double T_squared = T * T;
  double KE_peak = ship_mass * ship_radius * ship_radius * angle_radians * angle_radians / T_squared;
  return 2.0 * KE_peak / g_ship_turn_energy_per_fuel_ton;
}

// Simulate Drift over N sub-ticks and calculate total fuel
double SimulateDriftTotalCost(double angle_radians, double ship_mass, double ship_radius, int num_ticks) {
  double theta_total = fabs(angle_radians);
  double omega_max = 2.0 * theta_total / g_game_turn_duration;
  double I = 0.5 * ship_mass * ship_radius * ship_radius;

  double total_fuel = 0.0;

  for (int tick = 0; tick < num_ticks; tick++) {
    double phase_start = (double)tick / num_ticks;
    double phase_end = (double)(tick + 1) / num_ticks;

    double omega_start = GetTriangularOmega(phase_start, omega_max);
    double omega_end = GetTriangularOmega(phase_end, omega_max);

    double fuel_this_tick;

    // Check if this tick crosses the peak at phase=0.5
    if (phase_start < 0.5 && phase_end > 0.5) {
      // Tick spans the peak - calculate accel and decel separately
      double accel_energy = 0.5 * I * (omega_max * omega_max - omega_start * omega_start);
      double decel_energy = 0.5 * I * (omega_max * omega_max - omega_end * omega_end);
      fuel_this_tick = (accel_energy + decel_energy) / g_ship_turn_energy_per_fuel_ton;
    } else {
      // Tick doesn't cross peak - monotonic change
      fuel_this_tick = 0.5 * I * fabs(omega_end * omega_end - omega_start * omega_start)
                       / g_ship_turn_energy_per_fuel_ton;
    }

    total_fuel += fuel_this_tick;

    std::cout << "  Tick " << tick << " [" << std::setw(4) << phase_start << "→"
              << std::setw(4) << phase_end << "]: "
              << "ω=" << std::setw(7) << omega_start << "→" << std::setw(7) << omega_end << ", "
              << "fuel=" << std::setw(10) << fuel_this_tick;

    if (phase_start < 0.5 && phase_end > 0.5) {
      std::cout << " ★PEAK";
    }
    std::cout << std::endl;
  }

  return total_fuel;
}

int main() {
  std::cout << std::fixed << std::setprecision(6);

  std::cout << "=== Triangular Velocity Profile Verification ===" << std::endl;
  std::cout << "\nConstants: energy=" << g_ship_turn_energy_per_fuel_ton
            << ", duration=" << g_game_turn_duration << "s" << std::endl;

  double ship_mass = 70.0;
  double ship_radius = 12.0;
  std::cout << "Ship: mass=" << ship_mass << " tons, radius=" << ship_radius << " units" << std::endl;

  double test_angles_degrees[] = {10.0, 90.0, 180.0, 360.0};

  for (int i = 0; i < 4; i++) {
    double angle_deg = test_angles_degrees[i];
    double angle_rad = angle_deg * PI / 180.0;

    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "Test: " << angle_deg << "° (" << angle_rad << " rad)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    double setorder_cost = CalcSetOrderCost(angle_rad, ship_mass, ship_radius);
    std::cout << "SetOrder: " << setorder_cost << " tons" << std::endl;

    std::cout << "\nDrift (5 ticks):" << std::endl;
    double drift_total = SimulateDriftTotalCost(angle_rad, ship_mass, ship_radius, 5);

    std::cout << "Total: " << drift_total << " tons" << std::endl;

    double diff = fabs(setorder_cost - drift_total);
    double percent_error = (setorder_cost > 0) ? (diff / setorder_cost * 100.0) : 0.0;

    std::cout << "Error: " << percent_error << "% ";
    if (percent_error < 0.01) {
      std::cout << "✓ PASS" << std::endl;
    } else {
      std::cout << "✗ FAIL" << std::endl;
    }
  }

  return 0;
}