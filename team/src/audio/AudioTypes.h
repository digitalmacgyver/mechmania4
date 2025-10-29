/* AudioTypes.h
 * Shared audio event/data structures for the MechMania IV observer.
 * Provides light-weight containers used by the SDL_mixer-based audio stack.
 */

#ifndef MM4_AUDIO_AUDIO_TYPES_H_
#define MM4_AUDIO_AUDIO_TYPES_H_

#include <cmath>
#include <cstdint>
#include <string>

namespace mm4::audio {

// Logical grouping for different categories of playback.
enum class EventCategory {
  kEffect,
  kMusic
};

// Aggregate quantifiable information for a single logical sound effect.
// - logicalEvent: stable identifier, e.g. "team1.dock.default"
// - quantity: optional scalar payload (damage, vinyl delivered, etc).
// - count: number of occurrences collapsed into this request.
// - teamWorldIndex: world slot associated with the originating team, -1 if
//   the event is global or not attributed to a team.
// - metadata: additional display/debug context captured opportunistically.
struct EffectRequest {
  std::string logicalEvent;
  double quantity = 0.0;
  int count = 1;
  int teamWorldIndex = -1;
  std::string metadata;
  int requestedDelayTicks = 0;
  int requestedLoops = 1;
  bool preserveDuplicates = false;

  bool IsApproximatelyEqual(const EffectRequest& other) const {
    return logicalEvent == other.logicalEvent &&
           teamWorldIndex == other.teamWorldIndex &&
           std::fabs(quantity - other.quantity) < 1e-6;
  }
};

// Simple container describing a soundtrack change or command.
struct MusicRequest {
  std::string trackId;
  bool loop = true;
  EventCategory category = EventCategory::kMusic;
};

}  // namespace mm4::audio

#endif  // MM4_AUDIO_AUDIO_TYPES_H_
