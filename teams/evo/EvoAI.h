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
#include "GameConstants.h" // Required for global constants

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <sstream> // Required for stringstream

typedef std::map<std::string, double> ParamMap;

// --- EvoAI (CTeam Implementation) ---
class EvoAI : public CTeam {
public:
    EvoAI();
    ~EvoAI();

    void Init();
    void Turn();

    // Standard logging (redirects to LogStructured with DEBUG tag)
    void Log(const std::string& message);
    
    // Structured logging function
    // Format: TIME TAG DATA
    void LogStructured(const std::string& tag, const std::string& data);

    // Static configuration set by mm4team.C main() or command line args
    static bool s_loggingEnabled;
    static std::string s_paramFile;
    static std::string s_logFile;

private:
    ParamMap params_;
    void LoadParameters();
    void LogWorldState(); 
    std::ofstream logFileStream_;
    void InitializeLogging(); 
};

// Structure to cache GA parameters
struct CachedParams {
    // Heuristics
    double W_DISTANCE;
    double W_VINYL;
    double W_URANIUM;
    double W_FUEL_BOOST_FACTOR;
    double W_FUEL_COST_PENALTY;
    double W_TTI_PENALTY;
    double W_CONFLICT_PENALTY; 
    
    // Thresholds
    double THRESHOLD_RETURN_CARGO;
    // THRESHOLD_FUEL_LOW is now deprecated in favor of dynamic management
    double THRESHOLD_FUEL_TARGET;
    double THRESHOLD_MAX_SHIELD_BOOST; 

    // NEW: Dynamic Fuel Management
    double FUEL_COST_PER_DIST;
    double FUEL_SAFETY_MARGIN;
    
    // Navigation (P-Controller and Vector Navigation)
    double NAV_DESIRED_SPEED_FACTOR;
    double NAV_ALIGNMENT_STRICT_ANGLE;
    double NAV_ALIGNMENT_LOOSE_ANGLE;
    double NAV_CLOSE_ENOUGH_DIST;
    double NAV_PREDICTION_HORIZON; // NEW
    
    // Safety
    double NAV_AVOIDANCE_HORIZON;
    double NAV_SHIELD_BOOST_TTC;
    
    // Tactics
    double TACTICS_LASER_POWER;
    double TACTICS_LASER_RANGE;
};

// --- HarvesterBrain (CBrain Implementation) ---
class HarvesterBrain : public CBrain {
public:
    HarvesterBrain(EvoAI* pTeam, ParamMap* params);
    void Decide();
    CThing* GetCurrentTarget() const { return pTarget_; } // Expose target for coordination

private:
    enum BrainState {
        DEPARTING,
        HUNTING,
        INTERCEPTING,
        REFUELING,
        BREAKING
    };
    
    BrainState state_;
    EvoAI* pmyEvoTeam_;
    CachedParams cache_;
    CThing* pTarget_;
    std::string currentGoalDescription_; // Describes the intent/reason for the current action
    int successiveTurns_; // Tracks consecutive O_TURN orders
    double currentDynamicFuelLow_; // Stores the dynamically calculated fuel threshold

    void UpdateState();
    void ExecuteAction();
    void HandleDeparting();
    void SelectTarget();
    
    // Navigation
    bool NavigateVectorP();
    bool AvoidCollisions(double& imminent_ttc);
    double CalculateDepartureAngle(); 
    void TrackSuccessiveTurns(); // Updates the successiveTurns_ counter
    
    // Tactics
    bool HandleBreaking();

    // Evaluation
    double EvaluateAsteroid(CAsteroid* asteroid, bool prioritizeFuel, bool& too_large);
    double EstimateTTI(CThing* target);

    void TransitionState(BrainState newState);
    void CacheParameters(ParamMap* params);
    const char* StateToString(BrainState state);
    
    // Logging helpers
    void BrainLog(const std::string& message);
    void LogShipDecision(); 
    void UpdateGoalDescription(); 
};

#endif // EVOAI_H