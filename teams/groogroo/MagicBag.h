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

#include "Entry.h"

class MagicBag {
 private:
  Entry ***table;           // 2D array of Entry pointers [ship][target]
  unsigned int *length;     // Current number of entries per ship
  unsigned int num_drones;  // Number of ships (typically 4)
  unsigned int num_stuff;   // Max entries per ship (typically 100)

 public:
  // Constructor: drones = number of ships, len = max entries per ship
  MagicBag(unsigned int drones = 4, unsigned int len = 512);
  ~MagicBag();

  // Get specific entry for ship 'drone' at index 'elem'
  // Returns NULL if out of bounds
  Entry *getEntry(unsigned int drone, unsigned int elem);

  // Add new entry to ship's list (appends to end)
  void addEntry(unsigned int drone, Entry *entry);
};

#endif
