/* Ship.h
 * Prophecy of the Motherload.
 * Header for CShip
 * Inherited off of CThing
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#ifndef _SHIP_H_FKJEKJWEJFWEKJWEKJEFKKF
#define _SHIP_H_FKJEKJWEJFWEKJWEKJEFKKF

#include "Asteroid.h"
#include "Thing.h"
#include <memory>

class CTeam;
class CBrain;

const double dMaxStatTot = 60.0;

enum OrderKind {
  O_SHIELD,
  O_LASER,
  O_THRUST,
  O_TURN,
  O_JETTISON,
  O_ALL_ORDERS
};

enum ShipStat { S_CARGO, S_FUEL, S_SHIELD, S_ALL_STATS };

class CShip : public CThing {
 public:
  // Nested strategy interface for velocity/acceleration processing
  // Public so implementations can inherit from it
  class VelocityStrategy {
  public:
    virtual ~VelocityStrategy() = default;
    virtual double processThrustOrder(CShip* ship, OrderKind ord, double value) = 0;
    virtual void processThrustDrift(CShip* ship, double thrustamt, double dt) = 0;
  };

 private:
  // Strategy instance
  mutable std::unique_ptr<VelocityStrategy> velocityStrategy;

  // Initialize velocity strategy based on configuration
  void InitializeVelocityStrategy() const;

 public:
  CShip(CCoord StPos, CTeam* pteam = NULL, UINT ShNum = 0);
  virtual ~CShip();

  UINT GetShipNumber() const;
  bool IsDocked() const;

  double GetAmount(ShipStat st) const;          // Returns current amount
  double GetCapacity(ShipStat st) const;        // Returns max capacity
  double SetAmount(ShipStat st, double val);    // Returns new amt
  double SetCapacity(ShipStat st, double val);  // Returns new capacity

  // Brain management for tactical AI context switching
  // Ships can dynamically switch between different behavioral contexts
  CBrain* GetBrain();             // Returns current CBrain object
  CBrain* SetBrain(CBrain* pBr);  // Returns old CBrain object

  virtual double GetMass() const;

  virtual void Drift(double dt = 1.0);
  bool AsteroidFits(const CAsteroid* pAst);

  CThing* LaserTarget();          // Returns what laserbeam will hit if fired
  double GetLaserBeamDistance();  // Returns distance laserbeam will traverse
  double AngleToIntercept(const CThing& OthThing, double dtime);

  ShipStat AstToStat(AsteroidKind AsMat) const;
  AsteroidKind StatToAst(ShipStat ShStat) const;

  void ResetOrders();
  // NOTE: Use SetJettison() and GetJettison() helper functions instead of
  // calling SetOrder(O_JETTISON, ...) directly for better type safety and
  // readability
  double GetOrder(OrderKind Ord) const;  // Returns value of order
  double SetOrder(OrderKind Ord,
                  double value);  // Returns fuel consumed for order
  void SetJettison(AsteroidKind Mat, double amt);
  double GetJettison(AsteroidKind Mat);

  // Serialization routines
  unsigned GetSerialSize() const;
  unsigned SerialPack(char* buf, unsigned buflen) const;
  unsigned SerialUnpack(char* buf, unsigned buflen);

 protected:
  UINT myNum;
  bool bDockFlag;
  double dDockDist, dLaserDist;
  CBrain* pBrain;

  double adOrders[(UINT)O_ALL_ORDERS];
  double adStatCur[(UINT)S_ALL_STATS];
  double adStatMax[(UINT)S_ALL_STATS];

  virtual void HandleCollision(CThing* pOthThing, CWorld* pWorld = NULL);
  virtual void HandleJettison();

 public:
  // Helper methods for velocity strategies to access protected members
  // These allow strategies to manipulate ship state without direct member access
  void ApplyVelocityChange(const CTraj& newVel) { Vel = newVel; }
  void ApplyPositionChange(const CCoord& newPos) { Pos = newPos; }
  void SetDockFlag(bool docked) { bDockFlag = docked; }
  void SetImageSet(UINT imgSet) { uImgSet = imgSet; }
  double GetDockDistance() const { return dDockDist; }
  double GetBaseMass() const { return mass; }
  void SetThrustOrder(double value) { adOrders[(UINT)O_THRUST] = value; }
  void ClearTurnJettisonOrders() {
    adOrders[(UINT)O_TURN] = 0.0;
    adOrders[(UINT)O_JETTISON] = 0.0;
  }

};

#endif  // ! _SHIP_H_FKJEKJWEJFWEKJWEKJEFKKF
