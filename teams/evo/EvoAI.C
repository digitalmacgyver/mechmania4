#include "EvoAI.h"
#include "GameConstants.h"
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <vector>
#include <iostream> // Required for file operations in LoadParameters

// Initialize static members
bool EvoAI::s_loggingEnabled = false;
std::string EvoAI::s_paramFile = "EvoAI_params.txt";
std::string EvoAI::s_logFile = "EvoAI_game.log";

// Factory function
CTeam* CTeam::CreateTeam() {
    return new EvoAI;
}

// --- MagicBag Implementation ---
MagicBag::MagicBag(unsigned int drones) : num_drones(drones) {
    table.resize(drones);
}
MagicBag::~MagicBag() { clear(); }
Entry *MagicBag::getEntry(unsigned int drone, unsigned int elem) {
    if (drone >= num_drones || elem >= table[drone].size()) return NULL;
    return table[drone][elem];
}
void MagicBag::addEntry(unsigned int drone, Entry *entry) {
    if (drone >= num_drones) { delete entry; return; }
    table[drone].push_back(entry);
}
void MagicBag::clear() {
    for (auto& drone_entries : table) {
        for (Entry* entry : drone_entries) delete entry;
        drone_entries.clear();
    }
}

// --- EvoAI (CTeam) Implementation ---

EvoAI::EvoAI() : mb(NULL), hunter_config_count_(0) {
    // Initialize parameters for GA tuning

    // Resource Management
    params_["LOW_FUEL_THRESHOLD"] = 5.0;
    params_["RETURN_CARGO_THRESHOLD"] = 13.01;
    
    // Safety
    params_["MIN_SHIELD_LEVEL"] = 11.0;
    params_["EMERGENCY_FUEL_RESERVE"] = 5.0;
    
    // Navigation
    params_["NAV_ALIGNMENT_THRESHOLD"] = 0.1;

    // Team Composition & Configuration
    // TEAM_NUM_HUNTERS_CONFIG determines the physical configuration (fuel/cargo ratio)
    params_["TEAM_NUM_HUNTERS_CONFIG"] = 1.0; 
    params_["GATHERER_CARGO_RATIO"] = 0.666;  // e.g., 40 Cargo / 20 Fuel
    params_["HUNTER_CARGO_RATIO"] = 0.25;     // e.g., 15 Cargo / 45 Fuel

    // Combat Tactics
    params_["COMBAT_ENGAGEMENT_RANGE"] = 350.0;
    params_["COMBAT_MIN_FUEL_TO_HUNT"] = 15.0;
    params_["COMBAT_LASER_EFFICIENCY_RATIO"] = 3.0; // B/D ratio for favorable trade
    params_["COMBAT_OVERKILL_BUFFER"] = 1.0;        // Extra damage buffer (in shield units)

    // Strategy
    params_["STRATEGY_ENDGAME_TURN"] = 270.0;

    LoadParameters();
    srand(time(NULL));
}

EvoAI::~EvoAI() {
    if (mb) delete mb;
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* pSh = GetShip(i);
        if (pSh && pSh->GetBrain()) {
            delete pSh->GetBrain();
            pSh->SetBrain(NULL);
        }
    }
}

void EvoAI::LoadParameters() {
    std::ifstream file(s_paramFile.c_str());
    if (file.is_open()) {
        std::string key;
        double value;
        while (file >> key >> value) {
            if (params_.count(key)) {
                 params_[key] = value;
            }
        }
        file.close();
    }
}

