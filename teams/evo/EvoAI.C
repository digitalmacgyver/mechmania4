#include "EvoAI.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstring> // Required for strlen

// Initialize static members (Defaults)
bool EvoAI::s_loggingEnabled = false; // Default false, rely on launcher/args to enable if needed
std::string EvoAI::s_paramFile = "EvoAI_params.txt";
// Default log file template. Actual name often set in Init() or via command line.
std::string EvoAI::s_logFile = "EvoAI_game.log"; 

// Factory function
CTeam* CTeam::CreateTeam() {
    return new EvoAI;
}

// --- Helper Functions for Logging ---

const char* ThingKindToString(ThingKind kind) {
    switch(kind) {
        case ASTEROID: return "ASTEROID";
        case SHIP: return "SHIP";
        case STATION: return "STATION";
        case GENTHING: return "GENTHING";
        default: return "UNKNOWN";
    }
}

const char* MaterialToString(AsteroidKind mat) {
    if (mat == VINYL) return "VINYL";
    if (mat == URANIUM) return "URANIUM";
    return "UNKNOWN";
}

// --- EvoAI (CTeam) Implementation ---

EvoAI::EvoAI() {
    // Initialize default parameters (Genome)
    
    // Heuristics
    params_["W_DISTANCE"] = -1.0;
    params_["W_VINYL"] = 10.0;
    params_["W_URANIUM"] = 5.0;
    params_["W_FUEL_BOOST_FACTOR"] = 5.0;
    params_["W_FUEL_COST_PENALTY"] = 10.0;
    params_["W_TTI_PENALTY"] = 2.0;
    params_["W_CONFLICT_PENALTY"] = 50.0; 
    
    // Thresholds
    params_["THRESHOLD_RETURN_CARGO"] = 0.95;
    // THRESHOLD_FUEL_LOW is deprecated
    params_["THRESHOLD_FUEL_TARGET"] = 60.0;
    params_["THRESHOLD_MAX_SHIELD_BOOST"] = 30.0; 

    // Dynamic Fuel Management
    params_["FUEL_COST_PER_DIST"] = 0.08;
    params_["FUEL_SAFETY_MARGIN"] = 20.0;
    
    // Navigation (P-Controller and Vector Navigation)
    params_["NAV_DESIRED_SPEED_FACTOR"] = 0.9;
    params_["NAV_ALIGNMENT_STRICT_ANGLE"] = 0.05; 
    params_["NAV_ALIGNMENT_LOOSE_ANGLE"] = 1.0;  // ~57 degrees
    params_["NAV_CLOSE_ENOUGH_DIST"] = 25.0;
    params_["NAV_PREDICTION_HORIZON"] = 5.0; // NEW
    
    // Safety
    params_["NAV_AVOIDANCE_HORIZON"] = 10.0;
    params_["NAV_SHIELD_BOOST_TTC"] = 1.5;

    // Tactics
    params_["TACTICS_LASER_POWER"] = 1000.0;
    params_["TACTICS_LASER_RANGE"] = 100.0;

    // Configuration
    params_["SHIP_CARGO_RATIO"] = 0.7;

    LoadParameters();
    srand(time(NULL));

    // Logging initialization is deferred until Init() when Team ID is available.
}

EvoAI::~EvoAI() {
    if (logFileStream_.is_open()) {
        Log("EvoAI Shutdown.");
        logFileStream_.close();
    }
    // Use unsigned int
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* pSh = GetShip(i);
        if (pSh && pSh->GetBrain()) {
            delete pSh->GetBrain();
            pSh->SetBrain(NULL);
        }
    }
}

// Initialize logging using Team ID
void EvoAI::InitializeLogging() {
    if (s_loggingEnabled && !logFileStream_.is_open()) {
        std::string filename = s_logFile;

        // Check if the filename is the default.
        // If it's the default, we generate a unique name using the Team ID.
        // If it was specified via command line (e.g. by ga_optimizer.py), we respect that name.
        if (filename == "EvoAI_game.log") {
            std::stringstream ss;
            // Use GetTeamNumber() which returns the unique ID (e.g. 0 or 1) assigned by the engine.
            ss << "EvoAI_game_Team" << GetTeamNumber() << ".log";
            filename = ss.str();
        }
        
        // Open the log file, overwriting existing (trunc)
        logFileStream_.open(filename.c_str(), std::ios::out | std::ios::trunc);
        
        if (logFileStream_.is_open()) {
            Log("EvoAI Initialized. Logging to: " + filename);
        } else {
            // If opening fails, disable logging to prevent further errors
            s_loggingEnabled = false; 
        }
    }
}

// Structured logging implementation
void EvoAI::LogStructured(const std::string& tag, const std::string& data) {
    if (s_loggingEnabled && logFileStream_.is_open()) {
        CWorld* pWorld = GetWorld();
        // Use high precision for time
        double time = pWorld ? pWorld->GetGameTime() : 0.0;
        // Format: TIME TAG DATA
        logFileStream_ << std::fixed << std::setprecision(1) << time << " " 
                       << tag << " " << data << std::endl; // std::endl ensures flush
    }
}

// Redirect standard Log to structured logging
void EvoAI::Log(const std::string& message) {
    LogStructured("DEBUG_MSG", message);
}

