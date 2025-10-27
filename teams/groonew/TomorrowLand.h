#ifndef GROONEW_TOMORROW_LAND_H_
#define GROONEW_TOMORROW_LAND_H_

#include <unordered_map>

#include "Coord.h"
#include "GameConstants.h"
#include "Thing.h"
#include "World.h"

namespace TomorrowLand {

struct ThingForecast {
  CThing* thing = nullptr;
  CCoord predicted_pos;
  bool collision_predicted = false;
  double collision_time = g_no_collide_sentinel;
};

// Rebuilds the forecast cache for all alive world objects over the standard
// one-turn horizon. Should be called once per turn before tactical logic runs.
void Rebuild(CWorld* world);

// Retrieves the cached forecast entry for the given thing, or nullptr if the
// thing was not cached this turn.
const ThingForecast* Lookup(const CThing* thing);

// Provides read-only access to all cached forecasts.
const std::unordered_map<CThing*, ThingForecast>& AllForecasts();

}  // namespace TomorrowLand

#endif  // GROONEW_TOMORROW_LAND_H_
