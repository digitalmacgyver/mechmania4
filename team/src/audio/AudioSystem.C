/* AudioSystem.C
 * SDL_mixer backed audio coordinator for the MechMania IV observer.
 */

#include "audio/AudioSystem.h"

#ifdef MM4_USE_SDL_MIXER
#include <SDL2/SDL_mixer.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

namespace mm4::audio {

namespace {
#ifdef MM4_USE_SDL_MIXER
constexpr int kSampleRate = 44100;
constexpr int kAudioChannels = 2;
constexpr int kChunkSize = 4096;
constexpr int kInitFlags = MIX_INIT_MP3 | MIX_INIT_MOD;

void LogMixerError(const std::string& label) {
  std::cerr << "[audio] " << label << " failed: " << Mix_GetError()
            << std::endl;
}

Mix_Chunk* LoadChunk(const std::string& path) {
  Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
  if (!chunk) {
    LogMixerError("Mix_LoadWAV");
    std::cerr << "[audio] Mix_LoadWAV failed for " << path << std::endl;
    std::cout << "[audio] failed to load chunk=" << path << std::endl;
  }
  return chunk;
}

Mix_Music* LoadMusic(const std::string& path) {
  Mix_Music* music = Mix_LoadMUS(path.c_str());
  if (!music) {
    LogMixerError("Mix_LoadMUS");
    std::cerr << "[audio] Mix_LoadMUS failed for " << path << std::endl;
    std::cout << "[audio] failed to load music=" << path << std::endl;
  }
  return music;
}
#endif
}  // namespace

AudioSystem& AudioSystem::Instance() {
  static AudioSystem instance;
  return instance;
}

bool AudioSystem::Initialize(const std::string& configPath,
                             const std::string& assetsRoot,
                             bool verbose) {
  if (initialized_) {
    return true;
  }

#ifdef MM4_USE_SDL_MIXER
  int initResult = Mix_Init(kInitFlags);
  if ((initResult & kInitFlags) != kInitFlags) {
    std::cerr << "[audio] SDL_mixer init missing capabilities. Requested flags="
              << kInitFlags << " got=" << initResult << std::endl;
    std::cout << "[audio] SDL_mixer capabilities missing flags=" << kInitFlags
              << " got=" << initResult << std::endl;
    // Continue even if optional codecs missing; WAV will still function.
  }

  if (Mix_OpenAudio(kSampleRate, MIX_DEFAULT_FORMAT, kAudioChannels,
                    kChunkSize) != 0) {
    LogMixerError("Mix_OpenAudio");
    Mix_Quit();
    std::cout << "[audio] Mix_OpenAudio failed" << std::endl;
    return false;
  }

  Mix_ChannelFinished(nullptr);  // placeholder; will attach handler later

  verbose_ = verbose;
  library_.SetAssetRootOverride(assetsRoot);
  if (!library_.LoadDefaults(configPath)) {
    std::cerr << "[audio] Warning: sound library failed to load defaults."
              << std::endl;
  }

  requestBuffer_.ClearAll();
  pendingEffects_.clear();
  queueTailTicks_.clear();
  lastServiceTurn_ = 0;
  chunkCache_.clear();
  channels_.clear();
  ReleaseAllMusic();
  initialized_ = true;
  muted_ = false;
  effectsPaused_ = false;
  musicMuted_ = false;
  effectsMuted_ = false;
  lastMusicLog_ = std::chrono::steady_clock::time_point::min();
  lastEffectLog_ = std::chrono::steady_clock::time_point::min();
  musicPlayingReported_ = false;
  std::cout << "[audio] SDL_mixer initialized (rate=" << kSampleRate
            << "Hz, channels=" << kAudioChannels << ")" << std::endl;
  EnsureMusicPlaying();
  return true;
#else
  verbose_ = verbose;
  library_.SetAssetRootOverride(assetsRoot);
  if (!library_.LoadDefaults(configPath)) {
    std::cerr << "[audio] Warning: sound library failed to load defaults."
              << std::endl;
  }

  requestBuffer_.ClearAll();
  pendingEffects_.clear();
  queueTailTicks_.clear();
  lastServiceTurn_ = 0;
  initialized_ = true;
  muted_ = false;
  effectsPaused_ = false;
  musicMuted_ = false;
  effectsMuted_ = false;
  lastMusicLog_ = std::chrono::steady_clock::time_point::min();
  lastEffectLog_ = std::chrono::steady_clock::time_point::min();
  musicPlayingReported_ = false;
  std::cerr << "[audio] SDL_mixer not available; audio playback disabled."
            << std::endl;
  return true;
#endif
}

void AudioSystem::Shutdown() {
  if (!initialized_) {
    return;
  }

  requestBuffer_.ClearAll();
  pendingEffects_.clear();
  queueTailTicks_.clear();
  library_.Clear();

#ifdef MM4_USE_SDL_MIXER
  ReleaseAllChunks();
  ReleaseAllMusic();
  channels_.clear();
  chunkCache_.clear();
  Mix_CloseAudio();
  Mix_Quit();
#endif
  lastServiceTurn_ = 0;
  initialized_ = false;
  muted_ = false;
  effectsPaused_ = false;
  musicMuted_ = false;
  effectsMuted_ = false;
  verbose_ = false;
  musicPlayingReported_ = false;
  std::cout << "[audio] SDL_mixer shutdown complete." << std::endl;
}

void AudioSystem::BeginSubtick() {
  if (!initialized_ || effectsPaused_) {
    return;
  }
  requestBuffer_.BeginSubtick();
}

namespace {
int ComputeRequestedLoops(const EffectRequest& request,
                          const SoundEffectDescriptor& descriptor) {
  const auto& behavior = descriptor.behavior;
  if (!behavior.scale.has_value()) {
    return std::max(1, request.requestedLoops);
  }

  const auto& scale = behavior.scale.value();
  if (scale.perQuantity <= 0.0) {
    int loops = std::max(scale.minLoops, request.requestedLoops);
    return std::clamp(loops, scale.minLoops, scale.maxLoops);
  }

  double quantity = std::max(0.0, request.quantity);
  double scaled = quantity / scale.perQuantity;
  int computed = static_cast<int>(std::ceil(scaled));
  if (computed <= 0) {
    computed = scale.minLoops;
  }
  return std::clamp(computed, scale.minLoops, scale.maxLoops);
}

int ComputeDurationTicks(const EffectRequest& request,
                         const SoundEffectDescriptor& descriptor) {
  int base = descriptor.behavior.durationTicks;
  if (base <= 0) {
    base = 1;
  }
  int loops = std::max(1, request.requestedLoops);
  return base * loops;
}
}  // namespace

void AudioSystem::QueueEffect(const EffectRequest& request) {
  if (!initialized_ || effectsPaused_) {
    return;
  }

  auto descriptorOpt = library_.ResolveEffect(request.logicalEvent);
  if (!descriptorOpt.has_value()) {
    std::cerr << "[audio] Missing asset for logical event "
              << request.logicalEvent << std::endl;
    return;
  }

  const auto& descriptor = descriptorOpt.value();

  EffectRequest enriched = request;
  if (enriched.requestedDelayTicks <= 0) {
    enriched.requestedDelayTicks = descriptor.behavior.delayTicks;
  }
  enriched.requestedLoops = ComputeRequestedLoops(enriched, descriptor);
  enriched.preserveDuplicates =
      descriptor.behavior.mode == EffectPlaybackMode::kQueue;

  requestBuffer_.QueueEffect(enriched);
}

void AudioSystem::EndSubtick() {
  if (!initialized_) {
    return;
  }
  requestBuffer_.SealSubtick();
}

void AudioSystem::FlushPending(int currentTurn) {
  if (!initialized_) {
    return;
  }

#ifdef MM4_USE_SDL_MIXER
  ServiceActiveChannels(currentTurn);
#else
  lastServiceTurn_ = currentTurn;
#endif
  ProcessPendingEffects(currentTurn);

  auto pending = requestBuffer_.ConsumePending();
  for (auto& effect : pending) {
    auto descriptorOpt = library_.ResolveEffect(effect.logicalEvent);
    if (!descriptorOpt.has_value() || descriptorOpt->assetPath.empty()) {
      std::cerr << "[audio] Missing asset for logical event "
                << effect.logicalEvent << std::endl;
      continue;
    }

    ScheduledEffect scheduled;
    scheduled.request = effect;
    scheduled.descriptor = descriptorOpt.value();

    int startTick = currentTurn + effect.requestedDelayTicks;
    if (scheduled.descriptor.behavior.mode == EffectPlaybackMode::kQueue) {
      int tail = 0;
      if (auto it = queueTailTicks_.find(scheduled.descriptor.logicalId);
          it != queueTailTicks_.end()) {
        tail = it->second;
      }
      if (tail > startTick) {
        startTick = tail;
      }
      queueTailTicks_[scheduled.descriptor.logicalId] =
          startTick + ComputeDurationTicks(effect, scheduled.descriptor);
    }

    scheduled.scheduledTick = startTick;
    pendingEffects_.push_back(std::move(scheduled));

    std::cout << "[audio] tick=" << currentTurn << " schedule event="
              << effect.logicalEvent << " start_tick=" << startTick
              << " loops=" << effect.requestedLoops;
    if (!effect.metadata.empty()) {
      std::cout << " metadata=" << effect.metadata;
    }
    std::cout << std::endl;
  }

  ProcessPendingEffects(currentTurn);

  if (verbose_ && pending.empty()) {
    auto now = std::chrono::steady_clock::now();
    if (lastEffectLog_ == std::chrono::steady_clock::time_point::min() ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEffectLog_).count() >=
            1000) {
      std::cout << "[audio] no effects scheduled" << std::endl;
      lastEffectLog_ = now;
    }
  }
}

