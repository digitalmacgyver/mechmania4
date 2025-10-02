/* GameConstants.h
 * Global game timing constants for MechMania IV
 * These control the game's time progression and physics simulation
 */

#ifndef _GAME_CONSTANTS_H_MM4
#define _GAME_CONSTANTS_H_MM4

// Global game timing constants
// These are initialized from command-line arguments or defaults

// The in-game seconds for how long a game turn lasts.
// Each game_turn_duration the world is published from the server to clients
// who issue orders for the next turn. Defaults to 1.0. Must be > 0.
extern double g_game_turn_duration;

// The in-game seconds for how often the physics model steps items in the world.
// During each step things advance on their velocity, can collide, etc.
// For some ship orders, such as Thrust and Turn, their effect is evenly
// divided over the number of physics steps that fall in a game turn.
// Defaults to 0.2. Must be <= game_turn_duration and > 0.
extern double g_physics_simulation_dt;

// Maximum velocity magnitude for ships and asteroids in units per second.
// When any object's velocity exceeds this value, it is clamped to this speed.
// Defaults to 30.0. Must be > 0.
extern double g_game_max_speed;

// Maximum thrust order magnitude for ships in units per second.
// Thrust orders exceeding this value will be clamped to this maximum.
// Defaults to 60.0. Must be > 0.
extern double g_game_max_thrust_order_mag;

// Global epsilon when comparing floating-point magnitudes against zero.
// Values in this simulation range up to ~1.5e4, so 1e-7 comfortably masks
// accumulated rounding noise while keeping real signals intact.
extern const double g_fp_error_epsilon;

// ---------------------------------------------------------------------------
// Game setup constants (world initialization)
// ---------------------------------------------------------------------------

// Number of ships assigned to each team when the server seeds the world.
extern unsigned int g_initial_team_ship_count;

// Number of vinyl asteroids spawned at world initialization.
extern unsigned int g_initial_vinyl_asteroid_count;

// Number of uranium asteroids spawned at world initialization.
extern unsigned int g_initial_uranium_asteroid_count;

// Default mass (in tons) for asteroids created during world setup.
extern double g_initial_asteroid_mass;

// ---------------------------------------------------------------------------
// Ship spawn and configuration constants
// ---------------------------------------------------------------------------

// Collision radius assigned to new ships when they are constructed.
extern double g_ship_spawn_size;

// Base hull mass (excluding cargo/fuel) assigned to new ships.
extern double g_ship_spawn_mass;

// Maximum distance from a station at which a ship counts as docked.
extern double g_ship_default_docking_distance;

// Combined cargo + fuel capacity limit for a ship.
extern double g_ship_total_stat_capacity;

// Default fuel capacity assigned when a ship is constructed.
extern double g_ship_default_fuel_capacity;

// Default cargo capacity assigned when a ship is constructed.
extern double g_ship_default_cargo_capacity;

// Default maximum shield strength applied to ships at construction time.
extern double g_ship_default_shield_capacity;

// Default starting shield amount assigned to ships on spawn.
extern double g_ship_default_shield_amount;

// ---------------------------------------------------------------------------
// Combat economy and maneuvering constants
// ---------------------------------------------------------------------------

// Conversion between requested laser range (world units) and fuel consumed.
extern double g_laser_range_per_fuel_unit;

// Factor used to convert remaining laser length into impact mass.
extern double g_laser_mass_scale_per_remaining_unit;

// Divisor converting incoming laser mass into effective damage on shields.
extern double g_laser_damage_mass_divisor;

// Number of full ship revolutions achievable per ton of fuel consumed.
extern double g_ship_turn_full_rotations_per_fuel;

// ---------------------------------------------------------------------------
// Station defaults
// ---------------------------------------------------------------------------

// Collision radius assigned to stations at creation time.
extern double g_station_spawn_size;

// Effective mass assigned to stations so they behave as immovable objects.
extern double g_station_spawn_mass;

// Spin rate applied to stations for aesthetic animation.
extern double g_station_spawn_spin_rate;

// ---------------------------------------------------------------------------
// Asteroid generation and fragmentation constants
// ---------------------------------------------------------------------------

// Offset applied when randomly generating asteroid mass from RNG [0,1).
extern double g_asteroid_random_mass_offset;

// Range applied when randomly generating asteroid mass from RNG [0,1).
extern double g_asteroid_random_mass_range;

// Mass threshold where asteroids use the "large" sprite set.
extern double g_asteroid_large_mass_threshold;

// Mass threshold where asteroids use the "medium" sprite set.
extern double g_asteroid_medium_mass_threshold;

// Base collision radius assigned before mass-based scaling is applied.
extern double g_asteroid_size_base;

// Scaling multiplier applied to sqrt(mass) when setting asteroid radius.
extern double g_asteroid_size_mass_scale;

// Number of child asteroids spawned when a large asteroid fractures.
extern unsigned int g_asteroid_split_child_count;

// Divisor applied when converting laser mass into asteroid velocity impulse.
extern double g_asteroid_laser_impulse_divisor;

// Minimum laser mass required to vaporize (rather than deflect) an asteroid.
extern double g_asteroid_laser_shatter_threshold;

// ---------------------------------------------------------------------------
// Thing (base object) constraints
// ---------------------------------------------------------------------------

// Minimum allowable object mass to prevent degenerate physics behavior.
extern double g_thing_minmass;

// Minimum allowable object size to keep collision bounds well-defined.
extern double g_thing_minsize;

// Sentinel used to indicate no pending collision in time-to-impact calculations.
extern double g_no_collide_sentinel;

// Sentinel used to indicate no damage direction has been recorded.
extern double g_no_damage_sentinel;

// Initialize the global constants from the parser
// This should be called after argument parsing
void InitializeGameConstants(class ArgumentParser* parser);

#endif  // _GAME_CONSTANTS_H_MM4
