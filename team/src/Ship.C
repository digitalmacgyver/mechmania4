/* Ship.C
 * Ahh, the Motherload!!
 * Implementation of CShip class
 * For use with MechMania IV
 * Misha Voloshin 5/28/98
 */

#include "Brain.h"
#include "GameConstants.h"
#include "ParserModern.h"
#include "PhysicsUtils.h"
#include "Ship.h"
#include "Station.h"
#include "Team.h"
#include "World.h"
#include "CollisionTypes.h"  // For deterministic collision engine
#include <algorithm>         // For std::swap
#include <cmath>
#include <functional>        // For std::function (recursive lambda)

extern CParser* g_pParser;

///////////////////////////////////////////
// Helper functions (must be defined before use in member functions)

// Calculate angular velocity at a specific phase in the triangular velocity profile.
// Phase ∈ [0, 1] represents progress through the turn (0 = start, 1 = end).
// The triangular profile accelerates linearly to peak at phase=0.5, then decelerates.
static inline double GetTriangularOmega(double phase, double omega_max) {
  if (phase <= 0.5) {
    // Accelerating: ω increases linearly from 0 to ω_max
    return 2.0 * omega_max * phase;
  } else {
    // Decelerating: ω decreases linearly from ω_max to 0
    return 2.0 * omega_max * (1.0 - phase);
  }
}

// Integrate triangular angular velocity profile over a time interval.
// Handles the cusp at turn_phase = 0.5 explicitly so we don't lose magnitude
// when a timestep straddles the peak angular velocity.
static double IntegrateTriangularOmega(double time_start,
                                       double time_end,
                                       double omega_max,
                                       double turn_duration) {
  const double half_duration = turn_duration * 0.5;
  const double coeff = omega_max / turn_duration;

  // If segment doesn't cross the midpoint, integrate directly
  if (time_end <= half_duration) {
    // Entire segment in acceleration phase: ω(t) = (2*ω_max/T) * t
    // Integral: (ω_max/T) * t²
    return coeff * (time_end * time_end - time_start * time_start);
  }

  if (time_start >= half_duration) {
    // Entire segment in deceleration phase: ω(t) = 2*ω_max * (1 - t/T)
    // Integral: 2*ω_max*t - (ω_max/T)*t²
    return 2.0 * omega_max * (time_end - time_start) - coeff * (time_end * time_end - time_start * time_start);
  }

  // Segment straddles the cusp at half_duration - split into two parts
  double accel_part = coeff * (half_duration * half_duration - time_start * time_start);
  double decel_part = 2.0 * omega_max * (time_end - half_duration) - coeff * (time_end * time_end - half_duration * half_duration);
  return accel_part + decel_part;
}

namespace {

bool AsteroidFitsSnapshot(const CollisionState* ship_state,
                          const CollisionState* asteroid_state) {
  if (ship_state == NULL || asteroid_state == NULL) {
    return false;
  }
  if (ship_state->kind != SHIP || asteroid_state->kind != ASTEROID) {
    return false;
  }

  double asteroid_mass = asteroid_state->mass;
  AsteroidKind material = asteroid_state->asteroid_material;

  if (material == VINYL) {
    double max_cargo = ship_state->ship_cargo_capacity;
    if (max_cargo <= 0.0) {
      return false;
    }
    double projected_cargo = ship_state->ship_cargo + asteroid_mass;
    return projected_cargo <= max_cargo;
  }
  if (material == URANIUM) {
    double max_fuel = ship_state->ship_fuel_capacity;
    if (max_fuel <= 0.0) {
      return false;
    }
    double projected_fuel = ship_state->ship_fuel + asteroid_mass;
    return projected_fuel <= max_fuel;
  }
  return false;
}

double ComputeRelativeSpeedAlongNormal(const CollisionState* self_state,
                                       const CollisionState* other_state,
                                       double fallback_angle) {
  if (self_state == NULL || other_state == NULL) {
    return 0.0;
  }

  CCoord normal = self_state->position - other_state->position;
  double normal_mag_sq = normal.fX * normal.fX + normal.fY * normal.fY;

  if (normal_mag_sq < g_fp_error_epsilon) {
    normal.fX = cos(fallback_angle);
    normal.fY = sin(fallback_angle);
    normal_mag_sq = normal.fX * normal.fX + normal.fY * normal.fY;
  }

  if (normal_mag_sq < g_fp_error_epsilon) {
    return 0.0;
  }

  double normal_mag = sqrt(normal_mag_sq);
  double nx = normal.fX / normal_mag;
  double ny = normal.fY / normal_mag;

  CTraj relative_velocity = other_state->velocity - self_state->velocity;
  CCoord rel_cart = relative_velocity.ConvertToCoord();
  double v_rel_normal = rel_cart.fX * nx + rel_cart.fY * ny;

  return fabs(v_rel_normal);
}

}  // namespace

void CShip::AnnounceOutOfFuel() const {
  CTeam* team = GetTeam();
  if (team == NULL) {
    return;
  }
  CWorld* world = team->GetWorld();
  if (world == NULL) {
    return;
  }
  char msg[256];
  snprintf(msg, sizeof(msg), "%s ran out of fuel", GetName());
  world->AddAnnouncerMessage(msg);
}

void CShip::ProcessShieldOrder(double shieldamt) {
  if (shieldamt <= 0.0) {
    return;
  }

  double fuelcons = SetOrder(O_SHIELD, shieldamt);

  // Determine shield boost magnitude (legacy vs modern behavior)
  double shieldBoost;
  if (g_pParser && !g_pParser->UseNewFeature("velocity-limits")) {
    shieldBoost = shieldamt;
  } else {
    shieldBoost = GetOrder(O_SHIELD);
  }

  double oldFuel = GetAmount(S_FUEL);
  double newFuel = oldFuel - fuelcons;
  SetAmount(S_FUEL, newFuel);
  SetAmount(S_SHIELD, GetAmount(S_SHIELD) + shieldBoost);
  SetOrder(O_SHIELD, 0.0);

  if (oldFuel > 0.01 && newFuel <= 0.01) {
    printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", GetName(),
           GetTeam() ? GetTeam()->GetName() : "Unknown");
    AnnounceOutOfFuel();
  }
}

double CShip::IntegrateTurnOrder(double turnamt, double dt, double turn_phase) {
  if (turnamt == 0.0) {
    return 0.0;
  }

  if (dt <= 0.0) {
    return 0.0;
  }

  double omega_result = 0.0;

  if (g_pParser && !g_pParser->UseNewFeature("physics")) {
    // Legacy behavior
    double fuelcons = SetOrder(O_TURN, turnamt);
    omega_result = turnamt;

    double oldFuel = GetAmount(S_FUEL);
    double newFuel = oldFuel - fuelcons * dt;
    SetAmount(S_FUEL, newFuel);

    if (oldFuel > 0.01 && newFuel <= 0.01) {
      printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown");
      AnnounceOutOfFuel();
    }
  } else {
    // Modern behavior with triangular angular velocity profile
    double theta_total = fabs(turnamt);
    double omega_max = 2.0 * theta_total / g_game_turn_duration;

    double phase_duration = dt / g_game_turn_duration;
    double phase_start = turn_phase;
    double phase_end = turn_phase + phase_duration;

    double omega_start = GetTriangularOmega(phase_start, omega_max);
    double omega_end = GetTriangularOmega(phase_end, omega_max);

    double I = 0.5 * GetMass() * size * size;
    double fuelcons = 0.0;

    if (phase_start < 0.5 && phase_end > 0.5) {
      double accel_energy = 0.5 * I * (omega_max * omega_max - omega_start * omega_start);
      double decel_energy = 0.5 * I * (omega_max * omega_max - omega_end * omega_end);
      fuelcons = (accel_energy + decel_energy) / g_ship_turn_energy_per_fuel_ton;
    } else {
      fuelcons = 0.5 * I * fabs(omega_end * omega_end - omega_start * omega_start) /
                 g_ship_turn_energy_per_fuel_ton;
    }

    if (IsDocked()) {
      fuelcons = 0.0;
    }

    double time_start = turn_phase * g_game_turn_duration;
    double time_end = phase_end * g_game_turn_duration;
    double rotation_this_tick = IntegrateTriangularOmega(time_start, time_end, omega_max, g_game_turn_duration);

    omega_result = rotation_this_tick / dt;
    if (turnamt < 0.0) {
      omega_result = -omega_result;
    }

    double maxfuel = GetAmount(S_FUEL);
    if (fuelcons > maxfuel && !IsDocked()) {
      // Fuel clamp intentionally scales rotation linearly even though true kinetic cost is quadratic.
      // This keeps order scaling intuitive for teams despite deviating from real-world physics drag.
      double scale = (fuelcons > 0.0) ? (maxfuel / fuelcons) : 0.0;
      omega_result *= scale;
      fuelcons = maxfuel;
    }

    double oldFuel = GetAmount(S_FUEL);
    double newFuel = oldFuel - fuelcons;
    SetAmount(S_FUEL, newFuel);

    if (oldFuel > 0.01 && newFuel <= 0.01) {
      printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown");
      AnnounceOutOfFuel();
    }
  }

  if (turnamt < 0.0) {
    uImgSet = 3;
  } else {
    uImgSet = 4;
  }

  return omega_result;
}

void CShip::IntegrateThrustOrder(double thrustamt, double dt) {
  if (thrustamt == 0.0) {
    return;
  }

  if (g_pParser && !g_pParser->UseNewFeature("velocity-limits")) {
    ProcessThrustDriftOld(thrustamt, dt);
  } else {
    ProcessThrustDriftNew(thrustamt, dt);
  }
}

///////////////////////////////////////////
// Construction/Destruction

CShip::CShip(CCoord StPos, CTeam *pteam, unsigned int ShNum)
    : CThing(StPos.fX, StPos.fY) {
  TKind = SHIP;
  pmyTeam = pteam;
  myNum = ShNum;

  size = g_ship_spawn_size;
  mass = g_ship_spawn_mass;

  // Initial orientation: face toward map center for balance
  extern CParser* g_pParser;
  if (g_pParser && !g_pParser->UseNewFeature("initial-orientation")) {
    // Legacy mode: all ships face east (asymmetric)
    orient = 0.0;
  } else {
    // New mode: ships face toward map center (balanced)
    // Negative X stations: face east (0)
    // Positive X stations: face west (π)
    orient = (StPos.fX < 0.0) ? 0.0 : PI;
  }

  uImgSet = 0;
  pBrain = NULL;

  bDockFlag = true;
  bWasDocked = true;  // Start docked
  bLaunchedThisTurn = false;
  dDockDist = g_ship_default_docking_distance;
  dLaserDist = 0.0;
  omega = 0.0;

  for (unsigned int sh = (unsigned int)S_CARGO; sh < (unsigned int)S_ALL_STATS; ++sh) {
    adStatMax[sh] = 0.0;
    adStatCur[sh] = 0.0;
  }

  adStatMax[(unsigned int)S_CARGO] = g_ship_default_cargo_capacity;
  adStatCur[(unsigned int)S_CARGO] = 0.0;

  adStatMax[(unsigned int)S_FUEL] = g_ship_default_fuel_capacity;
  adStatCur[(unsigned int)S_FUEL] = g_ship_default_fuel_capacity;

  adStatMax[(unsigned int)S_SHIELD] = g_ship_default_shield_capacity;
  adStatCur[(unsigned int)S_SHIELD] = g_ship_default_shield_amount;
  ResetOrders();
}

CShip::~CShip() {}

////////////////////////////////////////////
// Data access

unsigned int CShip::GetShipNumber() const { return myNum; }

bool CShip::IsDocked() const { return bDockFlag; }

bool CShip::WasDocked() const { return bWasDocked; }

double CShip::GetAmount(ShipStat st) const {
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  return adStatCur[(unsigned int)st];
}

double CShip::GetCapacity(ShipStat st) const {
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  return adStatMax[(unsigned int)st];
}

double CShip::GetOrder(OrderKind ord) const {
  if (ord >= O_ALL_ORDERS) {
    return 0.0;
  }
  return adOrders[(unsigned int)ord];
}

double CShip::GetMass() const {
  double sum = mass;
  sum += GetAmount(S_CARGO);
  sum += GetAmount(S_FUEL);
  return sum;
}

// Deterministic collision engine - create snapshot with ship-specific fields
CollisionState CShip::MakeCollisionState() const {
  // Start with base class snapshot
  CollisionState state = CThing::MakeCollisionState();

  // CRITICAL FIX: Override mass with total mass (hull + cargo + fuel)
  // Base class copies hull-only mass, but ship physics needs total mass
  state.mass = GetMass();  // GetMass() returns hull + cargo + fuel

  // Populate ship-specific fields
  state.is_docked = bDockFlag;
  state.was_docked = bWasDocked;
  state.ship_shield = GetAmount(S_SHIELD);
  state.ship_cargo = GetAmount(S_CARGO);
  state.ship_fuel = GetAmount(S_FUEL);
  state.ship_shield_capacity = GetCapacity(S_SHIELD);
  state.ship_cargo_capacity = GetCapacity(S_CARGO);
  state.ship_fuel_capacity = GetCapacity(S_FUEL);

  return state;
}