// Log the entire world state
void EvoAI::LogWorldState() {
    CWorld* pWorld = GetWorld();
    if (!pWorld) return;

    // --- 1. Log Overall Game State and Scores ---
    std::stringstream ss_state;
    ss_state << std::fixed << std::setprecision(2);
    ss_state << "GameOver=" << (pWorld->bGameOver ? 1 : 0) << " Scores=[";

    for (unsigned int t = 0; t < pWorld->GetNumTeams(); ++t) {
        CTeam* team = pWorld->GetTeam(t);
        if (team) {
            if (t > 0) ss_state << ", ";
            // Ensure name is valid
            const char* name = team->GetName();
            if (!name || strlen(name) == 0) name = "(Unnamed)";
            ss_state << name << ":" << team->GetScore();
        }
    }
    ss_state << "]";
    LogStructured("WORLD_STATE", ss_state.str());

    // --- 2. Log Individual Objects ---
    // Use BAD_INDEX constant from World.h
    for (unsigned int i = pWorld->UFirstIndex; i != BAD_INDEX; i = pWorld->GetNextIndex(i)) {
        CThing* thing = pWorld->GetThing(i);

        if (!thing || !thing->IsAlive()) continue;

        std::stringstream ss_obj;
        ss_obj << std::fixed;

        // Common attributes
        ss_obj << "ID=" << thing->GetWorldIndex()
               << " Kind=" << ThingKindToString(thing->GetKind())
               << " Name=" << thing->GetName();

        const CCoord& pos = thing->GetPos();
        const CTraj& vel = thing->GetVelocity();
        
        // Position (X, Y) and Velocity (Rho, Theta).
        ss_obj << std::setprecision(2);
        ss_obj << " Pos=(" << pos.fX << "," << pos.fY << ")";
        // Higher precision for angles
        ss_obj << " Vel=(" << vel.rho << "," << std::setprecision(4) << vel.theta << ")";
        ss_obj << " Orient=" << std::setprecision(4) << thing->GetOrient();
        ss_obj << std::setprecision(2);
        ss_obj << " Mass=" << thing->GetMass();

        CTeam* team = thing->GetTeam();
        if (team) {
            const char* name = team->GetName();
            if (!name || strlen(name) == 0) name = "(Unnamed)";
            ss_obj << " Team=" << name;
        }

        // Specific attributes
        switch (thing->GetKind()) {
            case ASTEROID: {
                CAsteroid* asteroid = (CAsteroid*)thing;
                ss_obj << " Material=" << MaterialToString(asteroid->GetMaterial());
                break;
            }
            case SHIP: {
                CShip* ship = (CShip*)thing;
                ss_obj << " Fuel=(" << ship->GetAmount(S_FUEL) << "/" << ship->GetCapacity(S_FUEL) << ")"
                       << " Cargo=(" << ship->GetAmount(S_CARGO) << "/" << ship->GetCapacity(S_CARGO) << ")"
                       << " Shields=" << ship->GetAmount(S_SHIELD)
                       << " Docked=" << (ship->IsDocked() ? 1 : 0);
                break;
            }
            case STATION: {
                // Score already logged in WORLD_STATE
                break;
            }
            case GENTHING:
                // Less critical, but logged for completeness
                break;
        }
        
        LogStructured("OBJECT", ss_obj.str());
    }
}


void EvoAI::LoadParameters() {
    std::ifstream file(s_paramFile.c_str());
    if (file.is_open()) {
        std::string key;
        double value;
        while (file >> key >> value) {
            // Check if the key exists in the defaults before assigning
            if (params_.count(key)) {
                 params_[key] = value;
            }
        }
        file.close();
    }
}

void EvoAI::Init() {
    SetName("EvoAI");

    // Initialize logging now that Team ID is set by the engine
    InitializeLogging();

    double cargoRatio = params_["SHIP_CARGO_RATIO"];
    if (cargoRatio < 0.1) cargoRatio = 0.1;
    if (cargoRatio > 0.9) cargoRatio = 0.9;

    // dMaxStatTot is the total capacity constant.
    double cargo_capacity = dMaxStatTot * cargoRatio;
    double fuel_capacity = dMaxStatTot - cargo_capacity;

    // Use unsigned int
    for (unsigned int i = 0; i < GetShipCount(); i++) {
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
    if (!pWorld) return;

    // Log the world state at the beginning of the turn
    if (s_loggingEnabled) {
        LogWorldState();
    }
    
    if (pWorld->bGameOver) return;

    // Execute ship decisions
    // Use unsigned int
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (ship && ship->IsAlive() && ship->GetBrain()) {
            // Decide() will call LogShipDecision() at the end.
            ship->GetBrain()->Decide();
        }
    }
}


// --- HarvesterBrain (CBrain) Implementation ---

HarvesterBrain::HarvesterBrain(EvoAI* pTeam, ParamMap* params) 
    : CBrain(), state_(DEPARTING), pmyEvoTeam_(pTeam), pTarget_(NULL), currentGoalDescription_("Initializing"), successiveTurns_(0), currentDynamicFuelLow_(0.0) {
    // Initialize CBrain base class explicitly
    CacheParameters(params);
}

// Cache parameters (UPDATED)
void HarvesterBrain::CacheParameters(ParamMap* params) {
    // Helper lambda for safe access
    auto getParam = [&](const std::string& key, double defaultVal) {
        return params->count(key) ? (*params)[key] : defaultVal;
    };

    // Heuristics
    cache_.W_DISTANCE = getParam("W_DISTANCE", -1.0);
    cache_.W_VINYL = getParam("W_VINYL", 10.0);
    cache_.W_URANIUM = getParam("W_URANIUM", 5.0);
    cache_.W_FUEL_BOOST_FACTOR = getParam("W_FUEL_BOOST_FACTOR", 5.0);
    cache_.W_FUEL_COST_PENALTY = getParam("W_FUEL_COST_PENALTY", 10.0);
    cache_.W_TTI_PENALTY = getParam("W_TTI_PENALTY", 2.0);
    cache_.W_CONFLICT_PENALTY = getParam("W_CONFLICT_PENALTY", 50.0); 

    // Thresholds
    cache_.THRESHOLD_RETURN_CARGO = getParam("THRESHOLD_RETURN_CARGO", 0.95);
    cache_.THRESHOLD_FUEL_TARGET = getParam("THRESHOLD_FUEL_TARGET", 60.0);
    cache_.THRESHOLD_MAX_SHIELD_BOOST = getParam("THRESHOLD_MAX_SHIELD_BOOST", 30.0); 

    // Dynamic Fuel Management
    cache_.FUEL_COST_PER_DIST = getParam("FUEL_COST_PER_DIST", 0.08);
    cache_.FUEL_SAFETY_MARGIN = getParam("FUEL_SAFETY_MARGIN", 20.0);
    
    // Navigation (P-Controller)
    cache_.NAV_DESIRED_SPEED_FACTOR = getParam("NAV_DESIRED_SPEED_FACTOR", 0.9);
    cache_.NAV_ALIGNMENT_STRICT_ANGLE = getParam("NAV_ALIGNMENT_STRICT_ANGLE", 0.05);
    cache_.NAV_ALIGNMENT_LOOSE_ANGLE = getParam("NAV_ALIGNMENT_LOOSE_ANGLE", 1.0);
    cache_.NAV_CLOSE_ENOUGH_DIST = getParam("NAV_CLOSE_ENOUGH_DIST", 25.0);
    cache_.NAV_PREDICTION_HORIZON = getParam("NAV_PREDICTION_HORIZON", 5.0); // NEW

    // Safety
    cache_.NAV_AVOIDANCE_HORIZON = getParam("NAV_AVOIDANCE_HORIZON", 10.0);
    cache_.NAV_SHIELD_BOOST_TTC = getParam("NAV_SHIELD_BOOST_TTC", 1.5);
    
    // Tactics
    cache_.TACTICS_LASER_POWER = getParam("TACTICS_LASER_POWER", 1000.0);
    cache_.TACTICS_LASER_RANGE = getParam("TACTICS_LASER_RANGE", 100.0);
}