// Configure ships based on GA parameters. Roles are assigned dynamically in Turn().
void EvoAI::Init() {
    SetName("EvoAI-Dynamic");

    if (!mb) {
        mb = new MagicBag(GetShipCount());
    }

    // Determine team configuration based on parameters
    hunter_config_count_ = (int)params_["TEAM_NUM_HUNTERS_CONFIG"];
    if (hunter_config_count_ < 0) hunter_config_count_ = 0;
    if (hunter_config_count_ > (int)GetShipCount()) hunter_config_count_ = (int)GetShipCount();

    auto clampRatio = [](double ratio) {
        if (ratio < 0.0) return 0.0;
        if (ratio > 1.0) return 1.0;
        return ratio;
    };

    double gathererRatio = clampRatio(params_["GATHERER_CARGO_RATIO"]);
    double hunterRatio = clampRatio(params_["HUNTER_CARGO_RATIO"]);

    // Initialize roles vector size
    ship_roles_.resize(GetShipCount());

    // Configure ships and assign the UnifiedBrain
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (!ship) continue;

        // Determine configuration (Hunter specialized or Gatherer specialized)
        bool isHunterConfig = (i < (unsigned int)hunter_config_count_);
        double cargoRatio = isHunterConfig ? hunterRatio : gathererRatio;

        double fuel_capacity = g_ship_total_stat_capacity * (1.0 - cargoRatio);

        ship->SetCapacity(S_FUEL, fuel_capacity);

        char namebuf[maxnamelen];
        snprintf(namebuf, maxnamelen, "Ship-%d (%s)", i, isHunterConfig ? "H-Cfg" : "G-Cfg");
        ship->SetName(namebuf);

        // Assign the UnifiedBrain to all ships
        ship->SetBrain(new UnifiedBrain(this, &params_));
    }
}

void EvoAI::Turn() {
    CWorld* pWorld = GetWorld();
    if (!pWorld || pWorld->bGameOver) return;

    // 1. Analyze Global State (Includes resource tracking)
    AssessStrategy();

    // 2. Assign Roles Dynamically
    AssignRoles();

    // 3. Central Planning (Based on dynamic roles)
    PopulateMagicBag();

    // 4. Execute ship decisions
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (ship && ship->IsAlive() && ship->GetBrain() && i < ship_roles_.size()) {
            // The UnifiedBrain checks the dynamic role internally.
            ship->GetBrain()->Decide();
        }
    }
}

// Analyze the world state to determine strategic posture.
void EvoAI::AssessStrategy() {
    CWorld* pWorld = GetWorld();
    
    // Initialize assessment
    strategy = {false, false, false, false, 0, 0.0, 0.0};
    
    int undocked_enemies = 0;
    bool enemy_station_has_vinyl = false;
    double friendly_cargo = 0.0;

    // Iterate world objects to track resources and entities
    for (unsigned int index = pWorld->UFirstIndex; index != BAD_INDEX; index = pWorld->GetNextIndex(index)) {
        CThing* thing = pWorld->GetThing(index);
        if (!thing || !thing->IsAlive()) continue;

        if (thing->GetKind() == ASTEROID) {
            CAsteroid* asteroid = (CAsteroid*)thing;
            if (asteroid->GetMaterial() == URANIUM) strategy.uranium_left += asteroid->GetMass();
            else if (asteroid->GetMaterial() == VINYL) strategy.vinyl_left += asteroid->GetMass();
        }
        else if (thing->GetKind() == SHIP) {
            CShip* ship = (CShip*)thing;
            if (ship->GetTeam() != this) {
                if (!ship->IsDocked()) {
                    undocked_enemies++;
                }
            } else {
                friendly_cargo += ship->GetAmount(S_CARGO);
            }
        } else if (thing->GetKind() == STATION) {
            CStation* station = (CStation*)thing;
            if (station->GetTeam() != this && station->GetVinylStore() > 0.1) {
                enemy_station_has_vinyl = true;
            }
        }
    }

    // Determine strategic conditions (a-g)

    // a. No hunting targets
    if (undocked_enemies == 0 && !enemy_station_has_vinyl) {
        strategy.no_hunting_targets = true;
    }

    // d. Probably no more points
    if (strategy.vinyl_left < 0.1 && friendly_cargo < 0.1) {
        strategy.no_more_points = true;
    }

    // e. Fuel constrained
    if (strategy.uranium_left < 0.1) {
        strategy.fuel_constrained = true;
    }

    // g. Endgame
    double endgame_turn = params_["STRATEGY_ENDGAME_TURN"];
    if (pWorld->GetGameTime() >= endgame_turn) {
        strategy.endgame = true;
    }

    // Determine required hunters based on conditions
    if (strategy.no_hunting_targets) {
        // If no targets, everyone gather.
        strategy.active_hunters_needed = 0;
    } else if (strategy.no_more_points) {
        // If no points left but targets exist, everyone hunt.
        strategy.active_hunters_needed = GetShipCount(); 
    } else {
        // Default behavior: Maintain the configured number of hunters
        strategy.active_hunters_needed = hunter_config_count_;
    }
}

