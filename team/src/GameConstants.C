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

void InitializeGameConstants(ArgumentParser* parser) {
  if (parser) {
    g_game_turn_duration = parser->GetGameTurnDuration();
    g_physics_simulation_dt = parser->GetPhysicsSimulationDt();
    g_game_max_speed = parser->GetMaxSpeed();
    g_game_max_thrust_order_mag = parser->GetMaxThrustOrderMag();
  }
}
