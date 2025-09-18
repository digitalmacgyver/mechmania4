/* Station.h
 * Header file for CStation
 * Inherited off of CThing
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#ifndef _STATION_H_SDJKHSELKJFFAEJFLKA
#define _STATION_H_SDJKHSELKJFFAEJFLKA

#include "Thing.h"

class CTeam;

class CStation : public CThing
{
 public:
  CStation(CCoord StPos, CTeam* pTeam=NULL);
  virtual ~CStation();

  double GetVinylStore() const;
  double AddVinyl(double dvtons);

  // Serialization methods
  unsigned GetSerialSize() const;
  unsigned SerialPack (char *buf, unsigned buflen) const;
  unsigned SerialUnpack (char *buf, unsigned buflen);

 protected:
  double dCargo;

  virtual void HandleCollision (CThing* pOthThing, CWorld *pWorld=NULL);
};

#endif // ! _STATION_H_SDJKHSELKJFFAEJFLKA
