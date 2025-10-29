#!/usr/bin/env python3

import numpy as np
import subprocess
import os
import time
import sys
import signal
import re
import socket
import argparse
import shutil
import pickle
import concurrent.futures
import multiprocessing

# --- Configuration (Constants) ---

# Define the parameters (Genome) - UPDATED: Aligned with UnifiedBrain and Dynamic Strategy Architecture
PARAMETERS = {
    # --- Resource Management ---
    "LOW_FUEL_THRESHOLD": (2.0, 20.0),          # Fuel level triggering refueling priority
    "RETURN_CARGO_THRESHOLD": (5.0, 59.0),      # Cargo amount triggering return to base (Max 60)

    # --- Safety & Reserves ---
    "MIN_SHIELD_LEVEL": (5.0, 50.0),            # Minimum desired shield buffer
    "EMERGENCY_FUEL_RESERVE": (0.0, 15.0),      # Fuel kept aside (ignored during endgame/constrained)

    # --- Navigation ---
    "NAV_ALIGNMENT_THRESHOLD": (0.01, 0.3),     # Angle error tolerance (radians) for thrusting (approx 0.5 to 17 deg)

    # --- Team Composition & Configuration ---
    # Determines the physical configuration (fuel/cargo ratio) of the ships (0 to 4). Integer constraint applied later.
    "TEAM_NUM_HUNTERS_CONFIG": (0.0, 4.0),
    "GATHERER_CARGO_RATIO": (0.5, 0.9),         # High cargo focus for gatherers (Ratio of 60T)
    "HUNTER_CARGO_RATIO": (0.1, 0.5),           # Low cargo (high fuel) focus for hunters (Ratio of 60T)

    # --- Combat Tactics ---
    "COMBAT_ENGAGEMENT_RANGE": (100.0, 512.0),  # Distance to switch from navigation to shooting
    "COMBAT_MIN_FUEL_TO_HUNT": (5.0, 30.0),     # Minimum fuel required to actively pursue targets
    "COMBAT_LASER_EFFICIENCY_RATIO": (1.5, 5.0),# Desired Beam/Distance ratio (3.0 is fuel trade break-even)
    "COMBAT_OVERKILL_BUFFER": (0.0, 5.0),       # Extra damage buffer (in shield units)

    # --- Strategy ---
    "STRATEGY_ENDGAME_TURN": (250.0, 295.0)     # Turn number to trigger endgame logic (Max 300)
}


PARAM_KEYS = list(PARAMETERS.keys())
NUM_PARAMS = len(PARAMETERS.keys())

# GA Settings (Defaults)
DEFAULT_POPULATION_SIZE = 30
DEFAULT_NUM_GENERATIONS = 50
MUTATION_RATE = 0.15
MUTATION_STRENGTH = 0.2
CROSSOVER_RATE = 0.8
ELITISM_COUNT = 2
DEFAULT_GAMES_PER_EVAL = 3
DESIRED_TOURNAMENT_SIZE = 4

# File Paths and Executables
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# Assumes the script is run from the optimizer directory relative to the build dir
BUILD_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../build/"))

SERVER_EXEC = os.path.join(BUILD_DIR, "mm4serv")
OBSERVER_EXEC = os.path.join(BUILD_DIR, "mm4obs")
# Assuming EvoAI executable is named mm4team_evo in the build directory
EVO_AI_EXEC = os.path.join(BUILD_DIR, "mm4team_evo")
# Default opponent (e.g., baseline or reference team)
DEFAULT_OPPONENT_EXEC = os.path.join(BUILD_DIR, "mm4team")

# Path to graphics registry file required by the executables
GRAPHICS_REG_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "../../team/src/graphics.reg"))

# UPDATED: Must match the name set in EvoAI::Init()
TEAM_NAME = "EvoAI-Dynamic"
GAME_TIMEOUT = 240 # 4 minutes timeout for a single game simulation

# --- Helper Functions ---