// Dynamically assign roles based on strategic assessment and ship capabilities.
void EvoAI::AssignRoles() {
    int hunters_assigned = 0;
    int needed = strategy.active_hunters_needed;

    // Initialize all roles to GATHERER first
    for (unsigned int i = 0; i < GetShipCount(); ++i) {
        ship_roles_[i] = GATHERER;
    }

    // Pass 1: Assign Hunters, prioritizing Hunter configurations (ships with index < hunter_config_count_)
    for (unsigned int i = 0; i < (unsigned int)hunter_config_count_; ++i) {
        if (hunters_assigned >= needed) break;
        
        ship_roles_[i] = HUNTER;
        hunters_assigned++;
    }

    // Pass 2: Assign remaining Hunters (if needed, pull from Gatherer configs)
    for (unsigned int i = hunter_config_count_; i < GetShipCount(); ++i) {
        if (hunters_assigned >= needed) break;

        ship_roles_[i] = HUNTER;
        hunters_assigned++;
    }
    // Remaining ships are already set to GATHERER by the initialization step.
}


// Core Navigation Logic (Analytical Intercept Foundation)
FuelTraj EvoAI::determine_orders(CThing* thing, double time, CShip* ship) {
    FuelTraj result;
    if (!thing || time <= 0.0) return result;

    CCoord P1 = ship->GetPos();
    CCoord P2_future = thing->PredictPosition(time);

    // VectTo correctly handles toroidal wrapping
    CTraj displacement = P1.VectTo(P2_future); 
    CTraj V_required = displacement / time;

    if (V_required.rho > g_game_max_speed) return result;

    CTraj DeltaV = V_required - ship->GetVelocity();

    double target_angle = DeltaV.theta;
    double angle_error = target_angle - ship->GetOrient();
    
    while (angle_error > PI) angle_error -= PI2;
    while (angle_error < -PI) angle_error += PI2;

    double alignment_threshold = params_["NAV_ALIGNMENT_THRESHOLD"];

    if (fabs(angle_error) > alignment_threshold) {
        result.order_kind = O_TURN;
        result.order_mag = angle_error;
        // Simplified fuel estimation
        result.fuel_used = fabs(angle_error) * ship->GetMass() / (6.0 * PI2 * g_ship_spawn_mass);
    } else {
        result.order_kind = O_THRUST;
        result.order_mag = DeltaV.rho;
        // Simplified fuel estimation
        result.fuel_used = DeltaV.rho * ship->GetMass() / (6.0 * g_game_max_speed * g_ship_spawn_mass);
    }

    // Basic fuel check.
    if (!ship->IsDocked() && result.fuel_used > ship->GetAmount(S_FUEL)) {
        if (result.fuel_used > 0.0) {
            double scale = ship->GetAmount(S_FUEL) / result.fuel_used;
            result.order_mag *= scale;
            result.fuel_used = ship->GetAmount(S_FUEL);
        }
    }

    return result;
}


