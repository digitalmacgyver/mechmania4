/* ArgumentParser.C
   Modern argument parser implementation for MechMania IV
*/

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>

#include "ArgumentParser.h"
#include "GameConstants.h"
#include "third_party/cxxopts.hpp"

// Helper function to calculate worst-case asteroid count
// Returns the maximum number of asteroids that could exist simultaneously
// assuming all asteroids fragment to second-to-last generation, then
// all simultaneously shatter to final generation on the same turn.
static unsigned int CalculateMaxAsteroidCount(unsigned int initial_count,
                                               double mass,
                                               double min_mass) {
  if (mass < min_mass) {
    return 0;  // Can't create asteroids below minimum mass
  }

  // Calculate max generation N where mass/3^N >= min_mass
  unsigned int max_gen = 0;
  double current_mass = mass;
  while (current_mass / 3.0 >= min_mass) {
    current_mass /= 3.0;
    max_gen++;
  }

  if (max_gen == 0) {
    // No fragmentation possible - asteroids too small to split
    return initial_count;
  }

  // Worst case: all at generation (N-1) simultaneously fragment to generation N
  // Count = initial × (3^(N-1) + 3^N) = initial × 3^(N-1) × 4
  unsigned int multiplier = 4;
  for (unsigned int i = 1; i < max_gen; i++) {
    multiplier *= 3;
  }

  return initial_count * multiplier;
}

ArgumentParser::ArgumentParser() { InitializeFeatures(); }

ArgumentParser::~ArgumentParser() {}

void ArgumentParser::InitializeFeatures() {
  // Initialize all features with their default states
  // true = new behavior is default, false = old behavior is default

  // Physics features
  features["collision-detection"] = true;  // New collision detection is default
  features["velocity-limits"] = true;      // New velocity/acceleration limits is default
  features["asteroid-eat-damage"] = true;  // New: no damage when eating asteroids that fit
  features["physics"] = true;              // New: correct collision physics and momentum conservation
  features["collision-handling"] = true;   // New: deterministic snapshot/command collision pipeline (fixes multi-hit bugs)
  features["cargo-calc"] = true;           // New: tolerant cargo capacity checks for asteroid ingestion

  // Security features
  features["laser-exploit"] = false;       // New: TOCTOU vulnerability patched (validate before firing)

  // Docking features
  features["docking"] = true;              // New: fixed safe launch distance (48 units)

  // Laser range check
  features["rangecheck-bug"] = false;      // New: fixed floating-point range check (was: dLasRng > dLasPwr)

  // Announcer features
  features["announcer-velocity-clamping"] = false;  // Disabled by default

  // Initial orientation fix
  features["initial-orientation"] = true;  // New: ships face toward map center (balanced)

  // Facing detection
  features["facing-detection"] = true;  // New: toroidal shortest-path aware IsFacing

  // Add more features as they are implemented
}

