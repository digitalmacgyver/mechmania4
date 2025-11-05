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
#include <limits>

typedef std::map<std::string, double> ParamMap;

// --- Data Structures ---

class FuelTraj {
public:
    double fuel_used;
    OrderKind order_kind;
    double order_mag;
    FuelTraj() : fuel_used(-1.0), order_kind(O_THRUST), order_mag(0.0) {}
};

class Entry {
public:
    CThing *thing;
    FuelTraj fueltraj;
    double turns_total;
    Entry() : thing(NULL), turns_total(0.0) {}
};

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

struct StrategicAssessment {
    bool no_hunting_targets;
    bool no_more_points;
    bool fuel_constrained;
    bool endgame;
    int active_hunters_needed;
    double uranium_left;
    double vinyl_left;
};

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

    static bool s_loggingEnabled;
    static std::string s_paramFile;
    static std::string s_logFile;
    MagicBag* mb;
    StrategicAssessment strategy;
    std::vector<ShipRole> ship_roles_;
    FuelTraj determine_orders(CThing* thing, double time, CShip* ship);
private:
    ParamMap params_;
    ParamMap default_params_;  // Store default parameter values
    std::string loaded_param_file_;  // Track which file was loaded
    int hunter_config_count_;
    void LoadParameters();
    void PopulateMagicBag();
    void AssessStrategy();
    void AssignRoles();
    void PrintStartupInfo();  // Print parameter information at startup
};


// Structure to cache GA parameters (UPDATED with new Targeting Weights)
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

    // Targeting Weights
    double TARGET_WEIGHT_SHIP_BASE;
    double TARGET_WEIGHT_STATION_BASE;
    double TARGET_WEIGHT_SHIP_FUEL;
    double TARGET_WEIGHT_SHIP_CARGO;
    double TARGET_WEIGHT_STATION_VINYL;
    double TARGET_WEIGHT_DISTANCE_PENALTY;
    // NEW: Prioritize low shields
    double TARGET_WEIGHT_SHIP_LOW_SHIELD;
};

// --- UnifiedBrain (CBrain Implementation) ---
class UnifiedBrain : public CBrain {
public:
    UnifiedBrain(EvoAI* pTeam, ParamMap* params);
    void Decide();

private:
    EvoAI* pmyEvoTeam_;
    CachedParams cache_;
    CThing* pTarget;

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
    // NEW: Helper function for line of sight check
    bool CheckLineOfFire(const CCoord& origin, const CTraj& beam, CThing* target, double target_dist);
};

#endif // EVOAI_H