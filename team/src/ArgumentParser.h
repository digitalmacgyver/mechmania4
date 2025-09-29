/* ArgumentParser.h
   Modern argument parser with config file support for MechMania IV
   Supports feature flags and configuration files
*/

#ifndef _ARGUMENTPARSER_H_MM4
#define _ARGUMENTPARSER_H_MM4

#include <map>
#include <memory>
#include <string>
#include <vector>

class ArgumentParser {
 public:
  ArgumentParser();
  ~ArgumentParser();

  // Parse command line arguments
  bool Parse(int argc, char* argv[]);

  // Basic options (backward compatible with CParser)
  std::string hostname = "localhost";
  std::string gfxreg = "graphics.reg";
  int port = 2323;
  int numteams = 2;
  bool gfxflag = false;
  bool needhelp = false;
  bool retry = false;
  bool reconnect = false;

  // Team-specific options
  bool enableTeamLogging = false;
  std::string teamLogFile;      // Empty = use team default
  std::string teamParamsFile;   // Empty = use team default

  // Observer options
  bool verbose = false;          // Verbose output for observer

  // Config file
  std::string configFile;

  // Feature flags
  std::map<std::string, bool> features;

  // Game timing parameters
  double GetGameTurnDuration() const { return game_turn_duration_; }
  double GetPhysicsSimulationDt() const { return physics_simulation_dt_; }

  // Game physics parameters
  double GetMaxSpeed() const { return max_speed_; }
  double GetMaxThrustOrderMag() const { return max_thrust_order_mag_; }

  // Check if a feature should use new behavior
  bool UseNewFeature(const std::string& feature) const;

  // Print help message
  void PrintHelp() const;

  // Load configuration from file
  bool LoadConfig(const std::string& filename);

  // Save current configuration to file
  bool SaveConfig(const std::string& filename) const;

 private:
  // Game timing parameters (following Google C++ style with trailing underscore)
  double game_turn_duration_ = 1.0;   // In-game seconds per turn
  double physics_simulation_dt_ = 0.2; // Physics timestep in seconds

  // Game physics parameters
  double max_speed_ = 30.0;            // Maximum velocity magnitude
  double max_thrust_order_mag_ = 60.0; // Maximum thrust order magnitude

  // Apply a feature bundle
  void ApplyBundle(const std::string& bundle);

  // Initialize default features
  void InitializeFeatures();

  // Parse a JSON config file
  bool ParseConfigJson(const std::string& content);
};

#endif  // _ARGUMENTPARSER_H_MM4
