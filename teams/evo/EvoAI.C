#include "EvoAI.h"
#include "GameConstants.h"
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <fstream>
#include <algorithm> // Required for std::min
#include <vector>

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

EvoAI::EvoAI() : mb(NULL), uranium_left(0.0), vinyl_left(0.0) {
    // Initialize parameters for GA tuning

    // Resource Management (Groogroo defaults)
    params_["LOW_FUEL_THRESHOLD"] = 5.0;
    params_["RETURN_CARGO_THRESHOLD"] = 13.01;
    
    // Safety (Groogroo defaults)
    params_["MIN_SHIELD_LEVEL"] = 11.0;
    params_["EMERGENCY_FUEL_RESERVE"] = 5.0;
    
    // Navigation
    params_["NAV_ALIGNMENT_THRESHOLD"] = 0.1;

    // Team Composition & Configuration (New GA Knobs)
    params_["TEAM_NUM_HUNTERS"] = 1.0; // Number of ships dedicated to hunting (0 to 4)
    // High cargo for gatherers (e.g., 40 Cargo / 20 Fuel = 0.666)
    params_["GATHERER_CARGO_RATIO"] = 0.666; 
    // Low cargo for hunters (e.g., 15 Cargo / 45 Fuel = 0.25, ChromeFunk default)
    params_["HUNTER_CARGO_RATIO"] = 0.25; 

    // Combat Tactics (New GA Knobs, ChromeFunk defaults)
    params_["COMBAT_ENGAGEMENT_RANGE"] = 350.0; // Range to switch from navigation to shooting
    params_["COMBAT_LASER_OVERHEAD"] = 100.0; // Extra beam length for damage
    params_["COMBAT_MIN_FUEL_TO_HUNT"] = 15.0; // Minimum fuel needed to actively hunt

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
    // Standard parameter loading implementation
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

void EvoAI::Init() {
    SetName("EvoAI-Combat");
    // InitializeLogging();

    // Initialize the MagicBag structure
    if (!mb) {
        mb = new MagicBag(GetShipCount());
    }

    // Determine team composition
    int hunter_count = (int)params_["TEAM_NUM_HUNTERS"];
    if (hunter_count < 0) hunter_count = 0;
    if (hunter_count > (int)GetShipCount()) hunter_count = (int)GetShipCount();

    // Helper to clamp ratios (Allowing 0.0 to 1.0 range)
    auto clampRatio = [](double ratio) {
        if (ratio < 0.0) return 0.0;
        if (ratio > 1.0) return 1.0;
        return ratio;
    };

    double gathererRatio = clampRatio(params_["GATHERER_CARGO_RATIO"]);
    double hunterRatio = clampRatio(params_["HUNTER_CARGO_RATIO"]);

    // Configure ships and assign roles/brains
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        if (!ship) continue;

        ShipRole role = (i < (unsigned int)hunter_count) ? HUNTER : GATHERER;
        ship_roles_.push_back(role);

        double cargoRatio = (role == HUNTER) ? hunterRatio : gathererRatio;

        double cargo_capacity = g_ship_total_stat_capacity * cargoRatio;
        double fuel_capacity = g_ship_total_stat_capacity - cargo_capacity;

        ship->SetCapacity(S_FUEL, fuel_capacity);
        // ship->SetCapacity(S_CARGO, cargo_capacity); // SetCapacity automatically adjusts the other stat

        char namebuf[maxnamelen];
        if (role == HUNTER) {
            snprintf(namebuf, maxnamelen, "Hunter-%d", i);
            ship->SetBrain(new HunterBrain(this, &params_));
        } else {
            snprintf(namebuf, maxnamelen, "Gather-%d", i);
            ship->SetBrain(new GathererBrain(this, &params_));
        }
        ship->SetName(namebuf);
    }
}