bool ArgumentParser::Parse(int argc, char* argv[]) {
  try {
    cxxopts::Options options("mm4serv/mm4obs",
                             "MechMania IV: The Vinyl Frontier");

    options.add_options()("p,port", "Server port",
                          cxxopts::value<int>()->default_value("2323"))(
        "h,hostname", "Server hostname",
        cxxopts::value<std::string>()->default_value("localhost"))(
        "g,gfxreg", "Graphics registry file",
        cxxopts::value<std::string>()->default_value("graphics.reg"))(
        "assets-root", "Override base directory for audio assets",
        cxxopts::value<std::string>())(
        "T,numteams", "Number of teams",
        cxxopts::value<int>()->default_value("2"))("G,graphics",
                                                   "Enable full graphics mode")(
        "R,reconnect", "Attempt reconnect after disconnect")(
        "config", "Configuration file", cxxopts::value<std::string>())(
        "log", "Enable team logging")(
        "log-file", "Path to team log file",
         cxxopts::value<std::string>())(
        "params", "Path to team parameter file",
         cxxopts::value<std::string>())(
        "test-file", "Path to test moves file (for testteam), use '-' for stdin",
         cxxopts::value<std::string>())(
        "verbose", "Enable verbose output")(
        "enable-audio-test-ping",
         "Enable manual audio diagnostics ping (requires verbose for logs)")(
        "help", "Show help");

    // Feature flags
    options.add_options("Features")("legacy-collision-detection",
                                    "Use legacy collision detection")(
        "legacy-velocity-limits", "Use legacy velocity and acceleration limits")(
        "legacy-asteroid-eat-damage", "Ships take damage when eating asteroids (legacy behavior)")(
        "legacy-physics", "Use legacy collision physics and momentum conservation")(
        "legacy-collision-handling", "Use legacy collision processing (allows multi-hit bugs)")(
        "legacy-laser-exploit", "Enable TOCTOU laser exploit (fire before validation)")(
        "legacy-docking", "Use legacy docking (dDockDist+5, can get stuck re-docking)")(
        "legacy-rangecheck-bug", "Use buggy laser range check (floating-point comparison dLasRng > dLasPwr)")(
        "legacy-initial-orientation", "Use legacy initial orientation (all ships face east, asymmetric)")(
        "legacy-facing-detection", "Use legacy IsFacing (ignores toroidal shortest path)")(
        "legacy-cargo-calc", "Use strict cargo capacity check when collecting asteroids (legacy behavior)")(
        "announcer-velocity-clamping", "Enable velocity clamping announcements");

    // Feature bundles
    options.add_options("Bundles")("improved-physics",
                                   "Enable all new physics features")(
        "legacy-mode", "Use all old/legacy features");

    // Game timing options
    options.add_options("Timing")(
        "game-turn-duration",
        "Game turn duration in seconds (default: 1.0)",
        cxxopts::value<double>()->default_value("1.0"))(
        "physics-dt",
        "Physics simulation timestep in seconds (default: 0.2)",
        cxxopts::value<double>()->default_value("0.2"))(
        "max-turns",
        "Maximum number of turns (default: 300)",
        cxxopts::value<unsigned int>()->default_value("300"));

    // Game physics options
    options.add_options("Physics")(
        "max-speed",
        "Maximum velocity magnitude (default: 30.0)",
        cxxopts::value<double>()->default_value("30.0"))(
        "max-thrust-order-mag",
        "Maximum thrust order magnitude (default: 60.0)",
        cxxopts::value<double>()->default_value("60.0"));

    // World setup options
    options.add_options("World Setup")(
        "vinyl-num",
        "Number of initial vinyl asteroids (default: 5)",
        cxxopts::value<unsigned int>()->default_value("5"))(
        "vinyl-mass",
        "Mass of each vinyl asteroid in tons (default: 40.0)",
        cxxopts::value<double>()->default_value("40.0"))(
        "uranium-num",
        "Number of initial uranium asteroids (default: 5)",
        cxxopts::value<unsigned int>()->default_value("5"))(
        "uranium-mass",
        "Mass of each uranium asteroid in tons (default: 40.0)",
        cxxopts::value<double>()->default_value("40.0"));

    auto result = options.parse(argc, argv);

    // Basic options
    port = result["port"].as<int>();
    hostname = result["hostname"].as<std::string>();
    gfxreg = result["gfxreg"].as<std::string>();
    if (result.count("assets-root")) {
      assetsRoot = result["assets-root"].as<std::string>();
    }
    numteams = result["numteams"].as<int>();
    gfxflag = result.count("graphics") > 0;
    reconnect = retry = result.count("reconnect") > 0;
    needhelp = result.count("help") > 0;

    if (result.count("log")) {
      enableTeamLogging = true;
    }
    if (result.count("log-file")) {
      teamLogFile = result["log-file"].as<std::string>();
    }
    if (result.count("params")) {
      teamParamsFile = result["params"].as<std::string>();
    }
    if (result.count("test-file")) {
      testMovesFile = result["test-file"].as<std::string>();
    }

    verbose = result.count("verbose") > 0;
    enableAudioTestPing = result.count("enable-audio-test-ping") > 0;

    // Parse timing options
    if (result.count("game-turn-duration")) {
      game_turn_duration_ = result["game-turn-duration"].as<double>();
      if (game_turn_duration_ <= 0.0) {
        std::cerr << "Error: game-turn-duration must be > 0" << std::endl;
        return false;
      }
    }
    if (result.count("physics-dt")) {
      physics_simulation_dt_ = result["physics-dt"].as<double>();
      if (physics_simulation_dt_ <= 0.0) {
        std::cerr << "Error: physics-dt must be > 0" << std::endl;
        return false;
      }
      if (physics_simulation_dt_ > game_turn_duration_) {
        std::cerr << "Error: physics-dt must be <= game-turn-duration" << std::endl;
        return false;
      }
    }
    if (result.count("max-turns")) {
      max_turns_ = result["max-turns"].as<unsigned int>();
      if (max_turns_ == 0) {
        std::cerr << "Error: max-turns must be > 0" << std::endl;
        return false;
      }
    }

    // Parse physics options
    if (result.count("max-speed")) {
      max_speed_ = result["max-speed"].as<double>();
      if (max_speed_ <= 0.0) {
        std::cerr << "Error: max-speed must be > 0" << std::endl;
        return false;
      }
    }
    if (result.count("max-thrust-order-mag")) {
      max_thrust_order_mag_ = result["max-thrust-order-mag"].as<double>();
      if (max_thrust_order_mag_ <= 0.0) {
        std::cerr << "Error: max-thrust-order-mag must be > 0" << std::endl;
        return false;
      }
    }

    // Parse asteroid configuration
    if (result.count("vinyl-num")) {
      g_initial_vinyl_asteroid_count = result["vinyl-num"].as<unsigned int>();
    }
    if (result.count("vinyl-mass")) {
      g_initial_vinyl_asteroid_mass = result["vinyl-mass"].as<double>();
    }
    if (result.count("uranium-num")) {
      g_initial_uranium_asteroid_count = result["uranium-num"].as<unsigned int>();
    }
    if (result.count("uranium-mass")) {
      g_initial_uranium_asteroid_mass = result["uranium-mass"].as<double>();
    }

    // Validate minimum mass
    const double min_mass = 3.0;  // g_thing_minmass
    if (g_initial_vinyl_asteroid_mass < min_mass) {
      std::cerr << "Error: --vinyl-mass (" << g_initial_vinyl_asteroid_mass
                << ") is below minimum object size (" << min_mass << " tons)" << std::endl;
      exit(1);
    }
    if (g_initial_uranium_asteroid_mass < min_mass) {
      std::cerr << "Error: --uranium-mass (" << g_initial_uranium_asteroid_mass
                << ") is below minimum object size (" << min_mass << " tons)" << std::endl;
      exit(1);
    }

    // Validate world object limits
    const unsigned int MAX_WORLD_OBJECTS = 512;
    const unsigned int RESERVED_FOR_TEAMS = 40;  // 2 teams × (1 station + 4 ships) = 10, with safety margin
    const unsigned int MAX_ASTEROID_SLOTS = MAX_WORLD_OBJECTS - RESERVED_FOR_TEAMS;

    unsigned int max_vinyl = CalculateMaxAsteroidCount(
        g_initial_vinyl_asteroid_count, g_initial_vinyl_asteroid_mass, min_mass);
    unsigned int max_uranium = CalculateMaxAsteroidCount(
        g_initial_uranium_asteroid_count, g_initial_uranium_asteroid_mass, min_mass);
    unsigned int total_max = max_vinyl + max_uranium;

    if (total_max > MAX_ASTEROID_SLOTS) {
      std::cerr << "Error: Asteroid configuration exceeds world object limit!" << std::endl;
      std::cerr << std::endl;
      std::cerr << "Worst-case asteroid count calculation:" << std::endl;
      std::cerr << "  World limit: " << MAX_WORLD_OBJECTS << " objects" << std::endl;
      std::cerr << "  Reserved for teams: " << RESERVED_FOR_TEAMS << " objects" << std::endl;
      std::cerr << "  Available for asteroids: " << MAX_ASTEROID_SLOTS << " objects" << std::endl;
      std::cerr << std::endl;
      std::cerr << "Vinyl asteroids:" << std::endl;
      std::cerr << "  Initial count: " << g_initial_vinyl_asteroid_count << std::endl;
      std::cerr << "  Mass per asteroid: " << g_initial_vinyl_asteroid_mass << " tons" << std::endl;
      std::cerr << "  Max generations: calculated from mass/" << min_mass << " threshold" << std::endl;
      std::cerr << "  Worst-case count: " << max_vinyl << " asteroids" << std::endl;
      std::cerr << std::endl;
      std::cerr << "Uranium asteroids:" << std::endl;
      std::cerr << "  Initial count: " << g_initial_uranium_asteroid_count << std::endl;
      std::cerr << "  Mass per asteroid: " << g_initial_uranium_asteroid_mass << " tons" << std::endl;
      std::cerr << "  Worst-case count: " << max_uranium << " asteroids" << std::endl;
      std::cerr << std::endl;
      std::cerr << "Total worst-case: " << total_max << " asteroids (exceeds "
                << MAX_ASTEROID_SLOTS << " limit by " << (total_max - MAX_ASTEROID_SLOTS) << ")" << std::endl;
      std::cerr << std::endl;
      std::cerr << "Note: Worst case assumes all asteroids fragment to second-to-last generation," << std::endl;
      std::cerr << "      then all simultaneously shatter to final generation on same turn." << std::endl;
      std::cerr << "      Formula: initial_count × (3^N + 3^(N-1)) where N is max generation." << std::endl;
      exit(1);
    }

    if (needhelp) {
      std::cout << options.help() << std::endl;
      return true;
    }

    // Load config file first if specified
    if (result.count("config")) {
      configFile = result["config"].as<std::string>();
      if (!LoadConfig(configFile)) {
        std::cerr << "Warning: Could not load config file: " << configFile
                  << std::endl;
      }
    }

    // Process feature flags (command line overrides config)
    if (result.count("legacy-collision-detection")) {
      features["collision-detection"] = false;
    }
    if (result.count("legacy-velocity-limits")) {
      features["velocity-limits"] = false;
    }
    if (result.count("legacy-asteroid-eat-damage")) {
      features["asteroid-eat-damage"] = false;
    }
    if (result.count("legacy-physics")) {
      features["physics"] = false;
    }
    if (result.count("legacy-collision-handling")) {
      features["collision-handling"] = false;
    }
    if (result.count("legacy-laser-exploit")) {
      features["laser-exploit"] = true;  // Enable exploit (true = exploit enabled)
    }
    if (result.count("legacy-docking")) {
      features["docking"] = false;  // Disable fix (false = legacy buggy behavior)
    }
    if (result.count("legacy-rangecheck-bug")) {
      features["rangecheck-bug"] = true;  // Enable bug (true = buggy behavior)
    }
    if (result.count("legacy-initial-orientation")) {
      features["initial-orientation"] = false;  // Disable fix (false = legacy asymmetric behavior)
    }
    if (result.count("legacy-facing-detection")) {
      features["facing-detection"] = false;  // Disable toroidal shortest-path fix
    }
    if (result.count("legacy-cargo-calc")) {
      features["cargo-calc"] = false;  // Use strict legacy cargo capacity comparisons
    }
    if (result.count("announcer-velocity-clamping")) {
      features["announcer-velocity-clamping"] = true;
    }

    // Process bundles
    if (result.count("improved-physics")) {
      ApplyBundle("improved-physics");
    }
    if (result.count("legacy-mode")) {
      ApplyBundle("legacy-mode");
    }

    return true;
  } catch (const cxxopts::exceptions::exception& e) {
    std::cerr << "Error parsing options: " << e.what() << std::endl;
    needhelp = true;
    return false;
  }
}