const char* HarvesterBrain::StateToString(BrainState state) {
    switch(state) {
        case DEPARTING: return "DEPARTING";
        case HUNTING: return "HUNTING";
        case INTERCEPTING: return "INTERCEPTING";
        case REFUELING: return "REFUELING";
        case BREAKING: return "BREAKING";
        default: return "UNKNOWN";
    }
}

void HarvesterBrain::BrainLog(const std::string& message) {
    // Redirect BrainLog to structured logging for specific debug events
    if (EvoAI::s_loggingEnabled) {
        std::string msg = std::string(pShip->GetName()) + ": " + message;
        pmyEvoTeam_->LogStructured("DEBUG_BRAIN", msg);
    }
}

// Log the final decision state and orders (UPDATED)
void HarvesterBrain::LogShipDecision() {
    std::stringstream ss;
    ss << std::fixed;

    // Log Ship ID, State, Successive Turns
    ss << "ShipID=" << pShip->GetWorldIndex()
       << " State=" << StateToString(state_)
       << " SuccessiveTurns=" << successiveTurns_; // Log turn metric

    // Log Target ID
    int targetId = (pTarget_ && pTarget_->IsAlive()) ? (int)pTarget_->GetWorldIndex() : -1;
    ss << " TargetID=" << targetId;

    // Log the descriptive Goal (including nav metrics)
    ss << " Goal=\"" << currentGoalDescription_ << "\"";

    // Log dynamic fuel threshold
    ss << std::setprecision(2);
    ss << " DynFuelLow=" << currentDynamicFuelLow_;

    // Log the orders issued this turn
    ss << " Orders=[";
    bool orderLogged = false;

    // Movement Orders (Thrust/Turn are mutually exclusive in this AI)
    double thrust = pShip->GetOrder(O_THRUST);
    double turn = pShip->GetOrder(O_TURN);

    if (thrust != 0.0) {
        ss << "THRUST=" << std::setprecision(2) << thrust;
        orderLogged = true;
    }
    
    // O_TURN logs the desired orientation.
    if (turn != 0.0) {
        if (orderLogged) ss << ", ";
        // Higher precision for turn angle
        ss << "TURN=" << std::setprecision(4) << turn;
        orderLogged = true;
    }

    // Combat/Utility Orders
    double laser = pShip->GetOrder(O_LASER);
    double shield = pShip->GetOrder(O_SHIELD);

    if (laser != 0.0) {
        if (orderLogged) ss << ", ";
        ss << "LASER=" << std::setprecision(2) << laser;
        orderLogged = true;
    }

    if (shield != 0.0) {
        if (orderLogged) ss << ", ";
        ss << "SHIELD=" << std::setprecision(2) << shield;
        orderLogged = true;
    }
    
    // Jettison
    double jettisonV = pShip->GetJettison(VINYL);
    double jettisonU = pShip->GetJettison(URANIUM);

     if (jettisonV > 0.0) {
        if (orderLogged) ss << ", ";
        ss << "JETTISON_V=" << std::setprecision(2) << jettisonV;
        orderLogged = true;
    }
    if (jettisonU > 0.0) {
        if (orderLogged) ss << ", ";
        ss << "JETTISON_U=" << std::setprecision(2) << jettisonU;
        orderLogged = true;
    }

    if (!orderLogged) {
        ss << "None";
    }

    ss << "]";

    pmyEvoTeam_->LogStructured("DECISION", ss.str());
}

// Helper function to calculate the directed launch angle (DRY principle)
double HarvesterBrain::CalculateDepartureAngle() {
    CCoord center(0.0, 0.0);
    // Calculate the angle from the station towards the center.
    double baseTargetAngle = pShip->GetPos().AngleTo(center);

    // Get the ship index (0-3) within the team
    unsigned int shipIndex = 0;
    CTeam* pTeam = pShip->GetTeam();
    if (pTeam) {
        for (unsigned int i = 0; i < pTeam->GetShipCount(); ++i) {
            if (pTeam->GetShip(i) == pShip) {
                shipIndex = i;
                break;
            }
        }
    }

    // Apply offset based on index: 0, +0.5, -0.5, +1.0 radians
    double angleOffset = 0.0;
    if (shipIndex == 1) angleOffset = 0.5;
    else if (shipIndex == 2) angleOffset = -0.5;
    else if (shipIndex == 3) angleOffset = 1.0;

    double targetAngle = baseTargetAngle + angleOffset;
    // Normalize target angle
    if (targetAngle > PI) targetAngle -= PI2;
    if (targetAngle < -PI) targetAngle += PI2;
    
    return targetAngle;
}


