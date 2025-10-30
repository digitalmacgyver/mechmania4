/* AudioEventTracker.C
 * Implementation of AudioEventTracker.
 */

#include "audio/AudioEventTracker.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "World.h"

namespace mm4::audio {

void AudioEventTracker::Reset() {
  shipState_.clear();
  stationState_.clear();
  hasLastTurn_ = false;
  lastProcessedTurn_ = 0;
  lastLaunchTransitions_.clear();
  lastTransitionTurn_ = 0;
}

std::vector<EffectRequest> AudioEventTracker::GatherEvents(const CWorld& world) {
  std::vector<EffectRequest> events;

  unsigned int currentTurn = world.GetCurrentTurn();
  lastLaunchTransitions_.clear();
  lastTransitionTurn_ = currentTurn;
  if (hasLastTurn_ && currentTurn == lastProcessedTurn_) {
    return events;
  }

  std::unordered_map<std::string, ShipSnapshot> nextShipState;

  for (unsigned int t = 0; t < world.GetNumTeams(); ++t) {
    CTeam* team = world.GetTeam(t);
    if (!team) {
      continue;
    }

    int teamWorldIndex = static_cast<int>(team->GetWorldIndex());
    std::string teamTag = "team" + std::to_string(teamWorldIndex + 1);

    // Stations accumulate vinyl delivery totals.
    if (CStation* station = team->GetStation()) {
      double vinyl = station->GetVinylStore();
      double prevVinyl = 0.0;
      if (auto it = stationState_.find(teamWorldIndex);
          it != stationState_.end()) {
        prevVinyl = it->second.vinyl;
      }

      double delivered = vinyl - prevVinyl;
      if (delivered > 0.01) {
        EffectRequest req;
        req.logicalEvent = teamTag + ".deliver_vinyl.default";
        req.teamWorldIndex = teamWorldIndex;
        req.quantity = delivered;
        req.metadata = team->GetName();
        events.push_back(req);
      }

      stationState_[teamWorldIndex] = StationSnapshot{vinyl};
    }

    // Ships emit combat-related events.
    unsigned int shipCount = team->GetShipCount();
    for (unsigned int s = 0; s < shipCount; ++s) {
      CShip* ship = team->GetShip(s);
      if (!ship) {
        continue;
      }

      std::string key =
          MakeShipKey(teamWorldIndex, ship->GetShipNumber());

      ShipSnapshot snapshot;
      snapshot.shield = ship->GetAmount(S_SHIELD);
      snapshot.alive = ship->IsAlive();
      snapshot.docked = ship->IsDocked();

      auto prevIt = shipState_.find(key);
      if (prevIt != shipState_.end()) {
        double shieldDrop = prevIt->second.shield - snapshot.shield;
        if (shieldDrop > 0.05) {
          EffectRequest req;
          req.logicalEvent = teamTag + ".damage.shield";
          req.teamWorldIndex = teamWorldIndex;
          req.quantity = shieldDrop;
          req.metadata = ship->GetName();
          events.push_back(req);
        }

        if (prevIt->second.alive && !snapshot.alive) {
          EffectRequest req;
          req.logicalEvent = teamTag + ".ship_destroyed";
          req.teamWorldIndex = teamWorldIndex;
          req.metadata = ship->GetName();
          events.push_back(req);
        }

        if (!prevIt->second.docked && snapshot.docked) {
          EffectRequest req;
          req.logicalEvent = teamTag + ".dock.default";
          req.teamWorldIndex = teamWorldIndex;
          req.metadata = ship->GetName();
          events.push_back(req);
          if (verbose_) {
            std::cout << "[audio] dock transition ship=" << req.metadata
                      << " team=" << teamTag << " turn=" << currentTurn
                      << std::endl;
          }
        } else if (prevIt->second.docked && !snapshot.docked) {
          EffectRequest req;
          req.logicalEvent = teamTag + ".launch.default";
          req.teamWorldIndex = teamWorldIndex;
          req.metadata = ship->GetName();
          events.push_back(req);
          lastLaunchTransitions_.push_back(teamTag + ":" + req.metadata);
          if (verbose_) {
            std::cout << "[audio] launch event emitted ship=" << req.metadata
                      << " team=" << teamTag << " turn=" << currentTurn
                      << std::endl;
          }
        }
      }

      nextShipState.emplace(std::move(key), snapshot);
    }
  }

  shipState_.swap(nextShipState);
  lastProcessedTurn_ = currentTurn;
  hasLastTurn_ = true;

  return events;
}

std::string AudioEventTracker::MakeShipKey(int teamWorldIndex,
                                           unsigned int shipNumber) const {
  std::ostringstream oss;
  oss << teamWorldIndex << ":" << shipNumber;
  return oss.str();
}

}  // namespace mm4::audio
