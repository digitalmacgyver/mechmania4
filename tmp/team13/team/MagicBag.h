// MagicBag.h

#ifndef __MAGICBAG_H__
#define __MAGICBAG_H__

#include "Entry.h"

class MagicBag {
 private:
  Entry ***table;
  unsigned int *length;
  unsigned int num_drones;
  unsigned int num_stuff;

 public:
  MagicBag(unsigned int drones = 4, unsigned int len = 100);
  ~MagicBag();
  
  Entry *getEntry(unsigned int drone, unsigned int elem);
  void addEntry(unsigned int drone, Entry *entry);
};

#endif
