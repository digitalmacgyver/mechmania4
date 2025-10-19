/* CollisionTypes.h
 * Data structures for deterministic collision engine
 * Provides snapshot-based immutable state and command pattern
 * for collision processing without order-dependent side effects
 * For use with MechMania IV
 */

#ifndef _COLLISION_TYPES_H_MM4_DETERMINISTIC
#define _COLLISION_TYPES_H_MM4_DETERMINISTIC

#include "Coord.h"
#include "Traj.h"
#include "GameConstants.h"

// NOTE: We include these headers to get enum definitions (ThingKind, AsteroidKind)
// When Thing.h later includes CollisionTypes.h, we'll use forward declarations there
// to avoid circular dependency
#include "Thing.h"
#include "Asteroid.h"

// Forward declarations for other types
class CWorld;

// ============================================================================
// CollisionState - Immutable snapshot of object state at collision time
// ============================================================================
// This structure captures all relevant state of an object at the moment
// collision detection occurs. Both collision participants read from snapshots
// rather than live objects, ensuring deterministic behavior independent of
// processing order.
//
struct CollisionState {
  // Identity
  CThing* thing;                // Pointer for identity only (do not dereference members!)
  ThingKind kind;
  unsigned int world_index;

  // Physics state
  CCoord position;
  CTraj velocity;
  double mass;
  double size;
  double orient;
  double omega;

  // Ownership and status
  CTeam* team;                  // NULL for non-team objects (asteroids)
  bool is_alive;

  // Ship-specific state (valid only when kind == SHIP)
  bool is_docked;
  double ship_shield;
  double ship_cargo;
  double ship_fuel;

  // Asteroid-specific state (valid only when kind == ASTEROID)
  AsteroidKind asteroid_material;

  // Station-specific state (valid only when kind == STATION)
  double station_cargo;

  // Constructor for convenience
  CollisionState();
};

// ============================================================================
// CollisionCommand - Atomic state change directive
// ============================================================================
// Commands represent the outcome of collision logic as discrete, declarative
// state changes. Instead of mutating objects directly during collision
// processing, handlers emit commands that are applied later in deterministic
// order. This eliminates race conditions and order-dependent outcomes.
//
enum class CollisionCommandType {
  kNoOp,              // Do nothing (placeholder)
  kKillSelf,          // Mark target object as dead
  kSetVelocity,       // Set target velocity (for momentum transfer)
  kSetPosition,       // Set target position (for separation or docking)
  kAdjustShield,      // Adjust target shield by delta (can be negative)
  kAdjustCargo,       // Adjust target cargo by delta (can be negative)
  kAdjustFuel,        // Adjust target fuel by delta (can be negative)
  kSetDocked,         // Set target docked state (ships only)
  kRecordEatenBy,     // Record which ship ate this asteroid
  kAnnounceMessage    // Add message to world announcer
};

struct CollisionCommand {
  CollisionCommandType type;
  CThing* target;             // Which object this command applies to

  // Type-specific data (only one field used based on type)
  // We store all possible data types as separate fields to avoid union issues
  CTraj velocity;             // For kSetVelocity
  CCoord position;            // For kSetPosition
  double scalar;              // For kAdjustShield/Cargo/Fuel (delta value)
  bool bool_flag;             // For kSetDocked
  CThing* thing_ptr;          // For kRecordEatenBy (eater pointer)

  // CRITICAL FIX: Store message inline instead of pointer to avoid dangling pointer bug
  // Previously: const char* message (dangling pointer to stack memory)
  // Now: char message_buffer[256] (owned storage)
  char message_buffer[256];   // For kAnnounceMessage - inline storage prevents use-after-free

