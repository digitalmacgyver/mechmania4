/* AudioEventTracker.h
 * Observes world snapshots and emits normalized audio effect requests.
 */

#ifndef MM4_AUDIO_AUDIO_EVENT_TRACKER_H_
#define MM4_AUDIO_AUDIO_EVENT_TRACKER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "audio/AudioTypes.h"

class CWorld;
class CShip;
class CStation;

namespace mm4::audio {

 class AudioEventTracker {
 public:
  void Reset();
  std::vector<EffectRequest> GatherEvents(const CWorld& world);
  void SetVerbose(bool verbose) { verbose_ = verbose; }
  const std::vector<std::string>& LastLaunchTransitions() const {
    return lastLaunchTransitions_;
  }
  unsigned int LastLaunchTransitionTurn() const {
    return lastTransitionTurn_;
  }

 private:
  struct ShipSnapshot {
    double shield = 0.0;
    bool alive = false;
    bool docked = false;
  };

  struct StationSnapshot {
    double vinyl = 0.0;
  };

  std::string MakeShipKey(int teamWorldIndex, unsigned int shipNumber) const;

  unsigned int lastProcessedTurn_ = 0;
  bool hasLastTurn_ = false;

  bool verbose_ = false;
  std::vector<std::string> lastLaunchTransitions_;
  unsigned int lastTransitionTurn_ = 0;

  std::unordered_map<std::string, ShipSnapshot> shipState_;
  std::unordered_map<int, StationSnapshot> stationState_;
};

}  // namespace mm4::audio

#endif  // MM4_AUDIO_AUDIO_EVENT_TRACKER_H_
