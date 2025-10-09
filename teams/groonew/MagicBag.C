/* MagicBag.C
 * Implementation of centralized planning data structure
 * "Wrong! I'm perfectly sane, everyone else is insane and trying to take
 *  my magic bag!"
 */

#include <iostream>

#include "MagicBag.h"

// TODO: Add consts everywhere in here on args, returns, etc. where it makes sense.

MagicBag::MagicBag() {}

MagicBag::~MagicBag() {
  // TODO: Make sure wherever we stick Entry's into the Magic Bag we use std::shared
  // or static pointers.
}

Entry* MagicBag::getEntry(unsigned int drone, CThing* target) {
  try {
    return ship_paths.at(drone).at(target);
  } catch (const std::out_of_range& e) {
    return NULL;
  }
}

void MagicBag::addEntry(unsigned int drone, CThing* target, PathInfo* path) {
  try {
    ship_paths.at(drone)
  } catch (const std::out_of_range& e) {
    std::cerr << "ERROR: Trying to add an entry to an undefined ship (" << drone
         << ")" << std::endl;
    return;
  }
  ship_paths[drone][target] = path;
  return;
}
