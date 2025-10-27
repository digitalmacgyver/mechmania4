#include "Brain.h"
#include "Ship.h"
#include "Thing.h"
// Include TrenchRun.h for access to shared combat utilities
#include "TrenchRun.h"

#include <vector>

struct EmergencyOrders {
  unsigned int exclusive_order = O_ALL_ORDERS;
  double exclusive_order_amount = 0.0;
  double shield_order_amount = 0.0;
  double laser_order_amount = 0.0;
  CThing* laser_target = NULL;
};

class GetVinyl : public CBrain {
 public:
  GetVinyl();
  ~GetVinyl();

  void Decide();

  EmergencyOrders HandleImminentCollision(std::vector<CThing *> collisions, unsigned int turns, EmergencyOrders emergency_orders);
};
