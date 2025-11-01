/* AudioSystem.h
 * SDL_mixer backed audio coordinator for the MechMania IV observer.
 */

#ifndef MM4_AUDIO_AUDIO_SYSTEM_H_
#define MM4_AUDIO_AUDIO_SYSTEM_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio/AudioTypes.h"
#include "audio/SoundLibrary.h"
#include "audio/SoundRequestBuffer.h"

struct _Mix_Music;
typedef struct _Mix_Music Mix_Music;
struct Mix_Chunk;

namespace mm4::audio {

class AudioSystem {
 public:
  static AudioSystem& Instance();

  bool Initialize(const std::string& configPath,
                  const std::string& assetsRoot = std::string(),
                  bool verbose = false);
  void Shutdown();

  void BeginSubtick();
  void QueueEffect(const EffectRequest& request);
  void EndSubtick();
  void FlushPending(int currentTurn);

  void PauseEffects();
  void ResumeEffects();
  void SetMuted(bool muted);
  void SetMusicMuted(bool muted);
  void SetEffectsMuted(bool muted);

  bool IsInitialized() const { return initialized_; }
  bool IsMuted() const { return muted_; }
  bool EffectsPaused() const { return effectsPaused_; }
  bool MusicMuted() const { return musicMuted_; }
  bool EffectsMuted() const { return effectsMuted_; }
  bool Verbose() const { return verbose_; }
  void NextTrack(bool fromManual = false);
  std::string ActiveTrackId() const { return activeMusicId_; }
  std::vector<std::string> PlaylistSnapshot() const;
  void SetPlaylistSeed(uint32_t seed);
  uint32_t PlaylistSeed() const { return playlistSeed_; }
  void RefreshPlaylist();
  void OnTrackFinished();

 private:
  AudioSystem() = default;
  ~AudioSystem() = default;

  AudioSystem(const AudioSystem&) = delete;
  AudioSystem& operator=(const AudioSystem&) = delete;

  bool initialized_ = false;
  bool muted_ = false;
  bool effectsPaused_ = false;
  bool musicMuted_ = false;
  bool effectsMuted_ = false;
  bool verbose_ = false;
  std::chrono::steady_clock::time_point lastMusicLog_;
  std::chrono::steady_clock::time_point lastEffectLog_;
  bool musicPlayingReported_ = false;

  SoundLibrary library_;
  SoundRequestBuffer requestBuffer_;
  std::unordered_map<std::string, int> queueTailTicks_;

  struct ScheduledEffect {
    EffectRequest request;
    SoundEffectDescriptor descriptor;
    int scheduledTick = 0;
  };

  std::vector<ScheduledEffect> pendingEffects_;
  int lastServiceTurn_ = 0;

#ifdef MM4_USE_SDL_MIXER
  struct ChannelState {
    std::string logicalId;
    int loopsRemaining = 0;
    int durationTicks = 0;
    int channel = -1;
    bool enforceDuration = false;
  };

  Mix_Chunk* LoadEffectChunk(const SoundEffectDescriptor& descriptor);
  void ReleaseAllChunks();
  void ReleaseAllMusic();
  void ServiceActiveChannels(int currentTurn);
  void ProcessPendingEffects(int currentTurn);
  void DispatchEffect(const ScheduledEffect& pending);
  void EnsureMusicPlaying();
  bool StartTrack(const std::string& trackId, bool manualSource);
  void AdvancePlaylist(bool manualSource);
  void ReshufflePlaylist();
  void StopChannelsForLogicalId(const std::string& logicalId);
  void BeginMusicDuck();
  void EndMusicDuck();
  bool IsGameWonEffect(const std::string& logicalId) const;

  std::unordered_map<std::string, Mix_Chunk*> chunkCache_;
  std::vector<ChannelState> channels_;
  Mix_Music* activeMusic_ = nullptr;
  bool nextMenuToggleUsesAlt_ = false;
  std::atomic<bool> musicAdvancePending_{false};
#else
  void ProcessPendingEffects(int currentTurn);
#endif
  std::vector<std::string> playlistOrder_;
  size_t playlistIndex_ = 0;
  std::mt19937 playlistRng_{0x4D4D534F};  // Stable seed "MM5O"
  std::vector<std::string> basePlaylist_;
  std::string activeMusicId_;
  uint32_t playlistSeed_ = 0x4D4D534F;
  bool playlistSeedOverridden_ = false;
#ifdef MM4_USE_SDL_MIXER
  int musicDuckingCount_ = 0;
  int musicPrevVolume_ = 128;
  int musicConfiguredVolume_ = 128;
  int effectsConfiguredVolume_ = 128;
#endif
  void RebuildPlaylistOrder();
  void LogPlaylistState(const char* context) const;
};

}  // namespace mm4::audio

#endif  // MM4_AUDIO_AUDIO_SYSTEM_H_