// Updates goal description with context and navigation metrics
void HarvesterBrain::UpdateGoalDescription() {
    // Start with the base description set during state transitions or previous steps
    std::stringstream ss;
    ss << currentGoalDescription_;

    // Determine the navigation target position
    CCoord navTargetPos(0.0, 0.0); // Default for DEPARTING towards center
    bool hasNavTarget = false;

    if (pTarget_ && pTarget_->IsAlive()) {
        navTargetPos = pTarget_->GetPos();
        hasNavTarget = true;
    } else if (state_ == DEPARTING) {
        // In DEPARTING, the target is the center (0,0) conceptually.
        hasNavTarget = true;
    }

    // Add navigation metrics if relevant
    if (hasNavTarget) {
        // Calculate metrics
        double dist = pShip->GetPos().DistTo(navTargetPos);
        double targetAngle = pShip->GetPos().AngleTo(navTargetPos);
        double angleError = targetAngle - pShip->GetOrient();
        
        // Normalize angle error
        if (angleError > PI) angleError -= PI2;
        if (angleError < -PI) angleError += PI2;
        
        ss << std::fixed;
        ss << " | NavMetrics: Dist=" << std::setprecision(2) << dist;
        // AngleErr logged here is relative to the navTargetPos (e.g., the center or the asteroid)
        ss << ", AngleErr=" << std::setprecision(4) << angleError;

        // Specific check for "spinning while docked" issue
        if (pShip->IsDocked() && state_ == DEPARTING) {
            // To accurately log Turn vs Thrust during Directed Launch, we must check the error 
            // relative to the actual departure angle (including offset), not just the center.

            const double DEPARTURE_ALIGNMENT_THRESHOLD = 0.8;
            
            // Use the helper function to get the actual departure angle
            double actualDepartureAngle = CalculateDepartureAngle();

            double departureError = actualDepartureAngle - pShip->GetOrient();
            if (departureError > PI) departureError -= PI2;
            if (departureError < -PI) departureError += PI2;

            if (fabs(departureError) > DEPARTURE_ALIGNMENT_THRESHOLD) {
                 ss << " [Status: Docked, Turning]";
            } else {
                 ss << " [Status: Docked, Thrusting]";
            }
        }
    }

    currentGoalDescription_ = ss.str();
}

// Updates the successiveTurns_ counter
void HarvesterBrain::TrackSuccessiveTurns() {
    // O_TURN and O_THRUST are mutually exclusive in this AI.
    if (pShip->GetOrder(O_TURN) != 0.0) {
        successiveTurns_++;
    } else {
        // Reset if thrusting or coasting
        successiveTurns_ = 0;
    }
}

void HarvesterBrain::Decide() {
    if (!pShip || !pShip->IsAlive()) return;
    
    // Reset goal description at the start of the decision process
    currentGoalDescription_ = "Evaluating State";

    UpdateState();

    // Handle search states
    if (state_ == HUNTING || state_ == REFUELING) {
        // Update goal before searching
        if (state_ == HUNTING) currentGoalDescription_ = "Searching for resources";
        if (state_ == REFUELING) {
             std::stringstream ss_refuel;
             ss_refuel << "Searching for Uranium (Low Fuel, Threshold=" 
                       << std::fixed << std::setprecision(1) << currentDynamicFuelLow_ << ")";
             currentGoalDescription_ = ss_refuel.str();
        }
        
        SelectTarget(); 
        
        if (pTarget_) {
            // Goal updated within SelectTarget/TransitionState.
        } else {
            // If search failed, determine next step.
            if (state_ == HUNTING && pShip->GetAmount(S_CARGO) > 0.1) {
                pTarget_ = pShip->GetTeam()->GetStation();
                TransitionState(INTERCEPTING);
                currentGoalDescription_ = "Search failed, returning partial cargo.";
            } else if (state_ == REFUELING) {
                 TransitionState(HUNTING);
                 currentGoalDescription_ = "No fuel found, switching to general hunt.";
            } else {
                // If still no target, we will brake/idle.
                currentGoalDescription_ = "No targets found.";
            }
        }
    }

    // Update description with detailed metrics before executing actions
    UpdateGoalDescription();

    ExecuteAction();

    // Update the successive turn counter based on the orders issued this turn.
    TrackSuccessiveTurns();

    // Log the final decision summary after all actions are executed and orders set
    if (EvoAI::s_loggingEnabled) {
        LogShipDecision();
    }
}

// UPDATED: Dynamic Fuel Management
void HarvesterBrain::UpdateState() {
    // 1. Docking Check
    if (pShip->IsDocked()) {
        if (state_ != DEPARTING) {
            pTarget_ = NULL; // CRITICAL: Clear the target when docked.
            TransitionState(DEPARTING);
        }
        return;
    }

    if (state_ == DEPARTING) TransitionState(HUNTING);

    // 2. Fuel Check (Dynamic Thresholding)
    double current_fuel = pShip->GetAmount(S_FUEL);
    
    // Calculate dynamic threshold
    currentDynamicFuelLow_ = cache_.FUEL_SAFETY_MARGIN; // Start with safety margin
    CStation* base = pShip->GetTeam()->GetStation();
    if (base && base->IsAlive()) {
        double dist_to_base = pShip->GetPos().DistTo(base->GetPos());
        // Estimated fuel cost to return + safety margin
        double required_return_fuel = (dist_to_base * cache_.FUEL_COST_PER_DIST) + cache_.FUEL_SAFETY_MARGIN;
        
        currentDynamicFuelLow_ = required_return_fuel;
    }

    // State transitions based on fuel levels (Hysteresis)
    if (current_fuel < currentDynamicFuelLow_ && state_ != REFUELING) {
        TransitionState(REFUELING);
    } else if (current_fuel >= cache_.THRESHOLD_FUEL_TARGET && state_ == REFUELING) {
        TransitionState(HUNTING);
    }

    // 3. Cargo Check
    double capacity = pShip->GetCapacity(S_CARGO);
    double cargo_load = (capacity > 0) ? (pShip->GetAmount(S_CARGO) / capacity) : 0.0;

    // Only return if cargo is full AND we are not actively refueling/breaking.
    if (cargo_load >= cache_.THRESHOLD_RETURN_CARGO && state_ != REFUELING && state_ != BREAKING) {
        CStation* station = pShip->GetTeam()->GetStation();
        if (state_ != INTERCEPTING || pTarget_ != station) {
             pTarget_ = station;
             // Specific goal update before transition
             currentGoalDescription_ = "Cargo full, returning to base.";
             TransitionState(INTERCEPTING);
        }
        return;
    }

    // 4. Target Validation
    if (state_ == INTERCEPTING || state_ == REFUELING || state_ == BREAKING) {
        if (pTarget_ == pShip->GetTeam()->GetStation()) return;

        bool target_valid = false;
        if (pTarget_ && pTarget_->IsAlive() && pTarget_->GetKind() == ASTEROID) {
            CAsteroid* asteroid = (CAsteroid*)pTarget_;
            
            if (state_ == BREAKING) {
                // Valid if still too large to fit and large enough to split (Rule 10: >= 3 tons / minmass)
                if (!pShip->AsteroidFits(asteroid) && asteroid->GetMass() >= minmass) {
                    target_valid = true;
                }
            } else {
                 // INTERCEPTING or REFUELING: Valid if it fits
                if (pShip->AsteroidFits(asteroid)) {
                    if (state_ != REFUELING || asteroid->GetMaterial() == URANIUM) {
                        target_valid = true;
                    }
                }
            }
        }
        
        if (!target_valid) {
            pTarget_ = NULL;
            // If target is lost, return to HUNTING
            currentGoalDescription_ = "Target lost/invalidated.";
            TransitionState(HUNTING);
        }
    }
}

