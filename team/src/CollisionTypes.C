/* CollisionTypes.C
 * Implementation of deterministic collision engine data structures
 * For use with MechMania IV
 */

#include "CollisionTypes.h"
#include <cstring>  // for NULL

// ============================================================================
// CollisionState Implementation
// ============================================================================

CollisionState::CollisionState()
    : thing(NULL),
      kind(GENTHING),
      world_index(0),
      position(CCoord(0, 0)),
      velocity(CTraj(0, 0)),
      mass(0.0),
      size(0.0),
      orient(0.0),
      omega(0.0),
      team(NULL),
      is_alive(false),
      is_docked(false),
      was_docked(false),
      ship_shield(0.0),
      ship_cargo(0.0),
      ship_fuel(0.0),
      ship_shield_capacity(0.0),
      ship_cargo_capacity(0.0),
      ship_fuel_capacity(0.0),
      asteroid_material(static_cast<AsteroidKind>(0)),
      station_cargo(0.0) {
}

// ============================================================================
// CollisionCommand Implementation
// ============================================================================

CollisionCommand::CollisionCommand()
    : type(CollisionCommandType::kNoOp),
      target(NULL),
      velocity(CTraj(0, 0)),
      position(CCoord(0, 0)),
      scalar(0.0),
      bool_flag(false),
      thing_ptr(NULL) {
  // Initialize message buffer to empty string
  message_buffer[0] = '\0';
}

CollisionCommand CollisionCommand::NoOp() {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kNoOp;
  cmd.target = NULL;
  return cmd;
}

CollisionCommand CollisionCommand::Kill(CThing* target) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kKillSelf;
  cmd.target = target;
  return cmd;
}

CollisionCommand CollisionCommand::SetVelocity(CThing* target, const CTraj& vel) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kSetVelocity;
  cmd.target = target;
  cmd.velocity = vel;
  return cmd;
}

CollisionCommand CollisionCommand::SetPosition(CThing* target, const CCoord& pos) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kSetPosition;
  cmd.target = target;
  cmd.position = pos;
  return cmd;
}

CollisionCommand CollisionCommand::AdjustShield(CThing* target, double delta) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kAdjustShield;
  cmd.target = target;
  cmd.scalar = delta;
  return cmd;
}

CollisionCommand CollisionCommand::AdjustCargo(CThing* target, double delta) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kAdjustCargo;
  cmd.target = target;
  cmd.scalar = delta;
  return cmd;
}

CollisionCommand CollisionCommand::AdjustFuel(CThing* target, double delta) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kAdjustFuel;
  cmd.target = target;
  cmd.scalar = delta;
  return cmd;
}

CollisionCommand CollisionCommand::SetDocked(CThing* target, bool docked) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kSetDocked;
  cmd.target = target;
  cmd.bool_flag = docked;
  cmd.scalar = -1.0;
  return cmd;
}

CollisionCommand CollisionCommand::RecordEatenBy(CThing* asteroid, CThing* ship) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kRecordEatenBy;
  cmd.target = asteroid;
  cmd.thing_ptr = ship;
  return cmd;
}

CollisionCommand CollisionCommand::Announce(const char* msg) {
  CollisionCommand cmd;
  cmd.type = CollisionCommandType::kAnnounceMessage;
  cmd.target = NULL;

  // CRITICAL FIX: Copy message into owned buffer instead of storing pointer
  // This prevents dangling pointer bug when msg is stack-allocated in caller
  if (msg) {
    strncpy(cmd.message_buffer, msg, sizeof(cmd.message_buffer) - 1);
    cmd.message_buffer[sizeof(cmd.message_buffer) - 1] = '\0';  // Ensure null termination
  } else {
    cmd.message_buffer[0] = '\0';
  }

  return cmd;
}

// ============================================================================
// SpawnRequest Implementation
// ============================================================================

