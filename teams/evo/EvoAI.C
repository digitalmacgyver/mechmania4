#include "EvoAI.h"
#include "ParserModern.h"
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <limits>
#include <cmath>

extern CParser* g_pParser;

// Initialize static members (Defaults)
bool EvoAI::s_loggingEnabled = false;
std::string EvoAI::s_logFile = "EvoAI_game.log";
std::string EvoAI::s_paramFile = "EvoAI_params.txt";

// Factory function
CTeam* CTeam::CreateTeam() {
    return new EvoAI;
}

// --- EvoAI (CTeam) Implementation ---

EvoAI::EvoAI() {
    // Refresh configuration from the command-line parser (if available)
    if (g_pParser) {
        const ArgumentParser& args = g_pParser->GetModernParser();

        s_loggingEnabled = args.enableTeamLogging;

        if (!args.teamLogFile.empty()) {
            s_logFile = args.teamLogFile;
        } else {
            s_logFile = "EvoAI_game.log";
        }

        if (!args.teamParamsFile.empty()) {
            s_paramFile = args.teamParamsFile;
        } else {
            s_paramFile = "EvoAI_params.txt";
        }
    } else {
        // Fall back to defaults if parser is unavailable
        s_loggingEnabled = false;
        s_logFile = "EvoAI_game.log";
        s_paramFile = "EvoAI_params.txt";
    }

    // Initialize default parameters (Genome)
    // ... (Defaults remain the same) ...
    params_["W_DISTANCE"] = -1.0;
    params_["W_VINYL"] = 10.0;
    params_["W_URANIUM"] = 5.0;
    params_["W_FUEL_BOOST_FACTOR"] = 5.0;
    params_["THRESHOLD_RETURN_CARGO"] = 0.95;
    params_["THRESHOLD_FUEL_LOW"] = 15.0;
    params_["THRESHOLD_FUEL_TARGET"] = 40.0;
    params_["NAV_ANGLE_TOLERANCE"] = 0.1;
    params_["NAV_TURN_AGGRESSION"] = 1.2;
    params_["NAV_AVOIDANCE_HORIZON"] = 10.0;
    params_["NAV_THRUST_POWER"] = 20.0;
    params_["SHIP_CARGO_RATIO"] = 0.7;

    // LoadParameters uses the dynamically set filename (s_paramFile)
    LoadParameters();
    srand(time(NULL));

    // Open the log file if enabled via command line (s_loggingEnabled)
    if (s_loggingEnabled) {
        logFile_.open(s_logFile.c_str(), std::ios::out | std::ios::trunc);
        Log("EvoAI Initialized. Logging Enabled.");
        Log("Parameters loaded from: " + s_paramFile);
    }
}

EvoAI::~EvoAI() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
    // Clean up brains
    for (UINT i = 0; i < GetShipCount(); i++) {
        CShip* pSh = GetShip(i);
        if (pSh) {
            CBrain* pBr = pSh->GetBrain();
            if (pBr) {
                delete pBr;
                pSh->SetBrain(NULL);
            }
        }
    }
}

// Centralized logging function (Runtime check)
void EvoAI::Log(const std::string& message) {
    // Check the runtime flag. Negligible overhead.
    if (s_loggingEnabled && logFile_.is_open()) {
        CWorld* pWorld = GetWorld();
        double time = pWorld ? pWorld->GetGameTime() : 0.0;
        logFile_ << "T+" << std::fixed << std::setprecision(1) << time << ": " << message << std::endl;
    }
}

void EvoAI::LoadParameters() {
    // Use the dynamically set filename
    std::ifstream file(s_paramFile.c_str());
    if (file.is_open()) {
        std::string key;
        double value;
        while (file >> key >> value) {
            params_[key] = value;
        }
        file.close();
    }
}

void EvoAI::Init() {
    SetName("EvoAI");
    // Configure Ships (Initialization phase only)
    double cargoRatio = params_["SHIP_CARGO_RATIO"];
    if (cargoRatio < 0.1) cargoRatio = 0.1;
    if (cargoRatio > 0.9) cargoRatio = 0.9;
    double cargo_capacity = dMaxStatTot * cargoRatio;
    double fuel_capacity = dMaxStatTot - cargo_capacity;

    for (UINT i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (ship) {
            ship->SetCapacity(S_FUEL, fuel_capacity);
            ship->SetCapacity(S_CARGO, cargo_capacity);
            char namebuf[maxnamelen];
            snprintf(namebuf, maxnamelen, "Evo-%d", i);
            ship->SetName(namebuf);
            ship->SetBrain(new HarvesterBrain(this, &params_));
        }
    }
}