  // Constructors for convenience
  CollisionCommand();
  static CollisionCommand NoOp();
  static CollisionCommand Kill(CThing* target);
  static CollisionCommand SetVelocity(CThing* target, const CTraj& vel);
  static CollisionCommand SetPosition(CThing* target, const CCoord& pos);
  static CollisionCommand AdjustShield(CThing* target, double delta);
  static CollisionCommand AdjustCargo(CThing* target, double delta);
  static CollisionCommand AdjustFuel(CThing* target, double delta);
  static CollisionCommand SetDocked(CThing* target, bool docked);
  static CollisionCommand RecordEatenBy(CThing* asteroid, CThing* ship);
  static CollisionCommand Announce(const char* msg);
};

// ============================================================================
// SpawnRequest - Parameters for creating new objects
// ============================================================================
// When collisions create new objects (asteroid fragmentation, etc), we store
// the spawn parameters rather than pre-creating objects. This allows the
// world to manage object lifecycle and ensures spawns happen after all
// collision resolution completes.
//
struct SpawnRequest {
  ThingKind kind;
  CCoord position;
  CTraj velocity;
  double mass;
  double size;
  double orient;
  AsteroidKind material;      // Valid only when kind == ASTEROID

  // Constructor
  SpawnRequest();
  SpawnRequest(ThingKind k, const CCoord& pos, const CTraj& vel,
               double m, double s, double o, AsteroidKind mat = static_cast<AsteroidKind>(0));
};

// ============================================================================
// CollisionOutcome - Result of collision processing for one object
// ============================================================================
// Each collision participant generates an outcome containing zero or more
// commands and spawn requests. The world collects outcomes from both
// participants and applies them in deterministic order.
//
const unsigned int kMaxCommands = 32;  // Increased to handle complex collisions
const unsigned int kMaxSpawns = 8;     // Maximum fragments per collision

struct CollisionOutcome {
  CollisionCommand commands[kMaxCommands];
  SpawnRequest spawns[kMaxSpawns];
  unsigned int command_count;
  unsigned int spawn_count;

  // Constructor
  CollisionOutcome();

  // Helper methods to add commands (returns false if overflow)
  bool AddCommand(const CollisionCommand& cmd);
  bool AddSpawn(const SpawnRequest& spawn);

  // Overflow checking
  bool HasCommandOverflow() const { return command_count >= kMaxCommands; }
  bool HasSpawnOverflow() const { return spawn_count >= kMaxSpawns; }
};

// ============================================================================
// CollisionContext - Shared context for collision processing
// ============================================================================
// Provides handlers with access to world, parser flags, and timing info
// without needing to pass many individual parameters.
//
struct CollisionContext {
  CWorld* world;
  const CollisionState* self_state;   // State of object processing collision
  const CollisionState* other_state;  // State of collision partner
  double time_step;                   // Physics dt for this frame

  // Feature flags (derived from parser at collision time)
  bool use_new_physics;               // physics flag
  bool use_asteroid_eat_damage;       // asteroid-eat-damage flag
  bool use_docking_fix;               // docking flag

  // Random separation angle for ship-ship collisions (third preference fallback)
  // Used only when ships have same velocity AND same position
  // Both ships in a collision pair receive the same random angle
  double random_separation_angle;     // Uniform random in [-π, π)

  // Constructor
  CollisionContext();
  CollisionContext(CWorld* w, const CollisionState* self, const CollisionState* other,
                   double dt, bool physics, bool eat_dmg, bool dock, double random_angle = 0.0);
};

// ============================================================================
// Helper Functions
// ============================================================================

// Get priority for command type (lower = earlier execution)
// This defines deterministic ordering independent of emission order:
//   1. Kill commands (process deaths first)
//   2. Position updates (separation/docking)
//   3. Velocity updates (momentum transfer)
//   4. Docking state changes
//   5. Resource adjustments (shield, cargo, fuel)
//   6. Ownership records
//   7. Announcements
int GetCommandTypePriority(CollisionCommandType type);

// Check if two commands target the same object and same property
// Used to detect conflicts during command application
bool CommandsConflict(const CollisionCommand& a, const CollisionCommand& b);

#endif  // _COLLISION_TYPES_H_MM4_DETERMINISTIC
