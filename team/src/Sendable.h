/* Sendable.h
 * Header for CSendable, all objects
 * that can be serialized over a network
 * For use with MechMania IV
 *   8/24/98 by Misha Voloshin and Erik Gilling
 */

#ifndef _SENDABLE_H_SKLDJFNWLEJKFHKLWEHFLKWEHFKLJ
#define _SENDABLE_H_SKLDJFNWLEJKFHKLWEHFLKWEHFKLJ

#include "stdafx.h"

class CSendable 
{
 public:
  CSendable();
  virtual ~CSendable();

  virtual unsigned GetSerialSize() const;
  virtual unsigned SerialPack (char *buf, unsigned buflen) const;
  virtual unsigned SerialUnpack (char *buf, unsigned buflen);
  // All three functions return size of package [processed]

  // Following functions return buflen
  unsigned BufWrite (char *dest, const char *src, unsigned buflen) const;
  unsigned BufWrite (char *dest, bool src) const;
  unsigned BufWrite (char *dest, UINT src) const;
  unsigned BufWrite (char *dest, double src) const;

  unsigned BufRead (char *src, char *dest, unsigned buflen) const;
  unsigned BufRead (char *src, bool &dest) const;
  unsigned BufRead (char *src, UINT &dest) const;
  unsigned BufRead (char *src, double &dest) const;
};

#endif // _SENDABLE_H_SKLDJFNWLEJKFHKLWEHFLKWEHFKLJ
