/* MagicBag.h
 * Central planning data structure for Team Groogroo
 *
 * Purpose: Stores precalculated paths from each ship to all potential targets
 * Structure: 2D array where first dimension is ship index (0-3) and
 *           second dimension is list of possible target entries
 *
 * Quote: "Wrong! I'm perfectly sane, everyone else is insane and trying to take
 *         my magic bag!"
 */

#ifndef __MAGICBAG_H__
#define __MAGICBAG_H__

#include "PathInfo.h"

#include <unordered_map>

class MagicBag {
 private:
  // Map of maps, first key is ship number, second key is pinter to CThing, PathInfo is
  // infomation on how that ship cqn get to that thing.
  std::unordered_map<unsigned int, std::unordered_map<CThing*, PathInfo>> ship_paths;

 public:
  // Constructor: drones = number of ships, len = max entries per ship
  MagicBag();
  ~MagicBag();

  // Get specific entry for ship 'drone' path to dest - returns NULL if no path
  // information exists.
  Entry *getEntry(unsigned int drone, const CThing* dest);

  // Add new entry to ship's list (appends to end)
  void addEntry(unsigned int drone, CThing* dest, PathInfo* path);
};

#endif