// Deterministic collision engine - apply ship-specific commands
void CShip::ApplyCollisionCommandDerived(const CollisionCommand& cmd, const CollisionContext& ctx) {
  (void)ctx;
  // This method handles ship-specific command types
  // Base class already handled kKillSelf, kSetVelocity, kSetPosition

  switch (cmd.type) {
    case CollisionCommandType::kAdjustShield: {
      // Adjust shield by delta (can be negative for damage)
      double current = adStatCur[(unsigned int)S_SHIELD];
      double new_amount = current + cmd.scalar;

      // Clamp to valid range [0, max_capacity]
      if (new_amount < 0.0) {
        new_amount = 0.0;
      }
      double max_shield = adStatMax[(unsigned int)S_SHIELD];
      if (new_amount > max_shield) {
        new_amount = max_shield;
      }

      adStatCur[(unsigned int)S_SHIELD] = new_amount;

      // If shield drops to zero, ship is destroyed
      if (adStatCur[(unsigned int)S_SHIELD] <= 0.0) {
        DeadFlag = true;
      }
      break;
    }

    case CollisionCommandType::kAdjustCargo: {
      // Adjust cargo by delta (can be negative for delivery)
      double current = adStatCur[(unsigned int)S_CARGO];
      double new_amount = current + cmd.scalar;

      // Clamp to valid range [0, max_capacity]
      if (new_amount < 0.0) {
        new_amount = 0.0;
      }
      double max_cargo = adStatMax[(unsigned int)S_CARGO];
      if (new_amount > max_cargo) {
        new_amount = max_cargo;
      }

      adStatCur[(unsigned int)S_CARGO] = new_amount;
      break;
    }

    case CollisionCommandType::kAdjustFuel: {
      // Adjust fuel by delta (can be negative for consumption)
      double current = adStatCur[(unsigned int)S_FUEL];
      double new_amount = current + cmd.scalar;

      // Clamp to valid range [0, max_capacity]
      if (new_amount < 0.0) {
        new_amount = 0.0;
      }
      double max_fuel = adStatMax[(unsigned int)S_FUEL];
      if (new_amount > max_fuel) {
        new_amount = max_fuel;
      }

      adStatCur[(unsigned int)S_FUEL] = new_amount;
      break;
    }

    case CollisionCommandType::kSetDocked: {
      // Set docking state
      bDockFlag = cmd.bool_flag;
      break;
    }

    default:
      // Other command types not handled by ships
      break;
  }
}

// Deterministic collision engine - generate collision commands from snapshots
CollisionOutcome CShip::GenerateCollisionCommands(const CollisionContext& ctx) {
  CollisionOutcome outcome;

  const CollisionState* self_state = ctx.self_state;
  const CollisionState* other_state = ctx.other_state;

  if (self_state == NULL || other_state == NULL) {
    return outcome;
  }

  if (self_state->kind != SHIP) {
    return outcome;
  }

  if (self_state->thing == other_state->thing) {
    return outcome;
  }

  if (self_state->is_docked) {
    return outcome;
  }

  switch (other_state->kind) {
    case STATION:
      return HandleStationCollision(ctx, self_state, other_state);
    case GENTHING:
      return HandleLaserCollision(ctx, self_state, other_state);
    case ASTEROID:
      return HandleAsteroidCollision(ctx, self_state, other_state);
    case SHIP:
      if (other_state->team != NULL) {
        return HandleShipCollision(ctx, self_state, other_state);
      }
      break;
    default:
      break;
  }

  return outcome;
}

CollisionOutcome CShip::HandleStationCollision(const CollisionContext& ctx,
                                               const CollisionState* self_state,
                                               const CollisionState* other_state) const {
  CollisionOutcome outcome;

  outcome.AddCommand(CollisionCommand::SetPosition(self_state->thing, other_state->position));
  outcome.AddCommand(CollisionCommand::SetVelocity(self_state->thing, CTraj(0.0, 0.0)));
  outcome.AddCommand(CollisionCommand::SetDocked(self_state->thing, true));

  if (self_state->ship_cargo > 0.01) {
    if (self_state->team == other_state->team) {
      outcome.AddCommand(CollisionCommand::AdjustCargo(other_state->thing, self_state->ship_cargo));
      outcome.AddCommand(CollisionCommand::AdjustCargo(self_state->thing, -self_state->ship_cargo));

      if (ctx.world) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s delivered %.1f vinyl to %s",
                 self_state->thing->GetName(), self_state->ship_cargo, other_state->thing->GetName());
        outcome.AddCommand(CollisionCommand::Announce(msg));
      }
    } else {
      outcome.AddCommand(CollisionCommand::AdjustCargo(other_state->thing, self_state->ship_cargo));
      outcome.AddCommand(CollisionCommand::AdjustCargo(self_state->thing, -self_state->ship_cargo));

      if (ctx.world) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[ENEMY DELIVERY] %s delivered %.1f vinyl to enemy %s",
                 self_state->thing->GetName(), self_state->ship_cargo, other_state->thing->GetName());
        outcome.AddCommand(CollisionCommand::Announce(msg));
      }
    }
  }

  return outcome;
}

CollisionOutcome CShip::HandleLaserCollision(const CollisionContext& ctx,
                                             const CollisionState* self_state,
                                             const CollisionState* other_state) const {
  CollisionOutcome outcome;

  double laser_mass = other_state->mass;
  double damage = laser_mass / g_laser_damage_mass_divisor;

  outcome.AddCommand(CollisionCommand::AdjustShield(self_state->thing, -damage));

  if ((self_state->ship_shield - damage) <= 0.0 && ctx.world) {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s destroyed by laser", self_state->thing->GetName());
    outcome.AddCommand(CollisionCommand::Announce(msg));
  }

  if (ctx.use_new_physics) {
    double m_ship = self_state->mass;
    double m_laser = laser_mass;
    double total_mass = m_ship + m_laser;

    CCoord vel_ship = self_state->velocity.ConvertToCoord();
    CCoord vel_laser = other_state->velocity.ConvertToCoord();

    CCoord v_final;
    v_final.fX = (m_ship * vel_ship.fX + m_laser * vel_laser.fX) / total_mass;
    v_final.fY = (m_ship * vel_ship.fY + m_laser * vel_laser.fY) / total_mass;

    CTraj new_vel(v_final);
    if (new_vel.rho > g_game_max_speed) {
      new_vel.rho = g_game_max_speed;
    }

    outcome.AddCommand(CollisionCommand::SetVelocity(self_state->thing, new_vel));
  }

  return outcome;
}

CollisionOutcome CShip::HandleAsteroidCollision(const CollisionContext& ctx,
                                                const CollisionState* self_state,
                                                const CollisionState* other_state) const {
  CollisionOutcome outcome;

  bool asteroidFits = AsteroidFitsSnapshot(self_state, other_state);

  if (asteroidFits) {
    if (!ctx.disable_eat_damage) {
      double damage = (other_state->mass * (self_state->velocity - other_state->velocity).rho) / g_laser_damage_mass_divisor;
      outcome.AddCommand(CollisionCommand::AdjustShield(self_state->thing, -damage));
    }

    CTraj mom_total = self_state->velocity * self_state->mass + other_state->velocity * other_state->mass;
    double mass_total = self_state->mass + other_state->mass;
    CTraj new_vel = mom_total / mass_total;

    if (new_vel.rho > g_game_max_speed) {
      new_vel.rho = g_game_max_speed;
    }
    outcome.AddCommand(CollisionCommand::SetVelocity(self_state->thing, new_vel));

    AsteroidKind material = other_state->asteroid_material;
    if (material == VINYL) {
      outcome.AddCommand(CollisionCommand::AdjustCargo(self_state->thing, other_state->mass));
    } else if (material == URANIUM) {
      outcome.AddCommand(CollisionCommand::AdjustFuel(self_state->thing, other_state->mass));
    }
    // Asteroid owns its own kill/record commands; see CAsteroid::GenerateCollisionCommands.
    return outcome;
  }

  double damage;
  if (ctx.use_new_physics) {
    double m1 = self_state->mass;
    double m2 = other_state->mass;
    double reduced_mass = (m1 * m2) / (m1 + m2);
    double v_rel_normal = ComputeRelativeSpeedAlongNormal(self_state, other_state,
                                                          ctx.random_separation_angle);
    damage = (2.0 * reduced_mass * v_rel_normal) / g_laser_damage_mass_divisor;
  } else {
    CTraj rel_momentum = (other_state->velocity - self_state->velocity) * other_state->mass;
    damage = rel_momentum.rho / g_laser_damage_mass_divisor;
  }

  outcome.AddCommand(CollisionCommand::AdjustShield(self_state->thing, -damage));

  if ((self_state->ship_shield - damage) <= 0.0 && ctx.world) {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s destroyed by %s",
             self_state->thing->GetName(), other_state->thing->GetName());
    outcome.AddCommand(CollisionCommand::Announce(msg));
  }

  if (ctx.use_new_physics) {
    auto elastic = PhysicsUtils::CalculateElastic2DCollision(
        self_state->mass, self_state->velocity, self_state->position,
        other_state->mass, other_state->velocity, other_state->position,
        ctx.random_separation_angle, true);

    CTraj new_vel = elastic.v1_final;
    if (new_vel.rho > g_game_max_speed) {
      new_vel.rho = g_game_max_speed;
    }
    outcome.AddCommand(CollisionCommand::SetVelocity(self_state->thing, new_vel));
  } else {
    CTraj mom_total = self_state->velocity * self_state->mass + other_state->velocity * other_state->mass;
    double mass_total = self_state->mass + other_state->mass;
    CTraj new_vel = mom_total / mass_total;
    if (new_vel.rho > g_game_max_speed) {
      new_vel.rho = g_game_max_speed;
    }
    outcome.AddCommand(CollisionCommand::SetVelocity(self_state->thing, new_vel));
  }

  return outcome;
}

