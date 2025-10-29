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
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>
#include <algorithm>

typedef std::map<std::string, double> ParamMap;

// --- Groogroo Data Structures (Integrated into EvoAI) ---

// FuelTraj: Represents a trajectory and the fuel used to achieve it
class FuelTraj {
public:
    double fuel_used;
    CTraj traj; // The desired velocity vector
    OrderKind order_kind;
    double order_mag;
    FuelTraj() : fuel_used(-1.0), order_kind(O_THRUST), order_mag(0.0) {} // Default to invalid
};

// Entry: Represents a potential target in the MagicBag
class Entry {
public:
    CThing *thing;
    FuelTraj fueltraj; // The immediate order to execute this turn
    double total_fuel;
    double turns_total;
    int claimed_by_mech; // Ship index claiming this entry

    Entry() : thing(NULL), total_fuel(0.0), turns_total(0.0), claimed_by_mech(-1) {}
};

// MagicBag: Central planning data structure (Modernized with vectors)
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

// --- EvoAI (CTeam Implementation) ---

enum ShipRole {
    GATHERER,
    HUNTER
};

class EvoAI : public CTeam {
public:
    EvoAI();
    ~EvoAI();

    void Init();
    void Turn();

    // Logging functions (Stubs provided if detailed logging is not needed)
    void Log(const std::string& message) {}
    void LogStructured(const std::string& tag, const std::string& data) {}
    void InitializeLogging() {}
    void LogWorldState() {}

    // Static configuration
    static bool s_loggingEnabled;
    static std::string s_paramFile;
    static std::string s_logFile;

    // Groogroo specific members
    MagicBag* mb;
    double uranium_left;
    double vinyl_left;

    // Navigation function (Analytical Intercept)
    FuelTraj determine_orders(CThing* thing, double time, CShip* ship);

private:
    ParamMap params_;
    std::vector<ShipRole> ship_roles_; // Track role of each ship index
    void LoadParameters();
    void PopulateMagicBag();
};

// Structure to cache GA parameters (Shared base for Brains)
struct CachedParams {
    // Resource thresholds
    double LOW_FUEL_THRESHOLD;
    double RETURN_CARGO_THRESHOLD;
    // Safety
    double MIN_SHIELD_LEVEL;
    double EMERGENCY_FUEL_RESERVE;
    // Navigation
    double NAV_ALIGNMENT_THRESHOLD;
    // Combat (Used by Hunter)
    double COMBAT_ENGAGEMENT_RANGE;
    double COMBAT_LASER_OVERHEAD;
    double COMBAT_MIN_FUEL_TO_HUNT;
};

// Base Brain Class for common functionality
class EvoBrain : public CBrain {
protected:
    EvoAI* pmyEvoTeam_;
    CachedParams cache_;

    EvoBrain(EvoAI* pTeam, ParamMap* params);
    void CacheParameters(ParamMap* params);

    // Common utility functions
    bool HandleEmergencies();
    void MaintainShields(double remaining_fuel_est);
    void HandleDeparture();
    void ExecuteOrders(const FuelTraj& ft);
    double CalculateRemainingFuel();
};


// --- GathererBrain (CBrain Implementation) ---
// Focuses on resource collection using Groogroo's logic
class GathererBrain : public EvoBrain {
public:
    GathererBrain(EvoAI* pTeam, ParamMap* params);
    void Decide();

private:
    void NavigateAndGather();
};

// --- HunterBrain (CBrain Implementation) ---
// Focuses on combat using ChromeFunk's logic with Groogroo navigation
class HunterBrain : public EvoBrain {
public:
    HunterBrain(EvoAI* pTeam, ParamMap* params);
    void Decide();

private:
    CThing* pTarget;
    void SelectTarget();
    void NavigateAndEngage();
    bool AttemptToShoot(CThing* target);
};

#endif // EVOAI_H