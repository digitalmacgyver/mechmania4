/* Coord.C
 * Definition of class CCoord
 * Handles coordinate math 'n stuff
 * For use with MechMania IV
 * 4/29/98 by Misha Voloshin
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "Coord.h"
#include "GameConstants.h"
#include "Traj.h"

//////////////////////////////////////////////////
// Construction/destruction

CCoord::CCoord(double fx0, double fy0) {
  fX = fx0;
  fY = fy0;
}

CCoord::CCoord(const CCoord& OthCrd) { *this = OthCrd; }

CCoord::CCoord(const CTraj& OthTraj) { *this = OthTraj.ConvertToCoord(); }

CCoord::~CCoord() {}

/////////////////////////////////////////////////
// Methods

void CCoord::Normalize() {
  // --- Modernized Normalization: Readability and Standard Interval [Min, Max)
  // ---
  //
  // Goal: Normalize coordinates into the conventional half-open interval [Min,
  // Max). In this world: [-512.0, 512.0). This means -512.0 is included, but
  // 512.0 is excluded (it wraps around to -512.0).

  // The strategy is to implement the mathematical modulo operation. C++'s
  // fmod() calculates the remainder, which preserves the sign (e.g., fmod(-1,
  // 1024) == -1). Mathematical modulo always returns a positive result (e.g.,
  // mod(-1, 1024) == 1023).

  // --- X-Axis Normalization ---

  // 1. Shift the coordinate so it is relative to the minimum boundary (starts
  // at 0).
  double offsetX = fX - fWXMin;

  // 2. Calculate the remainder using fmod. This might be negative.
  double remainderX = fmod(offsetX, kWorldSizeX);

  // 3. Correct negative remainders to achieve true mathematical modulo.
  if (remainderX < 0.0) {
    remainderX += kWorldSizeX;
  }
  // remainderX is now guaranteed to be in the range [0, kWorldSizeX).

  // 4. Shift back to the world coordinate range [fWXMin, fWXMax).
  fX = remainderX + fWXMin;

  // --- Y-Axis Normalization (Identical logic) ---

  double offsetY = fY - fWYMin;
  double remainderY = fmod(offsetY, kWorldSizeY);

  if (remainderY < 0.0) {
    remainderY += kWorldSizeY;
  }

  fY = remainderY + fWYMin;

  /*
  // --- Comparison with Legacy Implementation and Asymmetry Resolution ---
  // The commented-out legacy implementation (below) used complex arithmetic
  optimized
  // for older hardware. This resulted in asymmetric boundary handling:
  //
  // Legacy Positive Overflow (fX > Max): Normalized to [Min, Max).
  // Legacy Negative Underflow (fX < Min): Normalized to (Min, Max].
  //
  // This new implementation consistently uses [Min, Max) for all inputs,
  resolving the asymmetry.
  //
  // Corner Case Difference:
  // Consider an input exactly on a negative boundary multiple, e.g., fX =
  -1536.0 (-512.0 - 1024.0).
  // - Legacy Behavior: Normalized this to 512.0 (Max).
  // - New Behavior:    Normalizes this consistently to -512.0 (Min).
  */

  /* Legacy Positional Normalization Implementation
  if (fX<fWXMin) {
    fX = fmod(fWXMin-fX,(fWXMax-fWXMin))+fWXMin;
    fX = fWXMax-fX+fWXMin;
  }
  if (fY<fWYMin) {
    fY = fmod(fWYMin-fY,(fWYMax-fWYMin))+fWYMin;
    fY = fWYMax-fY+fWYMin;
  }

  if (fX>fWXMax) fX = fmod(fX-fWXMin,(fWXMax-fWXMin))+fWXMin;
  if (fY>fWYMax) fY = fmod(fY-fWYMin,(fWYMax-fWYMin))+fWYMin;
  */

  // Safety assertion and recovery logic
  // NOTE: The assertion logic is updated for the half-open interval [Min, Max).
  // We must check if the coordinate is >= Max, as Max itself is now excluded.
  if (fX < fWXMin || fX >= fWXMax ||  // Changed from > to >=
      fY < fWYMin || fY >= fWYMax) {  // Changed from > to >=
    assert(false &&
           "CCoord::Normalize() failed to normalize coordinates properly");

    // Recovery: clamp to a valid boundary.
    if (fX < fWXMin) {
      fX = fWXMin;
    } else if (fX >= fWXMax) {
      // If normalization failed (e.g., due to floating point effects near the
      // boundary) and we hit or exceeded Max, we enforce the canonical
      // representation for the seam, which is Min.
      fX = fWXMin;
    }

    if (fY < fWYMin) {
      fY = fWYMin;
    } else if (fY >= fWYMax) {
      fY = fWYMin;
    }

    // Log the recovery for debugging
    printf(
        "WARNING: CCoord::Normalize() recovery executed. Clamped coordinates "
        "to (%f,%f)\n",
        fX, fY);
  }
}