CollisionOutcome CShip::HandleShipCollision(const CollisionContext& ctx,
                                            const CollisionState* self_state,
                                            const CollisionState* other_state) const {
  CollisionOutcome outcome;

  double damage;
  if (ctx.use_new_physics) {
    double m1 = self_state->mass;
    double m2 = other_state->mass;
    double reduced_mass = (m1 * m2) / (m1 + m2);
    double v_rel_normal = ComputeRelativeSpeedAlongNormal(self_state, other_state,
                                                          ctx.random_separation_angle);
    damage = (2.0 * reduced_mass * v_rel_normal) / g_laser_damage_mass_divisor;
  } else {
    CTraj rel_momentum = (other_state->velocity - self_state->velocity) * other_state->mass;
    damage = rel_momentum.rho / g_laser_damage_mass_divisor;
  }

  outcome.AddCommand(CollisionCommand::AdjustShield(self_state->thing, -damage));

  if ((self_state->ship_shield - damage) <= 0.0 && ctx.world) {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s destroyed by %s",
             self_state->thing->GetName(), other_state->thing->GetName());
    outcome.AddCommand(CollisionCommand::Announce(msg));
  }

  if (ctx.use_new_physics) {
    if (g_pParser && g_pParser->verbose) {
      printf("[SHIP-COLLISION-BEFORE] Turn %u: %s vs %s\n",
             ctx.world ? ctx.world->GetCurrentTurn() : 0,
             self_state->thing->GetName(),
             other_state->thing->GetName());
      printf("  pos_self=(%.2f, %.2f)  pos_other=(%.2f, %.2f)  dist=%.3f\n",
             self_state->position.fX, self_state->position.fY,
             other_state->position.fX, other_state->position.fY,
             self_state->position.DistTo(other_state->position));
      printf("  vel_self=(%.2f @ %.1f°)  vel_other=(%.2f @ %.1f°)\n",
             self_state->velocity.rho, self_state->velocity.theta * 180.0 / PI,
             other_state->velocity.rho, other_state->velocity.theta * 180.0 / PI);
      printf("  shield_self=%.2f  shield_other=%.2f\n",
             self_state->ship_shield, other_state->ship_shield);
    }

    auto elastic = PhysicsUtils::CalculateElastic2DCollision(
        self_state->mass, self_state->velocity, self_state->position,
        other_state->mass, other_state->velocity, other_state->position,
        ctx.random_separation_angle, true);

    CTraj new_vel = elastic.v1_final;
    if (new_vel.rho > g_game_max_speed) {
      new_vel.rho = g_game_max_speed;
    }
    outcome.AddCommand(CollisionCommand::SetVelocity(self_state->thing, new_vel));

    double separation_dist = self_state->size + g_ship_collision_bump;
    const char* separation_mode = nullptr;

    CTraj normal_dir = elastic.collision_normal;
    if (normal_dir.rho <= g_fp_error_epsilon) {
      // Fallback if elastic computation couldn't provide a normal
      double fallback_angle = self_state->position.AngleTo(other_state->position);
      normal_dir = CTraj(1.0, fallback_angle);
      separation_mode = "GEOMETRIC";
    }

    double separation_angle;
    if (elastic.used_random_normal) {
      // Ships share a randomized axis; use per-collision coin flip to pick direction.
      bool self_forward = ctx.random_separation_forward;
      if (ctx.world == nullptr) {
        // Fallback for legacy entry points that may not populate the flag.
        CThing* self_thing = self_state->thing;
        CThing* other_thing = other_state->thing;
        unsigned int self_index = self_thing ? self_thing->GetWorldIndex() : 0;
        unsigned int other_index = other_thing ? other_thing->GetWorldIndex() : 0;
        if (self_index != other_index) {
          self_forward = (self_index < other_index);
        } else {
          self_forward = (self_thing < other_thing);
        }
      }
      double base_angle = normal_dir.theta;
      if (!self_forward) {
        base_angle += PI;
      }
      separation_angle = base_angle;
      separation_mode = "RANDOM";
    } else {
      // Move self away from other: opposite the normal (which points self -> other)
      CTraj separation_axis(1.0, normal_dir.theta + PI);
      separation_angle = separation_axis.theta;
      if (separation_mode == nullptr) {
        separation_mode = "GEOMETRIC";
      }
    }

    CTraj separation_vec(separation_dist, separation_angle);
    CCoord bump_pos = self_state->position;
    bump_pos += separation_vec.ConvertToCoord();
    outcome.AddCommand(CollisionCommand::SetPosition(self_state->thing, bump_pos));

    if (g_pParser && g_pParser->verbose) {
      CCoord new_pos = bump_pos;
      double dist_from_other = new_pos.DistTo(other_state->position);
      double dist_moved = new_pos.DistTo(self_state->position);

      printf("[SHIP-COLLISION-AFTER] %s separation complete\n", self_state->thing->GetName());
      printf("  mode=%s  angle=%.1f°  separation_dist=%.1f\n",
             separation_mode,
             separation_angle * 180.0 / PI,
             separation_dist);
      printf("  old_pos=(%.2f, %.2f)  new_pos=(%.2f, %.2f)\n",
             self_state->position.fX, self_state->position.fY,
             new_pos.fX, new_pos.fY);
      printf("  dist_moved=%.3f  dist_from_other=%.3f (expected=%.1f)\n",
             dist_moved, dist_from_other, separation_dist);
      printf("  new_vel=(%.2f @ %.1f°)  damage=%.2f\n",
             new_vel.rho, new_vel.theta * 180.0 / PI,
             damage);
    }
  } else {
    double angle = other_state->position.AngleTo(self_state->position);
    double separation = other_state->size + 3.0;
    CTraj move_vec(separation, angle);

    CCoord new_pos = self_state->position;
    new_pos += move_vec.ConvertToCoord();
    outcome.AddCommand(CollisionCommand::SetPosition(self_state->thing, new_pos));

    double mass_ratio = other_state->mass / self_state->mass;
    CTraj vel_change = move_vec * mass_ratio;
    CTraj new_vel = self_state->velocity + vel_change;
    if (new_vel.rho > g_game_max_speed) {
      new_vel.rho = g_game_max_speed;
    }
    outcome.AddCommand(CollisionCommand::SetVelocity(self_state->thing, new_vel));
  }

  return outcome;
}


// Helper function: Calculate 2D elastic collision velocities
// Uses proper 2D elastic collision physics with normal/tangential decomposition
double CShip::GetLaserBeamDistance() { return dLaserDist; }

CBrain *CShip::GetBrain() { return pBrain; }

////////
// Incoming

double CShip::SetAmount(ShipStat st, double val) {
  if (val < 0.0) {
    val = 0.0;
  }
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  if (val >= GetCapacity(st)) {
    val = GetCapacity(st);
  }
  adStatCur[(unsigned int)st] = val;
  return GetAmount(st);
}

double CShip::SetCapacity(ShipStat st, double val) {
  if (st >= S_ALL_STATS) {
    return 0.0;
  }
  if (val < 0.0) {
    val = 0.0;
  }
  if (val > g_ship_total_stat_capacity) {
    val = g_ship_total_stat_capacity;
  }

  adStatMax[(unsigned int)st] = val;

  double tot = 0.0;
  tot += adStatMax[S_CARGO];
  tot += adStatMax[S_FUEL];

  if (tot > g_ship_total_stat_capacity) {
    tot -= g_ship_total_stat_capacity;
    if (st == S_CARGO) {
      adStatMax[(unsigned int)S_FUEL] -= tot;
      if (adStatMax[(unsigned int)S_FUEL] < 0.0) {
        adStatMax[(unsigned int)S_FUEL] = 0.0;
      }
    }
    if (st == S_FUEL) {
      adStatMax[(unsigned int)S_CARGO] -= tot;
      if (adStatMax[(unsigned int)S_CARGO] < 0.0) {
        adStatMax[(unsigned int)S_CARGO] = 0.0;
      }
    }
  }

  if (GetAmount(st) > GetCapacity(st)) {
    adStatCur[(unsigned int)st] = GetCapacity(st);
  }
  return GetCapacity(st);
}

CBrain *CShip::SetBrain(CBrain *pBr) {
  // Context switching: Replace current tactical behavior with new one
  // This enables dynamic AI behavior changes based on strategic context
  CBrain *pBrTmp = GetBrain();
  pBrain = pBr;
  if (pBrain != NULL) {
    pBrain->pShip = this;  // Link brain to this ship
  }
  return pBrTmp;  // Return previous brain for cleanup/restoration
}

////////////////////////////////////////////
// Ship control

void CShip::ResetOrders() {
  dLaserDist = 0.0;
  bWasDocked = bDockFlag;  // Save previous docking state for collision logging
  bLaunchedThisTurn = false;  // Reset launch flag for new turn
  for (unsigned int ord = (unsigned int)O_SHIELD; ord < (unsigned int)O_ALL_ORDERS; ++ord) {
    adOrders[ord] = 0.0;
  }
}

// SetOrder method used for computing fuel consumed for an order
double CShip::SetOrder(OrderKind ord, double value) {
  // NOTE: Use SetJettison() and GetJettison() helper functions instead of
  // calling SetOrder(O_JETTISON, ...) directly for better type safety and
  // readability
  double valtmp, fuelcon, maxfuel;
  CTraj AccVec;

  maxfuel = GetAmount(S_FUEL);
  if (IsDocked() == true) {
    maxfuel = GetCapacity(S_FUEL);
  }

  switch (ord) {
    case O_SHIELD:  // "value" is amt by which to boost shields
      if (value < 0.0) {
        value = 0.0;  // Can't lower shields
      }
      valtmp = value + GetAmount(S_SHIELD);
      if (valtmp > GetCapacity(S_SHIELD)) {
        value = GetCapacity(S_SHIELD) - GetAmount(S_SHIELD);
      }

      fuelcon = value;
      if (fuelcon > GetAmount(S_FUEL)) {  // Check for sufficient fuel
        fuelcon = GetAmount(S_FUEL);
        value = fuelcon;  // No, but here's how much we *can* do
      }

      adOrders[(unsigned int)O_SHIELD] = value;
      return fuelcon;  // Doesn't need a break since this returns

    case O_LASER:  // "value" is specified length of laser beam
      if (value < 0.0) {
        value = 0.0;
      }
      if (IsDocked()) {  // Can't shoot while docked
        value = 0.0;
        return 0.0;
      }
      if (value > (fWXMax - fWXMin) / 2.0) {
        value = (fWXMax - fWXMin) / 2.0;
      }
      if (value > (fWYMax - fWYMin) / 2.0) {
        value = (fWYMax - fWYMin) / 2.0;
      }

      fuelcon = value / g_laser_range_per_fuel_unit;
      if (fuelcon > GetAmount(S_FUEL)) {  // Check for sufficient fuel
        fuelcon = GetAmount(S_FUEL);
        value = fuelcon * g_laser_range_per_fuel_unit;  // No, but here's how much we *can* do
      }

      adOrders[(unsigned int)O_LASER] = value;
      return fuelcon;

    case O_THRUST:  // "value" is magnitude of acceleration vector
      // Use ArgumentParser to determine which thrust processing to use
      // Default to new behavior unless explicitly set to old
      if (g_pParser && !g_pParser->UseNewFeature("velocity-limits")) {
        return ProcessThrustOrderOld(ord, value);
      } else {
        return ProcessThrustOrderNew(ord, value);
      }

    case O_TURN: {  // "value" is angle, in radians, to turn
      if (value == 0.0) {
        return 0.0;
      }
      adOrders[(unsigned int)O_THRUST] = 0.0;
      adOrders[(unsigned int)O_JETTISON] = 0.0;

      auto normalize_angle = [](double angle) {
        while (angle > PI) angle -= PI2;
        while (angle < -PI) angle += PI2;
        return angle;
      };

      bool use_new_physics = true;
      if (g_pParser != NULL) {
        use_new_physics = g_pParser->UseNewFeature("physics");
      }

      if (!use_new_physics) {
        // Legacy behavior: take the requested angle verbatim.
        fuelcon = fabs(value) * GetMass() /
                  (g_ship_turn_full_rotations_per_fuel * PI2 * mass);
        if (IsDocked() == true) {
          fuelcon = 0.0;
        }
        if (fuelcon > maxfuel) {
          fuelcon = maxfuel;
          valtmp =
              (mass * g_ship_turn_full_rotations_per_fuel * PI2 * fuelcon) /
              GetMass();
          if (value <= 0.0) {
            value = -valtmp;
          } else {
            value = valtmp;
          }
        }
        adOrders[(unsigned int)O_TURN] = value;
        return fuelcon;
      }

      // New behavior: normalize the requested turn and use physical rotation model.
      double normalized_value = normalize_angle(value);

      // Calculate fuel using physically consistent rotation model (quadratic in angle)
      fuelcon = PhysicsUtils::CalcTurnCostPhysical(fabs(normalized_value), GetMass(), size);

      if (IsDocked() == true) {
        fuelcon = 0.0;
      }

      // If we don't have enough fuel, calculate the maximum angle we can afford
      if (fuelcon > maxfuel) {
        fuelcon = maxfuel;
        // Solve: M * R² * θ² / (T² * energy_per_ton) = fuelcon
        // θ = sqrt(fuelcon * energy_per_ton * T² / (M * R²))
        double limited_angle = sqrt(fuelcon * g_ship_turn_energy_per_fuel_ton / (GetMass() * size * size));

        // Preserve sign of original turn
        if (normalized_value <= 0.0) {
          normalized_value = -limited_angle;
        } else {
          normalized_value = limited_angle;
        }
        normalized_value = normalize_angle(normalized_value);
      }

      adOrders[(unsigned int)O_TURN] = normalized_value;
      return fuelcon;
    }

    case O_JETTISON: {  // "value" is tonnage: positive for fuel, neg for cargo
      // Jettison orders have no effect while docked; avoid clearing other orders.
      if (IsDocked()) {
        return 0.0;
      }

      // NOTE: Use SetJettison() and GetJettison() helper functions instead of
      // calling SetOrder(O_JETTISON, ...) directly for better type safety
      double requestedAmount = fabs(value);

      // 1. Minimum mass threshold check
      if (requestedAmount < g_thing_minmass) {
        adOrders[(unsigned int)O_JETTISON] = 0.0;
        return 0.0;
      }

      // 2. Cancel conflicting orders
      adOrders[(unsigned int)O_THRUST] = 0.0;
      adOrders[(unsigned int)O_TURN] = 0.0;

      // 3. Determine material and inventory stat
      ShipStat inventoryStat;
      bool isFuel = (value > 0.0);

      if (isFuel) {
        inventoryStat = S_FUEL;  // Uranium
      } else {
        inventoryStat = S_CARGO;  // Vinyl
      }

      // 4. Check available inventory and clamp the amount
      double availableAmount = GetAmount(inventoryStat);
      double actualAmount = requestedAmount;
      if (requestedAmount > availableAmount) {
        actualAmount = availableAmount;
      }

      // 5. Update the order, restoring the sign
      if (isFuel) {
        adOrders[(unsigned int)O_JETTISON] = actualAmount;
        return actualAmount;
      } else {
        adOrders[(unsigned int)O_JETTISON] = -actualAmount;
        return 0.0;
      }
    }

    case O_ALL_ORDERS:
      return 0.0;

      /*
      // LEGACY CODE: This is the legacy implementation of O_JETTISON, which is
      // replaced above. In addition to being confusing due to using the sign to
      // determine the type of material jettisoned (- for vinyl, + for uranium),
      // there is a bug where it does not clamp vinyl jettison to the amount of
      // vinyl we have. E.g. if we have a command to jettison 10 vinyl and have
      // only 5 then:
      //  * value will be -10
      //  * maxfuel=GetAmount((ShipStat)oit); will be 5
      //  * if (maxfuel<value) is then if (5<-10) which is false, so we don't
      //    clamp the order to -5, and go ahead and add an O_JETTISON of -10.

      if (fabs(value) < g_thing_minmass) {
          value=0.0;
          adOrders[(unsigned int)O_JETTISON]=0.0;
          return 0.0;  // Jettisoning costs no fuel
      }

      adOrders[(unsigned int)O_THRUST] = 0.0;
      adOrders[(unsigned int)O_TURN] = 0.0;

      AsMat=URANIUM;
      if (value<=0.0) AsMat=VINYL;
      oit = (unsigned int)AstToStat(AsMat);

      maxfuel=GetAmount((ShipStat)oit);  // Not necessarily fuel
      if (maxfuel<value) value=maxfuel;
      adOrders[(unsigned int)O_JETTISON] = value;

      if (AsMat==URANIUM) return value;  // We're spitting out this much fuel
      else return 0.0;       // Jettisoning itself takes no fuel
      */

      // LEGACY: Combined fuel calculation for O_SHIELD, O_LASER, and O_THRUST
      // orders (never used) This was intended as a "how much fuel will all my
      // orders take" convenience function but was never implemented or used
      // anywhere in the codebase.
      //
      // Issues with current implementation:
      // - Recursive SetOrder calls may have unintended side effects
      // - No clear interface or documentation for O_ALL_ORDERS usage
      // - Fuel calculation logic is unclear and potentially incorrect
      //
      // To make this useful, it would need:
      // - Clear documentation of intended behavior
      // - Non-recursive fuel calculation logic
      // - Proper handling of fuel conflicts between orders
      // - Better interface design for fuel budgeting
      /*
      default:
        valtmp=0.0;
        for (oit=(unsigned int)O_SHIELD; oit<=(unsigned int)O_THRUST; ++oit)
           valtmp += SetOrder((OrderKind)oit,GetOrder((OrderKind)oit));
        return valtmp;
      */
  }

  return 0.0;  // Should never reach here, but prevents compiler warning
}