SpawnRequest::SpawnRequest()
    : kind(GENTHING),
      position(CCoord(0, 0)),
      velocity(CTraj(0, 0)),
      mass(0.0),
      size(0.0),
      orient(0.0),
      material(static_cast<AsteroidKind>(0)) {
}

SpawnRequest::SpawnRequest(ThingKind k, const CCoord& pos, const CTraj& vel,
                           double m, double s, double o, AsteroidKind mat)
    : kind(k),
      position(pos),
      velocity(vel),
      mass(m),
      size(s),
      orient(o),
      material(mat) {
}

// ============================================================================
// CollisionOutcome Implementation
// ============================================================================

CollisionOutcome::CollisionOutcome()
    : command_count(0),
      spawn_count(0) {
  // Arrays initialized by default constructors
}

bool CollisionOutcome::AddCommand(const CollisionCommand& cmd) {
  if (command_count >= kMaxCommands) {
    // Overflow - cannot add more commands
    return false;
  }
  commands[command_count] = cmd;
  command_count++;
  return true;
}

bool CollisionOutcome::AddSpawn(const SpawnRequest& spawn) {
  if (spawn_count >= kMaxSpawns) {
    // Overflow - cannot add more spawns
    return false;
  }
  spawns[spawn_count] = spawn;
  spawn_count++;
  return true;
}

// ============================================================================
// CollisionContext Implementation
// ============================================================================

CollisionContext::CollisionContext()
    : world(NULL),
      self_state(NULL),
      other_state(NULL),
      time_step(0.0),
      use_new_physics(false),
      disable_eat_damage(false),
      use_docking_fix(false),
      preserve_nonfragmenting_asteroids(false),
      random_separation_angle(0.0),
      random_separation_forward(false) {
}

CollisionContext::CollisionContext(CWorld* w, const CollisionState* self,
                                   const CollisionState* other, double dt,
                                   bool physics, bool eat_dmg, bool dock,
                                   bool preserve_nonfrag,
                                   double random_angle, bool random_forward)
    : world(w),
      self_state(self),
      other_state(other),
      time_step(dt),
      use_new_physics(physics),
      disable_eat_damage(eat_dmg),
      use_docking_fix(dock),
      preserve_nonfragmenting_asteroids(preserve_nonfrag),
      random_separation_angle(random_angle),
      random_separation_forward(random_forward) {
}

// ============================================================================
// Helper Functions
// ============================================================================

int GetCommandTypePriority(CollisionCommandType type) {
  // Lower priority number = executed earlier
  // This ordering ensures deterministic behavior independent of emission order
  switch (type) {
    case CollisionCommandType::kKillSelf:
      return 1;  // Process deaths first
    case CollisionCommandType::kSetPosition:
      return 2;  // Position changes (separation/docking)
    case CollisionCommandType::kSetVelocity:
      return 3;  // Velocity changes (momentum transfer)
    case CollisionCommandType::kSetDocked:
      return 4;  // Docking state changes
    case CollisionCommandType::kAdjustShield:
    case CollisionCommandType::kAdjustCargo:
    case CollisionCommandType::kAdjustFuel:
      return 5;  // Resource adjustments
    case CollisionCommandType::kRecordEatenBy:
      return 6;  // Ownership records
    case CollisionCommandType::kAnnounceMessage:
      return 7;  // Announcements last
    case CollisionCommandType::kNoOp:
    default:
      return 99; // No-ops and unknowns at end
  }
}

bool CommandsConflict(const CollisionCommand& a, const CollisionCommand& b) {
  // Commands conflict if they target the same object and same property type
  if (a.target != b.target) {
    return false;  // Different targets, no conflict
  }

  if (a.target == NULL) {
    return false;  // NULL target (announcements, etc), no conflict
  }

  // Same target - check if they modify the same property
  // Most command types can coexist (e.g., position + velocity on same object)
  // Only exact type matches are conflicts (e.g., two SetVelocity on same target)
  return (a.type == b.type);
}