// Centralized Planning Phase
void EvoAI::PopulateMagicBag() {
    if (!mb) return;
    mb->clear();
    
    CWorld* pWorld = GetWorld();
    if (!pWorld) return;

    // 1. Identify potential targets (Resources tracked in AssessStrategy)
    std::vector<CThing*> targets;
    for (unsigned int index = pWorld->UFirstIndex; index != BAD_INDEX; index = pWorld->GetNextIndex(index)) {
        CThing* thing = pWorld->GetThing(index);
        if (!thing || !thing->IsAlive()) continue;

        if (thing->GetKind() == ASTEROID) {
            targets.push_back(thing);
        } 
        // Include enemy ships and stations as targets (IFF Check)
        else if ((thing->GetKind() == SHIP || thing->GetKind() == STATION) && thing->GetTeam() != this) {
            // Ensure ships aren't docked
            if (thing->GetKind() == SHIP && ((CShip*)thing)->IsDocked()) continue;
            
            // Optimization: Don't include empty stations
            if (thing->GetKind() == STATION && ((CStation*)thing)->GetVinylStore() < 0.1) continue;

            targets.push_back(thing);
        }
    }

    // 2. Calculate paths for each ship
    const int MAX_TURNS = 50; 

    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (!ship || !ship->IsAlive() || i >= ship_roles_.size()) continue;

        // Use the dynamically assigned role
        ShipRole role = ship_roles_[i];

        for (CThing* thing : targets) {
            
            // Filter targets based on role
            if (thing->GetKind() == ASTEROID) {
                // Asteroid Breaking Enabled: All sizes included.
            } else {
                // Optimization: Gatherers do not need paths to enemies calculated.
                if (role == GATHERER) continue;
            }

            // Search horizon
            for (int t = 1; t <= MAX_TURNS; t++) {
                FuelTraj ft = determine_orders(thing, (double)t, ship);

                if (ft.fuel_used >= 0.0) {
                    Entry* entry = new Entry();
                    entry->thing = thing;
                    entry->fueltraj = ft;
                    entry->turns_total = (double)t;
                    mb->addEntry(i, entry);
                    break; // Found the shortest time intercept
                }
            }
        }
    }
}

// --- UnifiedBrain Implementation ---

UnifiedBrain::UnifiedBrain(EvoAI* pTeam, ParamMap* params) : pmyEvoTeam_(pTeam), pTarget(NULL) {
    CacheParameters(params);
}

void UnifiedBrain::CacheParameters(ParamMap* params) {
    // Helper lambda for safe access
    auto getParam = [&](const std::string& key, double defaultVal) {
        return (params && params->count(key)) ? (*params)[key] : defaultVal;
    };

    // Load the GA-tunable parameters
    cache_.LOW_FUEL_THRESHOLD = getParam("LOW_FUEL_THRESHOLD", 5.0);
    cache_.RETURN_CARGO_THRESHOLD = getParam("RETURN_CARGO_THRESHOLD", 13.01);
    cache_.MIN_SHIELD_LEVEL = getParam("MIN_SHIELD_LEVEL", 11.0);
    cache_.EMERGENCY_FUEL_RESERVE = getParam("EMERGENCY_FUEL_RESERVE", 5.0);
    cache_.NAV_ALIGNMENT_THRESHOLD = getParam("NAV_ALIGNMENT_THRESHOLD", 0.1);
    cache_.COMBAT_ENGAGEMENT_RANGE = getParam("COMBAT_ENGAGEMENT_RANGE", 350.0);
    cache_.COMBAT_MIN_FUEL_TO_HUNT = getParam("COMBAT_MIN_FUEL_TO_HUNT", 15.0);
    cache_.COMBAT_LASER_EFFICIENCY_RATIO = getParam("COMBAT_LASER_EFFICIENCY_RATIO", 3.0);
    cache_.COMBAT_OVERKILL_BUFFER = getParam("COMBAT_OVERKILL_BUFFER", 1.0);
    cache_.STRATEGY_ENDGAME_TURN = getParam("STRATEGY_ENDGAME_TURN", 270.0);
}

