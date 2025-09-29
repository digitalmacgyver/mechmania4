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

 private:
  // Velocity processing implementations
  double ProcessThrustOrderOld(OrderKind ord, double value);  // Legacy
  double ProcessThrustOrderNew(OrderKind ord, double value);  // Improved
  void ProcessThrustDriftOld(double thrustamt, double dt);    // Legacy
  void ProcessThrustDriftNew(double thrustamt, double dt);    // Improved

  // Helper function used in both SetOrder and Drift. Ship users should use
  // SetOrder() to evaluate the fuel cost of any order.
  struct ThrustCost {
    bool fuel_limited; // True if thrust_achieved was limited by fuel capacity
    double thrust_cost;
    double governor_cost; // 0.0 if thrust was not governed
    double total_cost; // thrust_cost + governor_cost
    CTraj dv_achieved; // .rho is the magnitude of the achieved thurst, .theta is the heading
  };
  ThrustCost CalcThrustCost(double thrustamt, CTraj v, double orient, double mass, double fuel_avail, bool is_docked) const;

};

#endif  // ! _SHIP_H_FKJEKJWEJFWEKJWEKJEFKKF