#ifdef MM4_USE_SDL_MIXER

Mix_Chunk* AudioSystem::LoadEffectChunk(
    const SoundEffectDescriptor& descriptor) {
  if (auto it = chunkCache_.find(descriptor.assetPath);
      it != chunkCache_.end()) {
    return it->second;
  }

  Mix_Chunk* chunk = LoadChunk(descriptor.assetPath);
  if (!chunk) {
    std::cerr << "[audio] Failed to load chunk " << descriptor.assetPath
              << std::endl;
    return nullptr;
  }

  chunkCache_[descriptor.assetPath] = chunk;
  return chunk;
}

void AudioSystem::ReleaseAllChunks() {
  for (auto& entry : chunkCache_) {
    if (entry.second) {
      Mix_FreeChunk(entry.second);
    }
  }
  chunkCache_.clear();
}

void AudioSystem::ReleaseAllMusic() {
  if (activeMusic_) {
    Mix_HaltMusic();
    Mix_FreeMusic(activeMusic_);
    activeMusic_ = nullptr;
  }
  activeMusicId_.clear();
  musicPlayingReported_ = false;
}

void AudioSystem::ServiceActiveChannels(int currentTurn) {
  int delta = currentTurn - lastServiceTurn_;
  if (delta <= 0) {
    return;
  }

  auto it = channels_.begin();
  while (it != channels_.end()) {
    it->durationTicks -= delta;
    bool playing = it->channel >= 0 && Mix_Playing(it->channel) != 0;
    if (it->durationTicks <= 0 || !playing) {
      if (playing && it->channel >= 0) {
        Mix_HaltChannel(it->channel);
      }
      it = channels_.erase(it);
    } else {
      ++it;
    }
  }

  lastServiceTurn_ = currentTurn;
}