void HarvesterBrain::ExecuteAction() {
    double imminent_ttc = 999.0;
    bool movement_order_issued = false;

    // 1. Handle Departure
    if (state_ == DEPARTING) {
        HandleDeparting();
        return; // Departure movement is free
    }

    // 2. Handle Avoidance (High Priority)
    // Check fuel before attempting avoidance maneuvers
    if (pShip->GetAmount(S_FUEL) > 0.1 && AvoidCollisions(imminent_ttc)) {
        movement_order_issued = true;
        // Append high priority action to goal description
        currentGoalDescription_ += " | ACTION: Collision Avoidance Maneuver.";
    }

    // 3. Handle Tactics and Navigation
    if (!movement_order_issued && pShip->GetAmount(S_FUEL) > 0.1) {
        if (state_ == BREAKING) {
            // HandleBreaking manages both movement and laser fire internally.
            if (HandleBreaking()) {
                movement_order_issued = true;
            }
        } else if (state_ == INTERCEPTING) {
            NavigateVectorP(); // Use P-Controller
            movement_order_issued = true;
        } else if (state_ == HUNTING || state_ == REFUELING) {
            // Braking if searching and no target
            if (!pTarget_) {
                 NavigateVectorP();
                 movement_order_issued = true;
                 currentGoalDescription_ += " | ACTION: Braking.";
            }
        }
    }

    // 4. Handle Drifting/Idle/No Fuel
    if (!movement_order_issued) {
        // If we didn't issue a movement order, check why.
        if (pShip->GetAmount(S_FUEL) <= 0.1) {
             currentGoalDescription_ += " | STATUS: Out of Fuel (Drifting).";
        } else if (state_ != BREAKING) {
            // If we aren't breaking (which might involve coasting), we are idle.
            currentGoalDescription_ += " | STATUS: Idle/Drifting.";
        }
    }


    // 5. Reactive Shield Usage
    if (imminent_ttc < cache_.NAV_SHIELD_BOOST_TTC && pShip->GetAmount(S_FUEL) > 1.0) {
        
        if (pShip->GetAmount(S_SHIELD) < cache_.THRESHOLD_MAX_SHIELD_BOOST) {
            // Reduced boost magnitude (5.0) to conserve fuel.
            pShip->SetOrder(O_SHIELD, 5.0); 
            // Append shield boost to the goal description
            currentGoalDescription_ += " | ACTION: Boosting Shields (TTC imminent).";
        } else {
            currentGoalDescription_ += " | STATUS: Shields at Max Boost Cap (TTC imminent).";
        }
    }
}

// Implement Directed Launch and Looser Alignment Threshold.
void HarvesterBrain::HandleDeparting() {
    // Use the helper function to determine the directed launch angle
    double targetAngle = CalculateDepartureAngle();

    // Use direct orientation setting (P-Controller)
    double turnCommand = targetAngle;
    
    double angleError = targetAngle - pShip->GetOrient();
    if (angleError > PI) angleError -= PI2;
    if (angleError < -PI) angleError += PI2;

    // Turn OR Thrust
    // Use a much looser alignment threshold for departure (0.8 rads ~45 deg) 
    // to prevent oscillation/overshoot (Dock-Lock bug).
    const double DEPARTURE_ALIGNMENT_THRESHOLD = 0.8; 

    if (fabs(angleError) > DEPARTURE_ALIGNMENT_THRESHOLD) {
        pShip->SetOrder(O_TURN, turnCommand);
    } else {
        // Use global constant for max speed
        pShip->SetOrder(O_THRUST, g_game_max_speed);
    }
}


void HarvesterBrain::SelectTarget() {
    CWorld* pWorld = pShip->GetWorld();
    if (!pWorld) return;

    bool prioritizeFuel = (state_ == REFUELING);
    CAsteroid* bestTarget = NULL;
    double bestScore = -std::numeric_limits<double>::infinity();
    bool bestIsTooLarge = false;

    // Use BAD_INDEX constant
    for (unsigned int index = pWorld->UFirstIndex; index != BAD_INDEX; index = pWorld->GetNextIndex(index)) {
        CThing* pTh = pWorld->GetThing(index);
        if (pTh && pTh->GetKind() == ASTEROID && pTh->IsAlive()) {
            CAsteroid* asteroid = (CAsteroid*)pTh;

            // Ignore dust (< 3 tons / minmass)
            if (asteroid->GetMass() < minmass) continue;

            bool too_large = false;
            double score = EvaluateAsteroid(asteroid, prioritizeFuel, too_large);
            
            if (score > bestScore) {
                bestScore = score;
                bestTarget = asteroid;
                bestIsTooLarge = too_large;
            }
        }
    }
    
    pTarget_ = bestTarget;

    if (pTarget_) {
        // Determine next state based on size and material
        if (bestIsTooLarge) {
            // If it's Vinyl, break it.
            if (((CAsteroid*)pTarget_)->GetMaterial() == VINYL) {
                 TransitionState(BREAKING);
            } else {
                pTarget_ = NULL; // Should not happen if EvaluateAsteroid is correct
            }
        } else {
            TransitionState(INTERCEPTING);
        }
    }
}

