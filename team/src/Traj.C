/* Traj.C
 * Handles velocities and polar coordinate math
 * For use with MechMania IV
 * Misha Voloshin 5/29/98
 */

#include <cassert>

#include "Coord.h"
#include "Traj.h"

/////////////////////////////////////////////
// Construction/Destruction

CTraj::CTraj(double frho, double ftheta) {
  rho = frho;
  theta = ftheta;
  Normalize();
}

CTraj::CTraj(const CTraj& OthTraj) { *this = OthTraj; }

CTraj::CTraj(const CCoord& OCrd) { FromCoord(OCrd); }

CTraj::~CTraj() {}

///////////////////////////////////////////////
// Methods

CCoord CTraj::ConvertToCoord() const {
  CCoord CRes(cos(theta) * rho, sin(theta) * rho);
  return CRes;
}

void CTraj::Normalize() {
  if (rho == 0.0) {
    theta = 0.0;
  }

  if (rho < 0.0) {
    rho *= -1.0;
    theta += PI;
  }

  if (theta < -PI) {
    theta = PI - fmod(-PI - theta, PI2);
  }
  if (theta > PI) {
    theta = fmod(theta + PI, PI2) - PI;
  }

  if (theta < -(PI + 0.0001) || theta > (PI + 0.0001)) {
    assert(false && "CTraj::Normalize() failed to normalize theta properly");

    // Recovery: clamp to the nearest valid boundary
    if (theta < -PI) {
      theta = -PI;
    } else if (theta > PI) {
      theta = PI;
    }

    // Log the recovery for debugging
    printf(
        "WARNING: CTraj::Normalize() recovery executed. Clamped theta to %f\n",
        theta);
  }
}

CTraj& CTraj::FromCoord(const CCoord& OCrd) {
  CCoord Origin(0.0, 0.0);
  rho = Origin.DistTo(OCrd);
  theta = Origin.AngleTo(OCrd);
  return *this;
}

CTraj& CTraj::Rotate(double dtheta) {
  theta += dtheta;
  Normalize();
  return *this;
}

double CTraj::Dot(const CTraj& OthTraj) {
  double dres = 0, dth;

  dth = OthTraj.theta - theta;
  dres = rho * OthTraj.rho * cos(dth);
  return dres;
}

double CTraj::Cross(const CTraj& OthTraj) {
  double dres = 0, dth;

  dth = OthTraj.theta - theta;
  dres = rho * OthTraj.rho * sin(dth);
  return dres;
}

////////////////////////////////////////////////
// Operators

CTraj& CTraj::operator=(const CTraj& OthTraj) {
  rho = OthTraj.rho;
  theta = OthTraj.theta;
  return *this;
}

CTraj& CTraj::operator=(const CCoord& OthCrd) {
  FromCoord(OthCrd);
  return *this;
}

CTraj& CTraj::operator+=(const CTraj& OthTraj) {
  *this = (*this + OthTraj);
  return *this;
}

CTraj& CTraj::operator-=(const CTraj& OthTraj) {
  *this = (*this - OthTraj);
  return *this;
}

CTraj& CTraj::operator-() {
  theta += PI;
  Normalize();
  return *this;
}

// HISTORICAL NOTE: These equality operators have been commented out due to
// several logical problems in their implementation:
//
// 1. PI vs -PI Issue: The operators use numerical equality (theta !=
// OthTraj.theta)
//    but PI and -PI represent the same logical direction (straight left).
//    This means CTraj(5.0, PI) == CTraj(5.0, -PI) should be TRUE logically,
//    but these operators return FALSE due to numerical difference.
//
// 2. operator!= Logic Error: The implementation is backwards:
//    "if (OthTraj==*this) return FALSE;" should be "return !(*this ==
//    OthTraj);" The current logic returns FALSE when objects ARE equal, which
//    is wrong.
//
// 3. Unused in Codebase: These operators are never actually used anywhere
//    in the codebase, suggesting they were implemented but never needed.
//
// These operators are preserved for historical interest but commented out to
// prevent unexpected behavior if someone tries to use them in the future.

/*
bool CTraj::operator== (const CTraj& OthTraj) const
{
  if (rho!=OthTraj.rho) return false;
  if (theta!=OthTraj.theta) return false;
  return true;
}

bool CTraj::operator!= (const CTraj& OthTraj) const
{
  if (OthTraj==*this) return false;
  return true;
}
*/

///////////////////////////////////////////////
// Friend functions

CTraj operator+(const CTraj& T1, const CTraj& T2) {
  double x1, y1, x2, y2;
  CCoord CCtmp;
  CTraj TRes;

  x1 = T1.rho * cos(T1.theta);
  y1 = T1.rho * sin(T1.theta);
  x2 = T2.rho * cos(T2.theta);
  y2 = T2.rho * sin(T2.theta);
  TRes.rho = hypot(x1 + x2, y1 + y2);
  TRes.theta = atan2(y1 + y2, x1 + x2);
  TRes.Normalize();
  return TRes;
}

CTraj operator-(const CTraj& T1, const CTraj& T2) {
  CTraj TRes(T2);
  TRes = -TRes;
  TRes = TRes + T1;
  TRes.Normalize();
  return TRes;
}

CTraj operator*(const CTraj& T1, double scale) {
  CTraj TRes(T1);
  TRes.rho *= scale;
  TRes.Normalize();
  return TRes;
}

CTraj operator/(const CTraj& T1, double scale) {
  CTraj TRes(T1);
  TRes.rho /= scale;
  TRes.Normalize();
  return TRes;
}

CTraj operator*(double scale, const CTraj& T1) {
  CTraj TRes(T1);
  TRes = TRes * scale;
  return TRes;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CTraj::GetSerialSize() const {
  UINT tot = 0;
  tot += BufWrite(NULL, rho);
  tot += BufWrite(NULL, theta);
  return tot;
}

unsigned CTraj::SerialPack(char* buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufWrite(vpb, rho);
  vpb += BufWrite(vpb, theta);

  return (vpb - buf);
}

unsigned CTraj::SerialUnpack(char* buf, unsigned buflen) {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufRead(vpb, rho);
  vpb += BufRead(vpb, theta);

  return (vpb - buf);
}
