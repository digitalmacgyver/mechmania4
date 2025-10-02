/* MagicBag.C
 * Implementation of centralized planning data structure
 * "Wrong! I'm perfectly sane, everyone else is insane and trying to take
 *  my magic bag!"
 */

#include <iostream>

#include "MagicBag.h"
using namespace std;

MagicBag::MagicBag(unsigned int drones, unsigned int len) {
  num_drones = drones;  // Number of ships (usually 4)
  num_stuff = len;      // Max entries per ship (usually 100)

  // Allocate arrays
  length = new unsigned[drones];  // Track entries per ship
  table = new Entry **[drones];   // Array of ship arrays

  // Initialize each ship's entry list
  for (unsigned int i = 0; i < drones; ++i) {
    table[i] = new Entry *[len];  // Allocate entry array for this ship
    length[i] = 0;                // Start with 0 entries
  }
}

MagicBag::~MagicBag() {
  // Clean up all allocated memory
  for (unsigned int i = 0; i < num_drones; ++i) {
    // Delete all Entry objects for this ship
    for (unsigned int j = 0; j < length[i]; ++j) {
      delete table[i][j];
    }
    // Delete the array of Entry pointers
    delete table[i];
  }
  // Delete the main arrays
  delete table;
  delete length;
}

Entry *MagicBag::getEntry(unsigned int drone, unsigned int elem) {
  // Bounds checking
  if (drone >= num_drones || elem >= length[drone]) {
    return (Entry *)NULL;  // Invalid ship or index
  } else {
    return table[drone][elem];  // Return requested entry
  }
}

void MagicBag::addEntry(unsigned int drone, Entry *entry) {
  if (drone >= num_drones) {
    // Error: Invalid ship index
    cerr << "ERROR: Trying to add an entry to an undefined ship (" << drone
         << ")" << endl;
  } else if (length[drone] >= num_stuff) {
    // TODO: Handle overflow - currently silently drops entries past limit
    cerr << "WARNING: MagicBag full for ship " << drone << endl;
  } else {
    // Add entry to end of this ship's list
    table[drone][length[drone]] = entry;
    length[drone]++;  // Increment count
  }
}
