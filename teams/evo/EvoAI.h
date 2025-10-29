#ifndef EVOAI_H
#define EVOAI_H

// Include MechMania 4 headers
#include "Team.h"
#include "Ship.h"
#include "World.h"
#include "Asteroid.h"
#include "Station.h"
#include "Brain.h"
#include "Coord.h"
#include "Traj.h"
#include "GameConstants.h"

#include <map>
#include <string>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm>

typedef std::map<std::string, double> ParamMap;

// --- Data Structures ---

// FuelTraj: Represents a trajectory and the fuel used to achieve it
class FuelTraj {
public:
    double fuel_used;
    OrderKind order_kind;
    double order_mag;
    FuelTraj() : fuel_used(-1.0), order_kind(O_THRUST), order_mag(0.0) {}
};

// Entry: Represents a potential target in the MagicBag
class Entry {
public:
    CThing *thing;
    FuelTraj fueltraj;
    double turns_total;
    Entry() : thing(NULL), turns_total(0.0) {}
};

// MagicBag: Central planning data structure
class MagicBag {
private:
    std::vector<std::vector<Entry*>> table;
    unsigned int num_drones;
public:
    MagicBag(unsigned int drones);
    ~MagicBag();
    Entry *getEntry(unsigned int drone, unsigned int elem);
    void addEntry(unsigned int drone, Entry *entry);
    void clear();
};

// Strategic Assessment: Global state analysis
struct StrategicAssessment {
    // Key strategic flags
    bool no_hunting_targets;
    bool no_more_points;
    bool fuel_constrained;
    bool endgame;
    // Dynamically determined number of hunters needed this turn
    int active_hunters_needed;
    // Resource tracking
    double uranium_left;
    double vinyl_left;
};

// Ship Roles
enum ShipRole {
    GATHERER,
    HUNTER
};

// --- EvoAI (CTeam Implementation) ---

class EvoAI : public CTeam {
public:
    EvoAI();
    ~EvoAI();

    void Init();
    void Turn();

    // Logging stubs
    void Log(const std::string& message) {}
    void LogStructured(const std::string& tag, const std::string& data) {}
    void InitializeLogging() {}
    void LogWorldState() {}

    // Static configuration
    static bool s_loggingEnabled;
    static std::string s_paramFile;
    static std::string s_logFile;

    // Core members accessible by Brains
    MagicBag* mb;
    StrategicAssessment strategy;
    std::vector<ShipRole> ship_roles_; // Dynamically assigned roles

    // Navigation function
    FuelTraj determine_orders(CThing* thing, double time, CShip* ship);

private:
    ParamMap params_;
    int hunter_config_count_; // Number of ships configured with high fuel capacity in Init
    void LoadParameters();
    void PopulateMagicBag();
    void AssessStrategy();
    void AssignRoles();
};

// Structure to cache GA parameters
struct CachedParams {
    // Resource thresholds
    double LOW_FUEL_THRESHOLD;
    double RETURN_CARGO_THRESHOLD;
    // Safety
    double MIN_SHIELD_LEVEL;
    double EMERGENCY_FUEL_RESERVE;
    // Navigation
    double NAV_ALIGNMENT_THRESHOLD;
    // Combat
    double COMBAT_ENGAGEMENT_RANGE;
    double COMBAT_MIN_FUEL_TO_HUNT;
    double COMBAT_LASER_EFFICIENCY_RATIO;
    double COMBAT_OVERKILL_BUFFER;
    // Strategy
    double STRATEGY_ENDGAME_TURN;
};

// --- UnifiedBrain (CBrain Implementation) ---
// Handles both Gathering and Hunting logic based on the dynamic role.
class UnifiedBrain : public CBrain {
public:
    UnifiedBrain(EvoAI* pTeam, ParamMap* params);
    void Decide();

private:
    EvoAI* pmyEvoTeam_;
    CachedParams cache_;
    CThing* pTarget; // Used by Hunter logic for persistence

    // Initialization
    void CacheParameters(ParamMap* params);

    // Common utility functions
    bool HandleEmergencies();
    void MaintainShields(double remaining_fuel_est);
    void HandleDeparture();
    void ExecuteOrders(const FuelTraj& ft);
    double CalculateRemainingFuel();

    // Gatherer Logic
    void ExecuteGatherer();
    
    // Hunter Logic
    void ExecuteHunter();
    void SelectTarget();
    bool AttemptToShoot(CThing* target);
};

#endif // EVOAI_H