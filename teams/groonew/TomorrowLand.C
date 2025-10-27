#include "TomorrowLand.h"

#include <algorithm>
#include <vector>

#include "GameConstants.h"
#include "Pathfinding.h"
#include "Traj.h"

namespace TomorrowLand {
namespace {
std::unordered_map<CThing*, ThingForecast> g_forecasts;

bool IsCollidable(const CThing* thing) {
  if (thing == nullptr) {
    return false;
  }
  ThingKind kind = thing->GetKind();
  return kind == SHIP || kind == STATION || kind == ASTEROID;
}

}  // namespace

void Rebuild(CWorld* world) {
  g_forecasts.clear();
  if (world == nullptr) {
    return;
  }

  std::vector<CThing*> things;
  things.reserve(64);

  for (unsigned int idx = world->UFirstIndex; idx != BAD_INDEX;
       idx = world->GetNextIndex(idx)) {
    CThing* thing = world->GetThing(idx);
    if (thing == nullptr || !thing->IsAlive()) {
      continue;
    }
    if (thing->GetKind() == GENTHING) {
      continue;
    }
    ThingForecast forecast;
    forecast.thing = thing;
    forecast.predicted_pos = thing->PredictPosition(g_game_turn_duration);
    forecast.collision_predicted = false;
    forecast.collision_time = g_no_collide_sentinel;
    g_forecasts.emplace(thing, forecast);
    things.push_back(thing);
  }

  const size_t count = things.size();
  if (count <= 1) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    CThing* first = things[i];
    if (!IsCollidable(first)) {
      continue;
    }
    for (size_t j = i + 1; j < count; ++j) {
      CThing* second = things[j];
      if (!IsCollidable(second)) {
        continue;
      }
      double collision_time = first->DetectCollisionCourse(*second);
      if (collision_time == g_no_collide_sentinel) {
        continue;
      }
      if (collision_time <= g_game_turn_duration + g_fp_error_epsilon) {
        auto& first_forecast = g_forecasts[first];
        auto& second_forecast = g_forecasts[second];
        first_forecast.collision_predicted = true;
        second_forecast.collision_predicted = true;
        if (first_forecast.collision_time == g_no_collide_sentinel ||
            collision_time < first_forecast.collision_time) {
          first_forecast.collision_time = collision_time;
        }
        if (second_forecast.collision_time == g_no_collide_sentinel ||
            collision_time < second_forecast.collision_time) {
          second_forecast.collision_time = collision_time;
        }
      }
    }
  }
}

const ThingForecast* Lookup(const CThing* thing) {
  if (thing == nullptr) {
    return nullptr;
  }
  auto it = g_forecasts.find(const_cast<CThing*>(thing));
  if (it == g_forecasts.end()) {
    return nullptr;
  }
  return &it->second;
}

const std::unordered_map<CThing*, ThingForecast>& AllForecasts() {
  return g_forecasts;
}

}  // namespace TomorrowLand
