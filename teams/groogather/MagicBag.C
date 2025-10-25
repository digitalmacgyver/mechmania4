/* MagicBag.C
 * Implementation of centralized planning data structure
 * "Wrong! I'm perfectly sane, everyone else is insane and trying to take
 *  my magic bag!"
 */


#include "MagicBag.h"

#include <iostream>

// TODO: Add consts everywhere in here on args, returns, etc. where it makes sense.

MagicBag::MagicBag() {}

MagicBag::~MagicBag() {}

const PathInfo* MagicBag::getEntry(unsigned int drone, CThing* target) const {
  auto ship_it = ship_paths.find(drone);
  if (ship_it != ship_paths.end()) {
    auto path_it = ship_it->second.find(target);
    if (path_it != ship_it->second.end()) {
      return &(path_it->second);
    }
  }
  return NULL;
}

std::unordered_map<CThing*, PathInfo>& MagicBag::getShipPaths(unsigned int drone) {
  static std::unordered_map<CThing*, PathInfo> empty_map;
  auto it = ship_paths.find(drone);
  if (it != ship_paths.end()) {
    return it->second;
  }
  return empty_map;
}

void MagicBag::addEntry(unsigned int drone, CThing* target, const PathInfo& path) {
  // Just add the entry - if the ship doesn't exist, it will be created
  ship_paths[drone][target] = path;
}