void ArgumentParser::ApplyBundle(const std::string& bundle) {
  if (bundle == "improved-physics") {
    features["collision-detection"] = true;
    features["velocity-limits"] = true;
    features["asteroid-eat-damage"] = true;
    features["cargo-calc"] = true;
    features["physics"] = true;              // Enable correct collision physics and momentum
    features["collision-handling"] = true;   // Enable improved collision processing
    features["laser-exploit"] = false;      // Patch exploit
    features["docking"] = true;              // Fix docking
  } else if (bundle == "legacy-mode") {
    features["collision-detection"] = false;
    features["velocity-limits"] = false;
    features["asteroid-eat-damage"] = false;
    features["cargo-calc"] = false;
    features["physics"] = false;            // Use legacy collision physics (no laser momentum)
    features["collision-handling"] = false; // Use legacy collision processing (with multi-hit bugs)
    features["laser-exploit"] = true;       // Enable exploit for legacy mode
    features["docking"] = false;            // Enable docking bug for legacy mode
    features["rangecheck-bug"] = true;      // Enable range check bug for legacy mode
    features["initial-orientation"] = false; // Enable asymmetric orientation for legacy mode
    features["facing-detection"] = false;   // Use legacy facing for legacy mode
    // Set timing and physics parameters to default values for legacy mode
    game_turn_duration_ = 1.0;
    physics_simulation_dt_ = 0.2;
    max_speed_ = 30.0;
    max_thrust_order_mag_ = 60.0;
  }
}

