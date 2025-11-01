/* ShipArtUtil.C
 * Utilities for discovering and selecting ship art packs.
 */

#include "ShipArtUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <system_error>
#include <unordered_set>

namespace shipart {
namespace {

std::vector<std::filesystem::path> BuildSearchRoots(
    const std::string& assetsRootOverride) {
  std::vector<std::filesystem::path> roots = {
      std::filesystem::path("assets/star_control/graphics"),
      std::filesystem::path("../assets/star_control/graphics"),
      std::filesystem::path("../../assets/star_control/graphics")};

  if (!assetsRootOverride.empty()) {
    std::filesystem::path overridePath(assetsRootOverride);
    roots.push_back(overridePath);
    roots.push_back(overridePath / "star_control/graphics");
  }

  if (const char* env = std::getenv("MM4_ASSETS_DIR")) {
    roots.emplace_back(env);
  }

  if (const char* env = std::getenv("MM4_SHARE_DIR")) {
    roots.emplace_back(env);
    roots.emplace_back(std::filesystem::path(env) /
                       "assets/star_control/graphics");
  }

  return roots;
}

struct CaseInsensitiveLess {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    std::string ll = ToLower(lhs);
    std::string rl = ToLower(rhs);
    if (ll == rl) {
      return lhs < rhs;
    }
    return ll < rl;
  }
};

void DiscoverFromRoot(const std::filesystem::path& root,
                      std::set<std::string, CaseInsensitiveLess>* out) {
  if (!out) {
    return;
  }
  std::error_code statusEc;
  if (!std::filesystem::exists(root, statusEc) ||
      !std::filesystem::is_directory(root, statusEc)) {
    return;
  }

  std::error_code factionEc;
  std::filesystem::directory_iterator factionEnd;
  for (std::filesystem::directory_iterator factionIt(root, factionEc);
       factionIt != factionEnd; factionIt.increment(factionEc)) {
    if (factionEc) {
      factionEc.clear();
      continue;
    }
    const auto& factionEntry = *factionIt;
    std::error_code factionStatusEc;
    if (!std::filesystem::is_directory(factionEntry.path(), factionStatusEc) ||
        factionStatusEc) {
      continue;
    }

    const auto factionName = factionEntry.path().filename().string();

    std::error_code shipEc;
    for (std::filesystem::directory_iterator shipIt(factionEntry.path(), shipEc),
         shipEnd;
         shipIt != shipEnd; shipIt.increment(shipEc)) {
      if (shipEc) {
        shipEc.clear();
        continue;
      }
      const auto& shipEntry = *shipIt;
      std::error_code shipStatusEc;
      if (!std::filesystem::is_directory(shipEntry.path(), shipStatusEc) ||
          shipStatusEc) {
        continue;
      }

      const auto shipName = shipEntry.path().filename().string();

      if (EqualsIgnoreCase(factionName, "yehat") &&
          EqualsIgnoreCase(shipName, "shield")) {
        continue;  // reserved for overlays
      }

      bool hasAllFrames = true;
      for (int idx = 0; idx < 16; ++idx) {
        std::error_code frameEc;
        std::filesystem::path framePath =
            shipEntry.path() /
            (shipName + ".big." + std::to_string(idx) + ".png");
        if (!std::filesystem::exists(framePath, frameEc) || frameEc) {
          hasAllFrames = false;
          break;
        }
      }

      if (hasAllFrames) {
        out->insert(factionName + ":" + shipName);
      }
    }
  }
}

std::string Trim(const std::string& value) {
  size_t start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return std::string();
  }
  size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

}  // namespace

std::string ToLower(const std::string& value) {
  std::string lower(value.size(), '\0');
  std::transform(value.begin(), value.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower;
}

bool EqualsIgnoreCase(const std::string& lhs, const std::string& rhs) {
  return ToLower(lhs) == ToLower(rhs);
}

std::vector<std::string> DiscoverShipArtOptions(
    const std::string& assetsRootOverride) {
  std::set<std::string, CaseInsensitiveLess> dedup;
  auto roots = BuildSearchRoots(assetsRootOverride);
  for (const auto& root : roots) {
    DiscoverFromRoot(root, &dedup);
  }

  // Legacy sprite sheets for original assets.
  dedup.insert("legacy:t1");
  dedup.insert("legacy:t2");

  return std::vector<std::string>(dedup.begin(), dedup.end());
}

std::string CanonicalizeShipArtRequest(
    const std::string& request,
    const std::vector<std::string>& availableOptions) {
  if (availableOptions.empty()) {
    return std::string();
  }

  std::string trimmed = Trim(request);
  if (trimmed.empty()) {
    return std::string();
  }

  if (EqualsIgnoreCase(trimmed, "mm4orange")) {
    trimmed = "legacy:t1";
  } else if (EqualsIgnoreCase(trimmed, "mm4blue")) {
    trimmed = "legacy:t2";
  }

  auto matchFull = [&](const std::string& candidate) -> std::string {
    for (const auto& option : availableOptions) {
      if (EqualsIgnoreCase(option, candidate)) {
        return option;
      }
    }
    return std::string();
  };

  auto colonPos = trimmed.find(':');
  if (colonPos != std::string::npos) {
    std::string faction = Trim(trimmed.substr(0, colonPos));
    std::string ship = Trim(trimmed.substr(colonPos + 1));
    if (faction.empty() || ship.empty()) {
      return std::string();
    }

    std::string combined = faction + ":" + ship;
    std::string matched = matchFull(combined);
    if (!matched.empty()) {
      return matched;
    }

    for (const auto& option : availableOptions) {
      auto optColon = option.find(':');
      if (optColon == std::string::npos) {
        continue;
      }
      std::string optFaction = option.substr(0, optColon);
      std::string optShip = option.substr(optColon + 1);
      if (EqualsIgnoreCase(optFaction, faction) &&
          EqualsIgnoreCase(optShip, ship)) {
        return option;
      }
    }
    return std::string();
  }

  // Single token request - match by faction or ship name.
  for (const auto& option : availableOptions) {
    if (EqualsIgnoreCase(option, trimmed)) {
      return option;
    }
    auto optColon = option.find(':');
    if (optColon == std::string::npos) {
      continue;
    }
    std::string optFaction = option.substr(0, optColon);
    std::string optShip = option.substr(optColon + 1);
    if (EqualsIgnoreCase(optFaction, trimmed) ||
        EqualsIgnoreCase(optShip, trimmed)) {
      return option;
    }
  }

  return std::string();
}

std::string ChooseRandomShipArt(
    const std::vector<std::string>& availableOptions,
    const std::set<std::string>& excludeLower) {
  if (availableOptions.empty()) {
    return std::string();
  }

  std::vector<std::string> filtered;
  filtered.reserve(availableOptions.size());
  for (const auto& option : availableOptions) {
    if (excludeLower.find(ToLower(option)) != excludeLower.end()) {
      continue;
    }
    filtered.push_back(option);
  }

  const std::vector<std::string>& pool =
      filtered.empty() ? availableOptions : filtered;

  static std::mt19937 rng(
      static_cast<unsigned int>(std::random_device{}()));
  std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
  return pool[dist(rng)];
}

}  // namespace shipart
