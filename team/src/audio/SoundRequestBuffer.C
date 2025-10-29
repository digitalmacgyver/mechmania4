/* SoundRequestBuffer.C
 * Implementation of SoundRequestBuffer.
 */

#include "audio/SoundRequestBuffer.h"

#include <algorithm>
#include <utility>

namespace mm4::audio {

void SoundRequestBuffer::BeginSubtick() {
  if (subtickOpen_) {
    return;
  }
  currentSubtick_.clear();
  queueModeSubtick_.clear();
  subtickOpen_ = true;
}

void SoundRequestBuffer::QueueEffect(const EffectRequest& request) {
  if (!subtickOpen_) {
    BeginSubtick();
  }

  if (request.preserveDuplicates) {
    queueModeSubtick_.push_back(request);
    return;
  }

  auto key = request.logicalEvent + "#" + std::to_string(request.teamWorldIndex);
  auto it = currentSubtick_.find(key);
  if (it == currentSubtick_.end()) {
    currentSubtick_.emplace(std::move(key), request);
  } else {
    it->second.count += request.count;
    it->second.quantity += request.quantity;
    it->second.requestedLoops =
        std::max(it->second.requestedLoops, request.requestedLoops);
    if (!request.metadata.empty()) {
      if (!it->second.metadata.empty()) {
        it->second.metadata.append("; ");
      }
      it->second.metadata.append(request.metadata);
    }
  }
}

void SoundRequestBuffer::SealSubtick() {
  if (!subtickOpen_) {
    return;
  }

  for (const auto& request : queueModeSubtick_) {
    pendingFlush_.push_back(request);
  }
  queueModeSubtick_.clear();

  for (auto& entry : currentSubtick_) {
    pendingFlush_.push_back(std::move(entry.second));
  }

  currentSubtick_.clear();
  subtickOpen_ = false;
}

std::vector<EffectRequest> SoundRequestBuffer::ConsumePending() {
  std::vector<EffectRequest> out;
  out.swap(pendingFlush_);
  return out;
}

void SoundRequestBuffer::ClearAll() {
  currentSubtick_.clear();
  queueModeSubtick_.clear();
  pendingFlush_.clear();
  subtickOpen_ = false;
}

}  // namespace mm4::audio