bool ArgumentParser::UseNewFeature(const std::string& feature) const {
  auto it = features.find(feature);
  if (it != features.end()) {
    return it->second;
  }
  // Default to new behavior if feature not found
  return true;
}

bool ArgumentParser::LoadConfig(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return ParseConfigJson(buffer.str());
}

bool ArgumentParser::ParseConfigJson(const std::string& content) {
  // Simple JSON parser for our config format
  // Format: { "profile": "name", "features": { "feature-name": "new/old", ...
  // }, "options": { ... } }

  // This is a simplified parser - in production you'd want to use a proper JSON
  // library
  std::istringstream stream(content);
  std::string line;
  bool inFeatures = false;
  bool inOptions = false;

  while (std::getline(stream, line)) {
    // Remove whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    if (line.find("\"features\"") != std::string::npos) {
      inFeatures = true;
      inOptions = false;
    } else if (line.find("\"options\"") != std::string::npos) {
      inFeatures = false;
      inOptions = true;
    } else if (line.find("}") != std::string::npos) {
      inFeatures = false;
      inOptions = false;
    } else if (inFeatures && line.find(":") != std::string::npos) {
      // Parse feature line: "feature-name": true/false
      size_t colonPos = line.find(":");
      std::string key = line.substr(0, colonPos);
      std::string value = line.substr(colonPos + 1);

      // Remove quotes and whitespace
      key.erase(std::remove(key.begin(), key.end(), '\"'), key.end());
      key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
      value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());
      value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
      value.erase(std::remove(value.begin(), value.end(), ','), value.end());

      if (!key.empty() && !value.empty()) {
        if (key == "legacy-collision-detection") {
          features["collision-detection"] = !(value == "true");
        } else if (key == "legacy-velocity-limits") {
          features["velocity-limits"] = !(value == "true");
        } else if (key == "legacy-cargo-calc") {
          features["cargo-calc"] = !(value == "true");
        } else if (key == "announcer-velocity-clamping") {
          features[key] = (value == "true");
        } else if (key == "cargo-calc") {
          features["cargo-calc"] = (value == "true");
        }
      }
    } else if (inOptions && line.find(":") != std::string::npos) {
      // Parse option line
      size_t colonPos = line.find(":");
      std::string key = line.substr(0, colonPos);
      std::string value = line.substr(colonPos + 1);

      // Remove quotes and whitespace
      key.erase(std::remove(key.begin(), key.end(), '\"'), key.end());
      key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
      value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());
      value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
      value.erase(std::remove(value.begin(), value.end(), ','), value.end());

      if (key == "port") {
        port = std::stoi(value);
      } else if (key == "hostname") {
        hostname = value;
      } else if (key == "gfxreg") {
        gfxreg = value;
      } else if (key == "numteams") {
        numteams = std::stoi(value);
      } else if (key == "graphics") {
        gfxflag = (value == "true");
      } else if (key == "reconnect") {
        reconnect = retry = (value == "true");
      }
    } else if (line.find("\"profile\"") != std::string::npos) {
      // Parse profile line
      size_t colonPos = line.find(":");
      if (colonPos != std::string::npos) {
        std::string value = line.substr(colonPos + 1);
        value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());
        value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
        value.erase(std::remove(value.begin(), value.end(), ','), value.end());

        if (value == "competitive" || value == "improved") {
          ApplyBundle("improved-physics");
        } else if (value == "legacy" || value == "classic") {
          ApplyBundle("legacy-mode");
        }
      }
    }
  }

  return true;
}

