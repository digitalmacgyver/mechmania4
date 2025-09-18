/* Coord.h
 * Declaration of class CCoord
 * Stores coordinate positions objects in game world
 * For use with MechMania IV
 * 4/29/98 by Misha Voloshin
*/

#ifndef _COORD_H_DJFSELKFJHELFHLWKEJFHKLWEJHF
#define _COORD_H_DJFSELKFJHELFHLWKEJFHKLWEJHF

#define PI 3.14159
#define PI2 1.57079

#include "stdafx.h"
#include "Sendable.h"

const double fWXMin=-512.0,fWYMin=-512.0,
  fWXMax=512.0,fWYMax=512.0;

class CTraj;

class CCoord : public CSendable
{
 public:
  CCoord (double fx0=0.0, double fy0=0.0);
  CCoord (const CCoord& OthCrd);
  CCoord (const CTraj& OthTraj);
  virtual ~CCoord();

  double DistTo (const CCoord& OthCrd) const;  // Shortest straight-line distance
  double AngleTo (const CCoord& OthCrd) const; // Angle in radians, trig metric
  CTraj VectTo (const CCoord& OthCrd) const;  // Vector to other point

  // Operators
  CCoord& operator- ();
  CCoord& operator= (const CCoord& OthCrd);
  CCoord& operator= (const CTraj& OthTraj);

  CCoord& operator+= (const CCoord& OthCrd);
  CCoord& operator-= (const CCoord& OthCrd);

  CCoord& operator*= (const double scale);
  CCoord& operator/= (const double scale);

  BOOL operator==(const CCoord& OthCrd) const;
  BOOL operator!=(const CCoord& OthCrd) const;

  // Friends
  friend CCoord operator+ (const CCoord& C1, const CCoord& C2);
  friend CCoord operator- (const CCoord& C1, const CCoord& C2);

  friend CCoord operator* (const CCoord& C1, double scale);
  friend CCoord operator* (double scale, const CCoord& C1);
  friend CCoord operator/ (const CCoord& C1, double scale);

  // Values
  double fX,fY;

  void Normalize();  // Clips coordinates to game field

  // Serialization routines
  unsigned GetSerialSize() const;
  unsigned SerialPack (char *buf, unsigned buflen) const;
  unsigned SerialUnpack (char *buf, unsigned buflen);
};

#endif // !_COORD_H_DJFSELKFJHELFHLWKEJFHKLWEJHF
