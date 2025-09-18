/* Thing.h
 * Declaration of class CStuff
 * All objects floating around in
 * the game universe derive from
 * this class.
 * For use with MechMania IV
 * 4/29/98 by Misha Voloshin
*/ 

#ifndef _THING_H_SFEFLKJEFLJESNFJLS
#define _THING_H_SFEFLKJEFLJESNFJLS

#include "stdafx.h"
#include "Coord.h"
#include "Traj.h"
#include "Sendable.h"

const double minmass=3.0,
  minsize=1.0,
  maxspeed=30.0,
  NO_COLLIDE = -1.0,
  NO_DAMAGE = -123.45;

#ifndef maxnamelen
#define maxnamelen 16  // Increased to accommodate longer names
#endif

class CWorld;
class CTeam;

enum ThingKind {
  GENTHING, ASTEROID, STATION, SHIP
};

class CThing : public CSendable
{
 public:
  CThing (double fx0=0.0, double fy0=0.0);
  CThing (const CThing& OthThing);
  virtual ~CThing();

  const CCoord& GetPos() const;
  ThingKind GetKind() const;
  virtual double GetMass() const;
  double GetSize() const;
  double GetOrient() const;
  const CTraj& GetVelocity() const;
  CTraj GetMomentum() const;
  CTeam* GetTeam() const;
  bool IsAlive() const;
  UINT GetImage() const;
  void KillThing();

  const char *GetName() const;
  void SetName(const char *strsrc);

  void SetMass(double dm=1.0) { if (dm>=minmass) mass=dm; }
  void SetOrient(double ort=0.0) { orient=ort; }
  void SetSize(double sz=minsize) { if (sz>=minsize) size=sz; }
  void SetPos(CCoord& CPnew) { Pos = CPnew; }
  void SetVel(CTraj& CTnew) { Vel = CTnew; }
  void SetTeam(CTeam* pnewTeam) { pmyTeam = pnewTeam; }
  
  UINT GetWorldIndex() const { return uWldIndex; }
  CWorld* GetWorld() const { return pmyWorld; }
  void SetWorldIndex(UINT ind) { uWldIndex=ind; }
  void SetWorld(CWorld* pWld) { pmyWorld=pWld; }

  virtual void Drift(double dt=1.0);
  bool Collide(CThing *pOthThing, CWorld *pWorld=NULL);
  bool Overlaps(const CThing& OthThing) const;

  double DetectCollisionCourse(const CThing& OthThing) const;
  CCoord PredictPosition(double dt=1.0) const;
  CTraj RelativeVelocity(const CThing& OthThing) const;
  CTraj RelativeMomentum(const CThing& OthThing) const;
  bool IsFacing(const CThing& OthThing) const;

  CThing& operator= (const CThing& OthThing);
  bool operator== (const CThing& OthThing) const;
  bool operator!= (const CThing& OthThing) const;

  // Serialization routines
  virtual unsigned GetSerialSize() const;
  virtual unsigned SerialPack (char *buf, unsigned buflen) const;
  virtual unsigned SerialUnpack (char *buf, unsigned buflen);

  double bIsColliding, bIsGettingShot;   // Angle of damage origin

 protected:
  ThingKind TKind;       // Identifier of kind of stuff
  CCoord Pos;           // Current coordinates
  CTraj Vel;            // Current velocity
  double orient,omega;   // Orientation in radians, rate of change of angle
  double mass,size;      // Mass and size
  bool DeadFlag;         // true means this guy's DEAD
  CTeam *pmyTeam;        // NULL for non-team objects (asteroids)

  char Name[maxnamelen];   // Name.  Duh :P
  UINT uImgSet;            // Real image representation :)

  UINT uWldIndex;
  CWorld *pmyWorld;

  virtual void HandleCollision (CThing* pOthThing, CWorld *pWorld=NULL);

 private:
  UINT ulIDCookie;
};

#endif // !_THING_H_SFEFLKJEFLJESNF