void CShip::SetJettison(AsteroidKind Mat, double amt) {
  // Jettison orders while docked are ignored; bail out before mutating orders.
  if (IsDocked()) {
    return;
  }

  switch (Mat) {
    case URANIUM:
      SetOrder(O_JETTISON, amt);
      return;
    case VINYL:
      SetOrder(O_JETTISON, -amt);
      return;
    default:
      SetOrder(O_JETTISON, 0.0);
      return;
  }
}

double CShip::GetJettison(AsteroidKind Mat) {
  double amt = GetOrder(O_JETTISON);
  if (amt > 0.0 && Mat == URANIUM) {
    return amt;
  }
  if (amt < 0.0 && Mat == VINYL) {
    return -amt;
  }
  return 0.0;
}

////////////////////////////////////////////
// Inherited methods

void CShip::Drift(double dt, double turn_phase) {
  if (GetTeam()->GetWorld()->bGameOver == true) {
    CThing::Drift(0.0);  // Ships don't move when game is over
    return;
  }

  bIsColliding = g_no_damage_sentinel;
  bIsGettingShot = g_no_damage_sentinel;

  // Check for velocity clamping before applying it
  if (Vel.rho > g_game_max_speed) {
    Vel.rho = g_game_max_speed;
  }
  // From CThing::Drift

  double thrustamt = GetOrder(O_THRUST);
  double turnamt = GetOrder(O_TURN);
  double shieldamt = GetOrder(O_SHIELD);

  uImgSet = 0;  // Assume it's just drifting for now

  // Jettisonning, then movement stuff
  HandleJettison();

  ProcessShieldOrder(shieldamt);

  double omega_result = IntegrateTurnOrder(turnamt, dt, turn_phase);
  omega = omega_result;

  IntegrateThrustOrder(thrustamt, dt);

  // Finally, update position and orientation.
  // From CThing::Drift
  //
  // TODO - factor this out in Thing.C to be a common function for CThing::Drift and us to use.
  Pos += (Vel * dt).ConvertToCoord();

  // Apply rotation: omega is angular velocity (rad/s), multiply by dt to get radians
  if (g_pParser && !g_pParser->UseNewFeature("velocity-limits")) {
    orient += omega_result * dt;  // Legacy: omega holds full turn amount (rad)
  } else {
    orient += omega_result * dt;  // New: omega is angular velocity (rad/s), dt gives radians
  }

  if (orient < -PI || orient > PI) {
    CTraj VTmp(1.0, orient);
    VTmp.Normalize();
    orient = VTmp.theta;
  }

  omega = 0.0;       // Just for good measure
  dLaserDist = 0.0;  // Don't want lasers left on
}

bool CShip::AsteroidFits(const CAsteroid *pAst) {
  double othmass = pAst->GetMass();
  switch (pAst->GetMaterial()) {
    case VINYL:
      if ((othmass + GetAmount(S_CARGO)) > GetCapacity(S_CARGO)) {
        return false;
      }
      return true;
    case URANIUM:
      if ((othmass + GetAmount(S_FUEL)) > GetCapacity(S_FUEL)) {
        return false;
      }
      return true;
    default:
      return false;
  }
}

////////////////////////////////////////////
// Battle assistants

CThing *CShip::LaserTarget() {
  if (pmyTeam == NULL) {
    return NULL;
  }

  CWorld *pWorld = pmyTeam->GetWorld();
  if (pWorld == NULL) {
    return NULL;
  }

  CThing *pTCur, *pTRes = NULL;
  double dist, mindist = -1.0;  // Start with something invalid
  unsigned int i;

  dLaserDist = 0.0;

  for (i = pWorld->UFirstIndex; i != (unsigned int)-1; i = pWorld->GetNextIndex(i)) {
    pTCur = pWorld->GetThing(i);

    if (IsFacing(*pTCur) == false) {
      continue;  // Nevermind, we're not facing it
    }

    dist = GetPos().DistTo(pTCur->GetPos());
    if (dist < mindist || mindist == -1.0) {
      mindist = dist;
      pTRes = pTCur;
    }
  }

  dLaserDist = mindist;
  double dlaspwr = GetOrder(O_LASER);
  if (mindist > dlaspwr) {
    dLaserDist = dlaspwr;
  }
  return pTRes;
}

double CShip::AngleToIntercept(const CThing &OthThing, double dtime) {
  // COORDINATE SYSTEM NOTE: In our display, +Y points downward on screen
  // Angle orientation: 0 = right (+X), PI/2 = down (+Y), PI = left (-X), -PI/2 = up (-Y)
  CCoord myPos, hisPos;
  myPos = PredictPosition(dtime);
  hisPos = OthThing.PredictPosition(dtime);

  double ang = myPos.AngleTo(hisPos), face = GetOrient(), turn = ang - face;

  if (turn < -PI || turn > PI) {
    CTraj VTmp(1.0, turn);
    VTmp.Normalize();
    turn = VTmp.theta;
  }

  return turn;
}

ShipStat CShip::AstToStat(AsteroidKind AsMat) const {
  switch (AsMat) {
    case URANIUM:
      return S_FUEL;
    case VINYL:
      return S_CARGO;
    default:
      return S_ALL_STATS;
  }
}

AsteroidKind CShip::StatToAst(ShipStat ShStat) const {
  switch (ShStat) {
    case S_FUEL:
      return URANIUM;
    case S_CARGO:
      return VINYL;
    default:
      return GENAST;
  }
}

////////////////////////////////////////////
// Protected methods

void CShip::HandleCollision(CThing *pOthThing, CWorld *pWorld) {
  extern CParser* g_pParser;

  if (g_pParser && !g_pParser->UseNewFeature("collision-handling")) {
    HandleCollisionOld(pOthThing, pWorld);
  } else {
    HandleCollisionNew(pOthThing, pWorld);
  }
}

void CShip::HandleCollisionOld(CThing *pOthThing, CWorld *pWorld) {
  // LEGACY COLLISION HANDLING
  // Preserves original behavior including recursive Collide() call for ship-ship collisions.
  // Known issues: can process same collision multiple times in legacy CollisionEvaluation.

  if (*pOthThing == *this ||  // Can't collide with yourself!
      IsDocked() == true) {   // Nothing can hurt you at a station
    bIsColliding = g_no_damage_sentinel;
    return;
  }
  if (pWorld == NULL) {
    ;  // Okay for world to be NULL, this suppresses warning
  }

  ThingKind OthKind = pOthThing->GetKind();

  if (OthKind == STATION) {
    double old_dDockDist = dDockDist;
    dDockDist = Pos.DistTo(pOthThing->GetPos());
    bIsColliding = g_no_damage_sentinel;

    // Verbose logging for docking/re-docking
    if (g_pParser && g_pParser->verbose) {
      printf("[RE-DOCK] Ship %s docking at station (distance=%.2f, vel=%.2f, old_dDockDist=%.2f, new_dDockDist=%.2f)\n",
             GetName(), Pos.DistTo(pOthThing->GetPos()), Vel.rho, old_dDockDist, dDockDist);
    }

    Pos = pOthThing->GetPos();
    Vel = CTraj(0.0, 0.0);
    SetOrder(O_THRUST, 0.0);

    // Log vinyl delivery
    double vinylDelivered = GetAmount(S_CARGO);
    if (vinylDelivered > 0.01) {
      CStation *pStation = (CStation *)pOthThing;
      if (pStation->GetTeam() == this->GetTeam()) {
        printf("[DELIVERY] Ship %s delivered %.2f vinyl to HOME base (%s)\n",
               GetName(), vinylDelivered, GetTeam()->GetName());
        if (pWorld) {
          char msg[256];
          snprintf(msg, sizeof(msg), "%s delivered %.1f vinyl to %s",
                   GetName(), vinylDelivered, pStation->GetName());
          pWorld->AddAnnouncerMessage(msg);
        }
      } else {
        printf(
            "[ENEMY DELIVERY] Ship %s delivered %.2f vinyl to ENEMY base (%s "
            "to %s)\n",
            GetName(), vinylDelivered, GetTeam()->GetName(),
            pStation->GetTeam()->GetName());
      }
    }
    ((CStation *)pOthThing)->AddVinyl(vinylDelivered);
    adStatCur[(unsigned int)S_CARGO] = 0.0;

    bDockFlag = true;
    return;
  }

  double msh, dshield = GetAmount(S_SHIELD);
  if (OthKind == GENTHING) {  // Laser object
    msh = (pOthThing->GetMass());
    dshield -= (msh / g_laser_damage_mass_divisor);

    SetAmount(S_SHIELD, dshield);
    if (dshield < 0.0) {
      printf("[DESTROYED] Ship %s (%s) destroyed by laser\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown");
      if (pWorld) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s destroyed by laser", GetName());
        pWorld->AddAnnouncerMessage(msg);
      }
      KillThing();
    }

    // Apply momentum transfer from laser (new physics mode)
    if (g_pParser && g_pParser->UseNewFeature("physics")) {
      // LASER MOMENTUM TRANSFER: Photon momentum physics
      //
      // Lasers impart momentum based on photon physics: p = E/c
      // - Laser "mass" represents beam energy (30 × remaining_length)
      // - Speed of light in game: c = 30 u/s (max ship speed)
      // - Laser velocity set to c along beam direction (World.C:277-285)
      // - Momentum transfer: p = (laser_mass / 30) × 30 = laser_mass
      //
      // For a perfectly inelastic collision (photon absorbed):
      //   v_final = (m_ship × v_ship + m_laser × v_laser) / (m_ship + m_laser)
      //
      // Since m_laser << m_ship, this simplifies to:
      //   Δv ≈ (m_laser / m_ship) × (v_laser - v_ship_projected)
      //
      // Result: Ship velocity changes by ~1-7 u/s (4-24% of max speed) depending on
      // beam strength and ship mass, pushing the ship along the beam direction.

      double m_ship = GetMass();
      double m_laser = pOthThing->GetMass();
      double total_mass = m_ship + m_laser;

      CCoord vel_ship = Vel.ConvertToCoord();
      CCoord vel_laser = pOthThing->GetVelocity().ConvertToCoord();

      // Calculate momentum-weighted average velocity (perfectly inelastic collision)
      CCoord v_final;
      v_final.fX = (m_ship * vel_ship.fX + m_laser * vel_laser.fX) / total_mass;
      v_final.fY = (m_ship * vel_ship.fY + m_laser * vel_laser.fY) / total_mass;

      // Convert back to trajectory and apply
      CTraj new_vel(v_final);

      // Enforce maximum speed
      if (new_vel.rho > g_game_max_speed) {
        new_vel.rho = g_game_max_speed;
      }

      Vel = new_vel;
    }

    return;
  }

  // Check if this is an asteroid that fits (can be "eaten")
  bool is_eatable_asteroid = false;
  if (OthKind == ASTEROID) {
    is_eatable_asteroid = AsteroidFits((CAsteroid *)pOthThing);
  }

  // Determine if we should apply collision damage
  bool apply_damage = true;
  if (is_eatable_asteroid) {
    // Use feature flag to determine behavior
    // New behavior (asteroid-eat-damage = true): no damage when eating
    // Legacy behavior (asteroid-eat-damage = false): damage even when eating
    if (g_pParser && g_pParser->UseNewFeature("asteroid-eat-damage")) {
      apply_damage = false;  // New: no damage when eating asteroids that fit
    } else {
      apply_damage = true;   // Legacy: damage even when eating
    }
  }

  if (apply_damage) {
    double damage;

    // Use feature flag to determine damage model
    if (g_pParser && g_pParser->UseNewFeature("physics")) {
      // NEW PHYSICS: Damage based on momentum change |Δp|
      // Both ships in a collision experience equal magnitude momentum change
      // (Newton's 3rd law), so both take the same damage.
      // Formula: damage = |Δp| / divisor
      damage = CalculateCollisionMomentumChange(pOthThing) / g_laser_damage_mass_divisor;
    } else {
      // LEGACY: Damage based on relative momentum = m_other × v_rel
      // This causes asymmetric damage: lighter ships take more damage.
      damage = (RelativeMomentum(*pOthThing).rho) / g_laser_damage_mass_divisor;
    }

    dshield -= damage;
    SetAmount(S_SHIELD, dshield);

    if (dshield < 0.0) {
      const char *causeType = "unknown";
      if (pOthThing->GetKind() == SHIP) {
        causeType = "ship collision";
      } else if (pOthThing->GetKind() == ASTEROID) {
        causeType = "asteroid collision";
      }
      printf("[DESTROYED] Ship %s (%s) destroyed by %s\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown", causeType);
      if (pWorld) {
        char msg[256];
        const char *shortCause = (pOthThing->GetKind() == SHIP) ? "ship" : "asteroid";
        snprintf(msg, sizeof(msg), "%s destroyed by %s", GetName(), shortCause);
        pWorld->AddAnnouncerMessage(msg);
      }
      KillThing();
    }
  }

  if (OthKind == ASTEROID) {
    CThing *pEat = ((CAsteroid *)pOthThing)->EatenBy();
    if (pEat != NULL && !(*pEat == *this)) {
      // Already taken by another ship
      return;
    }

    bool asteroidFits = AsteroidFits((CAsteroid *)pOthThing);

    if (asteroidFits) {
      // SMALL ASTEROID (fits in cargo): Perfectly inelastic collision
      // Ship absorbs the asteroid, combining masses and conserving momentum.
      // This is correct physics for an inelastic collision.
      CTraj MomTot = GetMomentum() + pOthThing->GetMomentum();
      double othmass = pOthThing->GetMass();
      double masstot = GetMass() + othmass;
      Vel = MomTot / masstot;
      if (Vel.rho > g_game_max_speed) {
        Vel.rho = g_game_max_speed;
      }

      // Add asteroid mass to ship's cargo
      switch (((CAsteroid *)pOthThing)->GetMaterial()) {
        case VINYL:
          adStatCur[(unsigned int)S_CARGO] += othmass;
          break;
        case URANIUM:
          adStatCur[(unsigned int)S_FUEL] += othmass;
          break;
        default:
          break;
      }
    } else {
      // LARGE ASTEROID (doesn't fit): Collision physics depends on mode
      if (g_pParser && !g_pParser->UseNewFeature("physics")) {
        // Legacy mode: Inelastic collision (even though asteroid doesn't stick!)
        // This is physically incorrect but preserves old behavior
        CTraj MomTot = GetMomentum() + pOthThing->GetMomentum();
        double othmass = pOthThing->GetMass();
        double masstot = GetMass() + othmass;
        Vel = MomTot / masstot;
        if (Vel.rho > g_game_max_speed) {
          Vel.rho = g_game_max_speed;
        }
      } else {
        // New mode: Perfectly elastic collision
        // Ship and asteroid bounce off each other, conserving momentum and energy
        HandleElasticShipCollision(pOthThing);
      }
    }
  }

  if (OthKind == SHIP && pOthThing->GetTeam() != NULL) {
    CTeam *pTmpTm = pmyTeam;
    pmyTeam = NULL;  // Prevents an infinite recursive call.
    pOthThing->Collide(this, pWorld);
    pmyTeam = pTmpTm;
  }

  // Handle separation/positioning after collision
  // This section is only for ship-ship collisions in both legacy and new modes
  // Ship-asteroid collision physics is handled above (lines 758-808)
  if (OthKind == SHIP && pOthThing->GetTeam() != NULL) {
    if (g_pParser && !g_pParser->UseNewFeature("physics")) {
      // Legacy mode: Non-physical separation impulse (violates momentum conservation)
      double dang = pOthThing->GetPos().AngleTo(GetPos());
      double dsmov = pOthThing->GetSize() + 3.0;
      CTraj MovVec(dsmov, dang);
      CCoord MovCoord(MovVec);
      Pos += MovCoord;

      double dmassrat = pOthThing->GetMass() / GetMass();
      MovVec = MovVec * dmassrat;
      Vel += MovVec;
      if (Vel.rho > g_game_max_speed) {
        Vel.rho = g_game_max_speed;
      }
    } else {
      // New physics mode: Perfectly elastic collision between ships
      // Conserves both momentum and kinetic energy
      HandleElasticShipCollision(pOthThing);
    }
  }
}

