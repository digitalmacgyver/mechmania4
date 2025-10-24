#ifndef _FUELTRAJ_H_
#define _FUELTRAJ_H_

#include "Traj.h"
#include "Ship.h"

// Todo - rewrite this type to handle the "drift" case more naturally and not 
// confuse ourselves in the future with O_SHIELD orders. As it is this is a hack
// since it's generallyu safe/free to issue O_SHIELD orders with magnitude 0.
class FuelTraj {
 public:
  FuelTraj(bool found, OrderKind kind, double mag, double fuel_used,
     double time_to_arrive, unsigned int num_orders, double fuel_total)
   : path_found(found), order_kind(kind), order_mag(mag), fuel_used(fuel_used),
     time_to_arrive(time_to_arrive), num_orders(num_orders),
     fuel_total(fuel_total) {}

  FuelTraj() = default;

  // First order issued on our path.
  bool path_found = false; // No change to trajectory needed to get to target.
  OrderKind order_kind = O_SHIELD; // Order kind to get to target - it's always safe to set O_SHIELD on each tic.
  double order_mag = 0.0; // Order magnitude to get to target.
  double fuel_used = 0.0; // Estimated order cost (can be 0 if no order needed).

  // Estimated values for the path.
  double time_to_arrive = 0.0; // Estimated time to arrive at the target.
  unsigned int num_orders = 0; // Estimated number of orders to get to the target.
  double fuel_total = 0.0; // Estimated total fuel cost of the path.

};
#endif
