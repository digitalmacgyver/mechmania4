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

// Initialize the global constants from the parser
// This should be called after argument parsing
void InitializeGameConstants(class ArgumentParser* parser);

#endif  // _GAME_CONSTANTS_H_MM4