void UnifiedBrain::Decide() {
    if (!pShip || !pShip->IsAlive()) return;

    pShip->ResetOrders();

    if (pShip->IsDocked()) {
        pTarget = NULL; // Clear target when docked
        HandleDeparture();
        return;
    }

    // 1. Handle Emergencies (Jettison near enemy base)
    bool orders_locked = HandleEmergencies();

    // 2. Execute Role-Specific Logic
    if (!orders_locked) {
        // Get the dynamically assigned role
        unsigned int shipIndex = pShip->GetShipNumber();
        if (shipIndex < pmyEvoTeam_->ship_roles_.size()) {
            ShipRole role = pmyEvoTeam_->ship_roles_[shipIndex];
            
            if (role == HUNTER) {
                ExecuteHunter();
            } else {
                ExecuteGatherer();
            }
        }
    }

    // 3. Shield Maintenance
    MaintainShields(CalculateRemainingFuel());
}


void UnifiedBrain::HandleDeparture() {
    pShip->SetOrder(O_THRUST, 20.0);
}

// Implements emergency jettison logic (Shared by all roles)
bool UnifiedBrain::HandleEmergencies() {
    CWorld *pmyWorld = pShip->GetWorld();
    CTeam *pmyTeam = pShip->GetTeam();
    double cur_cargo = pShip->GetAmount(S_CARGO);

    if (cur_cargo < 0.01) return false;

    // Check for imminent collisions with enemy stations
    for (unsigned int thing_i = pmyWorld->UFirstIndex; thing_i != BAD_INDEX; thing_i = pmyWorld->GetNextIndex(thing_i)) {
        CThing *athing = pmyWorld->GetThing(thing_i);
        if (!athing || !athing->IsAlive() || athing->GetKind() != STATION) continue;

        // IFF Check
        if (athing->GetTeam() == pmyTeam) continue;

        double turns = pShip->DetectCollisionCourse(*athing);
        if (turns >= 0.0 && turns < 3.0) {
            if (turns < 2.0) {
                // Emergency jettison
                pShip->SetJettison(VINYL, cur_cargo);
                return true; // Orders locked
            } else {
                // Turn away
                double angle_to_station = pShip->GetPos().AngleTo(athing->GetPos());
                double angle_away = angle_to_station + PI;
                
                while (angle_away > PI) angle_away -= PI2;
                while (angle_away < -PI) angle_away += PI2;

                double turn_needed = angle_away - pShip->GetOrient();
                while (turn_needed > PI) turn_needed -= PI2;
                while (turn_needed < -PI) turn_needed += PI2;

                pShip->SetOrder(O_TURN, turn_needed);
                return true; // Orders locked
            }
        }
    }
    return false;
}

// Implements shield maintenance procedure (Shared by all roles)
void UnifiedBrain::MaintainShields(double remaining_fuel_est) {
    double cur_shields = pShip->GetAmount(S_SHIELD);

    // Determine the effective fuel reserve threshold
    double fuel_reserve = cache_.EMERGENCY_FUEL_RESERVE;
    
    // Strategic Override: If fuel constrained or endgame, allow burning the reserve.
    if (pmyEvoTeam_->strategy.fuel_constrained || pmyEvoTeam_->strategy.endgame) {
        fuel_reserve = 0.0;
    }

    if (cur_shields < cache_.MIN_SHIELD_LEVEL) {
        double available_fuel = remaining_fuel_est - fuel_reserve;

        if (available_fuel > 0.0) {
            double wanted_shields = cache_.MIN_SHIELD_LEVEL - cur_shields;
            double shield_boost = std::min(wanted_shields, available_fuel);
            
            if (shield_boost > 0.01) {
                 pShip->SetOrder(O_SHIELD, shield_boost);
            }
        }
    }
}

// Helper to execute navigation orders
void UnifiedBrain::ExecuteOrders(const FuelTraj& ft) {
    pShip->SetOrder(ft.order_kind, ft.order_mag);
}