double CCoord::DistTo(const CCoord& OthCrd) const {
  // Calculate the shortest distance to another coordinate in a toroidal space.

  // 1. Create a temporary coordinate initialized to the destination (OthCrd).
  CCoord CCmp(OthCrd);

  // 2. Calculate the displacement vector: CCmp = OthCrd - *this.
  // CRITICAL STEP: The CCoord::operator-= implementation calls Normalize()
  // after subtraction. When subtraction results in a vector outside the
  // standard bounds (e.g., A=(500,0), B=(-500,0), B-A = (-1000,0)), Normalize()
  // wraps this vector (e.g., (-1000,0) becomes (24,0)). This normalization
  // automatically selects the shortest path in toroidal space.
  CCmp -= *this;

  // 3. Calculate the Euclidean distance (hypotenuse/magnitude) of the
  // normalized difference vector. Since CCmp now represents the shortest path
  // vector, this returns the shortest toroidal distance.
  double dres = hypot(CCmp.fX, CCmp.fY);
  return dres;
}

double CCoord::AngleTo(const CCoord& OthCrd) const {
  // Calculate the angle towards the shortest path to another coordinate in a
  // toroidal space.

  // Handle the edge case where the start and end points are the same.
  if (*this == OthCrd) {
    return 0.0;
  }

  // 1. Create a temporary coordinate initialized to the destination.
  CCoord CCmp(OthCrd);

  // 2. Calculate the shortest displacement vector (Target - Source).
  // As explained in DistTo, the operator-= calls Normalize(), ensuring that
  // CCmp becomes the vector representing the shortest path, correctly
  // accounting for wrap-around.
  CCmp -= *this;

  // 3. Calculate the angle of the resulting shortest path vector.
  // atan2 provides the angle in radians from the X-axis to the vector
  // (range [-PI, PI]).
  double dres = atan2(CCmp.fY, CCmp.fX);
  return dres;
}

CTraj CCoord::VectTo(const CCoord& OthCrd) const {
  CTraj TVect(DistTo(OthCrd), AngleTo(OthCrd));
  return TVect;
}

/////////////////////////////////////////////////
// Operators

CCoord& CCoord::operator=(const CCoord& OthCrd) {
  fX = OthCrd.fX;
  fY = OthCrd.fY;
  return *this;
}

CCoord& CCoord::operator=(const CTraj& OthTraj) {
  *this = OthTraj.ConvertToCoord();
  return *this;
}

CCoord& CCoord::operator+=(const CCoord& OthCrd) {
  fX += OthCrd.fX;
  fY += OthCrd.fY;
  Normalize();
  return *this;
}

CCoord& CCoord::operator-() {
  fX = -fX;
  fY = -fY;
  return *this;
}

CCoord& CCoord::operator-=(const CCoord& OthCrd) {
  fX -= OthCrd.fX;
  fY -= OthCrd.fY;
  Normalize();
  return *this;
}

CCoord& CCoord::operator*=(const double scale) {
  *this = *this * scale;
  return *this;
}

CCoord& CCoord::operator/=(const double scale) {
  *this = *this / scale;
  return *this;
}

bool CCoord::operator==(const CCoord& OthCrd) const {
  return (fabs(fX - OthCrd.fX) < g_fp_error_epsilon) &&
         (fabs(fY - OthCrd.fY) < g_fp_error_epsilon);
}

bool CCoord::operator!=(const CCoord& OthCrd) const {
  if (OthCrd == *this) {
    return false;
  }
  return true;
}

///////////////////////////////////////////////////////
// Friends

CCoord operator+(const CCoord& C1, const CCoord& C2) {
  CCoord CRes(C1);
  CRes += C2;
  return CRes;
}