void CShip::HandleCollisionNew(CThing *pOthThing, CWorld *pWorld) {
  // NEW COLLISION HANDLING
  // Currently identical to legacy - will be updated to remove recursive Collide() calls
  // and fix multi-processing issues.

  if (*pOthThing == *this ||  // Can't collide with yourself!
      IsDocked() == true) {   // Nothing can hurt you at a station
    bIsColliding = g_no_damage_sentinel;
    return;
  }
  if (pWorld == NULL) {
    ;  // Okay for world to be NULL, this suppresses warning
  }

  ThingKind OthKind = pOthThing->GetKind();

  if (OthKind == STATION) {
    double old_dDockDist = dDockDist;
    dDockDist = Pos.DistTo(pOthThing->GetPos());
    bIsColliding = g_no_damage_sentinel;

    // Verbose logging for docking/re-docking
    if (g_pParser && g_pParser->verbose) {
      printf("[RE-DOCK] Ship %s docking at station (distance=%.2f, vel=%.2f, old_dDockDist=%.2f, new_dDockDist=%.2f)\n",
             GetName(), Pos.DistTo(pOthThing->GetPos()), Vel.rho, old_dDockDist, dDockDist);
    }

    Pos = pOthThing->GetPos();
    Vel = CTraj(0.0, 0.0);
    SetOrder(O_THRUST, 0.0);

    // Log vinyl delivery
    double vinylDelivered = GetAmount(S_CARGO);
    if (vinylDelivered > 0.01) {
      CStation *pStation = (CStation *)pOthThing;
      if (pStation->GetTeam() == this->GetTeam()) {
        printf("[DELIVERY] Ship %s delivered %.2f vinyl to HOME base (%s)\n",
               GetName(), vinylDelivered, GetTeam()->GetName());
        if (pWorld) {
          char msg[256];
          snprintf(msg, sizeof(msg), "%s delivered %.1f vinyl to %s",
                   GetName(), vinylDelivered, pStation->GetName());
          pWorld->AddAnnouncerMessage(msg);
        }
      } else {
        printf(
            "[ENEMY DELIVERY] Ship %s delivered %.2f vinyl to ENEMY base (%s "
            "to %s)\n",
            GetName(), vinylDelivered, GetTeam()->GetName(),
            pStation->GetTeam()->GetName());
      }
    }
    ((CStation *)pOthThing)->AddVinyl(vinylDelivered);
    adStatCur[(unsigned int)S_CARGO] = 0.0;

    bDockFlag = true;
    return;
  }

  double msh, dshield = GetAmount(S_SHIELD);
  if (OthKind == GENTHING) {  // Laser object
    msh = (pOthThing->GetMass());
    dshield -= (msh / g_laser_damage_mass_divisor);

    SetAmount(S_SHIELD, dshield);
    if (dshield < 0.0) {
      printf("[DESTROYED] Ship %s (%s) destroyed by laser\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown");
      if (pWorld) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s destroyed by laser", GetName());
        pWorld->AddAnnouncerMessage(msg);
      }
      KillThing();
    }

    // Apply momentum transfer from laser (new physics mode)
    if (g_pParser && g_pParser->UseNewFeature("physics")) {
      // LASER MOMENTUM TRANSFER: Photon momentum physics
      //
      // Lasers impart momentum based on photon physics: p = E/c
      // - Laser "mass" represents beam energy (30 × remaining_length)
      // - Speed of light in game: c = 30 u/s (max ship speed)
      // - Laser velocity set to c along beam direction (World.C:277-285)
      // - Momentum transfer: p = (laser_mass / 30) × 30 = laser_mass
      //
      // For a perfectly inelastic collision (photon absorbed):
      //   v_final = (m_ship × v_ship + m_laser × v_laser) / (m_ship + m_laser)
      //
      // Since m_laser << m_ship, this simplifies to:
      //   Δv ≈ (m_laser / m_ship) × (v_laser - v_ship_projected)
      //
      // Result: Ship velocity changes by ~1-7 u/s (4-24% of max speed) depending on
      // beam strength and ship mass, pushing the ship along the beam direction.

      double m_ship = GetMass();
      double m_laser = pOthThing->GetMass();
      double total_mass = m_ship + m_laser;

      CCoord vel_ship = Vel.ConvertToCoord();
      CCoord vel_laser = pOthThing->GetVelocity().ConvertToCoord();

      // Calculate momentum-weighted average velocity (perfectly inelastic collision)
      CCoord v_final;
      v_final.fX = (m_ship * vel_ship.fX + m_laser * vel_laser.fX) / total_mass;
      v_final.fY = (m_ship * vel_ship.fY + m_laser * vel_laser.fY) / total_mass;

      // Convert back to trajectory and apply
      CTraj new_vel(v_final);

      // Enforce maximum speed
      if (new_vel.rho > g_game_max_speed) {
        new_vel.rho = g_game_max_speed;
      }

      Vel = new_vel;
    }

    return;
  }

  // Check if this is an asteroid that fits (can be "eaten")
  bool is_eatable_asteroid = false;
  if (OthKind == ASTEROID) {
    is_eatable_asteroid = AsteroidFits((CAsteroid *)pOthThing);
  }

  // Determine if we should apply collision damage
  bool apply_damage = true;
  if (is_eatable_asteroid) {
    // Use feature flag to determine behavior
    // New behavior (asteroid-eat-damage = true): no damage when eating
    // Legacy behavior (asteroid-eat-damage = false): damage even when eating
    if (g_pParser && g_pParser->UseNewFeature("asteroid-eat-damage")) {
      apply_damage = false;  // New: no damage when eating asteroids that fit
    } else {
      apply_damage = true;   // Legacy: damage even when eating
    }
  }

  if (apply_damage) {
    double damage;

    // Use feature flag to determine damage model
    if (g_pParser && g_pParser->UseNewFeature("physics")) {
      // NEW PHYSICS: Damage based on momentum change |Δp|
      // Both ships in a collision experience equal magnitude momentum change
      // (Newton's 3rd law), so both take the same damage.
      // Formula: damage = |Δp| / divisor
      damage = CalculateCollisionMomentumChange(pOthThing) / g_laser_damage_mass_divisor;
    } else {
      // LEGACY: Damage based on relative momentum = m_other × v_rel
      // This causes asymmetric damage: lighter ships take more damage.
      damage = (RelativeMomentum(*pOthThing).rho) / g_laser_damage_mass_divisor;
    }

    dshield -= damage;
    SetAmount(S_SHIELD, dshield);

    if (pWorld && damage > 0.1) {  // Only announce significant collisions
      char msg[256];
      const char *targetName = "unknown";
      if (pOthThing->GetKind() == SHIP) {
        targetName = pOthThing->GetName();
      } else if (pOthThing->GetKind() == ASTEROID) {
        targetName = "asteroid";
      }
      snprintf(msg, sizeof(msg), "%s hit %s, %.1f damage",
               GetName(), targetName, damage);
      pWorld->AddAnnouncerMessage(msg);
    }
    if (dshield < 0.0) {
      const char *causeType = "unknown";
      if (pOthThing->GetKind() == SHIP) {
        causeType = "ship collision";
      } else if (pOthThing->GetKind() == ASTEROID) {
        causeType = "asteroid collision";
      }
      printf("[DESTROYED] Ship %s (%s) destroyed by %s\n", GetName(),
             GetTeam() ? GetTeam()->GetName() : "Unknown", causeType);
      if (pWorld) {
        char msg[256];
        const char *shortCause = (pOthThing->GetKind() == SHIP) ? "ship" : "asteroid";
        snprintf(msg, sizeof(msg), "%s destroyed by %s", GetName(), shortCause);
        pWorld->AddAnnouncerMessage(msg);
      }
      KillThing();
    }
  }

  if (OthKind == ASTEROID) {
    CThing *pEat = ((CAsteroid *)pOthThing)->EatenBy();
    if (pEat != NULL && !(*pEat == *this)) {
      // Already taken by another ship
      return;
    }

    bool asteroidFits = AsteroidFits((CAsteroid *)pOthThing);

    if (asteroidFits) {
      // SMALL ASTEROID (fits in cargo): Perfectly inelastic collision
      // Ship absorbs the asteroid, combining masses and conserving momentum.
      // This is correct physics for an inelastic collision.
      CTraj MomTot = GetMomentum() + pOthThing->GetMomentum();
      double othmass = pOthThing->GetMass();
      double masstot = GetMass() + othmass;
      Vel = MomTot / masstot;
      if (Vel.rho > g_game_max_speed) {
        Vel.rho = g_game_max_speed;
      }

      // Add asteroid mass to ship's cargo
      switch (((CAsteroid *)pOthThing)->GetMaterial()) {
        case VINYL:
          adStatCur[(unsigned int)S_CARGO] += othmass;
          break;
        case URANIUM:
          adStatCur[(unsigned int)S_FUEL] += othmass;
          break;
        default:
          break;
      }
    } else {
      // LARGE ASTEROID (doesn't fit): Collision physics depends on mode
      if (g_pParser && !g_pParser->UseNewFeature("physics")) {
        // Legacy mode: Inelastic collision (even though asteroid doesn't stick!)
        // This is physically incorrect but preserves old behavior
        CTraj MomTot = GetMomentum() + pOthThing->GetMomentum();
        double othmass = pOthThing->GetMass();
        double masstot = GetMass() + othmass;
        Vel = MomTot / masstot;
        if (Vel.rho > g_game_max_speed) {
          Vel.rho = g_game_max_speed;
        }
      } else {
        // New mode: Perfectly elastic collision
        // Ship and asteroid bounce off each other, conserving momentum and energy
        HandleElasticShipCollision(pOthThing);
      }
    }
  }

  if (OthKind == SHIP && pOthThing->GetTeam() != NULL) {
    CTeam *pTmpTm = pmyTeam;
    pmyTeam = NULL;  // Prevents an infinite recursive call.
    pOthThing->Collide(this, pWorld);
    pmyTeam = pTmpTm;
  }

  // Handle separation/positioning after collision
  // This section is only for ship-ship collisions in both legacy and new modes
  // Ship-asteroid collision physics is handled above (lines 758-808)
  if (OthKind == SHIP && pOthThing->GetTeam() != NULL) {
    if (g_pParser && !g_pParser->UseNewFeature("physics")) {
      // Legacy mode: Non-physical separation impulse (violates momentum conservation)
      double dang = pOthThing->GetPos().AngleTo(GetPos());
      double dsmov = pOthThing->GetSize() + 3.0;
      CTraj MovVec(dsmov, dang);
      CCoord MovCoord(MovVec);
      Pos += MovCoord;

      double dmassrat = pOthThing->GetMass() / GetMass();
      MovVec = MovVec * dmassrat;
      Vel += MovVec;
      if (Vel.rho > g_game_max_speed) {
        Vel.rho = g_game_max_speed;
      }
    } else {
      // New physics mode: Perfectly elastic collision between ships
      // Conserves both momentum and kinetic energy
      HandleElasticShipCollision(pOthThing);
    }
  }
}

