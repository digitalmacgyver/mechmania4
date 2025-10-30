/* SoundLibrary.C
 * Implementation of SoundLibrary.
 */

#include "audio/SoundLibrary.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace {

std::string Trim(const std::string& input) {
  const char* whitespace = " \t\r\n";
  const auto start = input.find_first_not_of(whitespace);
  if (start == std::string::npos) {
    return "";
  }
  const auto end = input.find_last_not_of(whitespace);
  return input.substr(start, end - start + 1);
}

std::string JoinPath(const std::vector<std::string>& stack,
                     size_t segmentCount = std::string::npos,
                     const std::string& leaf = std::string()) {
  std::ostringstream oss;
  size_t limit = segmentCount == std::string::npos ? stack.size() : segmentCount;
  for (size_t i = 0; i < limit && i < stack.size(); ++i) {
    if (i > 0) {
      oss << '.';
    }
    oss << stack[i];
  }
  if (!leaf.empty()) {
    if (limit > 0 && !oss.str().empty()) {
      oss << '.';
    }
    oss << leaf;
  }
  return oss.str();
}

struct ParseResult {
  std::unordered_map<std::string, std::string> scalars;
  std::unordered_map<std::string, std::vector<std::string>> lists;
};

bool EndsWith(const std::string& str, const std::string& suffix) {
  if (suffix.size() > str.size()) {
    return false;
  }
  return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

std::string StripSuffix(const std::string& str, const std::string& suffix) {
  return str.substr(0, str.size() - suffix.size());
}

int ParseInt(const std::string& input, int defaultValue) {
  try {
    size_t idx = 0;
    int value = std::stoi(input, &idx, 10);
    if (idx != input.size()) {
      return defaultValue;
    }
    return value;
  } catch (...) {
    return defaultValue;
  }
}

double ParseDouble(const std::string& input, double defaultValue) {
  try {
    size_t idx = 0;
    double value = std::stod(input, &idx);
    if (idx != input.size()) {
      return defaultValue;
    }
    return value;
  } catch (...) {
    return defaultValue;
  }
}

bool ParseSoundConfig(std::istream& input, ParseResult& outResult) {
  std::vector<std::string> keyStack;
  std::string line;
  size_t lineNumber = 0;

  while (std::getline(input, line)) {
    ++lineNumber;

    std::string::size_type indent = 0;
    while (indent < line.size() && line[indent] == ' ') {
      ++indent;
    }

    std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }

    size_t level = indent / 2;  // Configuration uses two-space indentation.
    while (keyStack.size() > level) {
      keyStack.pop_back();
    }

    if (trimmed.rfind("- ", 0) == 0) {
      std::string value = Trim(trimmed.substr(2));
      if (value.empty()) {
        continue;
      }
      std::string listPath = JoinPath(keyStack);
      if (!listPath.empty()) {
        outResult.lists[listPath].push_back(value);
      }
      continue;
    }

    auto colonPos = trimmed.find(':');
    if (colonPos == std::string::npos) {
      continue;
    }

    std::string key = Trim(trimmed.substr(0, colonPos));
    std::string value = Trim(trimmed.substr(colonPos + 1));

    if (key.empty()) {
      continue;
    }

    if (value.empty()) {
      // Nested block; extend the stack for downstream keys.
      keyStack.push_back(key);
    } else {
      std::string scalarPath = JoinPath(keyStack, keyStack.size(), key);
      outResult.scalars[scalarPath] = value;
    }
  }

  return true;
}

}  // namespace