// Estimate Time-To-Intercept (TTI)
double HarvesterBrain::EstimateTTI(CThing* target) {
    // Calculate relative velocity (V_target - V_ship)
    CTraj relVel = pShip->RelativeVelocity(*target);
    
    // Calculate distance (Toroidal aware)
    double dist = pShip->GetPos().DistTo(target->GetPos());

    if (dist < 0.1) return 0.0;

    // Get the displacement vector (from ship to target)
    CTraj displacement = pShip->GetPos().VectTo(target->GetPos());
    
    // We need the dot product of relative velocity and the normalized displacement vector.
    // CTraj (polar) must be converted to CCoord (Cartesian) for the dot product.
    
    // Convert relative velocity to Cartesian
    CCoord relVelCartesian = relVel.ConvertToCoord();
    
    // Normalize the displacement vector (make it a unit vector)
    displacement.rho = 1.0; 
    CCoord displacementUnitVector = displacement.ConvertToCoord();

    // Calculate the dot product (recession rate)
    double recession_rate = relVelCartesian.fX * displacementUnitVector.fX +
                            relVelCartesian.fY * displacementUnitVector.fY;

    // Use the global constant for max speed (30.0)
    const double MAX_SHIP_SPEED = g_game_max_speed;

    // If recession_rate is positive, the target is currently moving away.
    if (recession_rate > 0) {
        if (recession_rate > MAX_SHIP_SPEED) {
            return 9999.0; // High TTI penalty
        }
        // TTI is based on the speed difference.
        return dist / (MAX_SHIP_SPEED - recession_rate);
    } else {
        // If moving towards us or stationary (recession_rate <= 0)
        return dist / MAX_SHIP_SPEED;
    }
}


// EvaluateAsteroid includes TTI analysis and Conflict Detection.
// UPDATED: Includes deterministic tie-breaking for coordination.
double HarvesterBrain::EvaluateAsteroid(CAsteroid* asteroid, bool prioritizeFuel, bool& too_large) {
    // 1. Check Fit
    too_large = !pShip->AsteroidFits(asteroid);
    
    // If refueling, we MUST be able to fit the uranium.
    if (prioritizeFuel && too_large) {
        return -std::numeric_limits<double>::infinity();
    }

    // If it's too large and not Vinyl, we can't break it effectively, so ignore it.
    if (too_large && asteroid->GetMaterial() != VINYL) {
        return -std::numeric_limits<double>::infinity();
    }

    // 2. Basic Weighted Score
    // DistTo correctly handles toroidal wrap
    double distance = pShip->GetPos().DistTo(asteroid->GetPos());
    double mass = asteroid->GetMass();
    AsteroidKind material = asteroid->GetMaterial();

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

    // 3. Reachability Penalty (TTI)
    double tti = EstimateTTI(asteroid);
    // Apply penalty (W_TTI_PENALTY is expected to be positive)
    score -= cache_.W_TTI_PENALTY * tti;

    // 4. Fuel Cost Penalty
    CStation* base = pShip->GetTeam()->GetStation();
    // Only apply penalty if we intend a round trip (i.e., collecting, not breaking)
    if (base && !too_large) {
        double dist_to_base = asteroid->GetPos().DistTo(base->GetPos());
        // double total_dist = distance + dist_to_base;

        // Use the dynamic fuel cost parameter
        double estimated_fuel_cost = (distance + dist_to_base) * cache_.FUEL_COST_PER_DIST;

        if (estimated_fuel_cost > pShip->GetAmount(S_FUEL)) {
            score -= cache_.W_FUEL_COST_PENALTY * (estimated_fuel_cost - pShip->GetAmount(S_FUEL));
        }
    }
    
    // 5. Conflict Detection (Basic Coordination)
    CTeam* pTeam = pShip->GetTeam();
    if (pTeam) {
        for (unsigned int i = 0; i < pTeam->GetShipCount(); ++i) {
            CShip* otherShip = pTeam->GetShip(i);
            if (otherShip && otherShip != pShip && otherShip->IsAlive() && otherShip->GetBrain()) {
                HarvesterBrain* otherBrain = dynamic_cast<HarvesterBrain*>(otherShip->GetBrain());
                
                if (otherBrain && otherBrain->GetCurrentTarget() == asteroid) {
                    // If another ship is targeting this asteroid, check who is closer.
                    double otherDist = otherShip->GetPos().DistTo(asteroid->GetPos());
                    
                    // NEW: Deterministic Tie-Breaking
                    const double DISTANCE_TOLERANCE = 1.0; // Treat distances within 1 unit as equal

                    if (otherDist < distance - DISTANCE_TOLERANCE) {
                        // They are clearly closer, apply penalty.
                        score -= cache_.W_CONFLICT_PENALTY;
                    } else if (fabs(otherDist - distance) <= DISTANCE_TOLERANCE) {
                        // We are equidistant. Use Ship ID as tie-breaker.
                        // Lower ID takes priority.
                        if (pShip->GetWorldIndex() > otherShip->GetWorldIndex()) {
                            // Other ship has lower ID, apply penalty.
                            score -= cache_.W_CONFLICT_PENALTY;
                        }
                    }
                }
            }
        }
    }

    return score;
}

// --- Navigation and Tactics Implementation ---

