/* Coord.h
 * Declaration of class CCoord
 * Stores coordinate positions objects in game world
 * For use with MechMania IV
 * 4/29/98 by Misha Voloshin
 */

#ifndef _COORD_H_DJFSELKFJHELFHLWKEJFHKLWEJHF
#define _COORD_H_DJFSELKFJHELFHLWKEJFHKLWEJHF

#include "Sendable.h"
#include "stdafx.h"

const double fWXMin = -512.0;
const double fWYMin = -512.0;
const double fWXMax = 512.0;
const double fWYMax = 512.0;
const double kWorldSizeX = fWXMax - fWXMin;
const double kWorldSizeY = fWYMax - fWYMin;

class CTraj;

class CCoord : public CSendable {
 public:
  CCoord(double fx0 = 0.0, double fy0 = 0.0);
  CCoord(const CCoord& OthCrd);
  CCoord(const CTraj& OthTraj);
  virtual ~CCoord();

  double DistTo(const CCoord& OthCrd) const;  // Shortest straight-line distance
  double AngleTo(const CCoord& OthCrd) const;  // Angle in radians, trig metric
  CTraj VectTo(const CCoord& OthCrd) const;    // Vector to other point

  // Operators
  CCoord& operator-();
  CCoord& operator=(const CCoord& OthCrd);
  CCoord& operator=(const CTraj& OthTraj);

  CCoord& operator+=(const CCoord& OthCrd);
  CCoord& operator-=(const CCoord& OthCrd);

  CCoord& operator*=(const double scale);
  CCoord& operator/=(const double scale);

  bool operator==(const CCoord& OthCrd) const;
  bool operator!=(const CCoord& OthCrd) const;

  // Friends
  friend CCoord operator+(const CCoord& C1, const CCoord& C2);
  friend CCoord operator-(const CCoord& C1, const CCoord& C2);

  friend CCoord operator*(const CCoord& C1, double scale);
  friend CCoord operator*(double scale, const CCoord& C1);
  friend CCoord operator/(const CCoord& C1, double scale);

  // Values
  double fX, fY;

  void Normalize();  // Clips coordinates to game field

  // Serialization routines
  unsigned GetSerialSize() const;
  unsigned SerialPack(char* buf, unsigned buflen) const;
  unsigned SerialUnpack(char* buf, unsigned buflen);
};

#endif  // !_COORD_H_DJFSELKFJHELFHLWKEJFHKLWEJHF