void CShip::HandleJettison() {
  AsteroidKind AsMat;
  double dMass;

  if (GetTeam() == NULL) {
    return;
  }
  CWorld *pWld = GetTeam()->GetWorld();
  if (pWld == NULL) {
    return;
  }

  if (IsDocked()) {
    return;
  }

  AsMat = URANIUM;
  dMass = GetOrder(O_JETTISON);
  if (fabs(dMass) < g_thing_minmass) {
    return;
  }
  if (dMass < 0.0) {
    dMass *= -1;
    AsMat = VINYL;
  }

  CAsteroid *pAst = new CAsteroid(dMass, AsMat);
  CCoord AstPos(Pos);
  CTraj AstVel(Vel);

  // Place the asteroid at a distance from the ship to avoid overlap.
  CTraj MovVec(Vel);
  double totsize = GetSize() + pAst->GetSize();
  MovVec.rho = totsize * 1.15;
  MovVec.theta = GetOrient();
  // Pos -= MovVec.ConvertToCoord();
  AstPos += MovVec.ConvertToCoord();

  // Set the asteroid's stats and add it to the world
  AstVel.theta = GetOrient();
  pAst->SetPos(AstPos);
  pAst->SetVel(AstVel);
  pWld->AddThingToWorld(pAst);

  // Set your own stats to accommodate
  double dnewmass = GetMass() - dMass;
  MovVec = GetMomentum();

  // Use feature flag to control momentum conservation
  if (g_pParser && !g_pParser->UseNewFeature("physics")) {
    // Legacy mode: 2x recoil (buggy but historical)
    MovVec -= (pAst->GetMomentum() * 2.0);
  } else {
    // New mode: Correct conservation of momentum
    MovVec -= pAst->GetMomentum();
  }

  MovVec = MovVec / dnewmass;

  Vel = MovVec;
  if (Vel.rho > g_game_max_speed) {
    Vel.rho = g_game_max_speed;
  }
  SetOrder(O_JETTISON, 0.0);

  double matamt = GetAmount(AstToStat(AsMat));
  matamt -= dMass;
  SetAmount(AstToStat(AsMat), matamt);
}

///////////////////////////////////////////////////
// Private methods

// Helper math for the new thrust governor (velocity-circle projection).
// Kept local to this translation unit.
static inline double FuelPerDV(double current_mass, double hull_mass) {
  // 1 ton of fuel accelerates a naked ship (mass=hull_mass) from 0 to 6*V.
  // With payload, cost scales linearly with current total mass:
  //   cost_per_dv = current_mass / (6 * V * hull_mass)
  return current_mass / (6.0 * g_game_max_speed * hull_mass);
}
static inline CCoord UnitFromAngle(double ang) {
  CTraj t(1.0, ang);
  return t.ConvertToCoord();
}
static inline double Dot(const CCoord& a, const CCoord& b) {
  return a.fX * b.fX + a.fY * b.fY;
}
// Closed-form clamp for a single instantaneous impulse s along unit u,
// starting from velocity v (cartesian), with speed cap V and a "dv-budget"
// Smax (fuel_avail converted to dv units). See derivation in discussion:
//   If s reaches the speed circle: s_hit = -a + sqrt(a^2 + (V^2 - |v|^2)),
//   where a = v dot u. If Smax <= s_hit, take s=Smax. If request <= s_hit, take it.
//   Otherwise solve s + (|v+su|-V) = Smax  => closed form:
//     s = ((V + Smax)^2 - |v|^2) / (2 * (V + Smax + a)).
static inline double ClampSingleImpulseS(double s_req,
                                         const CCoord& vCart,
                                         const CCoord& u,
                                         double V,
                                         double Smax) {
  if (s_req <= 0.0) return 0.0;
  if (Smax <= g_fp_error_epsilon) return 0.0;
  const double vx = vCart.fX, vy = vCart.fY;
  const double v2 = vx * vx + vy * vy;
  const double a  = Dot(vCart, u);              // component of v along u
  double under = a * a + (V * V - v2);
  if (under < 0.0) under = 0.0;                 // numeric guard
  const double s_hit = -a + sqrt(under);        // first contact with |v+su|=V

  // If budget can't reach the circle, just spend the budget (or request).
  if (Smax <= s_hit + 1e-12) {
    double s = Smax;
    if (s > s_req) s = s_req;
    if (s < 0.0) s = 0.0;
    return s;
  }
  // If the request itself doesn't reach the circle, take it fully.
  if (s_req <= s_hit + 1e-12) {
    return s_req;
  }
  // Otherwise, budget allows overshoot; solve s + (|v+su|-V) = Smax.
  const double B = V + Smax;
  const double denom = 2.0 * (B + a);
  double s_star = (denom != 0.0) ? ((B * B - v2) / denom) : 0.0;
  if (s_star < 0.0) s_star = 0.0;
  if (s_star > s_req) s_star = s_req;          // never exceed the request
  return s_star;
}

// Helper to clamp velocity to max speed and calculate overshoot
static double ClampVelocityToMaxSpeed(CTraj& velocity) {
  double overshoot = 0.0;
  if (velocity.rho > g_game_max_speed + g_fp_error_epsilon) {
    overshoot = velocity.rho - g_game_max_speed;
    velocity.rho = g_game_max_speed;
  }
  return overshoot;
}

// Calculate cost and achieved delta-v for a single instantaneous thrust.
CShip::ThrustCost CShip::CalcThrustCost(double thrustamt,
                                        CTraj v,
                                        double orient,
                                        double current_mass,
                                        double fuel_avail,
                                        bool is_docked,
                                        bool launched_this_turn) const {
  if (thrustamt == 0.0) {
    return ThrustCost{false, 0.0, 0.0, 0.0, CTraj(0.0, 0.0)};
  }

  // === Phase 1: Validate and clamp thrust command ===
  if (thrustamt > g_game_max_thrust_order_mag) {
    thrustamt = g_game_max_thrust_order_mag;
  } else if (thrustamt < -g_game_max_thrust_order_mag) {
    thrustamt = -g_game_max_thrust_order_mag;
  }

  // === Phase 2: Calculate thrust parameters ===
  const double thrust_magnitude = (thrustamt >= 0.0) ? thrustamt : -thrustamt;

  // Command direction (flip 180 degrees if negative thrust)
  double thrust_angle = orient;
  if (thrustamt < 0.0) {
    thrust_angle += PI;
  }
  const CCoord thrust_direction = UnitFromAngle(thrust_angle);
  const CCoord velocity_cartesian = v.ConvertToCoord();

  // === Phase 3: Calculate fuel constraints ===
  // Cost-per-unit-delta-v for this impulse (uses current total mass and hull mass `mass`)
  const double fuel_cost_per_delta_v = FuelPerDV(current_mass, mass);

  // Check if thrust should be free (docked or launched this turn)
  bool is_free_thrust = (is_docked || launched_this_turn);

  // Budget in "dv-equivalent" units (so geometry & penalty share the same units)
  double max_delta_v_budget;
  if (is_free_thrust) {
    max_delta_v_budget = 1.0e300;  // Effectively unlimited
  } else if (fuel_cost_per_delta_v > 0.0) {
    max_delta_v_budget = fuel_avail / fuel_cost_per_delta_v;
  } else {
    max_delta_v_budget = 0.0;
  }

  // === Phase 4: Calculate achievable thrust within constraints ===
  // Maximum delta-v magnitude we can actually apply this tick, respecting fuel
  double applied_thrust_mag = ClampSingleImpulseS(thrust_magnitude, velocity_cartesian,
                                                   thrust_direction, g_game_max_speed,
                                                   max_delta_v_budget);

  // === Phase 5: Apply thrust and handle velocity clamping ===
  // Build the attempted velocity and clip to the speed circle if needed
  const double signed_thrust = (thrustamt >= g_fp_error_epsilon) ? applied_thrust_mag : -applied_thrust_mag;
  CTraj delta_v_attempted(signed_thrust, orient);
  CTraj desired_velocity = v + delta_v_attempted;  // pre-clamp ("desired")
  double overshoot = ClampVelocityToMaxSpeed(desired_velocity);
  CTraj actual_delta_v = desired_velocity - v;    // actually applied delta-v

  // === Phase 6: Calculate initial costs ===
  // Thrust cost is on applied_thrust_mag; governor cost is on overshoot length
  double thrust_cost   = is_free_thrust ? 0.0 : (fuel_cost_per_delta_v * applied_thrust_mag);
  double governor_cost = is_free_thrust ? 0.0 : (fuel_cost_per_delta_v * overshoot);
  double total_cost    = thrust_cost + governor_cost;

  // === Phase 7: Handle fuel budget overflow (rescaling if needed) ===
  // Numeric safety: never exceed available fuel by rounding
  if (!is_free_thrust && total_cost > fuel_avail + g_fp_error_epsilon) {
    const double scale = fuel_avail / total_cost;
    double scaled_thrust = (scale > g_fp_error_epsilon) ? (applied_thrust_mag * scale) : 0.0;
    const double scaled_signed_thrust = (thrustamt >= g_fp_error_epsilon) ? scaled_thrust : -scaled_thrust;
    CTraj scaled_delta_v(scaled_signed_thrust, orient);
    CTraj scaled_desired_velocity = v + scaled_delta_v;
    double scaled_overshoot = ClampVelocityToMaxSpeed(scaled_desired_velocity);

    actual_delta_v  = scaled_desired_velocity - v;
    thrust_cost     = fuel_cost_per_delta_v * scaled_thrust;
    governor_cost   = fuel_cost_per_delta_v * scaled_overshoot;
    total_cost      = thrust_cost + governor_cost;
    applied_thrust_mag = scaled_thrust;
  }

  // === Phase 8: Determine if thrust was fuel-limited ===
  const bool fuel_limited =
      (!is_free_thrust) && (thrust_magnitude > applied_thrust_mag + g_fp_error_epsilon ||
                            total_cost + g_fp_error_epsilon >= fuel_avail);

  // === Phase 9: Build and return result structure ===
  ThrustCost out;
  out.fuel_limited = fuel_limited;
  out.thrust_cost = thrust_cost;
  out.governor_cost = governor_cost;
  out.total_cost = total_cost;
  out.dv_achieved = actual_delta_v;
  return out;
}