// NavigateVectorP (P-Controller)
// Implements Dual-Threshold strategy, Oscillation Damping, and Predictive Intercept.
bool HarvesterBrain::NavigateVectorP() {
    
    // 1. Determine Desired Velocity Vector
    CTraj desiredVelocity;

    if (pTarget_) {
        
        // NEW: Predictive Intercept
        CCoord targetPos = pTarget_->GetPos();
        double dist = pShip->GetPos().DistTo(targetPos);
        double desired_speed = g_game_max_speed * cache_.NAV_DESIRED_SPEED_FACTOR;
        
        // Estimate time to reach target based on current distance and max desired speed.
        double estimated_time = (desired_speed > 0.1) ? (dist / desired_speed) : 0.0;

        // Cap the prediction horizon based on GA parameter.
        if (estimated_time > cache_.NAV_PREDICTION_HORIZON) {
            estimated_time = cache_.NAV_PREDICTION_HORIZON;
        }

        CCoord futurePos;
        // Use CThing::PredictPosition to estimate future location.
        // We assume PredictPosition handles toroidal wrapping internally based on the engine API.
        if (estimated_time > 0.1 && pTarget_->PredictPosition(estimated_time, futurePos)) {
             // Aim at the predicted position
             desiredVelocity = pShip->GetPos().VectTo(futurePos);
        } else {
             // Fallback to current position if prediction fails or time is too short
             desiredVelocity = pShip->GetPos().VectTo(targetPos);
        }

        
        // Set the magnitude (speed).

        // Velocity Matching: If close enough, match the target's velocity.
        // Use the actual distance to the target for proximity check.
        if (dist < cache_.NAV_CLOSE_ENOUGH_DIST) {
            desiredVelocity = pTarget_->GetVelocity();
        } else {
             // Otherwise, scale the vector towards the aimpoint by desired speed
             double current_dist_to_aimpoint = desiredVelocity.rho;

             if (current_dist_to_aimpoint < desired_speed) {
                desired_speed = current_dist_to_aimpoint; // Don't overshoot
             }
             // Handle near-zero vector normalization safely.
             if (desiredVelocity.rho > 0.001) {
                 desiredVelocity.rho = desired_speed;
             } else if (desired_speed > 0.001) {
                 desiredVelocity.rho = desired_speed;
             } else {
                 desiredVelocity.rho = 0.0;
             }
        }
    } else {
        // If no target (e.g., braking), desired velocity is zero.
        desiredVelocity.rho = 0.0;
        desiredVelocity.theta = 0.0;
    }

    // 2. Determine Required Acceleration Vector (Delta-V)
    CTraj currentVelocity = pShip->GetVelocity();
    CTraj requiredAcceleration = desiredVelocity - currentVelocity;

    // 3. Determine Required Orientation and Thrust Magnitude
    double targetAngle = requiredAcceleration.theta;
    double requiredThrustMagnitude = requiredAcceleration.rho;

    // If Delta-V is negligible, we don't need to act.
    if (requiredThrustMagnitude < 0.1) {
        return true;
    }

    // 4. Calculate Turn Command (P-Controller / Direct Orientation)
    double turnCommand = targetAngle; 

    // 5. Decision: Turn or Thrust (Mutually exclusive)

    // Calculate normalized angle error
    double angleError = targetAngle - pShip->GetOrient();
    if (angleError > PI) angleError -= PI2;
    if (angleError < -PI) angleError += PI2;

    const double MAX_THRUST = g_game_max_thrust_order_mag;
    double available_thrust = std::min(requiredThrustMagnitude, MAX_THRUST);

    // Strategy: Dual Thresholds with Oscillation Damping

    // NEW: Oscillation Damping. If we have turned too many times consecutively, 
    // relax the strict alignment requirement to allow thrust and break the cycle.
    const int MAX_SUCCESSIVE_TURNS = 2;
    double current_strict_angle = cache_.NAV_ALIGNMENT_STRICT_ANGLE;

    if (successiveTurns_ > MAX_SUCCESSIVE_TURNS) {
        current_strict_angle = cache_.NAV_ALIGNMENT_LOOSE_ANGLE;
        currentGoalDescription_ += " | DAMPING: Relaxing alignment (Excessive Turns).";
    }


    // Case A: Perfectly Aligned (Strict Threshold or Damped Threshold)
    if (fabs(angleError) < current_strict_angle) {
        pShip->SetOrder(O_THRUST, available_thrust);
    }
    // Case B: Perfectly Backwards
    else if (fabs(angleError) > PI - current_strict_angle) {
        pShip->SetOrder(O_THRUST, -available_thrust);
    }
    // Case C: Somewhat Aligned (Loose Threshold) - Apply Thrust
    else if (fabs(angleError) < cache_.NAV_ALIGNMENT_LOOSE_ANGLE) {
        
        // Calculate the component of the thrust vector that aligns with the required acceleration.
        double effective_thrust = cos(angleError) * available_thrust;

        // Apply the full available thrust along the current orientation.
        if (effective_thrust > 0.1) {
             pShip->SetOrder(O_THRUST, available_thrust);
        } else {
            // If effective thrust is negligible or negative (we are past 90 degrees), prioritize turning.
            pShip->SetOrder(O_TURN, turnCommand);
        }
    }
    // Case D: Significantly Misaligned - Prioritize Turning
    else {
        pShip->SetOrder(O_TURN, turnCommand);
    }

    return true;
}


