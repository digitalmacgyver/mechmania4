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

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>

typedef std::map<std::string, double> ParamMap;

// --- EvoAI (CTeam Implementation) ---
class EvoAI : public CTeam {
public:
    EvoAI();
    ~EvoAI();

    void Init();
    void Turn();

    // Logging utility
    void Log(const std::string& message);

    // Static configuration set by mm4team.C main() before instantiation
    static bool s_loggingEnabled;
    static std::string s_logFile;
    static std::string s_paramFile;

private:
    ParamMap params_;
    void LoadParameters();
    std::ofstream logFile_;
};

// Structure to cache GA parameters for fast access (Performance Optimization)
struct CachedParams {
    double W_DISTANCE;
    double W_VINYL;
    double W_URANIUM;
    double W_FUEL_BOOST_FACTOR;
    double THRESHOLD_RETURN_CARGO;
    double THRESHOLD_FUEL_LOW;
    double THRESHOLD_FUEL_TARGET;
    double NAV_ANGLE_TOLERANCE;
    double NAV_TURN_AGGRESSION;
    double NAV_AVOIDANCE_HORIZON;
    double NAV_THRUST_POWER;
};

// --- HarvesterBrain (CBrain Implementation) ---
class HarvesterBrain : public CBrain {
public:
    HarvesterBrain(EvoAI* pTeam, ParamMap* params);
    void Decide();

private:
    enum BrainState {
        DEPARTING,
        HUNTING,
        INTERCEPTING,
        REFUELING
    };
    
    BrainState state_;
    EvoAI* pmyEvoTeam_;
    CachedParams cache_;
    CThing* pTarget_;

    void UpdateState();
    void ExecuteAction();
    void HandleDeparting();
    void SelectTarget();
    bool Navigate();
    bool AvoidCollisions();
    double EvaluateAsteroid(CAsteroid* asteroid, bool prioritizeFuel);
    void TransitionState(BrainState newState);
    void CacheParameters(ParamMap* params);
    const char* StateToString(BrainState state);
    void BrainLog(const std::string& message);
};

#endif // EVOAI_H
