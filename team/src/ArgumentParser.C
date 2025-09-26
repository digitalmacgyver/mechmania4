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
  features["elastic-collisions"] = false;  // Old behavior is default
  features["conserve-momentum"] = false;   // Old behavior is default

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
        "help", "Show help");

    // Feature flags
    options.add_options("Features")("old-collision-detection",
                                    "Use old collision detection")(
        "new-collision-detection", "Use new collision detection (default)")(
        "old-elastic-collisions", "Use old elastic collisions (default)")(
        "new-elastic-collisions", "Use new elastic collisions")(
        "old-conserve-momentum", "Use old momentum conservation (default)")(
        "new-conserve-momentum", "Use new momentum conservation");

    // Feature bundles
    options.add_options("Bundles")("improved-physics",
                                   "Enable all new physics features")(
        "legacy-mode", "Use all old/legacy features");

    auto result = options.parse(argc, argv);

    // Basic options
    port = result["port"].as<int>();
    hostname = result["hostname"].as<std::string>();
    gfxreg = result["gfxreg"].as<std::string>();
    numteams = result["numteams"].as<int>();
    gfxflag = result.count("graphics") > 0;
    reconnect = retry = result.count("reconnect") > 0;
    needhelp = result.count("help") > 0;

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
    if (result.count("old-collision-detection")) {
      features["collision-detection"] = false;
    }
    if (result.count("new-collision-detection")) {
      features["collision-detection"] = true;
    }
    if (result.count("old-elastic-collisions")) {
      features["elastic-collisions"] = false;
    }
    if (result.count("new-elastic-collisions")) {
      features["elastic-collisions"] = true;
    }
    if (result.count("old-conserve-momentum")) {
      features["conserve-momentum"] = false;
    }
    if (result.count("new-conserve-momentum")) {
      features["conserve-momentum"] = true;
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
    features["elastic-collisions"] = true;
    features["conserve-momentum"] = true;
  } else if (bundle == "legacy-mode") {
    features["collision-detection"] = false;
    features["elastic-collisions"] = false;
    features["conserve-momentum"] = false;
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
      // Parse feature line: "feature-name": "new/old"
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
        features[key] = (value == "new");
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
    file << "    \"" << name << "\": \"" << (useNew ? "new" : "old") << "\"";
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