///////////////////////////////////////////////////
// Serialization routines

unsigned CShip::GetSerialSize() const {
  unsigned int totsize = 0;

  totsize += CThing::GetSerialSize();
  totsize += BufWrite(NULL, myNum);
  totsize += BufWrite(NULL, bDockFlag);
  totsize += BufWrite(NULL, dDockDist);
  totsize += BufWrite(NULL, dLaserDist);

  unsigned int i;
  for (i = 0; i < (unsigned int)O_ALL_ORDERS; ++i) {
    totsize += BufWrite(NULL, adOrders[i]);
  }

  for (i = 0; i < (unsigned int)S_ALL_STATS; ++i) {
    totsize += BufWrite(NULL, adStatCur[i]);
    totsize += BufWrite(NULL, adStatMax[i]);
  }

  return totsize;
}

unsigned CShip::SerialPack(char *buf, unsigned buflen) const {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char *vpb = buf;

  vpb += (CThing::SerialPack(buf, buflen));
  vpb += BufWrite(vpb, myNum);
  vpb += BufWrite(vpb, bDockFlag);
  vpb += BufWrite(vpb, dDockDist);
  vpb += BufWrite(vpb, dLaserDist);

  unsigned int i;
  for (i = 0; i < (unsigned int)O_ALL_ORDERS; ++i) {
    vpb += BufWrite(vpb, adOrders[i]);
  }

  for (i = 0; i < (unsigned int)S_ALL_STATS; ++i) {
    vpb += BufWrite(vpb, adStatCur[i]);
    vpb += BufWrite(vpb, adStatMax[i]);
  }

  return (vpb - buf);
}

unsigned CShip::SerialUnpack(char *buf, unsigned buflen) {
  if (buflen < GetSerialSize()) {
    return 0;
  }
  char *vpb = buf;

  vpb += (CThing::SerialUnpack(buf, buflen));
  vpb += BufRead(vpb, myNum);
  vpb += BufRead(vpb, bDockFlag);
  vpb += BufRead(vpb, dDockDist);
  vpb += BufRead(vpb, dLaserDist);

  unsigned int i;
  for (i = 0; i < (unsigned int)O_ALL_ORDERS; ++i) {
    vpb += BufRead(vpb, adOrders[i]);
  }

  for (i = 0; i < (unsigned int)S_ALL_STATS; ++i) {
    vpb += BufRead(vpb, adStatCur[i]);
    vpb += BufRead(vpb, adStatMax[i]);
  }

  return (vpb - buf);
}


///////////////////////////////////////////////////
// Feature Flag Controlled Methods - Note - Never call these methods directly,
// call the public methods in the class and it will redirect to the correct ones
// here based on command line options.

double CShip::ProcessThrustOrderNew(OrderKind ord, double value) {
  (void)ord;
  // This is substantially similar to the legacy code, however we leave speed
  // enforcement to the Drift engine entirely in the new version. Drift knows
  // about the real situaion, perhaps we've collided or otherwise our velocity
  // has changed since the order was issued, and Drift goes in dt ticks (usually
  // 0.2 seconds) - so the fuelcon here is only an estimate.
  if (value == 0.0) {
    return 0.0;
  }

  // Clamp our order value to the maximum possible order value.
  if (value > g_game_max_thrust_order_mag) {
    value = g_game_max_thrust_order_mag;
  } else if (value < -g_game_max_thrust_order_mag) {
    value = -g_game_max_thrust_order_mag;
  }

  // Cancel conflicting orderst this turn
  adOrders[(unsigned int)O_TURN] = 0.0;
  adOrders[(unsigned int)O_JETTISON] = 0.0;

  // Use an integer step counter so the number of physics ticks is immune to
  // floating-point accumulation error from "t += tstep" comparisons. In older
  // builds the final iteration could be skipped if rounding nudged t past maxt.
  // This block retains the desired behavior of always running the loop at least
  // once, even if tstep >= maxt.
  int stepCount = 0;
  if (g_game_turn_duration > 0.0 && g_physics_simulation_dt > 0.0) {
    stepCount = static_cast<int>(g_game_turn_duration / g_physics_simulation_dt);
    if (static_cast<double>(stepCount) * g_physics_simulation_dt <
        g_game_turn_duration) {
      stepCount++;
    }
    if (stepCount <= 0) {
      stepCount = 1;
    }
  }

  CTraj v_sim = Vel;
  double current_mass = GetMass();
  double fuel_avail = GetAmount(S_FUEL);
  double est_cost = 0.0;

  for (int i = 0; i < stepCount; ++i) {
    if (fuel_avail <= g_fp_error_epsilon) {
      break;
    }

    ThrustCost tc = CalcThrustCost(value*g_physics_simulation_dt, v_sim, GetOrient(), current_mass, fuel_avail, IsDocked(), bLaunchedThisTurn);
    fuel_avail -= tc.total_cost;
    est_cost += tc.total_cost;
    current_mass -= tc.total_cost; // -1 fuel == -1 ton of mass
    v_sim += tc.dv_achieved;
  }

  adOrders[(unsigned int)O_THRUST] = value;
  return est_cost;
}

double CShip::ProcessThrustOrderOld(OrderKind ord, double value) {
  (void)ord;
  // Legacy thrust order processing - contains the current SetOrder O_THRUST logic
  double valtmp, fuelcon, maxfuel;
  CTraj AccVec;

  maxfuel = GetAmount(S_FUEL);
  if (IsDocked() == true) {
    maxfuel = GetCapacity(S_FUEL);
  }

  if (value == 0.0) {
    return 0.0;
  }
  adOrders[(unsigned int)O_TURN] = 0.0;
  adOrders[(unsigned int)O_JETTISON] = 0.0;

  AccVec = CTraj(value, orient);
  AccVec += Vel;
  if (AccVec.rho > g_game_max_speed) {
    AccVec.rho = g_game_max_speed;
  }
  AccVec = AccVec - Vel;  // Should = what it was before, in most cases
  if (value <= 0.0) {
    value = -AccVec.rho;
  } else {
    value = AccVec.rho;
  }

  // 1 ton of fuel accelerates a naked ship from zero to 6.0*maxspeed
  fuelcon = fabs(value) * GetMass() / (6.0 * g_game_max_speed * mass);
  if (fuelcon > maxfuel && IsDocked() == false) {
    fuelcon = maxfuel;
    valtmp = fuelcon * 6.0 * g_game_max_speed * mass / GetMass();
    // If our original requested thrust was negative, make our clamped value
    // negative as well.
    if (value <= 0.0) {
      value = -valtmp;
    } else {
      value = valtmp;
    }
  }
  if (IsDocked() == true) {
    fuelcon = 0.0;
  }

  adOrders[(unsigned int)O_THRUST] = value;
  return fuelcon;
}

void CShip::ProcessThrustDriftNew(double thrustamt, double dt) {
  const double fuel_avail = GetAmount(S_FUEL);

  ThrustCost tc = CalcThrustCost(thrustamt*dt, Vel, GetOrient(), GetMass(), fuel_avail, IsDocked(), bLaunchedThisTurn);

  SetAmount(S_FUEL, fuel_avail - tc.total_cost);

  Vel += tc.dv_achieved;

  // Check if out of fuel
  if (fuel_avail > 0.01 && GetAmount(S_FUEL) <= 0.01) {
    printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", GetName(),
           GetTeam() ? GetTeam()->GetName() : "Unknown");
    AnnounceOutOfFuel();
  }

  // Special Docking Departure Positional Adjustment.
  if (IsDocked()) {
    // Calculate launch distance based on docking feature flag
    double launch_distance;
    CStation* pStation = pmyTeam->GetStation();

    if (g_pParser && !g_pParser->UseNewFeature("docking")) {
      // Legacy mode: use historical dDockDist + 5.0 (can cause re-docking bug)
      launch_distance = dDockDist + 5.0;
    } else {
      // New mode: fixed safe distance = station_radius + ship_radius + (ship_radius / 2.0)
      // = 30 + 12 + 6 = 48 units
      // This guarantees clearance beyond collision threshold (30 + 12 = 42 units)
      double station_radius = pStation->GetSize();
      double ship_radius = GetSize();
      launch_distance = station_radius + ship_radius + (ship_radius / 2.0);
    }

    CTraj VOff(launch_distance, orient);
    if (thrustamt > 0.0) {
      Pos += VOff.ConvertToCoord();
    } else {
      Pos -= VOff.ConvertToCoord();
    }

    // Verbose logging for undocking
    if (g_pParser && g_pParser->verbose) {
      CCoord station_pos = pStation->GetPos();
      double actual_distance = Pos.DistTo(station_pos);
      const char* mode = (g_pParser->UseNewFeature("docking")) ? "NEW" : "LEGACY";
      printf("[UNDOCK-%s] Ship %s launching from station (dDockDist=%.2f, launch_distance=%.2f, actual_distance=%.2f, orient=%.2f, vel=%.2f)\n",
             mode, GetName(), dDockDist, launch_distance, actual_distance, orient, Vel.rho);
    }

    bDockFlag = false;
    bLaunchedThisTurn = true;  // Mark that we launched this turn (makes thrust free)
  }

  if (thrustamt < 0.0) {
    uImgSet = 2;
  } else {
    uImgSet = 1;
  }
}


void CShip::ProcessThrustDriftOld(double thrustamt, double dt) {
  // Legacy thrust drift processing - contains the current Drift thrusting logic
  double fuelcons;

  // Calculate fuel consumption directly using legacy method
  //
  // NOTE: There was a bug (or maybe intentional game balancing?) here where the
  // full fuel cost of the thrust is applied on each game tick. Since elsewhere
  // we document the fuel costs it seems more like a bug, but maybe that's why
  // there is a 6x multiplier in the calculation of how much thrust you get per
  // ton of fuel to cancel out the bug. Anyway - the legacy behavior is left
  // alone here.
  fuelcons = ProcessThrustOrderOld(O_THRUST, thrustamt);
  double oldFuel = GetAmount(S_FUEL);
  double newFuel = oldFuel - fuelcons;
  SetAmount(S_FUEL, newFuel);

  // Check if out of fuel
  if (oldFuel > 0.01 && newFuel <= 0.01) {
    printf("[OUT OF FUEL] Ship %s (%s) ran out of fuel\n", GetName(),
           GetTeam() ? GetTeam()->GetName() : "Unknown");
    AnnounceOutOfFuel();
  }

  CTraj Accel(thrustamt, orient);
  Vel += (Accel * dt);
  if (Vel.rho > g_game_max_speed) {
    Vel.rho = g_game_max_speed;
  }

  if (bDockFlag == true) {
    // Calculate launch distance based on docking feature flag (same logic as new mode)
    double launch_distance;
    CStation* pStation = pmyTeam->GetStation();

    if (g_pParser && !g_pParser->UseNewFeature("docking")) {
      // Legacy mode: use historical dDockDist + 5.0 (can cause re-docking bug)
      launch_distance = dDockDist + 5.0;
    } else {
      // New mode: fixed safe distance = station_radius + ship_radius + (ship_radius / 2.0)
      double station_radius = pStation->GetSize();
      double ship_radius = GetSize();
      launch_distance = station_radius + ship_radius + (ship_radius / 2.0);
    }

    CTraj VOff(launch_distance, orient);
    if (GetOrder(O_THRUST) > 0.0) {
      Pos += VOff.ConvertToCoord();
    } else {
      Pos -= VOff.ConvertToCoord();
    }
    Vel = Accel;  // Leave station at full speed
    bDockFlag = false;
  }

  if (thrustamt < 0.0) {
    uImgSet = 2;
  } else {
    uImgSet = 1;
  }
}