void EvoAI::Turn() {
    CWorld* pWorld = GetWorld();
    if (!pWorld || pWorld->bGameOver) return;
    for (UINT i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (ship && ship->IsAlive() && ship->GetBrain()) {
            ship->GetBrain()->Decide();
        }
    }
}

// --- HarvesterBrain (CBrain) Implementation ---

HarvesterBrain::HarvesterBrain(EvoAI* pTeam, ParamMap* params) 
    : pmyEvoTeam_(pTeam), pTarget_(NULL), state_(DEPARTING) {
    // Optimization: Cache parameters immediately.
    CacheParameters(params);
}

// Optimization: Cache parameters to avoid string lookups in hot loops.
void HarvesterBrain::CacheParameters(ParamMap* params) {
    cache_.W_DISTANCE = (*params)["W_DISTANCE"];
    cache_.W_VINYL = (*params)["W_VINYL"];
    cache_.W_URANIUM = (*params)["W_URANIUM"];
    cache_.W_FUEL_BOOST_FACTOR = (*params)["W_FUEL_BOOST_FACTOR"];
    cache_.THRESHOLD_RETURN_CARGO = (*params)["THRESHOLD_RETURN_CARGO"];
    cache_.THRESHOLD_FUEL_LOW = (*params)["THRESHOLD_FUEL_LOW"];
    cache_.THRESHOLD_FUEL_TARGET = (*params)["THRESHOLD_FUEL_TARGET"];
    cache_.NAV_ANGLE_TOLERANCE = (*params)["NAV_ANGLE_TOLERANCE"];
    cache_.NAV_TURN_AGGRESSION = (*params)["NAV_TURN_AGGRESSION"];
    cache_.NAV_AVOIDANCE_HORIZON = (*params)["NAV_AVOIDANCE_HORIZON"];
    cache_.NAV_THRUST_POWER = (*params)["NAV_THRUST_POWER"];
}

const char* HarvesterBrain::StateToString(BrainState state) {
    switch(state) {
        case DEPARTING: return "DEPARTING";
        case HUNTING: return "HUNTING";
        case INTERCEPTING: return "INTERCEPTING";
        case REFUELING: return "REFUELING";
        default: return "UNKNOWN";
    }
}

void HarvesterBrain::BrainLog(const std::string& message) {
    // Check the runtime flag.
    if (EvoAI::s_loggingEnabled) {
        std::string msg = std::string(pShip->GetName()) + ": " + message;
        pmyEvoTeam_->Log(msg);
    }
}

// RESTRUCTURED Decide(): Fixes CPU spinning by guaranteeing SelectTarget runs at most once per tick.
void HarvesterBrain::Decide() {
    if (!pShip || !pShip->IsAlive()) return;
    
    // 1. Evaluate current metrics and target validity.
    UpdateState();

    // 2. If the resulting state requires a search, perform it.
    if (state_ == HUNTING || state_ == REFUELING) {
        SelectTarget(); // O(N) search, executed at most once.
        
        if (pTarget_) {
            TransitionState(INTERCEPTING);
        } else {
            // FIX: If search failed (e.g., nothing fits), return existing cargo instead of spinning.
            if (state_ == HUNTING && pShip->GetAmount(S_CARGO) > 0.1) {
                BrainLog("HUNTING failed (nothing fits/found). Returning cargo.");
                pTarget_ = pShip->GetTeam()->GetStation();
                TransitionState(INTERCEPTING);
            } else if (state_ == REFUELING) {
                 BrainLog("REFUELING failed (no uranium found/fits). Switching to HUNTING.");
                 TransitionState(HUNTING);
            }
        }
    }

    // 3. Execute actions based on the final state and target.
    ExecuteAction();
}