bool ArgumentParser::SaveConfig(const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  file << "{\n";
  file << "  \"description\": \"MechMania IV Tournament Configuration\",\n";

  // Write options
  file << "  \"options\": {\n";
  file << "    \"port\": " << port << ",\n";
  file << "    \"hostname\": \"" << hostname << "\",\n";
  file << "    \"gfxreg\": \"" << gfxreg << "\",\n";
  file << "    \"numteams\": " << numteams << ",\n";
  file << "    \"graphics\": " << (gfxflag ? "true" : "false") << ",\n";
  file << "    \"reconnect\": " << (reconnect ? "true" : "false") << "\n";
  file << "  },\n";

  // Write features
  file << "  \"features\": {\n";
  bool first = true;
  for (const auto& [name, useNew] : features) {
    if (!first) {
      file << ",\n";
    }
    if (name == "collision-detection") {
      // Convert to legacy-collision-detection boolean format
      file << "    \"legacy-collision-detection\": " << (useNew ? "false" : "true");
    } else if (name == "velocity-limits") {
      // Convert to legacy-velocity-limits boolean format
      file << "    \"legacy-velocity-limits\": " << (useNew ? "false" : "true");
    } else {
      // Other features use direct boolean format
      file << "    \"" << name << "\": " << (useNew ? "true" : "false");
    }
    first = false;
  }
  file << "\n  }\n";

  file << "}\n";
  file.close();
  return true;
}

void ArgumentParser::PrintHelp() const {
  // This is handled by cxxopts in Parse() when --help is used
}
