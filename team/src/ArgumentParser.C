/* ArgumentParser.C
   Modern argument parser implementation for MechMania IV
*/

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ArgumentParser.h"
#include "third_party/cxxopts.hpp"

ArgumentParser::ArgumentParser() { InitializeFeatures(); }

ArgumentParser::~ArgumentParser() {}

void ArgumentParser::InitializeFeatures() {
  // Initialize all features with their default states
  // true = new behavior is default, false = old behavior is default

  // Physics features
  features["collision-detection"] = true;  // New collision detection is default
  features["velocity-limits"] = true;      // New velocity/acceleration limits is default
  features["asteroid-eat-damage"] = true;  // New: no damage when eating asteroids that fit

  // Announcer features
  features["announcer-velocity-clamping"] = false;  // Disabled by default

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
        "T,numteams", "Number of teams",
        cxxopts::value<int>()->default_value("2"))("G,graphics",
                                                   "Enable full graphics mode")(
        "R,reconnect", "Attempt reconnect after disconnect")(
        "config", "Configuration file", cxxopts::value<std::string>())(
        "log", "Enable team logging")(
        "log-file", "Path to team log file",
         cxxopts::value<std::string>())(
        "params", "Path to team parameter file",
         cxxopts::value<std::string>())("verbose", "Enable verbose output")(
        "help", "Show help");

    // Feature flags
    options.add_options("Features")("legacy-collision-detection",
                                    "Use legacy collision detection")(
        "legacy-velocity-limits", "Use legacy velocity and acceleration limits")(
        "legacy-asteroid-eat-damage", "Ships take damage when eating asteroids (legacy behavior)")(
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
        cxxopts::value<double>()->default_value("0.2"));

    // Game physics options
    options.add_options("Physics")(
        "max-speed",
        "Maximum velocity magnitude (default: 30.0)",
        cxxopts::value<double>()->default_value("30.0"))(
        "max-thrust-order-mag",
        "Maximum thrust order magnitude (default: 60.0)",
        cxxopts::value<double>()->default_value("60.0"));

    auto result = options.parse(argc, argv);

    // Basic options
    port = result["port"].as<int>();
    hostname = result["hostname"].as<std::string>();
    gfxreg = result["gfxreg"].as<std::string>();
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

    verbose = result.count("verbose") > 0;

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
  } else if (bundle == "legacy-mode") {
    features["collision-detection"] = false;
    features["velocity-limits"] = false;
    features["asteroid-eat-damage"] = false;
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
        } else if (key == "announcer-velocity-clamping") {
          features[key] = (value == "true");
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
