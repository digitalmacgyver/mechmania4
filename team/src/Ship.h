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
#include "GameConstants.h"
#include "Thing.h"

class CTeam;
class CBrain;

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
  CShip(CCoord StPos, CTeam* pteam = NULL, unsigned int ShNum = 0);
  virtual ~CShip();

  unsigned int GetShipNumber() const;
  bool IsDocked() const;
  bool WasDocked() const;  // Returns previous turn's docking state

  double GetAmount(ShipStat st) const;          // Returns current amount
  double GetCapacity(ShipStat st) const;        // Returns max capacity
  double SetAmount(ShipStat st, double val);    // Returns new amt
  double SetCapacity(ShipStat st, double val);  // Returns new capacity

  // Brain management for tactical AI context switching
  // Ships can dynamically switch between different behavioral contexts
  CBrain* GetBrain();             // Returns current CBrain object
  CBrain* SetBrain(CBrain* pBr);  // Returns old CBrain object

  virtual double GetMass() const;

  // Deterministic collision engine - override to populate ship-specific fields
  virtual CollisionState MakeCollisionState() const;

  // Deterministic collision engine - override to handle ship-specific commands
  virtual void ApplyCollisionCommandDerived(const CollisionCommand& cmd, const CollisionContext& ctx);

  // Deterministic collision engine - generate collision commands from snapshots
  CollisionOutcome GenerateCollisionCommands(const CollisionContext& ctx);

  virtual void Drift(double dt = 1.0, double turn_phase = 0.0);
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
  unsigned int myNum;
  bool bDockFlag;
  bool bWasDocked;  // Previous turn's docking state (for collision logging)
  bool bLaunchedThisTurn;  // True if ship undocked this turn (makes thrust free for entire turn)
  double dDockDist, dLaserDist;
  CBrain* pBrain;

  double adOrders[(unsigned int)O_ALL_ORDERS];
  double adStatCur[(unsigned int)S_ALL_STATS];
  double adStatMax[(unsigned int)S_ALL_STATS];

  virtual void HandleCollision(CThing* pOthThing, CWorld* pWorld = NULL);
  virtual void HandleJettison();

 public:
  // Helper for 2D elastic collision calculations (public so Asteroid can use it)
  struct ElasticCollisionResult {
    CTraj v1_final;  // Final velocity of object 1
    CTraj v2_final;  // Final velocity of object 2
  };
  static ElasticCollisionResult CalculateElastic2DCollision(
      double m1, const CTraj& v1, const CCoord& p1,
      double m2, const CTraj& v2, const CCoord& p2,
      double random_angle = 0.0, bool has_random = false);

 private:
  // Velocity processing implementations
  double ProcessThrustOrderOld(OrderKind ord, double value);  // Legacy
  double ProcessThrustOrderNew(OrderKind ord, double value);  // Improved
  void ProcessThrustDriftOld(double thrustamt, double dt);    // Legacy
  void ProcessThrustDriftNew(double thrustamt, double dt);    // Improved

  // Collision physics implementations
  void HandleElasticShipCollision(CThing* pOtherShip);  // Proper elastic collision
  double CalculateCollisionMomentumChange(const CThing* pOtherThing) const;  // Calculate |Δp| for collision

  // Collision processing implementations
  void HandleCollisionOld(CThing* pOthThing, CWorld* pWorld);  // Legacy
  void HandleCollisionNew(CThing* pOthThing, CWorld* pWorld);  // New system

  // Helper function used in both SetOrder and Drift. Ship users should use
  // SetOrder() to evaluate the fuel cost of any order.
  struct ThrustCost {
    bool fuel_limited; // True if thrust_achieved was limited by fuel capacity
    double thrust_cost;
    double governor_cost; // 0.0 if thrust was not governed
    double total_cost; // thrust_cost + governor_cost
    CTraj dv_achieved; // .rho is the magnitude of the achieved thurst, .theta is the heading
  };
  ThrustCost CalcThrustCost(double thrustamt, CTraj v, double orient, double mass, double fuel_avail, bool is_docked, bool launched_this_turn) const;

};

#endif  // ! _SHIP_H_FKJEKJWEJFWEKJWEKJEFKKF
