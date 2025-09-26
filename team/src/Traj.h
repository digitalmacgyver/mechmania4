/* Traj.h
 * CTraj, trajectory class
 * Handles velocity
 * Kinda like CCoord but with polars
 * For use with MechMania IV
 * Misha Voloshin 5/29/98
 */

#ifndef _TRAJ_H_ESFJHFLKJWEJKLFH
#define _TRAJ_H_ESFJHFLKJWEJKLFH

#include "stdafx.h"
#include "Sendable.h"

class CCoord;

const double PIi=3.1415926,
  PIi2=6.2831853;

class CTraj : public CSendable
{
 public:
  CTraj(double frho=0.0,double ftheta=0.0);
  CTraj(const CTraj& OthTraj);
  CTraj(const CCoord& OCrd);
  ~CTraj();

  CCoord ConvertToCoord() const;
  CTraj& FromCoord(const CCoord& OCrd);
  CTraj& Rotate(double dtheta);

  double Dot(const CTraj& OthTraj);
  double Cross(const CTraj& OthTraj);

  // Operators
  CTraj& operator= (const CTraj& OthTraj);
  CTraj& operator= (const CCoord& OthCrd);
  CTraj& operator+= (const CTraj& OthTraj);
  CTraj& operator-= (const CTraj& OthTraj);
  CTraj& operator- ();

  // HISTORICAL NOTE: Equality operators commented out due to logical problems
  // See Traj.C for detailed explanation of issues with PI/-PI equivalence
  // and backwards operator!= logic. These are preserved for historical
  // interest but disabled to prevent unexpected behavior.
  // bool operator== (const CTraj& OthTraj) const;
  // bool operator!= (const CTraj& OthTraj) const;

  // Friends
  friend CTraj operator+ (const CTraj& T1, const CTraj& T2);
  friend CTraj operator- (const CTraj& T1, const CTraj& T2);

  friend CTraj operator* (const CTraj& T1, double scale);
  friend CTraj operator* (double scale, const CTraj& T1);
  friend CTraj operator/ (const CTraj& T1, double scale);

  // Values
  double rho,theta;

  void Normalize();   // Keeps -PI<theta<PI, rho>0.0

  // Serialization routines
  unsigned GetSerialSize() const;
  unsigned SerialPack (char *buf, unsigned buflen) const;
  unsigned SerialUnpack (char *buf, unsigned buflen);
};

#endif // ! _TRAJ_H_ESFJHFLKJWEJKLFH
