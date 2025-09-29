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

#include "Coord.h"
#include "Sendable.h"
#include "Traj.h"
#include "stdafx.h"

const double minmass = 3.0, minsize = 1.0,
             NO_COLLIDE = -1.0, NO_DAMAGE = -123.45;

#ifndef maxnamelen
#define maxnamelen 14  // Thing names buffer: 14 bytes (13 characters + NUL)
#endif

class CWorld;
class CTeam;

enum ThingKind { GENTHING, ASTEROID, STATION, SHIP };

class CThing : public CSendable {
 public:
  CThing(double fx0 = 0.0, double fy0 = 0.0);
  CThing(const CThing& OthThing);
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
  unsigned int GetImage() const;
  void KillThing();

  const char* GetName() const;
  void SetName(const char* strsrc);

  void SetMass(double dm = 1.0) {
    if (dm >= minmass)
      mass = dm;
  }
  void SetOrient(double ort = 0.0) { orient = ort; }
  void SetSize(double sz = minsize) {
    if (sz >= minsize)
      size = sz;
  }
  void SetPos(CCoord& CPnew) { Pos = CPnew; }
  void SetVel(CTraj& CTnew) { Vel = CTnew; }
  void SetTeam(CTeam* pnewTeam) { pmyTeam = pnewTeam; }

  unsigned int GetWorldIndex() const { return uWldIndex; }
  CWorld* GetWorld() const { return pmyWorld; }
  void SetWorldIndex(unsigned int ind) { uWldIndex = ind; }
  void SetWorld(CWorld* pWld) { pmyWorld = pWld; }

  virtual void Drift(double dt = 1.0);
  bool Collide(CThing* pOthThing, CWorld* pWorld = NULL);
  bool Overlaps(const CThing& OthThing) const;

  double DetectCollisionCourse(const CThing& OthThing) const;
  CCoord PredictPosition(double dt = 1.0) const;
  CTraj RelativeVelocity(const CThing& OthThing) const;
  CTraj RelativeMomentum(const CThing& OthThing) const;
  bool IsFacing(const CThing& OthThing) const;

  CThing& operator=(const CThing& OthThing);
  bool operator==(const CThing& OthThing) const;
  bool operator!=(const CThing& OthThing) const;

  // Serialization routines
  virtual unsigned GetSerialSize() const;
  virtual unsigned SerialPack(char* buf, unsigned buflen) const;
  virtual unsigned SerialUnpack(char* buf, unsigned buflen);

  double bIsColliding, bIsGettingShot;  // Angle of damage origin

 protected:
  ThingKind TKind;       // Identifier of kind of stuff
  CCoord Pos;            // Current coordinates (NOTE: +Y points down on screen)
  CTraj Vel;             // Current velocity
  double orient, omega;  // Orientation in radians (0=right, PI/2=down, PI=left, -PI/2=up), rate of change
  double mass, size;     // Mass and size
  bool DeadFlag;         // true means this guy's DEAD
  CTeam* pmyTeam;        // NULL for non-team objects (asteroids)

  char Name[maxnamelen];  // Name.  Duh :P
  unsigned int uImgSet;           // Real image representation :)

  unsigned int uWldIndex;
  CWorld* pmyWorld;

  virtual void HandleCollision(CThing* pOthThing, CWorld* pWorld = NULL);

 private:
  unsigned int ulIDCookie;

  // Collision detection implementations (Private)
  double DetectCollisionCourseOld(const CThing& OthThing) const; // Legacy
  double DetectCollisionCourseNew(const CThing& OthThing) const; // Quadratic
};

#endif  // !_THING_H_SFEFLKJEFLJESNF
