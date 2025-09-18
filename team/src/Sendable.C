/* Sendable.C
 * Implementation CSendable, base class for all
 * objects that can be serialized over a network
 * For use with MechMania IV
 *   8/24/98 by Misha Voloshin and Erik Gilling
 */

#include <netinet/in.h>     // For byte-order conversion code
#include "Sendable.h"

//////////////////////////////////////////
// Construction/Destruction

CSendable::CSendable()
{
  
}

CSendable::~CSendable()
{

}

////////////////////////////////////////////
// Virtual methods

unsigned CSendable::GetSerialSize() const
{
  return 0;
}

unsigned CSendable::SerialPack (char *buf, unsigned buflen) const
{
  if (buf==NULL) return (UINT)-1;
  if (buflen<GetSerialSize()) return (UINT)-1;
  return 0;
}
  
unsigned CSendable::SerialUnpack (char *buf, unsigned buflen)
{
  if (buf==NULL) return (UINT)-1;
  if (buflen<GetSerialSize()) return (UINT)-1;
  return 0;
}

///////////////////////////////////////////////
// Assistants

// Writing out

unsigned CSendable::BufWrite (char *dest, const char *src, unsigned buflen) const
{
  if (dest!=NULL)
    memcpy (dest, src, buflen);

  return buflen;
}

unsigned CSendable::BufWrite (char *dest, bool src) const
{
  UINT buflen=sizeof(UINT);
  UINT val = htonl((UINT)src);

  if (dest!=NULL) 
    memcpy (dest, &val, buflen);

  return buflen;
}

unsigned CSendable::BufWrite (char *dest, UINT src) const
{
  UINT val = htonl(src);
  UINT buflen=sizeof(UINT);

  if (dest!=NULL)
    memcpy (dest, &val, buflen);

  return buflen;
}

unsigned CSendable::BufWrite (char *dest, double src) const
{
  int val = (int)(src*1000.0);
  UINT buflen=sizeof(int);
  val = htonl(val);

  if (dest!=NULL)
    memcpy (dest, &val, buflen);

  return buflen;
}

// Reading in

unsigned CSendable::BufRead (char *src, char *dest, unsigned buflen) const
{
  memcpy (dest,src, buflen);
  return buflen;
}

unsigned CSendable::BufRead (char *src, bool &dest) const
{
  UINT buflen=sizeof(UINT);
  UINT val;

  memcpy (&val,src, buflen);

  dest=(bool)ntohl(val);
  return buflen;
}

unsigned CSendable::BufRead (char *src, UINT &dest) const
{
  UINT val;
  UINT buflen=sizeof(UINT);

  memcpy (&val,src, buflen);

  dest=ntohl(val);
  return buflen;
}

unsigned CSendable::BufRead (char *src, double &dest) const
{
  int val;
  UINT buflen=sizeof(int);

  memcpy (&val,src, buflen);
  val = ntohl(val);

  dest = ((double)val)/1000.0;
  return buflen;
}