// AvoidCollisions (Reactive Avoidance)
// UPDATED: Implements Impulse Avoidance and Threat Filtering.
bool HarvesterBrain::AvoidCollisions(double& imminent_ttc) {
    CWorld* pWorld = pShip->GetWorld();
    if (!pWorld || pShip->IsDocked()) return false;

    CThing* threat = NULL;
    double min_ttc = std::numeric_limits<double>::max();

    // Use BAD_INDEX constant
    for (unsigned int index = pWorld->UFirstIndex; index != BAD_INDEX; index = pWorld->GetNextIndex(index)) {
        CThing* pTh = pWorld->GetThing(index);
        
        // Basic checks
        if (!pTh || pTh == pShip || !pTh->IsAlive()) continue;

        // FIX 1: Ignore small debris (Threshold 5.0)
        if (pTh->GetMass() < 5.0) continue; 

        // FIX 2: Handle target avoidance based on state.
        if (pTh == pTarget_) {
            // If INTERCEPTING, we WANT to hit the target (mine it), so ignore it for avoidance.
            if (state_ == INTERCEPTING) continue;
            // If BREAKING, we still want to avoid accidental collisions.
        }

        // Use the public DetectCollisionCourse() method
        double ttc = pShip->DetectCollisionCourse(*pTh);

        if (ttc != NO_COLLIDE && ttc < cache_.NAV_AVOIDANCE_HORIZON && ttc < min_ttc) {
            min_ttc = ttc;
            threat = pTh;
        }
    }

    if (threat) {
        imminent_ttc = min_ttc; // Update the reference for shield logic

        // Evasive maneuver calculation
        CTraj evasionVector = threat->GetPos().VectTo(pShip->GetPos());
        evasionVector.Rotate(PI / 4.0); // Rotate 45 degrees away (bias)

        double turnCommand = evasionVector.theta;

        double angleError = evasionVector.theta - pShip->GetOrient();
        if (angleError > PI) angleError -= PI2;
        if (angleError < -PI) angleError += PI2;

        // FIX 3: Impulse Avoidance Strategy
        // If the current orientation provides ANY positive component towards the evasion vector (angle < 90 deg), THRUST NOW.

        if (cos(angleError) > 0.0) {
             // We are generally pointing away. Thrust immediately.
             pShip->SetOrder(O_THRUST, g_game_max_thrust_order_mag);
        } else {
             // We are pointing towards the threat. Prioritize turning immediately.
             pShip->SetOrder(O_TURN, turnCommand);
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

// HandleBreaking (Movement and Laser Firing)
// UPDATED: Added Oscillation Damping.
bool HarvesterBrain::HandleBreaking() {
    if (!pTarget_) return false;

    // Goal: Align with asteroid, maintain optimal range, and fire.
    currentGoalDescription_ += " | ACTION: Maneuvering to Break Asteroid.";

    // 1. Orientation and Distance
    double targetAngle = pShip->GetPos().AngleTo(pTarget_->GetPos());
    double turnCommand = targetAngle;
    double dist = pShip->GetPos().DistTo(pTarget_->GetPos());

    double angleError = targetAngle - pShip->GetOrient();
    if (angleError > PI) angleError -= PI2;
    if (angleError < -PI) angleError += PI2;

    // 2. Range Management Parameters
    const double MAX_RANGE = cache_.TACTICS_LASER_RANGE;
    const double DEAD_ZONE_MAX = MAX_RANGE * 0.90; // Coast if within 90%
    const double DEAD_ZONE_MIN = MAX_RANGE * 0.75; // Coast if above 75%
    const double MAX_MANEUVER_SPEED = 20.0; // Cautious speed

    // NEW: Oscillation Damping.
    const int MAX_SUCCESSIVE_TURNS = 2;
    double current_strict_angle = cache_.NAV_ALIGNMENT_STRICT_ANGLE;

    if (successiveTurns_ > MAX_SUCCESSIVE_TURNS) {
        current_strict_angle = cache_.NAV_ALIGNMENT_LOOSE_ANGLE;
        currentGoalDescription_ += " | DAMPING: Relaxing alignment (Excessive Turns).";
    }

    // Use strict alignment (or damped threshold) for tactical maneuvering
    if (fabs(angleError) > current_strict_angle) {
        // Priority 1: Align
        pShip->SetOrder(O_TURN, turnCommand);
    } else {
        // Aligned: Manage Range using Proportional Control
        if (dist > DEAD_ZONE_MAX) {
            // Too far, move closer. Proportional thrust based on distance error.
            double thrust = (dist - DEAD_ZONE_MAX);
            if (thrust > MAX_MANEUVER_SPEED) thrust = MAX_MANEUVER_SPEED;
            pShip->SetOrder(O_THRUST, thrust);
        } else if (dist < DEAD_ZONE_MIN) {
            // Too close, back off. Proportional thrust.
            double thrust = (dist - DEAD_ZONE_MIN); // This will be negative
            if (thrust < -MAX_MANEUVER_SPEED) thrust = -MAX_MANEUVER_SPEED;
            pShip->SetOrder(O_THRUST, thrust);
        }
        // If in the dead zone, coast (O_THRUST defaults to 0)
    }

    // 3. Firing Solution (Laser is independent of movement)
    // Fire if reasonably aligned (using strict alignment, NOT damped) and in range.
    if (dist <= MAX_RANGE && fabs(angleError) < cache_.NAV_ALIGNMENT_STRICT_ANGLE) { 
        pShip->SetOrder(O_LASER, cache_.TACTICS_LASER_POWER);
        currentGoalDescription_ += " Firing Laser.";
    }
    return true;
}


void HarvesterBrain::TransitionState(BrainState newState) {
    if (state_ != newState) {
        
        // Update the base goal description based on the new state, unless already set by a specific condition (e.g., "Cargo full").
        
        // Check if the current description is a generic "Evaluating" or "Target lost" type message.
        bool needsUpdate = (currentGoalDescription_.find("Evaluating") != std::string::npos) ||
                           (currentGoalDescription_.find("Target lost") != std::string::npos);

        std::stringstream goalSS;
        switch(newState) {
            case DEPARTING:
                goalSS << "Docked at station, preparing departure.";
                break;
            case HUNTING:
                if (needsUpdate) goalSS << "Searching for resources.";
                break;
            case REFUELING:
                goalSS << "Fuel low, prioritizing Uranium.";
                break;
            case INTERCEPTING:
                if (needsUpdate && pTarget_) {
                    goalSS << "Intercepting target ";
                    if (pTarget_->GetKind() == ASTEROID) {
                        goalSS << "Asteroid ID " << pTarget_->GetWorldIndex();
                    } else if (pTarget_->GetKind() == STATION) {
                        goalSS << "Station (Returning to Base)";
                    }
                }
                break;
            case BREAKING:
                if (pTarget_) {
                    goalSS << "Breaking large Asteroid ID " << pTarget_->GetWorldIndex();
                }
                break;
        }
        
        // Apply the new goal description if one was generated
        if (!goalSS.str().empty()) {
            currentGoalDescription_ = goalSS.str();
        }

        if (EvoAI::s_loggingEnabled) {
            std::stringstream ss;
            ss << "State Change: " << StateToString(state_) << " -> " << StateToString(newState);
            BrainLog(ss.str());
        }
        state_ = newState;
    }
}