void EvoAI::Turn() {
    CWorld* pWorld = GetWorld();
    if (!pWorld || pWorld->bGameOver) return;

    // 1. Populate MagicBag (Central Planning)
    PopulateMagicBag();

    // 2. Execute ship decisions (Decentralized Execution)
    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        // Ensure ship_roles_ is properly initialized (defensive programming)
        if (ship && ship->IsAlive() && ship->GetBrain() && i < ship_roles_.size()) {
            ship->GetBrain()->Decide();
        }
    }
}

// Core Navigation Logic (Analytical Intercept Foundation)
// This function is toroidally aware by using VectTo.
FuelTraj EvoAI::determine_orders(CThing* thing, double time, CShip* ship) {
    FuelTraj result;
    // result.fuel_used is initialized to -1.0 (failure)

    if (!thing || time <= 0.0) return result;

    CCoord P1 = ship->GetPos();
    CCoord P2_future = thing->PredictPosition(time);

    // VectTo correctly handles toroidal wrapping, returning the shortest path vector.
    CTraj displacement = P1.VectTo(P2_future); 
    CTraj V_required = displacement / time;

    // Check if the required velocity exceeds the maximum speed
    if (V_required.rho > g_game_max_speed) return result;

    // Calculate Delta-V needed (Vector subtraction)
    CTraj DeltaV = V_required - ship->GetVelocity();

    // Determine the immediate order (Turn or Thrust)
    double target_angle = DeltaV.theta;
    double angle_error = target_angle - ship->GetOrient();
    
    // Normalize angle error to [-PI, PI] (shortest rotation delta)
    while (angle_error > PI) angle_error -= PI2;
    while (angle_error < -PI) angle_error += PI2;

    // Use the alignment threshold from parameters
    double alignment_threshold = params_["NAV_ALIGNMENT_THRESHOLD"];

    if (fabs(angle_error) > alignment_threshold) {
        result.order_kind = O_TURN;
        result.order_mag = angle_error; // Use the rotation delta
        // Estimate fuel cost (simplified legacy formula)
        result.fuel_used = fabs(angle_error) * ship->GetMass() / (6.0 * PI2 * g_ship_spawn_mass);
    } else {
        result.order_kind = O_THRUST;
        result.order_mag = DeltaV.rho;
        // Estimate fuel cost (simplified legacy formula)
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
// Update: Now includes large asteroids for breaking.
void EvoAI::PopulateMagicBag() {
    if (!mb) return;
    mb->clear();
    uranium_left = 0.0;
    vinyl_left = 0.0;

    CWorld* pWorld = GetWorld();
    if (!pWorld) return;

    // 1. Track global resources and identify potential targets
    std::vector<CThing*> targets;
    for (unsigned int index = pWorld->UFirstIndex; index != BAD_INDEX; index = pWorld->GetNextIndex(index)) {
        CThing* thing = pWorld->GetThing(index);
        if (!thing || !thing->IsAlive()) continue;

        if (thing->GetKind() == ASTEROID) {
            CAsteroid* asteroid = (CAsteroid*)thing;
            if (asteroid->GetMaterial() == URANIUM) uranium_left += asteroid->GetMass();
            else if (asteroid->GetMaterial() == VINYL) vinyl_left += asteroid->GetMass();
            targets.push_back(thing);
        } 
        // Include enemy ships and stations as targets (IFF Check)
        else if ((thing->GetKind() == SHIP || thing->GetKind() == STATION) && thing->GetTeam() != this) {
            // Ensure ships aren't docked (they are invulnerable while docked)
            if (thing->GetKind() == SHIP && ((CShip*)thing)->IsDocked()) continue;
            
            // Optimization: Don't include empty stations in the planning phase
            if (thing->GetKind() == STATION && ((CStation*)thing)->GetVinylStore() < 0.1) continue;

            targets.push_back(thing);
        }
    }

    // 2. Calculate paths for each ship
    // Extended horizon (50s) allows ample maneuvering time for distant targets.
    const int MAX_TURNS = 50; 

    for (unsigned int i = 0; i < GetShipCount(); i++) {
        CShip* ship = GetShip(i);
        // Ensure ship_roles_ is properly initialized
        if (!ship || !ship->IsAlive() || i >= ship_roles_.size()) continue;

        ShipRole role = ship_roles_[i];

        for (CThing* thing : targets) {
            
            // Filter targets based on role
            if (thing->GetKind() == ASTEROID) {
                // Asteroid Breaking Enabled: We no longer check AsteroidFits here.
                // Ships should navigate to large asteroids to break them.
            } else {
                // Optimization: Gatherers do not need paths to enemies calculated.
                if (role == GATHERER) continue;
            }

            // Search horizon
            for (int t = 1; t <= MAX_TURNS; t++) {
                // Find the best navigation order this turn to intercept at time t
                FuelTraj ft = determine_orders(thing, (double)t, ship);

                if (ft.fuel_used >= 0.0) {
                    // Found a valid trajectory
                    Entry* entry = new Entry();
                    entry->thing = thing;
                    entry->fueltraj = ft;
                    entry->turns_total = (double)t;
                    entry->total_fuel = ft.fuel_used; // Simplified estimation

                    mb->addEntry(i, entry);
                    break; // Found the shortest time intercept, move to the next object
                }
            }
        }
    }
}

// --- EvoBrain (Base Class) Implementation ---

EvoBrain::EvoBrain(EvoAI* pTeam, ParamMap* params) : pmyEvoTeam_(pTeam) {
    CacheParameters(params);
}

void EvoBrain::CacheParameters(ParamMap* params) {
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
    cache_.COMBAT_LASER_OVERHEAD = getParam("COMBAT_LASER_OVERHEAD", 100.0);
    cache_.COMBAT_MIN_FUEL_TO_HUNT = getParam("COMBAT_MIN_FUEL_TO_HUNT", 15.0);
}

void EvoBrain::HandleDeparture() {
    // Simple departure logic: Thrust forward
    pShip->SetOrder(O_THRUST, 20.0);
}

// Implements Groogroo's emergency jettison logic (Shared by all roles)
// This function is toroidally aware by using DetectCollisionCourse and AngleTo.
bool EvoBrain::HandleEmergencies() {
    CWorld *pmyWorld = pShip->GetWorld();
    CTeam *pmyTeam = pShip->GetTeam();
    double cur_cargo = pShip->GetAmount(S_CARGO);

    // If we have no cargo, no need to check for jettison
    if (cur_cargo < 0.01) return false;

    // Iterate through objects to check for imminent collisions with enemy stations
    for (unsigned int thing_i = pmyWorld->UFirstIndex; thing_i != BAD_INDEX; thing_i = pmyWorld->GetNextIndex(thing_i)) {
        CThing *athing = pmyWorld->GetThing(thing_i);
        if (!athing || !athing->IsAlive() || athing->GetKind() != STATION) continue;

        // IFF Check: Check if it's an enemy station
        if (athing->GetTeam() == pmyTeam) continue;

        // DetectCollisionCourse is toroidally aware.
        double turns = pShip->DetectCollisionCourse(*athing);
        if (turns < 0.0) continue;

        // Groogroo's thresholds: < 2.0 (imminent) and < 3.0 (near)
        if (turns < 3.0) {
            if (turns < 2.0) {
                // Emergency jettison
                pShip->SetJettison(VINYL, cur_cargo);
                return true; // Orders locked
            } else {
                // Turn away
                // AngleTo is toroidally aware.
                double angle_to_station = pShip->GetPos().AngleTo(athing->GetPos());
                double angle_away = angle_to_station + PI;
                
                // Normalize angle_away
                while (angle_away > PI) angle_away -= PI2;
                while (angle_away < -PI) angle_away += PI2;

                // Calculate the required turn delta (relative rotation)
                double turn_needed = angle_away - pShip->GetOrient();
                // Normalize turn_needed
                while (turn_needed > PI) turn_needed -= PI2;
                while (turn_needed < -PI) turn_needed += PI2;

                pShip->SetOrder(O_TURN, turn_needed);
                return true; // Orders locked
            }
        }
    }
    return false; // No emergencies locked the orders
}

// Implements Groogroo's shield maintenance procedure (Shared by all roles)
void EvoBrain::MaintainShields(double remaining_fuel_est) {
    double cur_shields = pShip->GetAmount(S_SHIELD);

    // Check if shields are below the desired buffer level
    if (cur_shields < cache_.MIN_SHIELD_LEVEL) {
        // Calculate how much fuel is available above the reserve
        double available_fuel = remaining_fuel_est - cache_.EMERGENCY_FUEL_RESERVE;

        if (available_fuel > 0.0) {
            double wanted_shields = cache_.MIN_SHIELD_LEVEL - cur_shields;
            // Determine the amount to boost: the lesser of what we want and what we can afford
            double shield_boost = std::min(wanted_shields, available_fuel);
            
            if (shield_boost > 0.01) {
                 pShip->SetOrder(O_SHIELD, shield_boost);
            }
        }
    }
}

// Helper to execute navigation orders
void EvoBrain::ExecuteOrders(const FuelTraj& ft) {
    pShip->SetOrder(ft.order_kind, ft.order_mag);
}

// Helper to calculate remaining fuel after issued orders
double EvoBrain::CalculateRemainingFuel() {
    double fuel_used_est = 0.0;
    
    // Movement estimates (O_THRUST and O_TURN are mutually exclusive)
    if (pShip->GetOrder(O_THRUST) != 0.0) {
        // Re-call SetOrder to get the engine's estimate
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


// --- GathererBrain Implementation ---

GathererBrain::GathererBrain(EvoAI* pTeam, ParamMap* params) : EvoBrain(pTeam, params) {}

void GathererBrain::Decide() {
    if (!pShip || !pShip->IsAlive()) return;

    pShip->ResetOrders();

    if (pShip->IsDocked()) {
        HandleDeparture();
        return;
    }

    // 1. Handle Emergencies (Jettison/Collision Avoidance)
    bool orders_locked = HandleEmergencies();

    // 2. Navigation and Resource Gathering
    if (!orders_locked) {
        NavigateAndGather();
    }

    // 3. Shield Maintenance
    MaintainShields(CalculateRemainingFuel());
}

// Implements Groogroo's target selection and navigation strategy (Gatherer specific)
// Update: Now handles both collectible and large (fragmentable) asteroids.
void GathererBrain::NavigateAndGather() {
    EvoAI* pEvoAI = pmyEvoTeam_;
    if (!pEvoAI || !pEvoAI->mb) return;

    MagicBag* mbp = pEvoAI->mb;
    unsigned int shipnum = pShip->GetShipNumber();
    double cur_fuel = pShip->GetAmount(S_FUEL);
    double cur_cargo = pShip->GetAmount(S_CARGO);

    // 1. Determine Priority: Return Home vs Gather
    if ((cur_cargo > cache_.RETURN_CARGO_THRESHOLD) || (pEvoAI->vinyl_left < 0.01 && cur_cargo > 0.01)) {
        // Return to station
        CStation* station = pShip->GetTeam()->GetStation();
        
        // Use the extended horizon for station return
        const int MAX_STATION_SEARCH = 50;
        for (int t = 1; t <= MAX_STATION_SEARCH; t++) {
            FuelTraj ft = pEvoAI->determine_orders(station, (double)t, pShip);
            if (ft.fuel_used >= 0.0) {
                ExecuteOrders(ft);
                return;
            }
        }
        // If no path found, continue below (e.g., search for fuel if needed).
    }

    // 2. Determine Preferred Resource: Fuel vs Vinyl
    bool prioritize_fuel = (cur_fuel <= cache_.LOW_FUEL_THRESHOLD && pEvoAI->uranium_left > 0.0) ||
                           (pEvoAI->vinyl_left < 0.01 && pEvoAI->uranium_left > 0.0);
    AsteroidKind preferred = prioritize_fuel ? URANIUM : VINYL;
    AsteroidKind secondary = prioritize_fuel ? VINYL : URANIUM;

    // 3. Select Best Target from MagicBag
    Entry *best_preferred = NULL;
    Entry *best_secondary = NULL;

    // Iterate through the MagicBag to find the shortest time intercept for both types.
    // We must iterate the whole bag because it is not globally sorted by time.
    for (unsigned int i = 0; ; ++i) {
        Entry* e = mbp->getEntry(shipnum, i);
        if (e == NULL) break; // End of list

        // Note: The targets in the MagicBag can now be large asteroids that don't fit.
        // The goal is to collide and break them.
        if (e->thing && e->thing->GetKind() == ASTEROID) {
            CAsteroid* asteroid = (CAsteroid*)e->thing;
            
            // Find shortest time for preferred
            if (asteroid->GetMaterial() == preferred) {
                if (best_preferred == NULL || e->turns_total < best_preferred->turns_total) {
                    best_preferred = e;
                }
            } 
            // Find shortest time for secondary
            else if (asteroid->GetMaterial() == secondary) {
                 if (best_secondary == NULL || e->turns_total < best_secondary->turns_total) {
                    best_secondary = e;
                }
            }
        }
    }

    // 4. Execute Orders
    // Take the preferred resource if available, otherwise fall back to the secondary resource.
    if (best_preferred != NULL) {
        ExecuteOrders(best_preferred->fueltraj);
    } else if (best_secondary != NULL) {
        // Ensure the secondary resource actually exists globally before pursuing it
        if ((secondary == VINYL && pEvoAI->vinyl_left > 0.01) || (secondary == URANIUM && pEvoAI->uranium_left > 0.01)) {
             ExecuteOrders(best_secondary->fueltraj);
        }
    }
}

// --- HunterBrain Implementation ---

HunterBrain::HunterBrain(EvoAI* pTeam, ParamMap* params) : EvoBrain(pTeam, params), pTarget(NULL) {}

void HunterBrain::Decide() {
    if (!pShip || !pShip->IsAlive()) return;

    pShip->ResetOrders();

    if (pShip->IsDocked()) {
        pTarget = NULL; // Clear target when docked
        HandleDeparture();
        return;
    }

    // 1. Handle Emergencies (Jettison near enemy base)
    bool orders_locked = HandleEmergencies();

    // 2. Combat Logic
    if (!orders_locked) {
        SelectTarget();
        NavigateAndEngage();
    }

    // 3. Shield Maintenance
    MaintainShields(CalculateRemainingFuel());
}

// Select the highest priority combat target (IFF aware)
// Confirmed: Ignores docked ships and empty stations.
void HunterBrain::SelectTarget() {
    // 1. Validate current target
    if (pTarget && pTarget->IsAlive()) {
        // IFF Check: Ensure target is still an enemy
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
            pTarget = NULL; // Friendly fire prevention or neutral object
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
            // DistTo is toroidally aware.
            double dist = pShip->GetPos().DistTo(thing->GetPos());

            if (thing->GetKind() == SHIP) {
                CShip* enemy = (CShip*)thing;
                if (enemy->IsDocked()) continue; // Skip docked ships

                score = 1000.0;
                // Prioritize ships carrying cargo
                if (enemy->GetAmount(S_CARGO) > 0.1) {
                    score += 500.0;
                }
            } else if (thing->GetKind() == STATION) {
                // Prioritize stations with vinyl reserves
                if (((CStation*)thing)->GetVinylStore() > 0.1) {
                    score = 1500.0;
                } else {
                    // Ignore stations with no vinyl
                    continue; 
                }
            }

            // Penalize distance
            score -= dist;

            if (score > best_score) {
                best_score = score;
                best_target = thing;
            }
        }
        pTarget = best_target;
    }
}

// Integrates ChromeFunk's engagement logic with Groogroo's navigation
void HunterBrain::NavigateAndEngage() {
    EvoAI* pEvoAI = pmyEvoTeam_;
    if (!pEvoAI || !pEvoAI->mb) return;

    // 1. Check if we need fuel (Hunters prioritize fuel over combat if low)
    double cur_fuel = pShip->GetAmount(S_FUEL);
    
    // Check for critical fuel level (Groogroo threshold) OR minimum combat fuel level
    bool low_fuel = (cur_fuel <= cache_.LOW_FUEL_THRESHOLD) || (cur_fuel <= cache_.COMBAT_MIN_FUEL_TO_HUNT);

    if (low_fuel && pEvoAI->uranium_left > 0.0) {
        // Search MagicBag for Uranium
        MagicBag* mbp = pEvoAI->mb;
        unsigned int shipnum = pShip->GetShipNumber();
        Entry *best_fuel = NULL;

        // Find the shortest time uranium intercept by iterating the whole bag.
        for (unsigned int i = 0; ; ++i) {
            Entry* e = mbp->getEntry(shipnum, i);
            if (e == NULL) break;
            // Note: We now include large asteroids here as well (to break them for fuel).
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

    // 2. Engage Target (If we have enough fuel or couldn't find any)
    if (!pTarget) return;

    // DistTo is toroidally aware.
    double distance = pShip->GetPos().DistTo(pTarget->GetPos());

    // ChromeFunk logic: If within engagement range, attempt to shoot
    if (distance <= cache_.COMBAT_ENGAGEMENT_RANGE) {
        if (AttemptToShoot(pTarget)) {
            return; // Shot taken
        }
    }

    // 3. If not shooting, navigate towards target (Stalking) using MagicBag
    // Search MagicBag for the best path to the target
    MagicBag* mbp = pEvoAI->mb;
    unsigned int shipnum = pShip->GetShipNumber();
    Entry *best_path = NULL;

    // Find the shortest time intercept for the specific target
    for (unsigned int i = 0; ; ++i) {
        Entry* e = mbp->getEntry(shipnum, i);
        if (e == NULL) break;
        
        if (e->thing == pTarget) {
            // Since PopulateMagicBag only adds the shortest path for this specific target, 
            // the first match found is the best path.
            best_path = e;
            break;
        }
    }

    if (best_path) {
        ExecuteOrders(best_path->fueltraj);
    }
}

// Implements ChromeFunk's shooting logic (lead calculation and firing)
// Toroidally aware: Uses PredictPosition and VectTo.
bool HunterBrain::AttemptToShoot(CThing* target) {
    // Final safety check: Ensure we aren't docked and the target is valid.
    if (!target || pShip->IsDocked()) return false;

    // Although SelectTarget ensures the target isn't docked, we add a redundant check here 
    // as a safeguard against shooting invulnerable docked ships.
    if (target->GetKind() == SHIP && ((CShip*)target)->IsDocked()) return false;


    // Lasers fire AFTER movement in the same turn. We need to predict where 
    // the target will be when the laser fires (1 second from now).

    // Predict both positions 1s ahead.
    CCoord MyPos = pShip->PredictPosition(g_game_turn_duration);
    CCoord TargPos = target->PredictPosition(g_game_turn_duration); 

    // VectTo handles toroidal wrapping, finding the shortest path vector to the future position.
    CTraj VectToTarget = MyPos.VectTo(TargPos);
    double target_angle = VectToTarget.theta;
    double distance = VectToTarget.rho;

    // Calculate the required turn delta based on current orientation
    double angle_error = target_angle - pShip->GetOrient();
    // Normalize angle error
    while (angle_error > PI) angle_error -= PI2;
    while (angle_error < -PI) angle_error += PI2;

    // Issue Turn order (This cancels any thrust/jettison orders)
    pShip->SetOrder(O_TURN, angle_error);

    // Calculate laser power (distance + overhead)
    double laser_power = distance + cache_.COMBAT_LASER_OVERHEAD;
    
    // Clamp max laser length (half the world size, typically 512.0)
    // Using constants for clarity. fWXMax/fWYMax are typically 512.0.
    double max_laser_range = std::min(fWXMax, fWYMax); 
    if (laser_power > max_laser_range) {
        laser_power = max_laser_range;
    }

    // Issue Laser order (engine checks fuel and clamps if necessary)
    pShip->SetOrder(O_LASER, laser_power);

    return true;
}