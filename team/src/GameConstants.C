/* GameConstants.C
 * Implementation of global game timing constants
 */

#include "GameConstants.h"
#include "ArgumentParser.h"

// Define the global constants with default values
double g_game_turn_duration = 1.0;
double g_physics_simulation_dt = 0.2;
double g_game_max_speed = 30.0;
double g_game_max_thrust_order_mag = 60.0;
const double g_fp_error_epsilon = 1e-7;

// Game setup defaults
unsigned int g_initial_team_ship_count = 4;
unsigned int g_initial_vinyl_asteroid_count = 5;
unsigned int g_initial_uranium_asteroid_count = 5;
double g_initial_asteroid_mass = 40.0;

// Ship spawn defaults
double g_ship_spawn_size = 12.0;
double g_ship_spawn_mass = 40.0;
double g_ship_default_docking_distance = 30.0;
double g_ship_total_stat_capacity = 60.0;
double g_ship_default_fuel_capacity = 30.0;
double g_ship_default_cargo_capacity =
    g_ship_total_stat_capacity - g_ship_default_fuel_capacity;
double g_ship_default_shield_capacity = 8000.0;
double g_ship_default_shield_amount = 30.0;
double g_ship_collision_separation_clearance = 3.0;

// Combat economics and maneuvering
double g_laser_range_per_fuel_unit = 50.0;
double g_laser_mass_scale_per_remaining_unit = 30.0;
double g_laser_damage_mass_divisor = 1000.0;
double g_ship_turn_full_rotations_per_fuel = 6.0;

// Energy available from 1 ton of fuel, derived from thrust physics:
// 1 ton accelerates 40-ton ship to 180 units/s (6 * max_speed)
// KE = 0.5 * 40 * 180^2 = 648,000
double g_ship_turn_energy_per_fuel_ton = 648000.0;

// Station defaults
double g_station_spawn_size = 30.0;
double g_station_spawn_mass = 99999.9;
double g_station_spawn_spin_rate = 0.9;

// Asteroid generation defaults
double g_asteroid_random_mass_offset = 1.0;
double g_asteroid_random_mass_range = 10.0;
double g_asteroid_large_mass_threshold = 40.0;
double g_asteroid_medium_mass_threshold = 10.0;
double g_asteroid_size_base = 3.0;
double g_asteroid_size_mass_scale = 1.6;
unsigned int g_asteroid_split_child_count = 3;
double g_asteroid_laser_impulse_divisor = 3.0;
double g_asteroid_laser_shatter_threshold = 1000.0;

// Thing (base object) constraints
double g_thing_minmass = 3.0;
double g_thing_minsize = 1.0;
double g_no_collide_sentinel = -1.0;
double g_no_damage_sentinel = -123.45;

void InitializeGameConstants(ArgumentParser* parser) {
  if (parser) {
    g_game_turn_duration = parser->GetGameTurnDuration();
    g_physics_simulation_dt = parser->GetPhysicsSimulationDt();
    g_game_max_speed = parser->GetMaxSpeed();
    g_game_max_thrust_order_mag = parser->GetMaxThrustOrderMag();
  }

  // Keep cargo capacity consistent with total/fuel defaults.
  g_ship_default_cargo_capacity =
      g_ship_total_stat_capacity - g_ship_default_fuel_capacity;
}
