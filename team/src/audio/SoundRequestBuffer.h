/* SoundRequestBuffer.h
 * In-memory aggregator that coalesces effect requests per subtick before
 * dispatching them to SDL_mixer. Ensures we never enqueue duplicates within
 * the same simulation slice while still emitting all unique effects for the
 * enclosing tick.
 */

#ifndef MM4_AUDIO_SOUND_REQUEST_BUFFER_H_
#define MM4_AUDIO_SOUND_REQUEST_BUFFER_H_

#include <unordered_map>
#include <vector>

#include "audio/AudioTypes.h"

namespace mm4::audio {

class SoundRequestBuffer {
 public:
  void BeginSubtick();
  void QueueEffect(const EffectRequest& request);
  void SealSubtick();
  std::vector<EffectRequest> ConsumePending();
  void ClearAll();

 private:
  using EffectMap = std::unordered_map<std::string, EffectRequest>;

  EffectMap currentSubtick_;
  std::vector<EffectRequest> queueModeSubtick_;
  std::vector<EffectRequest> pendingFlush_;
  bool subtickOpen_ = false;
};

}  // namespace mm4::audio

#endif  // MM4_AUDIO_SOUND_REQUEST_BUFFER_H_