void AudioSystem::ProcessPendingEffects(int currentTurn) {
  EnsureMusicPlaying();

  auto it = pendingEffects_.begin();
  while (it != pendingEffects_.end()) {
    if (it->scheduledTick <= currentTurn) {
      DispatchEffect(*it);
      it = pendingEffects_.erase(it);
    } else {
      ++it;
    }
  }
  if (lastServiceTurn_ < currentTurn) {
    lastServiceTurn_ = currentTurn;
  }
}

void AudioSystem::DispatchEffect(const ScheduledEffect& pending) {
  if (effectsMuted_) {
    return;
  }

  Mix_Chunk* chunk = LoadEffectChunk(pending.descriptor);
  if (!chunk) {
    return;
  }

  int loops = std::max(1, pending.request.requestedLoops);
  int channel = Mix_PlayChannel(-1, chunk, loops - 1);
  if (channel == -1) {
    LogMixerError("Mix_PlayChannel");
    return;
  }

  ChannelState state;
  state.logicalId = pending.descriptor.logicalId;
  state.loopsRemaining = loops;
  state.durationTicks = ComputeDurationTicks(pending.request, pending.descriptor);
  state.channel = channel;
  channels_.push_back(state);

  std::cout << "[audio] effect playing event=" << pending.request.logicalEvent
            << " channel=" << channel << std::endl;

  if (verbose_) {
    auto now = std::chrono::steady_clock::now();
    if (lastEffectLog_ == std::chrono::steady_clock::time_point::min() ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEffectLog_).count() >=
            1000) {
      std::cout << "[audio] effect playing event=" << pending.request.logicalEvent
                << " channel=" << channel << std::endl;
      lastEffectLog_ = now;
    }
  }
}