// UpdateState: Optimized (No loops, uses cached parameters).
void HarvesterBrain::UpdateState() {
    // 1. Docking Check
    if (pShip->IsDocked()) {
        if (state_ != DEPARTING) TransitionState(DEPARTING);
        return;
    }

    if (state_ == DEPARTING) TransitionState(HUNTING);

    // 2. Fuel Check
    double current_fuel = pShip->GetAmount(S_FUEL);
    if (current_fuel < cache_.THRESHOLD_FUEL_LOW && state_ != REFUELING) {
        BrainLog("Fuel low. Switching to REFUELING.");
        TransitionState(REFUELING);
    } else if (current_fuel >= cache_.THRESHOLD_FUEL_TARGET && state_ == REFUELING) {
        BrainLog("Fuel satisfied. Switching to HUNTING.");
        TransitionState(HUNTING);
    }

    // 3. Cargo Check
    double capacity = pShip->GetCapacity(S_CARGO);
    double cargo_load = (capacity > 0) ? (pShip->GetAmount(S_CARGO) / capacity) : 0.0;

    if (cargo_load >= cache_.THRESHOLD_RETURN_CARGO && state_ != REFUELING) {
        CStation* station = pShip->GetTeam()->GetStation();
        if (state_ != INTERCEPTING || pTarget_ != station) {
             BrainLog("Cargo full. Returning to base.");
             pTarget_ = station;
             TransitionState(INTERCEPTING);
        }
        return;
    }

    // 4. Target Validation
    if (state_ == INTERCEPTING || state_ == REFUELING) {
        if (pTarget_ == pShip->GetTeam()->GetStation()) return;

        bool target_valid = false;
        if (pTarget_ && pTarget_->IsAlive() && pTarget_->GetKind() == ASTEROID) {
            CAsteroid* asteroid = (CAsteroid*)pTarget_;
            // Check if it still fits (Crucial performance check)
            if (pShip->AsteroidFits(asteroid)) {
                if (state_ != REFUELING || asteroid->GetMaterial() == URANIUM) {
                    target_valid = true;
                }
            }
        }
        
        if (!target_valid) {
            if (pTarget_) BrainLog("Target invalidated, depleted, or no longer fits.");
            pTarget_ = NULL;
            if (state_ == INTERCEPTING) TransitionState(HUNTING);
            // If REFUELING, remain in REFUELING.
        }
    }
}

// (ExecuteAction, HandleDeparting remain similar, utilizing BrainLog)

void HarvesterBrain::ExecuteAction() {
    if (pShip->GetAmount(S_SHIELD) < 30.0 && pShip->GetAmount(S_FUEL) > 1.0) {
        pShip->SetOrder(O_SHIELD, 5.0);
    }
    switch (state_) {
        case DEPARTING: HandleDeparting(); break;
        case HUNTING:
        case REFUELING:
            if (!pTarget_ && pShip->GetVelocity().rho > 10.0) {
                 pShip->SetOrder(O_THRUST, -5.0);
                 BrainLog("Searching... Braking.");
            }
            break;
        case INTERCEPTING:
            if (!AvoidCollisions()) Navigate();
            break;
    }
}

void HarvesterBrain::HandleDeparting() {
    CCoord center(0.0, 0.0);
    double targetAngle = pShip->GetPos().AngleTo(center);
    double turn = targetAngle - pShip->GetOrient();
    if (turn < -PI) turn += PI2;
    if (turn > PI) turn -= PI2;
    pShip->SetOrder(O_TURN, turn);
    if (fabs(turn) < 0.1) pShip->SetOrder(O_THRUST, maxspeed);
}


// SelectTarget (O(N) search, now uses cached parameters)
void HarvesterBrain::SelectTarget() {
    CWorld* pWorld = pShip->GetWorld();
    if (!pWorld) return;

    bool prioritizeFuel = (state_ == REFUELING);
    CAsteroid* bestAsteroid = NULL;
    double bestScore = -std::numeric_limits<double>::infinity();

    // Optimized hot path.
    for (UINT index = pWorld->UFirstIndex; index != (UINT)-1; index = pWorld->GetNextIndex(index)) {
        CThing* pTh = pWorld->GetThing(index);
        if (pTh && pTh->GetKind() == ASTEROID && pTh->IsAlive()) {
            CAsteroid* asteroid = (CAsteroid*)pTh;

            // Crucial check: Must fit.
            if (!pShip->AsteroidFits(asteroid)) continue;

            // Evaluation now uses fast cached parameters.
            double score = EvaluateAsteroid(asteroid, prioritizeFuel);
            
            if (score > bestScore) {
                bestScore = score;
                bestAsteroid = asteroid;
            }
        }
    }
    pTarget_ = bestAsteroid;

    if (pTarget_ && EvoAI::s_loggingEnabled) {
        std::stringstream ss;
        ss << "Target Selected: " << pTarget_->GetName() << " (Score: " << std::fixed << std::setprecision(2) << bestScore << ")";
        BrainLog(ss.str());
    }
}