// Helper to calculate remaining fuel after issued orders
double UnifiedBrain::CalculateRemainingFuel() {
    double fuel_used_est = 0.0;
    
    // Movement estimates (O_THRUST and O_TURN are mutually exclusive)
    if (pShip->GetOrder(O_THRUST) != 0.0) {
        fuel_used_est += pShip->SetOrder(O_THRUST, pShip->GetOrder(O_THRUST));
    } else if (pShip->GetOrder(O_TURN) != 0.0) {
        fuel_used_est += pShip->SetOrder(O_TURN, pShip->GetOrder(O_TURN));
    } 
    
    // Laser estimate (Independent of movement)
    if (pShip->GetOrder(O_LASER) != 0.0) {
        fuel_used_est += pShip->SetOrder(O_LASER, pShip->GetOrder(O_LASER));
    }

    // Jettisoning fuel counts towards usage
    fuel_used_est += pShip->GetJettison(URANIUM);

    return pShip->GetAmount(S_FUEL) - fuel_used_est;
}

// --- Gatherer Logic ---

void UnifiedBrain::ExecuteGatherer() {
    // Clear combat target when switching to gatherer
    pTarget = NULL;

    EvoAI* pEvoAI = pmyEvoTeam_;
    if (!pEvoAI || !pEvoAI->mb) return;

    MagicBag* mbp = pEvoAI->mb;
    unsigned int shipnum = pShip->GetShipNumber();
    double cur_fuel = pShip->GetAmount(S_FUEL);
    double cur_cargo = pShip->GetAmount(S_CARGO);

    // 1. Determine Priority: Return Home vs Gather
    if ((cur_cargo > cache_.RETURN_CARGO_THRESHOLD) || (pEvoAI->strategy.vinyl_left < 0.01 && cur_cargo > 0.01)) {
        // Return to station
        CStation* station = pShip->GetTeam()->GetStation();
        
        const int MAX_STATION_SEARCH = 50;
        for (int t = 1; t <= MAX_STATION_SEARCH; t++) {
            FuelTraj ft = pEvoAI->determine_orders(station, (double)t, pShip);
            if (ft.fuel_used >= 0.0) {
                ExecuteOrders(ft);
                return;
            }
        }
        // If no path found, continue below.
    }

    // 2. Determine Preferred Resource: Fuel vs Vinyl
    bool prioritize_fuel = (cur_fuel <= cache_.LOW_FUEL_THRESHOLD && pEvoAI->strategy.uranium_left > 0.0) ||
                           (pEvoAI->strategy.vinyl_left < 0.01 && pEvoAI->strategy.uranium_left > 0.0);
    AsteroidKind preferred = prioritize_fuel ? URANIUM : VINYL;
    AsteroidKind secondary = prioritize_fuel ? VINYL : URANIUM;

    // 3. Select Best Target from MagicBag
    Entry *best_preferred = NULL;
    Entry *best_secondary = NULL;

    for (unsigned int i = 0; ; ++i) {
        Entry* e = mbp->getEntry(shipnum, i);
        if (e == NULL) break;

        // Includes large asteroids for breaking.
        if (e->thing && e->thing->GetKind() == ASTEROID) {
            CAsteroid* asteroid = (CAsteroid*)e->thing;
            
            if (asteroid->GetMaterial() == preferred) {
                if (best_preferred == NULL || e->turns_total < best_preferred->turns_total) {
                    best_preferred = e;
                }
            } 
            else if (asteroid->GetMaterial() == secondary) {
                 if (best_secondary == NULL || e->turns_total < best_secondary->turns_total) {
                    best_secondary = e;
                }
            }
        }
    }

    // 4. Execute Orders
    if (best_preferred != NULL) {
        ExecuteOrders(best_preferred->fueltraj);
    } else if (best_secondary != NULL) {
        // Ensure the secondary resource actually exists globally
        if ((secondary == VINYL && pEvoAI->strategy.vinyl_left > 0.01) || (secondary == URANIUM && pEvoAI->strategy.uranium_left > 0.01)) {
             ExecuteOrders(best_secondary->fueltraj);
        }
    }
}

// --- Hunter Logic ---