void AudioSystem::EnsureMusicPlaying() {
  if (musicMuted_) {
#ifdef MM4_USE_SDL_MIXER
    if (Mix_PlayingMusic() != 0) {
      Mix_HaltMusic();
    }
#endif
    musicPlayingReported_ = false;
    return;
  }

#ifdef MM4_USE_SDL_MIXER
  if (Mix_PlayingMusic() != 0) {
    if (verbose_) {
      auto now = std::chrono::steady_clock::now();
    if (!musicPlayingReported_ ||
        lastMusicLog_ == std::chrono::steady_clock::time_point::min() ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMusicLog_).count() >=
            1000) {
      std::cout << "[audio] music playing track="
                << (activeMusicId_.empty() ? "<unknown>" : activeMusicId_)
                << std::endl;
      lastMusicLog_ = now;
      musicPlayingReported_ = true;
    }
    }
    return;
  }

  std::string defaultId = library_.DefaultSoundtrackId();
  if (defaultId.empty()) {
    return;
  }

  if (!activeMusic_ || activeMusicId_ != defaultId) {
    ReleaseAllMusic();
    std::string asset = library_.ResolveMusicAsset(defaultId);
    if (asset.empty()) {
      std::cerr << "[audio] Missing music asset for track " << defaultId
                << std::endl;
      return;
    }
    activeMusic_ = LoadMusic(asset);
    if (!activeMusic_) {
      return;
    }
    activeMusicId_ = defaultId;
    if (verbose_) {
      std::cout << "[audio] music loaded track=" << asset << std::endl;
    }
  }

  if (activeMusic_) {
    if (Mix_PlayMusic(activeMusic_, -1) != 0) {
      LogMixerError("Mix_PlayMusic");
      ReleaseAllMusic();
      musicPlayingReported_ = false;
      return;
    }
    if (verbose_) {
      std::cout << "[audio] music start track="
                << (activeMusicId_.empty() ? "<unknown>" : activeMusicId_)
                << std::endl;
    }
    lastMusicLog_ = std::chrono::steady_clock::now();
    musicPlayingReported_ = true;
  }
#endif
}

#else  // MM4_USE_SDL_MIXER

void AudioSystem::ProcessPendingEffects(int currentTurn) {
  auto it = pendingEffects_.begin();
  while (it != pendingEffects_.end()) {
    if (it->scheduledTick <= currentTurn) {
      if (!effectsMuted_) {
        const auto& effect = it->request;
        const auto& descriptor = it->descriptor;
        std::cout << "[audio] (stub) tick=" << currentTurn << " dispatch event="
                  << effect.logicalEvent << " asset=" << descriptor.assetPath
                  << " loops=" << effect.requestedLoops << std::endl;
      }
      it = pendingEffects_.erase(it);
    } else {
      ++it;
    }
  }
  lastServiceTurn_ = currentTurn;
}

#endif  // MM4_USE_SDL_MIXER

void AudioSystem::PauseEffects() {
  effectsPaused_ = true;
#ifdef MM4_USE_SDL_MIXER
  Mix_HaltChannel(-1);
  channels_.clear();
#endif
}

void AudioSystem::ResumeEffects() {
  effectsPaused_ = false;
}

void AudioSystem::SetMuted(bool muted) {
  SetMusicMuted(muted);
  SetEffectsMuted(muted);
  muted_ = musicMuted_ && effectsMuted_;
}

void AudioSystem::SetMusicMuted(bool muted) {
  musicMuted_ = muted;
#ifdef MM4_USE_SDL_MIXER
  if (musicMuted_) {
    Mix_VolumeMusic(0);
    Mix_HaltMusic();
    musicPlayingReported_ = false;
  } else {
    Mix_VolumeMusic(MIX_MAX_VOLUME);
    EnsureMusicPlaying();
  }
#else
  (void)muted;
#endif
  muted_ = musicMuted_ && effectsMuted_;
}

void AudioSystem::SetEffectsMuted(bool muted) {
  effectsMuted_ = muted;
#ifdef MM4_USE_SDL_MIXER
  int volume = effectsMuted_ ? 0 : MIX_MAX_VOLUME;
  Mix_Volume(-1, volume);
  if (effectsMuted_) {
    Mix_HaltChannel(-1);
    channels_.clear();
  }
#else
  (void)muted;
#endif
  muted_ = musicMuted_ && effectsMuted_;
}

}  // namespace mm4::audio
