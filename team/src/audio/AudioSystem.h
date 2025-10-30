/* AudioSystem.h
 * SDL_mixer backed audio coordinator for the MechMania IV observer.
 */

#ifndef MM4_AUDIO_AUDIO_SYSTEM_H_
#define MM4_AUDIO_AUDIO_SYSTEM_H_

#include <chrono>
#include <mutex>
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

  std::unordered_map<std::string, Mix_Chunk*> chunkCache_;
  std::vector<ChannelState> channels_;
  Mix_Music* activeMusic_ = nullptr;
  std::string activeMusicId_;
#else
  void ProcessPendingEffects(int currentTurn);
#endif
};

}  // namespace mm4::audio

#endif  // MM4_AUDIO_AUDIO_SYSTEM_H_
