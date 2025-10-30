/* SoundLibrary.h
 * Lightweight asset catalog for the SDL_mixer audio system.
 * Responsible for resolving logical sound identifiers to concrete files.
 */

#ifndef MM4_AUDIO_SOUND_LIBRARY_H_
#define MM4_AUDIO_SOUND_LIBRARY_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mm4::audio {

enum class EffectPlaybackMode {
  kSimultaneous,
  kQueue,
  kTruncate
};

struct EffectScaleRule {
  double perQuantity = 0.0;
  int minLoops = 1;
  int maxLoops = 1;
};

struct EffectBehavior {
  EffectPlaybackMode mode = EffectPlaybackMode::kSimultaneous;
  int durationTicks = 0;
  int delayTicks = 0;
  std::optional<EffectScaleRule> scale;
};

struct SoundEffectDescriptor {
  std::string logicalId;
  std::string assetPath;
  EffectBehavior behavior;
};

class SoundLibrary {
 public:
  bool LoadDefaults(const std::string& configPath);

  std::optional<SoundEffectDescriptor> ResolveEffect(const std::string& logicalEvent) const;
  std::string ResolveMusicAsset(const std::string& trackId) const;
  std::string DefaultSoundtrackId() const;
  std::vector<std::string> AllSoundtrackIds() const;
  void SetAssetRootOverride(const std::string& assetRoot);
  void Clear();
  int SoundtrackVolumePercent() const { return soundtrackVolumePercent_; }
  int EffectsVolumePercent() const { return effectsVolumePercent_; }

 private:
  void RegisterDefaultFallbacks();

  std::unordered_map<std::string, SoundEffectDescriptor> effectAssets_;
  std::unordered_map<std::string, std::string> musicAssets_;
  std::string defaultSoundtrackId_;
  std::string baseDirectory_;
  std::string assetRootOverride_;
  int soundtrackVolumePercent_ = 100;
  int effectsVolumePercent_ = 100;
};

}  // namespace mm4::audio

#endif  // MM4_AUDIO_SOUND_LIBRARY_H_