void UnifiedBrain::ExecuteHunter() {
    EvoAI* pEvoAI = pmyEvoTeam_;
    if (!pEvoAI || !pEvoAI->mb) return;

    // 1. Select/Validate Target
    SelectTarget();

    // 2. Check if we need fuel (Hunters prioritize fuel over combat if low)
    double cur_fuel = pShip->GetAmount(S_FUEL);
    
    bool low_fuel = (cur_fuel <= cache_.LOW_FUEL_THRESHOLD) || (cur_fuel <= cache_.COMBAT_MIN_FUEL_TO_HUNT);

    if (low_fuel && pEvoAI->strategy.uranium_left > 0.0) {
        // Search MagicBag for Uranium
        MagicBag* mbp = pEvoAI->mb;
        unsigned int shipnum = pShip->GetShipNumber();
        Entry *best_fuel = NULL;

        for (unsigned int i = 0; ; ++i) {
            Entry* e = mbp->getEntry(shipnum, i);
            if (e == NULL) break;
            if (e->thing && e->thing->GetKind() == ASTEROID && ((CAsteroid*)e->thing)->GetMaterial() == URANIUM) {
                if (best_fuel == NULL || e->turns_total < best_fuel->turns_total) {
                    best_fuel = e;
                }
            }
        }

        if (best_fuel) {
            ExecuteOrders(best_fuel->fueltraj);
            return;
        }
    }

    // 3. Engage Target (If we have enough fuel or couldn't find any)
    if (!pTarget) return;

    double distance = pShip->GetPos().DistTo(pTarget->GetPos());

    // If within engagement range, attempt to shoot
    if (distance <= cache_.COMBAT_ENGAGEMENT_RANGE) {
        if (AttemptToShoot(pTarget)) {
            return; // Shot taken or turn made
        }
    }

    // 4. If not shooting, navigate towards target (Stalking) using MagicBag
    MagicBag* mbp = pEvoAI->mb;
    unsigned int shipnum = pShip->GetShipNumber();
    Entry *best_path = NULL;

    for (unsigned int i = 0; ; ++i) {
        Entry* e = mbp->getEntry(shipnum, i);
        if (e == NULL) break;
        
        if (e->thing == pTarget) {
            best_path = e;
            break; // First match is the shortest path
        }
    }

    if (best_path) {
        ExecuteOrders(best_path->fueltraj);
    }
}

// Select the highest priority combat target (IFF aware)
void UnifiedBrain::SelectTarget() {
    // 1. Validate current target
    if (pTarget && pTarget->IsAlive()) {
        // IFF Check
        if (pTarget->GetTeam() != NULL && pTarget->GetTeam() != pShip->GetTeam()) {
            
            // Vulnerability Check: Ensure ships aren't docked
            if (pTarget->GetKind() == SHIP && ((CShip*)pTarget)->IsDocked()) {
                pTarget = NULL; 
            }
            // Efficiency Check: Ensure stations have vinyl
            else if (pTarget->GetKind() == STATION && ((CStation*)pTarget)->GetVinylStore() < 0.1) {
                pTarget = NULL;
            }

        } else {
            pTarget = NULL;
        }
    }

    // 2. If target is invalid, find a new one
    if (!pTarget) {
        CWorld* pWorld = pShip->GetWorld();
        CThing* best_target = NULL;
        double best_score = -1.0;

        for (unsigned int index = pWorld->UFirstIndex; index != BAD_INDEX; index = pWorld->GetNextIndex(index)) {
            CThing* thing = pWorld->GetThing(index);
            // IFF Check
            if (!thing || !thing->IsAlive() || thing->GetTeam() == NULL || thing->GetTeam() == pShip->GetTeam()) continue;

            double score = 0.0;
            double dist = pShip->GetPos().DistTo(thing->GetPos());

            if (thing->GetKind() == SHIP) {
                CShip* enemy = (CShip*)thing;
                if (enemy->IsDocked()) continue;

                score = 1000.0;
                if (enemy->GetAmount(S_CARGO) > 0.1) {
                    score += 500.0;
                }
            } else if (thing->GetKind() == STATION) {
                if (((CStation*)thing)->GetVinylStore() > 0.1) {
                    score = 1500.0;
                } else {
                    continue; 
                }
            }

            score -= dist;

            if (score > best_score) {
                best_score = score;
                best_target = thing;
            }
        }
        pTarget = best_target;
    }
}