def initialize_filenames(pid):
    """Initializes unique filenames based on the PID."""
    filenames = {
        'pid': pid,
        'history_log': f"optimization_history_pid{pid}.log",
        'checkpoint_file': f"EvoAI_checkpoint_pid{pid}.pkl",
        'param_file_best': f"EvoAI_params_best_pid{pid}.txt",
        # Prefix for temporary active files used by workers
        'active_prefix': f"EvoAI_params_active_pid{pid}"
    }
    return filenames

def find_free_port():
    """Finds a free TCP port."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        return s.getsockname()[1]

# UPDATED: Handles integer formatting
def save_parameters(params, filename):
    """Saves parameters to a specified file."""
    try:
        with open(filename, 'w') as f:
            for i, key in enumerate(PARAM_KEYS):
                value = params[i]
                # Format integers appropriately
                if key == "TEAM_NUM_HUNTERS_CONFIG":
                     f.write(f"{key} {int(value)}\n")
                else:
                    f.write(f"{key} {value}\n")
    except IOError as e:
        print(f"Warning: Could not save parameters to {filename}: {e}")

# UPDATED: Handles integer formatting
def log_history(filename, generation, stats, clamped_params):
    """Logs detailed statistics for the generation."""
    try:
        with open(filename, 'a') as f:
            # Format: Gen X, GenBest: Y.YY, AvgFit: A.AA, StdDev: S.SS, BestFit: B.BB, Params: ...
            f.write(f"Gen {generation}, GenBest: {stats['gen_best']:.2f}, AvgFit: {stats['avg']:.2f}, StdDev: {stats['std']:.2f}, BestFit: {stats['overall_best']:.2f}, Params: ")
            # Log the clamped parameters
            param_strings = []
            for i in range(NUM_PARAMS):
                key = PARAM_KEYS[i]
                value = clamped_params[i]
                # Format integers appropriately
                if key == "TEAM_NUM_HUNTERS_CONFIG":
                     param_strings.append(f"{key}: {int(value)}")
                else:
                    param_strings.append(f"{key}: {value:.4f}")

            f.write(", ".join(param_strings) + "\n")
    except IOError as e:
         print(f"Warning: Could not write to history log {filename}: {e}")


def parse_score(server_output, team_name):
    """Parses the final score (vinyl returned to station) from the mm4serv stdout."""
    # This metric is correct as the server reports the official score.
    pattern = re.escape(team_name) + r":\s*([\d\.]+)\s*vinyl"
    # Find all matches (P1 will be the first match in the server output)
    matches = re.findall(pattern, server_output)
    
    if matches:
        try:
            # Return the score of the first team listed (P1, the candidate)
            return float(matches[0])
        except ValueError:
            return 0.0
    return 0.0

def terminate_process_group(process):
    """Terminates a process and its children forcefully."""
    if process:
        try:
            if sys.platform == "win32":
                process.terminate()
            else:
                # Use process groups for reliable cleanup.
                # Check if process is still running before attempting to kill
                if process.poll() is None:
                    # Check if PID still exists before getting PGID
                    try:
                        pgid = os.getpgid(process.pid)
                    except ProcessLookupError:
                        return # Process already gone

                    os.killpg(pgid, signal.SIGTERM)
                    # Wait briefly for graceful termination
                    try:
                        process.wait(timeout=0.5)
                    except subprocess.TimeoutExpired:
                        # If it didn't terminate (unstable simulation), force kill
                        os.killpg(pgid, signal.SIGKILL)
        except (ProcessLookupError, OSError):
            pass # Process might already be dead

def check_executables(config):
    """Checks if all required executables exist."""
    if config.mode == 'pvb':
        opponent_exec = EVO_AI_EXEC
    else:
        opponent_exec = config.opponent_exec

    execs = [SERVER_EXEC, OBSERVER_EXEC, EVO_AI_EXEC, opponent_exec]
    missing = [exe for exe in execs if not os.path.exists(exe)]
    if missing:
        print(f"\nError: Missing required executables. Please verify paths (relative to script location: {SCRIPT_DIR}):")
        for missed in missing:
            print(f"  - {missed}")
        print(f"Ensure the project is built and executables are located in {BUILD_DIR}")
        sys.exit(1)
    if not os.path.exists(GRAPHICS_REG_PATH):
        print(f"\nError: Missing required graphics registry file: {GRAPHICS_REG_PATH}")
        sys.exit(1)

# UPDATED: Handles integer constraints and removes obsolete constraints.
def prepare_parameters(params):
    """Clamps parameters and enforces constraints."""
    clamped_params = []
    # Ensure params is treated correctly if it's a list or numpy array
    if not isinstance(params, np.ndarray):
        # Handle potential NoneType if worker failed completely
        if params is None:
            return [0.0] * NUM_PARAMS
        params = np.array(params)

    for i, key in enumerate(PARAM_KEYS):
        min_val, max_val = PARAMETERS[key]
        # Use np.clip on the individual element
        clamped_val = np.clip(params[i], min_val, max_val)
        
        # Apply Integer Constraint
        if key == "TEAM_NUM_HUNTERS_CONFIG":
            clamped_val = int(round(clamped_val))

        clamped_params.append(clamped_val)
    
    # Note: Previous constraints (e.g., NAV_ALIGNMENT_LOOSE_ANGLE >= NAV_ALIGNMENT_STRICT_ANGLE) 
    # are removed as those parameters no longer exist.

    return clamped_params

# --- Checkpointing ---

def save_checkpoint(filename, state):
    """Saves the current GA state to a pickle file atomically."""
    try:
        temp_filename = filename + ".tmp_save"
        with open(temp_filename, 'wb') as f:
            pickle.dump(state, f)
        # Ensure the move operation is robust
        if os.path.exists(temp_filename):
             shutil.move(temp_filename, filename)
    except Exception as e:
        print(f"Warning: Failed to save checkpoint {filename}: {e}")

def load_checkpoint(filename):
    """Loads the GA state from a pickle file."""
    if os.path.exists(filename):
        try:
            with open(filename, 'rb') as f:
                return pickle.load(f)
        except Exception as e:
            print(f"Error loading checkpoint file {filename}: {e}. Starting fresh.")
            return None
    return None

# --- Simulation (Worker Function) ---

def run_simulation(candidate_id, params, config, filenames):
    """Runs the game simulation in a separate process and returns the fitness score."""
    
    # 1. Prepare Parameters and Filenames
    clamped_params = prepare_parameters(params)
    # Create a unique parameter file for this worker instance
    param_filename = f"{filenames['active_prefix']}_worker{candidate_id}.txt"
    save_parameters(clamped_params, param_filename)
    
    port = find_free_port()
    server_process = None
    observer_process = None
    p1_process = None
    p2_process = None
    
    # Determine opponents based on mode
    p1_exec = EVO_AI_EXEC
    # Assuming the C++ main() parses the '-params' argument to set the parameter file.
    p1_args = [p1_exec, "-p", str(port), "-params", param_filename]

    if config.mode == 'pve':
        p2_exec = config.opponent_exec
        p2_args = [p2_exec, "-p", str(port)]
    elif config.mode == 'pvb':
        p2_exec = EVO_AI_EXEC 
        # P2 uses the best parameters found so far
        # Check if the file exists (it might not in Gen 0)
        if os.path.exists(filenames['param_file_best']):
             p2_args = [p2_exec, "-p", str(port), "-params", filenames['param_file_best']]
        else:
             # Fallback: use the current candidate's parameters if best is missing
             p2_args = [p2_exec, "-p", str(port), "-params", param_filename]
    else:
        # 'self' mode is handled in evaluate_population
        return 0.0

    # Set environment variables if needed (though args are preferred)
    env = os.environ.copy()

    try:
        # Use start_new_session=True to create a process group for reliable termination.
        kwargs = {'start_new_session': True} if sys.platform != "win32" else {}

        # 2. Launch Server
        # -t 300 ensures the game runs for the full duration.
        server_args = [SERVER_EXEC, "-p", str(port), "-t", "300", "-r", GRAPHICS_REG_PATH]
        server_process = subprocess.Popen(
            server_args, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE, 
            text=True,
            env=env,
            **kwargs
        )
        
        # Allow server time to initialize
        time.sleep(0.1) 
        
        # 3. Launch Observer (Required for the simulation loop to proceed)
        observer_args = [OBSERVER_EXEC, "-p", str(port), "-r", GRAPHICS_REG_PATH]
        observer_process = subprocess.Popen(
            observer_args,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=env,
            **kwargs
        )

        # 4. Launch Clients (Teams)
        # Add graphics path to client arguments as well
        p1_args.extend(["-r", GRAPHICS_REG_PATH])
        p2_args.extend(["-r", GRAPHICS_REG_PATH])

        p1_process = subprocess.Popen(
            p1_args, 
            stdout=subprocess.DEVNULL, 
            stderr=subprocess.DEVNULL,
            env=env,
            **kwargs
        )
        p2_process = subprocess.Popen(
            p2_args, 
            stdout=subprocess.DEVNULL, 
            stderr=subprocess.DEVNULL,
            env=env,
            **kwargs
        )
        
        # 5. Monitor Simulation
        try:
            # Wait for the server process to complete (which means the game ended)
            stdout, stderr = server_process.communicate(timeout=GAME_TIMEOUT)
            
            # 6. Calculate Fitness
            score = parse_score(stdout, TEAM_NAME)
            # Fitness is simply the score achieved
            fitness = score

        except subprocess.TimeoutExpired:
            # Handle simulation timeout
            fitness = 0.0
            
    except Exception as e:
        # print(f"Error running simulation for candidate {candidate_id}: {e}")
        fitness = 0.0
        
    finally:
        # 7. Cleanup
        # Terminate any remaining processes in the group
        terminate_process_group(p1_process)
        terminate_process_group(p2_process)
        terminate_process_group(observer_process)
        terminate_process_group(server_process)
        
        # Remove the temporary parameter file
        if os.path.exists(param_filename):
            try:
                os.remove(param_filename)
            except OSError:
                pass

    return fitness

# --- Genetic Algorithm Components ---

# UPDATED: Handles integer initialization
def initialize_population(size):
    """Initializes a population with random parameters within defined ranges."""
    population = []
    for _ in range(size):
        params = []
        for key in PARAM_KEYS:
            min_val, max_val = PARAMETERS[key]
            val = np.random.uniform(min_val, max_val)
            # Apply Integer Constraint immediately upon creation
            if key == "TEAM_NUM_HUNTERS_CONFIG":
                val = int(round(val))
            params.append(val)
        population.append(params)
    return np.array(population)

def evaluate_population(population, config, filenames):
    """Evaluates the fitness of the entire population using parallel execution."""
    fitness_scores = np.zeros(len(population))
    
    # Determine the number of workers
    num_workers = config.workers if config.workers > 0 else multiprocessing.cpu_count()

    # Use ThreadPoolExecutor for managing the external processes (as in the original snippet)
    # ProcessPoolExecutor could also be used here.
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_workers) as executor:
        future_to_index = {}

        # Handle 'self' play mode (Tournament style)
        if config.mode == 'self':
            print("Warning: 'self' play mode not fully implemented. Falling back to PVE evaluation.")
            config.mode = 'pve'

        # Handle PVE/PVB modes
        if config.mode in ['pve', 'pvb']:
            for i, params in enumerate(population):
                # Run multiple games per evaluation to average out randomness
                for game_num in range(config.games_per_eval):
                    # Create a unique ID for the task
                    task_id = f"{i}_{game_num}"
                    future = executor.submit(run_simulation, task_id, params, config, filenames)
                    # Store the population index associated with the future
                    if i not in future_to_index:
                        future_to_index[i] = []
                    future_to_index[i].append(future)

        # Collect results
        for i in future_to_index:
            scores = []
            for future in future_to_index[i]:
                try:
                    scores.append(future.result())
                except Exception as e:
                    print(f"Error collecting result for individual {i}: {e}")
                    scores.append(0.0)

            # Average the scores from the multiple games
            if scores:
                fitness_scores[i] = np.mean(scores)

    return fitness_scores

def selection(population, fitness_scores, tournament_size=DESIRED_TOURNAMENT_SIZE):
    """Selects parents using tournament selection."""
    pop_size = len(population)
    # Ensure tournament size is practical
    tournament_size = min(tournament_size, pop_size)
    
    parents = []
    for _ in range(pop_size):
        # Randomly select candidates for the tournament
        indices = np.random.choice(pop_size, tournament_size, replace=False)
        # Find the best candidate among the selected indices
        best_idx = indices[np.argmax(fitness_scores[indices])]
        parents.append(population[best_idx])
    return np.array(parents)

def crossover(parents, crossover_rate=CROSSOVER_RATE):
    """Performs uniform crossover."""
    children = []
    pop_size = len(parents)
    
    for i in range(0, pop_size, 2):
        # Handle odd population size
        if i + 1 >= pop_size:
            children.append(parents[i])
            break
            
        p1 = parents[i]
        p2 = parents[i+1]
        
        c1 = p1.copy()
        c2 = p2.copy()
        
        if np.random.rand() < crossover_rate:
            # Uniform crossover mask
            mask = np.random.randint(0, 2, size=NUM_PARAMS).astype(bool)
            c1[mask] = p2[mask]
            c2[mask] = p1[mask]
            
        children.append(c1)
        children.append(c2)
        
    return np.array(children)

# UPDATED: Handles integer constraints during mutation
def mutation(children, mutation_rate=MUTATION_RATE, mutation_strength=MUTATION_STRENGTH):
    """Applies Gaussian mutation."""
    for child in children:
        for i in range(NUM_PARAMS):
            if np.random.rand() < mutation_rate:
                key = PARAM_KEYS[i]
                min_val, max_val = PARAMETERS[key]
                # Calculate mutation amount based on the parameter's range
                range_size = max_val - min_val
                # Apply Gaussian noise scaled by strength and range
                noise = np.random.normal(0, mutation_strength * range_size)
                child[i] += noise
                
                # Clamping and integer constraints are applied here for immediate validity
                child[i] = np.clip(child[i], min_val, max_val)
                if key == "TEAM_NUM_HUNTERS_CONFIG":
                    child[i] = int(round(child[i]))

    return children

# --- Main Optimization Loop ---

def run_optimizer(config):
    """The main genetic algorithm optimization loop."""
    
    # Initialize
    pid = os.getpid()
    filenames = initialize_filenames(pid)
    print(f"Starting optimization (PID: {pid})")
    print(f"Mode: {config.mode}, Generations: {config.generations}, Population: {config.population}, Workers: {config.workers}")
    
    check_executables(config)

    population = initialize_population(config.population)
    start_generation = 0
    best_fitness = -np.inf
    best_params = None

    # Load checkpoint if exists
    if config.resume:
        state = load_checkpoint(filenames['checkpoint_file'])
        if state:
            population = state['population']
            start_generation = state['generation'] + 1
            best_fitness = state['best_fitness']
            best_params = state['best_params']
            print(f"Resuming from generation {start_generation} with best fitness {best_fitness:.2f}")
            # Ensure the best parameter file is consistent with the loaded state
            if best_params is not None:
                save_parameters(best_params, filenames['param_file_best'])
        else:
             print("Resume requested but no valid checkpoint found. Starting fresh.")

    if start_generation == 0:
        # Clear previous history log if starting fresh
        if os.path.exists(filenames['history_log']):
            os.remove(filenames['history_log'])


    if start_generation >= config.generations:
        print("Optimization already completed.")
        return

    # Main Loop
    for generation in range(start_generation, config.generations):
        start_time = time.time()
        print(f"\n--- Generation {generation} ---")
        
        # Evaluate
        # Use a copy of config for evaluation to allow dynamic mode switching (e.g., PVB fallback)
        eval_config = argparse.Namespace(**vars(config))
        fitness_scores = evaluate_population(population, eval_config, filenames)
        
        # Analyze Generation Results
        gen_best_idx = np.argmax(fitness_scores)
        gen_best_fitness = fitness_scores[gen_best_idx]
        avg_fitness = np.mean(fitness_scores)
        std_fitness = np.std(fitness_scores)

        print(f"Avg Fitness: {avg_fitness:.2f} | Std Dev: {std_fitness:.2f} | Best this Gen: {gen_best_fitness:.2f}")

        # Update Overall Best
        if gen_best_fitness > best_fitness:
            best_fitness = gen_best_fitness
            # Ensure parameters are clamped before saving as best
            best_params = prepare_parameters(population[gen_best_idx])
            save_parameters(best_params, filenames['param_file_best'])
            print(f"New overall best fitness found: {best_fitness:.2f}")

        # Logging
        stats = {
            'gen_best': gen_best_fitness,
            'avg': avg_fitness,
            'std': std_fitness,
            'overall_best': best_fitness
        }
        # Log the parameters corresponding to the overall best found so far
        if best_params is not None:
            # Ensure the logged parameters are also the clamped version
            clamped_best_params = prepare_parameters(best_params)
            log_history(filenames['history_log'], generation, stats, clamped_best_params)

        # Create next generation
        next_population = []
        
        # Elitism: Carry over the best individuals
        if ELITISM_COUNT > 0:
            elite_indices = np.argsort(fitness_scores)[-ELITISM_COUNT:]
            for idx in elite_indices:
                 # Ensure elites are also clamped (important if constraints changed)
                next_population.append(prepare_parameters(population[idx]))

        # Selection, Crossover, Mutation
        # Adjust tournament size based on current population size
        current_tournament_size = min(DESIRED_TOURNAMENT_SIZE, len(population))

        # Ensure parents are clamped versions
        clamped_population = np.array([prepare_parameters(p) for p in population])

        parents = selection(clamped_population, fitness_scores, tournament_size=current_tournament_size)
        # Generate enough children to fill the remaining slots
        children_needed = config.population - len(next_population)
        children = crossover(parents[:children_needed])
        children = mutation(children)
        
        next_population.extend(children)
        population = np.array(next_population)

        # Checkpointing
        state = {
            'generation': generation,
            'population': population,
            'best_fitness': best_fitness,
            'best_params': best_params
        }
        save_checkpoint(filenames['checkpoint_file'], state)

        end_time = time.time()
        print(f"Generation {generation} completed in {end_time - start_time:.2f} seconds.")

    print("\nOptimization finished.")
    print(f"Best fitness achieved: {best_fitness:.2f}")
    print(f"Best parameters saved to: {filenames['param_file_best']}")

if __name__ == "__main__":
    # Set start method for multiprocessing (required for stability on some OSes)
    try:
        # Use 'spawn' for safety when launching external processes
        multiprocessing.set_start_method('spawn', force=True)
    except RuntimeError:
        pass

    parser = argparse.ArgumentParser(description="Genetic Algorithm Optimizer for EvoAI")
    parser.add_argument('-g', '--generations', type=int, default=DEFAULT_NUM_GENERATIONS, help='Number of generations')
    parser.add_argument('-p', '--population', type=int, default=DEFAULT_POPULATION_SIZE, help='Population size')
    parser.add_argument('-m', '--mode', choices=['pve', 'pvb', 'self'], default='pve', help='Optimization mode: pve (vs baseline), pvb (vs current best), self (self-play - experimental)')
    parser.add_argument('-o', '--opponent_exec', type=str, default=DEFAULT_OPPONENT_EXEC, help='Path to the opponent executable (for pve mode)')
    parser.add_argument('-w', '--workers', type=int, default=0, help='Number of parallel workers (0 = CPU count)')
    parser.add_argument('-n', '--games_per_eval', type=int, default=DEFAULT_GAMES_PER_EVAL, help='Number of games run per individual evaluation')
    parser.add_argument('--resume', action='store_true', help='Resume from the latest checkpoint')
    
    args = parser.parse_args()
    
    # Handle relative paths for opponent executable
    if not os.path.isabs(args.opponent_exec):
        args.opponent_exec = os.path.abspath(os.path.join(SCRIPT_DIR, args.opponent_exec))

    run_optimizer(args)