void CShip::HandleElasticShipCollision(CThing* pOtherShip) {
  // Implements proper elastic collision physics between two ships
  // using standard 2D elastic collision formulas that conserve both
  // momentum and kinetic energy.
  //
  // Reference: https://en.wikipedia.org/wiki/Elastic_collision#Two-dimensional_collision_with_two_moving_objects
  //
  // For elastic collisions in 2D:
  // v1' = v1 - (2*m2 / (m1+m2)) * ((v1-v2) · (x1-x2)) / |x1-x2|² * (x1-x2)
  // v2' = v2 - (2*m1 / (m1+m2)) * ((v2-v1) · (x2-x1)) / |x2-x1|² * (x2-x1)
  //
  // Where:
  // - v1, v2 are initial velocities (vectors)
  // - v1', v2' are final velocities (vectors)
  // - x1, x2 are positions (vectors)
  // - m1, m2 are masses
  // - · denotes dot product

  extern CParser* g_pParser;

  // Get masses
  double m1 = GetMass();
  double m2 = pOtherShip->GetMass();

  // Get positions and velocities as Cartesian coordinates
  CCoord pos1 = GetPos();
  CCoord pos2 = pOtherShip->GetPos();

  CCoord vel1 = GetVelocity().ConvertToCoord();
  CCoord vel2 = pOtherShip->GetVelocity().ConvertToCoord();

  // Calculate position difference vector (x1 - x2) using toroidal geometry
  CTraj delta_pos_traj = pos2.VectTo(pos1);
  CCoord dx = delta_pos_traj.ConvertToCoord();

  // Calculate velocity difference vector (v1 - v2)
  CCoord dv;
  dv.fX = vel1.fX - vel2.fX;
  dv.fY = vel1.fY - vel2.fY;

  // Calculate dot product (v1-v2) · (x1-x2)
  double dot_dv_dx = dv.fX * dx.fX + dv.fY * dx.fY;

  // Calculate squared distance |x1-x2|²
  double dx_squared = dx.fX * dx.fX + dx.fY * dx.fY;

  // Handle special case: ships at same position (can happen when multiple ships
  // leave station together with same orientation and thrust)
  if (dx_squared < g_fp_error_epsilon) {
    if (g_pParser && g_pParser->verbose) {
      printf("[ELASTIC] %s <-> %s: SAME POSITION (dist²=%.6f)\n",
             GetName(), pOtherShip->GetName(), dx_squared);
    }

    // Ships are at same position - check relative velocity
    double dv_squared = dv.fX * dv.fX + dv.fY * dv.fY;

    if (dv_squared > g_fp_error_epsilon) {
      // CASE 1: Ships moving through each other - use relative velocity as collision normal
      // This is a head-on collision: reflect velocities along relative velocity axis
      if (g_pParser && g_pParser->verbose) {
        printf("[ELASTIC]   CASE 1: Moving through each other (dv²=%.3f)\n", dv_squared);
        printf("[ELASTIC]   Before: vel1=(%.2f,%.2f) vel2=(%.2f,%.2f)\n",
               vel1.fX, vel1.fY, vel2.fX, vel2.fY);
      }

      double dv_mag = sqrt(dv_squared);
      CCoord collision_normal;
      collision_normal.fX = dv.fX / dv_mag;  // Normalize
      collision_normal.fY = dv.fY / dv_mag;

      // For head-on collision, use simplified elastic formula
      // v1' = ((m1-m2)*v1 + 2*m2*v2) / (m1+m2)
      // v2' = ((m2-m1)*v2 + 2*m1*v1) / (m1+m2)
      double total_mass = m1 + m2;
      CCoord new_vel1;
      new_vel1.fX = ((m1 - m2) * vel1.fX + 2.0 * m2 * vel2.fX) / total_mass;
      new_vel1.fY = ((m1 - m2) * vel1.fY + 2.0 * m2 * vel2.fY) / total_mass;

      CTraj new_vel1_polar(new_vel1);
      if (new_vel1_polar.rho > g_game_max_speed) {
        new_vel1_polar.rho = g_game_max_speed;
      }
      Vel = new_vel1_polar;

      // Separate ships along relative velocity direction
      double separation_distance = GetSize() + pOtherShip->GetSize() + g_ship_collision_separation_clearance;
      CTraj separation_vec(separation_distance, atan2(collision_normal.fY, collision_normal.fX));
      CCoord new_pos = Pos + separation_vec.ConvertToCoord();
      Pos = new_pos;

      if (g_pParser && g_pParser->verbose) {
        printf("[ELASTIC]   After: vel1=(%.2f,%.2f) separation=%.1f@%.1f° pos=(%.1f,%.1f)\n",
               Vel.ConvertToCoord().fX, Vel.ConvertToCoord().fY,
               separation_distance, atan2(collision_normal.fY, collision_normal.fX) * 180.0 / PI,
               Pos.fX, Pos.fY);
      }
    } else {
      // Ships have same velocity - check if moving or stationary
      double v1_speed_squared = vel1.fX * vel1.fX + vel1.fY * vel1.fY;
      double separation_distance = GetSize() + pOtherShip->GetSize() + g_ship_collision_separation_clearance;

      // Deterministic assignment based on ship addresses
      uintptr_t this_addr = reinterpret_cast<uintptr_t>(this);
      uintptr_t other_addr = reinterpret_cast<uintptr_t>(pOtherShip);
      bool this_goes_forward = (this_addr < other_addr);

      if (v1_speed_squared > g_fp_error_epsilon) {
        // CASE 2: Both moving with same velocity - separate along/opposite velocity direction
        if (g_pParser && g_pParser->verbose) {
          printf("[ELASTIC]   CASE 2: Same velocity, moving (v²=%.3f) %s goes %s\n",
                 v1_speed_squared, GetName(), this_goes_forward ? "forward" : "backward");
        }

        double v1_angle = atan2(vel1.fY, vel1.fX);
        double separation_angle = this_goes_forward ? v1_angle : (v1_angle + PI);

        CTraj separation_vec(separation_distance, separation_angle);
        CCoord new_pos = Pos + separation_vec.ConvertToCoord();
        Pos = new_pos;

        if (g_pParser && g_pParser->verbose) {
          printf("[ELASTIC]   After: separation=%.1f@%.1f° pos=(%.1f,%.1f)\n",
                 separation_distance, separation_angle * 180.0 / PI, Pos.fX, Pos.fY);
        }
      } else {
        // CASE 3: Both stationary at same position - random separation
        if (g_pParser && g_pParser->verbose) {
          printf("[ELASTIC]   CASE 3: Both stationary, %s goes %s\n",
                 GetName(), this_goes_forward ? "forward" : "backward");
        }

        // Use ship addresses to generate deterministic random angle
        uintptr_t addr_sum = this_addr + other_addr;
        double random_angle = -PI + (addr_sum % 10000) * PI2 / 10000.0;

        // Ship with lower address gets angle θ, higher address gets θ + π
        if (!this_goes_forward) {
          random_angle += PI;  // Opposite direction
        }

        CTraj separation_vec(separation_distance, random_angle);
        CCoord new_pos = Pos + separation_vec.ConvertToCoord();
        Pos = new_pos;

        if (g_pParser && g_pParser->verbose) {
          printf("[ELASTIC]   After: separation=%.1f@%.1f° pos=(%.1f,%.1f)\n",
                 separation_distance, random_angle * 180.0 / PI, Pos.fX, Pos.fY);
        }
      }

      // Velocities remain unchanged in degenerate same-velocity cases
    }
    return;
  }

  // Normal case: ships at different positions
  if (g_pParser && g_pParser->verbose) {
    double dist = delta_pos_traj.rho;
    printf("[ELASTIC] %s <-> %s: NORMAL (dist=%.3f)\n",
           GetName(), pOtherShip->GetName(), dist);
    printf("[ELASTIC]   Before: pos1=(%.1f,%.1f) vel1=(%.2f,%.2f) m1=%.1f\n",
           pos1.fX, pos1.fY, vel1.fX, vel1.fY, m1);
    printf("[ELASTIC]   Before: pos2=(%.1f,%.1f) vel2=(%.2f,%.2f) m2=%.1f\n",
           pos2.fX, pos2.fY, vel2.fX, vel2.fY, m2);
  }

  // Calculate scalar factor for this ship: (2*m2 / (m1+m2)) * dot / dx²
  double factor1 = (2.0 * m2) / (m1 + m2) * (dot_dv_dx / dx_squared);

  // Calculate new velocity for this ship: v1' = v1 - factor1 * (x1-x2)
  CCoord new_vel1;
  new_vel1.fX = vel1.fX - factor1 * dx.fX;
  new_vel1.fY = vel1.fY - factor1 * dx.fY;

  // Convert to polar and apply to this ship
  CTraj new_vel1_polar(new_vel1);

  // Apply speed clamp (game constraint)
  if (new_vel1_polar.rho > g_game_max_speed) {
    new_vel1_polar.rho = g_game_max_speed;
  }

  Vel = new_vel1_polar;

  // Separate the ships to prevent overlap
  // Move this ship away from the other ship
  double separation_angle = pos2.AngleTo(pos1);  // Direction from ship2 to ship1
  double separation_distance = GetSize() + pOtherShip->GetSize() + g_ship_collision_separation_clearance;
  CTraj separation_vec(separation_distance, separation_angle);
  CCoord old_pos = Pos;
  Pos = pos2;  // Start from other ship's position
  Pos += separation_vec.ConvertToCoord();  // Move away

  if (g_pParser && g_pParser->verbose) {
    printf("[ELASTIC]   After: vel1'=(%.2f,%.2f) speed=%.2f factor=%.4f\n",
           Vel.ConvertToCoord().fX, Vel.ConvertToCoord().fY, Vel.rho, factor1);
    printf("[ELASTIC]   After: pos moved (%.1f,%.1f)->(%.1f,%.1f) separation=%.1f@%.1f°\n",
           old_pos.fX, old_pos.fY, Pos.fX, Pos.fY,
           separation_distance, separation_angle * 180.0 / PI);
  }
}

double CShip::CalculateCollisionMomentumChange(const CThing* pOtherThing) const {
  // Calculate the magnitude of momentum change |Δp| that will occur for this ship
  // when it collides with pOtherThing.
  //
  // For elastic collisions, we use the impulse formula. The momentum change depends
  // on the collision type:
  //
  // - Ship-Ship elastic collision: Both momentum and KE conserved
  // - Ship-Asteroid (large): Elastic collision
  // - Ship-Asteroid (small/fits): Perfectly inelastic (ship absorbs asteroid)
  //
  // For elastic collisions in 1D (head-on):
  //   Δp = (2 * m1 * m2 / (m1 + m2)) * v_rel
  //
  // For 2D elastic collisions, the momentum change magnitude is:
  //   |Δp| = (2 * m1 * m2 / (m1 + m2)) * |v_rel · n|
  // where n is the collision normal (unit vector from this ship to other).
  //
  // For perfectly inelastic collisions:
  //   |Δp| = |m2 / (m1 + m2)| * |m1 * v_rel|

  double m1 = GetMass();
  double m2 = pOtherThing->GetMass();

  ThingKind other_kind = pOtherThing->GetKind();

  // Get relative velocity
  CTraj v_rel_traj = RelativeVelocity(*pOtherThing);
  double v_rel_mag = v_rel_traj.rho;

  // Check if collision is inelastic (small asteroid that fits)
  bool is_inelastic = false;
  if (other_kind == ASTEROID) {
    const CAsteroid* pAst = (const CAsteroid*)pOtherThing;
    // Use const_cast to call non-const AsteroidFits (doesn't modify state)
    is_inelastic = const_cast<CShip*>(this)->AsteroidFits(const_cast<CAsteroid*>(pAst));
  }

  if (is_inelastic) {
    // Perfectly inelastic collision: objects stick together
    // Momentum change: Δp = (m2 / (m1 + m2)) * m1 * v_rel
    //                     = (m1 * m2 / (m1 + m2)) * v_rel
    double reduced_mass = (m1 * m2) / (m1 + m2);
    return reduced_mass * v_rel_mag;
  } else {
    // Elastic collision (ship-ship or ship-large asteroid)
    // For 2D elastic collision, we need the component along the collision normal

    CCoord pos1 = GetPos();
    CCoord pos2 = pOtherThing->GetPos();

    // Calculate position difference vector (collision normal direction) using toroidal geometry
    CTraj delta_pos = pos2.VectTo(pos1);
    CCoord dx = delta_pos.ConvertToCoord();

    double dx_squared = dx.fX * dx.fX + dx.fY * dx.fY;

    // Handle degenerate case: objects at same position
    if (dx_squared < g_fp_error_epsilon) {
      // Use relative velocity as collision direction
      CCoord v_rel = v_rel_traj.ConvertToCoord();
      double dv_squared = v_rel.fX * v_rel.fX + v_rel.fY * v_rel.fY;
      if (dv_squared < g_fp_error_epsilon) {
        // Both at same position with same velocity - no momentum change
        return 0.0;
      }
      // Head-on collision: use full relative velocity
      // |Δp| = (2 * m1 * m2 / (m1 + m2)) * |v_rel|
      double reduced_mass = (2.0 * m1 * m2) / (m1 + m2);
      return reduced_mass * v_rel_mag;
    }

    // Normal case: calculate velocity component along collision normal
    CCoord v_rel = v_rel_traj.ConvertToCoord();

    // Dot product: (v1-v2) · (x1-x2)
    double dot_v_dx = v_rel.fX * dx.fX + v_rel.fY * dx.fY;

    // Relative velocity along collision normal: v_rel_normal = dot / |dx|
    double dx_mag = delta_pos.rho;
    double v_rel_normal = fabs(dot_v_dx) / dx_mag;

    // Momentum change magnitude for elastic collision
    // |Δp| = (2 * m1 * m2 / (m1 + m2)) * v_rel_normal
    double reduced_mass = (2.0 * m1 * m2) / (m1 + m2);
    return reduced_mass * v_rel_normal;
  }
}