// Implements advanced shooting logic: Efficiency, Overkill Prevention, and Lead calculation.
bool UnifiedBrain::AttemptToShoot(CThing* target) {
    // Constants (Defined in GameConstants.h, used here for calculation)
    const double DAMAGE_PER_REMAINING_LENGTH = 30.0; // g_laser_mass_scale_per_remaining_unit
    const double SHIELD_PER_DAMAGE = 1000.0;         // g_laser_damage_mass_divisor
    const double MAX_LASER_RANGE = 512.0;            // std::min(fWXMax, fWYMax)

    // Safety checks
    if (!target || pShip->IsDocked()) return false;
    if (target->GetKind() == SHIP && ((CShip*)target)->IsDocked()) return false;

    // 1. Calculate Trajectory (Lead the target 1s ahead)
    CCoord MyPos = pShip->PredictPosition(g_game_turn_duration);
    CCoord TargPos = target->PredictPosition(g_game_turn_duration); 

    CTraj VectToTarget = MyPos.VectTo(TargPos);
    double target_angle = VectToTarget.theta;
    double distance_D = VectToTarget.rho;

    // 2. Calculate Required Damage (Overkill Prevention)
    double target_health = 0.0; // In shield/vinyl units

    if (target->GetKind() == SHIP) {
        target_health = ((CShip*)target)->GetAmount(S_SHIELD);
    } else if (target->GetKind() == STATION) {
        target_health = ((CStation*)target)->GetVinylStore();
    }

    // Calculate damage needed to kill/empty, plus the defined buffer
    double required_damage = (target_health + cache_.COMBAT_OVERKILL_BUFFER) * SHIELD_PER_DAMAGE;

    // 3. Calculate Required Beam Length (B_kill)
    // Formula: B = (Damage / 30) + D
    double B_kill = (required_damage / DAMAGE_PER_REMAINING_LENGTH) + distance_D;
    B_kill = std::min(B_kill, MAX_LASER_RANGE);

    // 4. Evaluate Efficiency
    double E_desired = cache_.COMBAT_LASER_EFFICIENCY_RATIO; // Default 3.0

    // Calculate Minimum Efficient Beam Length (B_eff_min = Ratio * D)
    double B_eff_min = E_desired * distance_D;
    B_eff_min = std::min(B_eff_min, MAX_LASER_RANGE);

    // 5. Determine Optimal Beam (B_opt)
    double B_opt = 0.0;

    // Decision Logic: Balance Efficiency vs Securing the Kill
    
    // If the target is a station, efficiency doesn't matter (stations can't heal).
    if (target->GetKind() == STATION) {
        B_opt = B_kill;
    } else {
        // Target is a ship.
        if (B_kill >= B_eff_min) {
            // The kill shot is efficient. Take it.
            B_opt = B_kill;
        } else {
            // The kill shot is inefficient (B_kill < B_eff_min).
            
            // Strategic Override: Should we fire an inefficient shot?
            // If it's late game, or fuel is constrained (use it or lose it), allow the inefficient shot.
            if (pmyEvoTeam_->strategy.endgame || pmyEvoTeam_->strategy.fuel_constrained) {
                 B_opt = B_kill;
            } else {
                // Otherwise, prioritize efficiency. We choose NOT to fire and rely on navigation 
                // to close the distance for a better shot later.
                return false;
            }
        }
    }

    // 6. Execute Orders
    
    // Final check: Ensure the beam actually hits the target
    if (B_opt > distance_D + 0.01) {
        // Calculate the required turn delta
        double angle_error = target_angle - pShip->GetOrient();
        while (angle_error > PI) angle_error -= PI2;
        while (angle_error < -PI) angle_error += PI2;

        // Issue Turn order
        pShip->SetOrder(O_TURN, angle_error);

        // Issue Laser order (engine checks fuel and clamps if necessary)
        pShip->SetOrder(O_LASER, B_opt);
        return true;
    }

    // Decided not to shoot (beam doesn't reach)
    return false; 
}