// EvaluateAsteroid (Uses cached parameters)
double HarvesterBrain::EvaluateAsteroid(CAsteroid* asteroid, bool prioritizeFuel) {
    double distance = pShip->GetPos().DistTo(asteroid->GetPos());
    double mass = asteroid->GetMass();
    AsteroidKind material = asteroid->GetMaterial();

    // Accessing cache_.* fields is significantly faster than map lookups.
    double score = cache_.W_DISTANCE * distance;

    if (material == VINYL) {
        double weight = cache_.W_VINYL;
        if (prioritizeFuel) weight *= 0.01;
        score += weight * mass;
    } else if (material == URANIUM) {
        double weight = cache_.W_URANIUM;
        if (prioritizeFuel) weight *= cache_.W_FUEL_BOOST_FACTOR;
        score += weight * mass;
    }
    return score;
}

// Navigate (Uses cached parameters)
bool HarvesterBrain::Navigate() {
    if (!pTarget_) return false;

    // 1. Estimate time to intercept (dt)
    CTraj RelVel = pShip->RelativeVelocity(*pTarget_);
    double dist = pShip->GetPos().DistTo(pTarget_->GetPos());
    
    double dt;
    if (RelVel.rho > 0.5) {
        dt = sqrt(dist / RelVel.rho);
        if (dist > 0.001) dt += 1000.0 / dist;
    } else {
        dt = 5.0;
    }

    // 2. Calculate intercept angle
    double dang = pShip->AngleToIntercept(*pTarget_, dt);

    // 3. Execute Turn or Thrust
    if (fabs(dang) < cache_.NAV_ANGLE_TOLERANCE) {
        pShip->SetOrder(O_THRUST, cache_.NAV_THRUST_POWER);
    } else if (fabs(dang) > (PI - cache_.NAV_ANGLE_TOLERANCE)) {
        pShip->SetOrder(O_THRUST, -cache_.NAV_THRUST_POWER * 0.5);
    }
    else {
        pShip->SetOrder(O_TURN, dang * cache_.NAV_TURN_AGGRESSION);
    }
    return true;
}

// AvoidCollisions (Uses cached parameters)
bool HarvesterBrain::AvoidCollisions() {
    CWorld* pWorld = pShip->GetWorld();
    if (!pWorld || pShip->IsDocked()) return false;

    CThing* threat = NULL;
    double min_ttc = std::numeric_limits<double>::max();

    for (UINT index = pWorld->UFirstIndex; index != (UINT)-1; index = pWorld->GetNextIndex(index)) {
        CThing* pTh = pWorld->GetThing(index);
        if (!pTh || pTh == pShip || !pTh->IsAlive() || pTh == pTarget_) continue;

        double ttc = pShip->DetectCollisionCourse(*pTh);

        if (ttc != NO_COLLIDE && ttc < cache_.NAV_AVOIDANCE_HORIZON && ttc < min_ttc) {
            min_ttc = ttc;
            threat = pTh;
        }
    }

    if (threat) {
        // Evasive maneuver
        CTraj evasionVector = threat->GetPos().VectTo(pShip->GetPos());
        evasionVector.Rotate(PI / 4.0);

        double turnNeeded = evasionVector.theta - pShip->GetOrient();
        if (turnNeeded > PI) turnNeeded -= PI2;
        if (turnNeeded < -PI) turnNeeded += PI2;

        pShip->SetOrder(O_TURN, turnNeeded);

        if (fabs(turnNeeded) < 0.2) {
            pShip->SetOrder(O_THRUST, maxspeed);
        }

        if (EvoAI::s_loggingEnabled) {
            std::stringstream ss;
            ss << "EVADING " << threat->GetName() << ". TTC: " << std::fixed << std::setprecision(2) << min_ttc;
            BrainLog(ss.str());
        }
        return true;
    }
    return false;
}

void HarvesterBrain::TransitionState(BrainState newState) {
    if (state_ != newState) {
        if (EvoAI::s_loggingEnabled) {
            std::stringstream ss;
            ss << "State Change: " << StateToString(state_) << " -> " << StateToString(newState);
            BrainLog(ss.str());
        }
        state_ = newState;
    }
}