CCoord operator-(const CCoord& C1, const CCoord& C2) {
  CCoord CRes(C1);
  CRes -= C2;
  return CRes;
}

CCoord operator*(const CCoord& C1, double scale) {
  CCoord CRes(C1);
  CRes.fX *= scale;
  CRes.fY *= scale;
  CRes.Normalize();
  return CRes;
}

CCoord operator/(const CCoord& C1, double scale) {
  CCoord CRes(C1);
  if (scale == 0.0) {
    return CRes;
  }
  CRes.fX /= scale;
  CRes.fY /= scale;
  CRes.Normalize();
  return CRes;
}

CCoord operator*(double scale, const CCoord& C1) {
  CCoord CRes(C1);
  CRes = CRes * scale;
  return CRes;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CCoord::GetSerialSize() const {
  unsigned int tot = 0;
  tot += BufWrite(NULL, fX);
  tot += BufWrite(NULL, fY);
  return tot;
}

unsigned CCoord::SerialPack(char* buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufWrite(vpb, fX);
  vpb += BufWrite(vpb, fY);

  return vpb - buf;
}

namespace {
inline double Square(double value) { return value * value; }

inline double MidpointDistanceSquared(const CCoord& reference,
                                      const CCoord& delta) {
  double mid_x = reference.fX + 0.5 * delta.fX;
  double mid_y = reference.fY + 0.5 * delta.fY;
  CCoord mid(mid_x, mid_y);
  mid.Normalize();
  return Square(mid.fX) + Square(mid.fY);
}

void NormalizeWithBias(CCoord* value, const CCoord& reference,
                       bool prefer_center) {
  if (value == NULL) {
    return;
  }

  const double half_x = kWorldSizeX * 0.5;
  const double half_y = kWorldSizeY * 0.5;
  const double tie_eps = std::max(1e-6, g_fp_error_epsilon * 10.0);

  CCoord candidates[4];
  int count = 0;
  candidates[count++] = *value;

  auto nearly_equal = [&](double a, double b) {
    return fabs(a - b) <= tie_eps;
  };

  bool tie_x =
      nearly_equal(fabs(candidates[0].fX), half_x);
  bool tie_y =
      nearly_equal(fabs(candidates[0].fY), half_y);

  if (tie_x) {
    CCoord alt = candidates[0];
    alt.fX += (alt.fX >= 0.0) ? -kWorldSizeX : kWorldSizeX;
    candidates[count++] = alt;
  }

  int base_for_y = count;
  if (tie_y) {
    for (int i = 0; i < base_for_y; ++i) {
      CCoord alt = candidates[i];
      alt.fY += (alt.fY >= 0.0) ? -kWorldSizeY : kWorldSizeY;
      candidates[count++] = alt;
    }
  }

  if (count == 1) {
    *value = candidates[0];
    value->Normalize();
    return;
  }

  double best_length_sq = std::numeric_limits<double>::infinity();
  double best_mid_metric = prefer_center
                               ? std::numeric_limits<double>::infinity()
                               : -std::numeric_limits<double>::infinity();
  int best_index = 0;

  for (int i = 0; i < count; ++i) {
    const CCoord& candidate = candidates[i];
    double length_sq = Square(candidate.fX) + Square(candidate.fY);

    if (length_sq < best_length_sq - tie_eps) {
      best_length_sq = length_sq;
      best_mid_metric = MidpointDistanceSquared(reference, candidate);
      best_index = i;
      continue;
    }

    if (fabs(length_sq - best_length_sq) <= tie_eps) {
      double mid_metric = MidpointDistanceSquared(reference, candidate);
      bool better = prefer_center ? (mid_metric < best_mid_metric - tie_eps)
                                  : (mid_metric > best_mid_metric + tie_eps);
      if (better) {
        best_mid_metric = mid_metric;
        best_index = i;
      }
    }
  }

  *value = candidates[best_index];
}
}  // namespace

void CCoord::NormalizeCentered(const CCoord& reference) {
  NormalizeWithBias(this, reference, true);
}

void CCoord::NormalizeEdges(const CCoord& reference) {
  NormalizeWithBias(this, reference, false);
}

unsigned CCoord::SerialUnpack(char* buf, unsigned buflen) {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char* vpb = buf;

  vpb += BufRead(vpb, fX);
  vpb += BufRead(vpb, fY);

  return vpb - buf;
}
