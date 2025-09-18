//MagicBag.C
// "Wrong!  I'm perfectly sane, everyone else is insane and trying to take
//  my magic bag!"

#include "MagicBag.h"

MagicBag::MagicBag(unsigned int drones, unsigned int len) {
  num_drones = drones;
  num_stuff = len;

  length = new unsigned[drones];
  table = new (Entry**) [drones];
  for (unsigned int i = 0; i < drones; i++) {
    table[i] = new (Entry*) [len];
    length[i] = 0;
  }
}

MagicBag::~MagicBag() {
  for(unsigned int i = 0; i < num_drones; i++) {
    for (unsigned int j = 0; j < length[i]; j++)
      delete table[i][j];
    delete table[i];
  }
  delete table;
  delete length;
}

Entry *MagicBag::getEntry(unsigned int drone, unsigned int elem) {
  if (drone >= num_drones || elem >= length[drone]) {
    return (Entry *)NULL;
  } else {
    return table[drone][elem];
  }
}

void MagicBag::addEntry(unsigned int drone, Entry *entry) {
  if (drone >= num_drones) {
    cerr << "DORK: Trying to add an entry to an undefined drone." << endl;
  } else {
    table[drone][ length[drone] ] = entry;
    length[drone]++;
  }
}
  
