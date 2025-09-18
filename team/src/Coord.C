/* Coord.C
 * Definition of class CCoord
 * Handles coordinate math 'n stuff
 * For use with MechMania IV
 * 4/29/98 by Misha Voloshin
*/

#include "Coord.h"
#include "Traj.h"

//////////////////////////////////////////////////
// Construction/destruction

CCoord::CCoord (double fx0, double fy0)
{
  fX=fx0;
  fY=fy0;
}

CCoord::CCoord (const CCoord& OthCrd)
{
  *this = OthCrd;
}

CCoord::CCoord (const CTraj& OthTraj)
{
  *this = OthTraj.ConvertToCoord();
}

CCoord::~CCoord()
{

}

/////////////////////////////////////////////////
// Methods 

void CCoord::Normalize()
{
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
 
  if (fX<fWXMin ||
      fX>fWXMax ||
      fY<fWYMin ||
      fY>fWYMax) {
    //printf ("MM4 ENGINE ERROR: CCoord::Normalize()\n");
    fX=0.0;
    fY=0.0;
  }
}

double CCoord::DistTo (const CCoord& OthCrd) const
{
  CCoord CCmp(OthCrd);
  CCmp -= *this;
  double dres = hypot(CCmp.fX,CCmp.fY);
  return dres;
}

double CCoord::AngleTo (const CCoord& OthCrd) const
{
  if (*this==OthCrd) return 0.0;
  CCoord CCmp(OthCrd);
  CCmp -= *this;
  double dres = atan2(CCmp.fY,CCmp.fX);
  return dres;
}

CTraj CCoord::VectTo (const CCoord& OthCrd) const
{
  CTraj TVect(DistTo(OthCrd),AngleTo(OthCrd));
  return TVect;
}

/////////////////////////////////////////////////
// Operators

CCoord& CCoord::operator= (const CCoord& OthCrd)
{
  fX = OthCrd.fX;
  fY = OthCrd.fY;
  return *this;
}

CCoord& CCoord::operator= (const CTraj& OthTraj)
{
  *this = OthTraj.ConvertToCoord();
  return *this;
}

CCoord& CCoord::operator+= (const CCoord& OthCrd)
{
  fX += OthCrd.fX;
  fY += OthCrd.fY;
  Normalize();
  return *this;
}

CCoord& CCoord::operator- ()
{
  fX = -fX;
  fY = -fY;
  return *this;
}

CCoord& CCoord::operator-= (const CCoord& OthCrd)
{
  fX -= OthCrd.fX;
  fY -= OthCrd.fY;
  Normalize();
  return *this;
}

CCoord& CCoord::operator*= (const double scale)
{
  *this = *this * scale;
  return *this;
}

CCoord& CCoord::operator/= (const double scale)
{
  *this = *this / scale;
  return *this;
}

bool CCoord::operator== (const CCoord& OthCrd) const
{
  if (fX!=OthCrd.fX) return false;
  if (fY!=OthCrd.fY) return false;
  return true;
}

bool CCoord::operator!= (const CCoord& OthCrd) const
{
  if (OthCrd==*this) return false;
  return true;
}

///////////////////////////////////////////////////////
// Friends

CCoord operator+ (const CCoord& C1, const CCoord& C2)
{
  CCoord CRes(C1);
  CRes+=C2;
  return CRes;
}

CCoord operator- (const CCoord& C1, const CCoord& C2)
{
  CCoord CRes(C1);
  CRes-=C2;
  return CRes;
}

CCoord operator* (const CCoord& C1, double scale)
{
  CCoord CRes(C1);
  CRes.fX*=scale;
  CRes.fY*=scale;
  CRes.Normalize();
  return CRes;
}

CCoord operator/ (const CCoord& C1, double scale)
{
  CCoord CRes(C1);
  if (scale==0.0) return CRes;
  CRes.fX/=scale;
  CRes.fY/=scale;
  CRes.Normalize();
  return CRes;
}

CCoord operator* (double scale, const CCoord& C1)
{
  CCoord CRes(C1);
  CRes = CRes * scale;
  return CRes;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CCoord::GetSerialSize() const
{
  UINT tot=0;
  tot += BufWrite(NULL, fX);
  tot += BufWrite(NULL, fY);
  return tot;
}

unsigned CCoord::SerialPack (char *buf, unsigned buflen) const
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += BufWrite(vpb, fX);
  vpb += BufWrite(vpb, fY);

  return vpb-buf;
}
  
unsigned CCoord::SerialUnpack (char *buf, unsigned buflen)
{
  if (buflen<GetSerialSize()) return 0;
  char *vpb = buf;

  vpb += BufRead (vpb, fX);
  vpb += BufRead (vpb, fY);

  return vpb-buf;
}