namespace mm4::audio {

bool SoundLibrary::LoadDefaults(const std::string& configPath) {
  std::string overrideBackup = assetRootOverride_;
  Clear();
  assetRootOverride_ = overrideBackup;
  RegisterDefaultFallbacks();

  std::filesystem::path cfgPath(configPath);
  if (std::filesystem::is_directory(cfgPath)) {
    baseDirectory_ = cfgPath.string();
  } else if (std::filesystem::exists(cfgPath)) {
    baseDirectory_ = cfgPath.parent_path().string();
  } else {
    std::cerr << "[audio] Warning: sound config path not found: " << configPath
              << std::endl;
    baseDirectory_.clear();
    return true;
  }

  std::ifstream input(cfgPath);
  if (!input.is_open()) {
    std::cerr << "[audio] Warning: failed to open sound config " << cfgPath
              << std::endl;
    return true;
  }

  ParseResult parsed;
  if (!ParseSoundConfig(input, parsed)) {
    std::cerr << "[audio] Warning: unable to parse sound config " << cfgPath
              << std::endl;
    return true;
  }

  if (auto it = parsed.scalars.find("volume.soundtrack"); it != parsed.scalars.end()) {
    soundtrackVolumePercent_ = std::clamp(ParseInt(it->second, 100), 0, 100);
  }
  if (auto it = parsed.scalars.find("volume.effects"); it != parsed.scalars.end()) {
    effectsVolumePercent_ = std::clamp(ParseInt(it->second, 100), 0, 100);
  }

  auto resolvePath = [&](const std::string& relative) {
    std::filesystem::path rel(relative);
    if (rel.is_absolute()) {
      return rel.lexically_normal().string();
    }

    if (!assetRootOverride_.empty() && rel.is_relative()) {
      std::filesystem::path overridePath =
          std::filesystem::path(assetRootOverride_) / rel;
      std::error_code ec;
      if (std::filesystem::exists(overridePath, ec)) {
        return overridePath.lexically_normal().string();
      }
    }

    if (!baseDirectory_.empty() && rel.is_relative()) {
      std::filesystem::path base(baseDirectory_);
      std::filesystem::path alt = base.parent_path() / rel;
      std::error_code baseEc;
      if (!alt.empty() && std::filesystem::exists(alt, baseEc)) {
        return alt.lexically_normal().string();
      }
      rel = base / rel;
    }
    return rel.lexically_normal().string();
  };

  // Load soundtrack listings.
  if (auto it = parsed.lists.find("game.soundtrack.songs");
      it != parsed.lists.end()) {
    if (!it->second.empty()) {
      defaultSoundtrackId_ = it->second.front();
    }
    for (const auto& song : it->second) {
      if (song.empty()) {
        continue;
      }
      musicAssets_[song] = resolvePath(song);
    }
  }

  // Load explicit scalar mappings (overrides fallback entries).
struct PendingEffect {
  std::string assetPath;
  bool hasAsset = false;
  EffectBehavior behavior;
  bool hasBehavior = false;
  std::string inheritId;
  bool hasInherit = false;
};

  std::unordered_map<std::string, PendingEffect> pendingEffects;

  for (const auto& entry : parsed.scalars) {
    const std::string& path = entry.first;
    const std::string& value = entry.second;
    if (value.empty()) {
      continue;
    }

    if (path.rfind("teams.", 0) == 0) {
      std::string remainder = path.substr(std::string("teams.").size());

      if (EndsWith(remainder, ".file")) {
        std::string logicalId = StripSuffix(remainder, ".file");
        auto& pending = pendingEffects[logicalId];
        pending.assetPath = resolvePath(value);
        pending.hasAsset = true;
      } else if (EndsWith(remainder, ".inherit")) {
        std::string logicalId = StripSuffix(remainder, ".inherit");
        auto& pending = pendingEffects[logicalId];
        pending.inheritId = value;
        pending.hasInherit = true;
      } else if (EndsWith(remainder, ".behavior.mode")) {
        std::string logicalId = StripSuffix(remainder, ".behavior.mode");
        auto& pending = pendingEffects[logicalId];
        if (value == "queue") {
          pending.behavior.mode = EffectPlaybackMode::kQueue;
        } else if (value == "truncate" || value == "cutoff") {
          pending.behavior.mode = EffectPlaybackMode::kTruncate;
        } else {
          pending.behavior.mode = EffectPlaybackMode::kSimultaneous;
        }
        pending.hasBehavior = true;
      } else if (EndsWith(remainder, ".behavior.duration_ticks")) {
        std::string logicalId = StripSuffix(remainder, ".behavior.duration_ticks");
        auto& pending = pendingEffects[logicalId];
        pending.behavior.durationTicks = std::max(0, ParseInt(value, 0));
        pending.hasBehavior = true;
      } else if (EndsWith(remainder, ".behavior.delay_ticks")) {
        std::string logicalId = StripSuffix(remainder, ".behavior.delay_ticks");
        auto& pending = pendingEffects[logicalId];
        pending.behavior.delayTicks = std::max(0, ParseInt(value, 0));
        pending.hasBehavior = true;
      } else if (EndsWith(remainder, ".behavior.scale.per_quantity")) {
        std::string logicalId = StripSuffix(remainder, ".behavior.scale.per_quantity");
        auto& pending = pendingEffects[logicalId];
        if (!pending.behavior.scale.has_value()) {
          pending.behavior.scale = EffectScaleRule();
        }
        pending.behavior.scale->perQuantity = std::max(0.0, ParseDouble(value, 0.0));
        pending.hasBehavior = true;
      } else if (EndsWith(remainder, ".behavior.scale.min_loops")) {
        std::string logicalId = StripSuffix(remainder, ".behavior.scale.min_loops");
        auto& pending = pendingEffects[logicalId];
        if (!pending.behavior.scale.has_value()) {
          pending.behavior.scale = EffectScaleRule();
        }
        pending.behavior.scale->minLoops = std::max(1, ParseInt(value, 1));
        pending.hasBehavior = true;
      } else if (EndsWith(remainder, ".behavior.scale.max_loops")) {
        std::string logicalId = StripSuffix(remainder, ".behavior.scale.max_loops");
        auto& pending = pendingEffects[logicalId];
        if (!pending.behavior.scale.has_value()) {
          pending.behavior.scale = EffectScaleRule();
        }
        int parsed = std::max(1, ParseInt(value, pending.behavior.scale->maxLoops));
        pending.behavior.scale->maxLoops = parsed;
        if (pending.behavior.scale->maxLoops < pending.behavior.scale->minLoops) {
          pending.behavior.scale->maxLoops = pending.behavior.scale->minLoops;
        }
        pending.hasBehavior = true;
      } else {
        // Legacy shorthand: logicalId maps directly to a file path.
        auto& pending = pendingEffects[remainder];
        pending.assetPath = resolvePath(value);
        pending.hasAsset = true;
      }
    } else if (path.rfind("game.", 0) == 0) {
      std::string musicId = path.substr(std::string("game.").size());
      musicAssets_[musicId] = resolvePath(value);
    }
  }

  for (auto& entry : pendingEffects) {
    const std::string& logicalId = entry.first;
    PendingEffect& pending = entry.second;

    auto existing = effectAssets_.find(logicalId);
    SoundEffectDescriptor descriptor;

    if (existing != effectAssets_.end()) {
      descriptor = existing->second;
    } else {
      descriptor.logicalId = logicalId;
      descriptor.behavior = EffectBehavior();
    }

    if (pending.hasInherit) {
      auto baseIt = effectAssets_.find(pending.inheritId);
      if (baseIt != effectAssets_.end()) {
        descriptor = baseIt->second;
        descriptor.logicalId = logicalId;
      } else {
        std::cerr << "[audio] Warning: inherit target not found for "
                  << logicalId << " -> " << pending.inheritId << std::endl;
      }
    }

    if (pending.hasAsset && !pending.assetPath.empty()) {
      descriptor.assetPath = pending.assetPath;
    }

    if (pending.hasBehavior) {
      // Merge behavior overrides with existing descriptor.
      descriptor.behavior.mode = pending.behavior.mode;
      descriptor.behavior.durationTicks = pending.behavior.durationTicks;
      descriptor.behavior.delayTicks = pending.behavior.delayTicks;
      descriptor.behavior.scale = pending.behavior.scale;
    }

    // Ensure scale loops maintain ordering if both min/max set.
    if (descriptor.behavior.scale.has_value()) {
      auto& scale = descriptor.behavior.scale.value();
      if (scale.maxLoops < scale.minLoops) {
        scale.maxLoops = scale.minLoops;
      }
    }

    if (!descriptor.assetPath.empty()) {
      effectAssets_[logicalId] = descriptor;
    }
  }

  return true;
}

std::optional<SoundEffectDescriptor> SoundLibrary::ResolveEffect(const std::string& logicalEvent) const {
  if (auto it = effectAssets_.find(logicalEvent); it != effectAssets_.end()) {
    return it->second;
  }

  auto dotPos = logicalEvent.find('.');
  if (dotPos != std::string::npos) {
    std::string suffix = logicalEvent.substr(dotPos);
    std::string teamFallback = "team" + suffix;
    if (auto it = effectAssets_.find(teamFallback); it != effectAssets_.end()) {
      return it->second;
    }
  }

  return std::nullopt;
}

std::string SoundLibrary::ResolveMusicAsset(const std::string& trackId) const {
  if (auto it = musicAssets_.find(trackId); it != musicAssets_.end()) {
    return it->second;
  }
  return {};
}

std::string SoundLibrary::DefaultSoundtrackId() const {
  if (!defaultSoundtrackId_.empty()) {
    return defaultSoundtrackId_;
  }
  if (auto it = musicAssets_.find("soundtrack.default");
      it != musicAssets_.end()) {
    return "soundtrack.default";
  }
  return {};
}

std::vector<std::string> SoundLibrary::AllSoundtrackIds() const {
  std::vector<std::string> ids;
  ids.reserve(musicAssets_.size());
  for (const auto& entry : musicAssets_) {
    ids.push_back(entry.first);
  }
  return ids;
}

void SoundLibrary::SetAssetRootOverride(const std::string& assetRoot) {
  if (assetRoot.empty()) {
    assetRootOverride_.clear();
    return;
  }
  std::filesystem::path root(assetRoot);
  if (root.is_relative()) {
    root = std::filesystem::absolute(root);
  }
  assetRootOverride_ = root.lexically_normal().string();
}

void SoundLibrary::Clear() {
  effectAssets_.clear();
  musicAssets_.clear();
  defaultSoundtrackId_.clear();
  baseDirectory_.clear();
  assetRootOverride_.clear();
  soundtrackVolumePercent_ = 100;
  effectsVolumePercent_ = 100;
}

void SoundLibrary::RegisterDefaultFallbacks() {
  // Baseline fallbacks keep the observer functional even if the user provides
  // no explicit sound configuration. Entries may be overridden by parsed
  // values from defaults.txt.
  auto registerEffect = [&](const std::string& id, const std::string& path) {
    SoundEffectDescriptor descriptor;
    descriptor.logicalId = id;
    descriptor.assetPath = path;
    descriptor.behavior = EffectBehavior();
    effectAssets_[id] = descriptor;
  };

  registerEffect("team.launch.default", "sound/launch_default.wav");
  registerEffect("team.dock.default", "sound/dock_default.wav");
  registerEffect("team.damage.shield", "sound/shield_hit.wav");
  registerEffect("team.deliver_vinyl.default", "sound/vinyl_delivered.wav");
  registerEffect("team.ship.destroyed", "sound/ship_destroyed.wav");

  musicAssets_["soundtrack.default"] = "sound/soundtrack_loop.mp3";
  defaultSoundtrackId_.clear();
}

}  // namespace mm4::audio
