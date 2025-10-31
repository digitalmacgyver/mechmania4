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
from datetime import datetime

# --- Configuration (Constants) ---

# Define the parameters (Genome)
PARAMETERS = {
    # --- Resource Management ---
    "LOW_FUEL_THRESHOLD": (2.0, 20.0),
    "RETURN_CARGO_THRESHOLD": (5.0, 59.0),

    # --- Safety & Reserves ---
    "MIN_SHIELD_LEVEL": (5.0, 50.0),
    "EMERGENCY_FUEL_RESERVE": (0.0, 15.0),

    # --- Navigation ---
    "NAV_ALIGNMENT_THRESHOLD": (0.01, 0.3),

    # --- Team Composition & Configuration ---
    "TEAM_NUM_HUNTERS_CONFIG": (0.0, 4.0),
    "GATHERER_CARGO_RATIO": (0.5, 0.9),
    "HUNTER_CARGO_RATIO": (0.1, 0.5),

    # --- Combat Tactics ---
    "COMBAT_ENGAGEMENT_RANGE": (100.0, 512.0),
    "COMBAT_MIN_FUEL_TO_HUNT": (5.0, 30.0),
    "COMBAT_LASER_EFFICIENCY_RATIO": (1.5, 5.0),
    "COMBAT_OVERKILL_BUFFER": (0.0, 5.0),

    # --- Strategy ---
    "STRATEGY_ENDGAME_TURN": (250.0, 295.0)
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

# File Paths, Executables, and Logging
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../build/"))

# NEW: Output Directory for results (logs, checkpoints, best params, active params)
# Defined relative to the Current Working Directory (CWD)
OUTPUT_DIR = os.path.abspath(os.path.join(os.getcwd(), "output/"))


# Logging Configuration (Simulation Logs - stdout/stderr of binaries)
# Prefer /tmp if available and writable, otherwise use a local 'ga_logs' directory
if os.path.exists("/tmp") and os.access("/tmp", os.W_OK):
    LOG_DIR = "/tmp/evo_ga_logs/"
else:
    # Fallback relative to SCRIPT_DIR if /tmp is unavailable
    LOG_DIR = os.path.join(SCRIPT_DIR, "ga_logs/")


SERVER_EXEC = os.path.join(BUILD_DIR, "mm4serv")
OBSERVER_EXEC = os.path.join(BUILD_DIR, "mm4obs")
EVO_AI_EXEC = os.path.join(BUILD_DIR, "mm4team_evo")
DEFAULT_OPPONENT_EXEC = os.path.join(BUILD_DIR, "mm4team")

GRAPHICS_REG_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "../../team/src/graphics.reg"))

TEAM_NAME = "EvoAI-Dynamic"
GAME_TIMEOUT = 240 # 4 minutes timeout

# --- Helper Functions ---

def initialize_environment():
    """Initializes the environment, including creating log and output directories."""
    # Create Log Directory
    if not os.path.exists(LOG_DIR):
        try:
            os.makedirs(LOG_DIR)
            print(f"Created logging directory: {LOG_DIR}")
        except OSError as e:
            print(f"Error: Failed to create logging directory {LOG_DIR}: {e}")
            sys.exit(1)
    
    # Create Output Directory
    if not os.path.exists(OUTPUT_DIR):
        try:
            os.makedirs(OUTPUT_DIR)
            print(f"Created output directory: {OUTPUT_DIR}")
        except OSError as e:
            print(f"Error: Failed to create output directory {OUTPUT_DIR}: {e}")
            sys.exit(1)

def initialize_filenames(pid):
    """Initializes unique filenames based on the PID, located in OUTPUT_DIR."""
    # Prepend OUTPUT_DIR to the filenames
    filenames = {
        'pid': pid,
        'history_log': os.path.join(OUTPUT_DIR, f"optimization_history_pid{pid}.log"),
        'checkpoint_file': os.path.join(OUTPUT_DIR, f"EvoAI_checkpoint_pid{pid}.pkl"),
        'param_file_best': os.path.join(OUTPUT_DIR, f"EvoAI_params_best_pid{pid}.txt"),
        # Active prefix is used for temporary files during simulation
        'active_prefix': os.path.join(OUTPUT_DIR, f"EvoAI_params_active_pid{pid}")
    }
    return filenames

def find_free_port():
    """Finds a free TCP port."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        return s.getsockname()[1]

def save_parameters(params, filename):
    """Saves parameters to a specified file."""
    try:
        with open(filename, 'w') as f:
            for i, key in enumerate(PARAM_KEYS):
                value = params[i]
                if key == "TEAM_NUM_HUNTERS_CONFIG":
                     f.write(f"{key} {int(value)}\n")
                else:
                    f.write(f"{key} {value}\n")
    except IOError as e:
        print(f"Warning: Could not save parameters to {filename}: {e}")

def log_history(filename, generation, stats, clamped_params):
    """Logs detailed statistics for the generation."""
    try:
        with open(filename, 'a') as f:
            f.write(f"Gen {generation}, GenBest: {stats['gen_best']:.2f}, AvgFit: {stats['avg']:.2f}, StdDev: {stats['std']:.2f}, BestFit: {stats['overall_best']:.2f}, Params: ")
            param_strings = []
            for i in range(NUM_PARAMS):
                key = PARAM_KEYS[i]
                value = clamped_params[i]
                if key == "TEAM_NUM_HUNTERS_CONFIG":
                     param_strings.append(f"{key}: {int(value)}")
                else:
                    param_strings.append(f"{key}: {value:.4f}")

            f.write(", ".join(param_strings) + "\n")
    except IOError as e:
         print(f"Warning: Could not write to history log {filename}: {e}")


def parse_score_from_file(log_filepath, team_name):
    """Parses the final score from the server log file and checks for game completion."""
    pattern = re.escape(team_name) + r":\s*([\d\.]+)\s*vinyl"
    score = 0.0
    game_over_seen = False

    try:
        with open(log_filepath, 'r') as f:
            content = f.read()
            
            if "Game Over" in content:
                game_over_seen = True

            matches = re.findall(pattern, content)
            if matches:
                try:
                    # Return the score of the first team listed (P1)
                    score = float(matches[0])
                except ValueError:
                    pass

    except IOError:
        return 0.0, False # Failed to read log

    # If Game Over was not seen, the simulation didn't complete properly.
    if not game_over_seen:
        return 0.0, False
        
    return score, True

def terminate_process_group(process):
    """Terminates a process and its children forcefully."""
    if process:
        try:
            if sys.platform == "win32":
                process.terminate()
            else:
                if process.poll() is None:
                    try:
                        pgid = os.getpgid(process.pid)
                        os.killpg(pgid, signal.SIGTERM)
                    except ProcessLookupError:
                        return # Process already gone

                    try:
                        process.wait(timeout=0.5)
                    except subprocess.TimeoutExpired:
                        # Force kill if graceful termination fails
                        try:
                             os.killpg(pgid, signal.SIGKILL)
                        except ProcessLookupError:
                             pass
        except (ProcessLookupError, OSError):
            pass

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

def prepare_parameters(params):
    """Clamps parameters and enforces constraints."""
    clamped_params = []
    if params is None:
        return [0.0] * NUM_PARAMS
    if not isinstance(params, np.ndarray):
        params = np.array(params)

    for i, key in enumerate(PARAM_KEYS):
        min_val, max_val = PARAMETERS[key]
        clamped_val = np.clip(params[i], min_val, max_val)
        
        if key == "TEAM_NUM_HUNTERS_CONFIG":
            clamped_val = int(round(clamped_val))

        clamped_params.append(clamped_val)
    
    return clamped_params

# NEW: Function to load seed parameters
def load_seed_parameters(filepath):
    """Loads parameters from a file and converts them into a list matching PARAM_KEYS order."""
    seed_dict = {}
    # Ensure the path is absolute
    abs_filepath = os.path.abspath(filepath)

    print(f"Attempting to load seed file from: {abs_filepath}")
    try:
        with open(abs_filepath, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) == 2:
                    key, value = parts[0], parts[1]
                    if key in PARAMETERS:
                        seed_dict[key] = float(value)
    except IOError as e:
        print(f"Error loading seed parameters from {abs_filepath}: {e}. Exiting.")
        sys.exit(1)

    if len(seed_dict) != NUM_PARAMS:
        print(f"Error: Seed file {abs_filepath} does not contain all required parameters. Found {len(seed_dict)}, expected {NUM_PARAMS}. Exiting.")
        sys.exit(1)

    # Convert dictionary to ordered list
    seed_list = []
    for key in PARAM_KEYS:
        val = seed_dict[key]
        # Ensure integer constraints are applied correctly when loading
        if key == "TEAM_NUM_HUNTERS_CONFIG":
             val = int(round(val))
        seed_list.append(val)
    
    return seed_list

# --- Checkpointing ---

def save_checkpoint(filename, state):
    """Saves the current GA state to a pickle file atomically."""
    try:
        temp_filename = filename + ".tmp_save"
        with open(temp_filename, 'wb') as f:
            pickle.dump(state, f)
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
    
    # 1. Prepare Parameters
    clamped_params = prepare_parameters(params)
    # Active parameter files are now located in OUTPUT_DIR
    param_filename = f"{filenames['active_prefix']}_worker{candidate_id}.txt"
    save_parameters(clamped_params, param_filename)
    
    port = find_free_port()
    
    # 2. Setup Logging Files
    timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
    log_prefix = f"evo-pid{filenames['pid']}-cid{candidate_id}-{timestamp}"
    
    log_files = {
        'server': os.path.join(LOG_DIR, f"{log_prefix}-mm4serv.log"),
        'observer': os.path.join(LOG_DIR, f"{log_prefix}-mm4obs.log"),
        'p1': os.path.join(LOG_DIR, f"{log_prefix}-p1_evo.log"),
        'p2': os.path.join(LOG_DIR, f"{log_prefix}-p2_opp.log"),
    }
    
    log_handles = {}
    try:
        for key, filepath in log_files.items():
            # Open in write mode (defaults to text mode)
            log_handles[key] = open(filepath, 'w')
    except IOError as e:
        print(f"Error: Failed to open log file for simulation {candidate_id}: {e}")
        # Clean up already opened handles
        for handle in log_handles.values():
            handle.close()
        return 0.0

    # 3. Define Processes and Arguments
    processes = {'server': None, 'observer': None, 'p1': None, 'p2': None}

    p1_exec = EVO_AI_EXEC
    p1_args = [p1_exec, "-p", str(port), "--params", param_filename]

    if config.mode == 'pve':
        p2_exec = config.opponent_exec
        p2_args = [p2_exec, "-p", str(port)]
    elif config.mode == 'pvb':
        p2_exec = EVO_AI_EXEC
        if os.path.exists(filenames['param_file_best']):
             p2_args = [p2_exec, "-p", str(port), "--params", filenames['param_file_best']]
        else:
             p2_args = [p2_exec, "-p", str(port), "--params", param_filename]
    else:
        # Clean up handles if mode is invalid
        for handle in log_handles.values():
            handle.close()
        return 0.0

    env = os.environ.copy()
    fitness = 0.0
    simulation_failed = False

    try:
        kwargs = {'start_new_session': True} if sys.platform != "win32" else {}

        # 4. Launch Processes (redirecting output to logs)
        
        # Server
        server_args = [SERVER_EXEC, "-p", str(port), "--max-turns", "300", "-g", GRAPHICS_REG_PATH]
        # Redirect stdout/stderr to the log file.
        processes['server'] = subprocess.Popen(
            server_args,
            stdout=log_handles['server'],
            stderr=log_handles['server'],
            env=env,
            **kwargs
        )

        # Allow server time to initialize
        time.sleep(0.5)

        # Observer
        observer_args = [OBSERVER_EXEC, "-p", str(port), "-g", GRAPHICS_REG_PATH, "--mute", "--audio-lead-ms", "0"]
        processes['observer'] = subprocess.Popen(
            observer_args,
            stdout=log_handles['observer'],
            stderr=log_handles['observer'],
            env=env,
            **kwargs
        )

        # Clients (no need to add graphics registry, they don't use it)
        processes['p1'] = subprocess.Popen(
            p1_args,
            stdout=log_handles['p1'],
            stderr=log_handles['p1'],
            env=env,
            **kwargs
        )
        processes['p2'] = subprocess.Popen(
            p2_args,
            stdout=log_handles['p2'],
            stderr=log_handles['p2'],
            env=env,
            **kwargs
        )
        
        # 5. Monitor Simulation
        try:
            # Wait for the server process to complete
            processes['server'].wait(timeout=GAME_TIMEOUT)

            # Check return codes for errors
            # Wait briefly for clients/observer to finish after server exits
            time.sleep(0.2) 

            # Define expected signals for graceful termination
            expected_termination_codes = [0]
            if sys.platform != "win32":
                # Include codes resulting from SIGTERM if termination was graceful
                expected_termination_codes.append(-signal.SIGTERM)


            for name_key in processes.keys():
                proc = processes[name_key]
                if proc:
                    # Ensure process has terminated and get return code
                    if proc.poll() is None:
                        proc.terminate()
                        try:
                            proc.wait(timeout=1)
                        except subprocess.TimeoutExpired:
                            pass # Process termination will be handled by terminate_process_group

                    if proc.returncode not in expected_termination_codes and proc.returncode is not None:
                         print(f"Warning: Process '{name_key}' (ID {candidate_id}) exited with code {proc.returncode}. Logs retained: {log_files[name_key]}")
                         simulation_failed = True

        except subprocess.TimeoutExpired:
            print(f"Warning: Simulation Timeout (ID {candidate_id}). Logs retained.")
            simulation_failed = True
            
    except Exception as e:
        print(f"Error launching simulation for candidate {candidate_id}: {e}. Logs retained.")
        simulation_failed = True
        
    finally:
        # 6. Cleanup Processes
        for proc in processes.values():
            terminate_process_group(proc)
        
        # 7. Close Log Handles
        # Ensure all output is flushed before closing
        for handle in log_handles.values():
            try:
                handle.flush()
                handle.close()
            except Exception:
                pass

        # 8. Parse Score
        # Read the server log to get the score.
        try:
            fitness, game_completed = parse_score_from_file(log_files['server'], TEAM_NAME)
            
            if not game_completed and not simulation_failed:
                 print(f"Warning: 'Game Over' not found in server output for candidate {candidate_id}. Simulation may have crashed early. Check log: {log_files['server']}")
                 simulation_failed = True
            
        except Exception as e:
            print(f"Error during score parsing for {candidate_id}: {e}")
            simulation_failed = True

        # 9. Log Retention Policy
        if not simulation_failed and not config.keep_all_logs:
            # If successful and we don't want to keep logs, delete them
            for path in log_files.values():
                 if os.path.exists(path):
                    try:
                        os.remove(path)
                    except OSError:
                        pass

        # 10. Remove the temporary parameter file
        if os.path.exists(param_filename):
            try:
                os.remove(param_filename)
            except OSError:
                pass

    return fitness

# --- Genetic Algorithm Components ---

# UPDATED: Added seed_params argument
def initialize_population(size, seed_params=None):
    """Initializes a population, optionally seeding the first individual."""
    population = []
    
    # Add the seed first if provided
    if seed_params is not None:
        print(f"Seeding individual 0 with provided parameters.")
        # Ensure the seed is clamped/valid before adding (although load_seed_parameters also does this)
        clamped_seed = prepare_parameters(seed_params)
        population.append(clamped_seed)
        start_index = 1
    else:
        start_index = 0

    # Initialize the rest randomly
    for _ in range(start_index, size):
        params = []
        for key in PARAM_KEYS:
            min_val, max_val = PARAMETERS[key]
            val = np.random.uniform(min_val, max_val)
            if key == "TEAM_NUM_HUNTERS_CONFIG":
                val = int(round(val))
            params.append(val)
        population.append(params)
        
    return np.array(population)

# (evaluate_population, selection, crossover, mutation remain the same)

def evaluate_population(population, config, filenames):
    """Evaluates the fitness of the entire population using parallel execution."""
    fitness_scores = np.zeros(len(population))
    
    num_workers = config.workers if config.workers > 0 else multiprocessing.cpu_count()

    with concurrent.futures.ThreadPoolExecutor(max_workers=num_workers) as executor:
        future_to_index = {}

        if config.mode == 'self':
            print("Warning: 'self' play mode not fully implemented. Falling back to PVE evaluation.")
            config.mode = 'pve'

        if config.mode in ['pve', 'pvb']:
            for i, params in enumerate(population):
                for game_num in range(config.games_per_eval):
                    task_id = f"{i}_{game_num}"
                    future = executor.submit(run_simulation, task_id, params, config, filenames)
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

            if scores:
                # Filter out potential NaN/inf scores if any occurred
                valid_scores = [s for s in scores if np.isfinite(s)]
                if valid_scores:
                    fitness_scores[i] = np.mean(valid_scores)

    return fitness_scores

def selection(population, fitness_scores, tournament_size=DESIRED_TOURNAMENT_SIZE):
    """Selects parents using tournament selection."""
    pop_size = len(population)
    if pop_size == 0: return np.array([])
    tournament_size = min(tournament_size, pop_size)
    
    parents = []
    for _ in range(pop_size):
        indices = np.random.choice(pop_size, tournament_size, replace=False)
        # Handle potential NaNs in fitness scores during selection
        tournament_fitness = fitness_scores[indices]
        if np.all(np.isnan(tournament_fitness)):
             best_idx = indices[0] # If all are NaN, pick randomly
        else:
            best_idx = indices[np.nanargmax(tournament_fitness)]
        parents.append(population[best_idx])
    return np.array(parents)

def crossover(parents, crossover_rate=CROSSOVER_RATE):
    """Performs uniform crossover."""
    children = []
    pop_size = len(parents)
    
    for i in range(0, pop_size, 2):
        if i + 1 >= pop_size:
            if i < pop_size:
                children.append(parents[i])
            break
            
        p1 = parents[i]
        p2 = parents[i+1]
        
        c1 = p1.copy()
        c2 = p2.copy()
        
        if np.random.rand() < crossover_rate:
            mask = np.random.randint(0, 2, size=NUM_PARAMS).astype(bool)
            c1[mask] = p2[mask]
            c2[mask] = p1[mask]
            
        children.append(c1)
        children.append(c2)
        
    return np.array(children)

def mutation(children, mutation_rate=MUTATION_RATE, mutation_strength=MUTATION_STRENGTH):
    """Applies Gaussian mutation."""
    for child in children:
        for i in range(NUM_PARAMS):
            if np.random.rand() < mutation_rate:
                key = PARAM_KEYS[i]
                min_val, max_val = PARAMETERS[key]
                range_size = max_val - min_val
                noise = np.random.normal(0, mutation_strength * range_size)
                child[i] += noise
                
                child[i] = np.clip(child[i], min_val, max_val)
                if key == "TEAM_NUM_HUNTERS_CONFIG":
                    child[i] = int(round(child[i]))

    return children

# --- Main Optimization Loop ---

def run_optimizer(config):
    """The main genetic algorithm optimization loop."""
    
    # Initialize environment (create log/output dirs)
    initialize_environment()

    # Initialize filenames
    pid = os.getpid()
    filenames = initialize_filenames(pid)
    print(f"Starting optimization (PID: {pid})")
    print(f"Mode: {config.mode}, Generations: {config.generations}, Population: {config.population}, Workers: {config.workers}")
    print(f"Primary output (logs, checkpoints, results) will be saved to: {OUTPUT_DIR}")
    print(f"Simulation logs (binary stdout/stderr) will be saved to: {LOG_DIR}")

    
    check_executables(config)

    # Initialize state variables
    population = None
    start_generation = 0
    best_fitness = -np.inf
    best_params = None

    # Load checkpoint
    if config.resume:
        state = load_checkpoint(filenames['checkpoint_file'])
        if state:
            population = state['population']
            start_generation = state['generation'] + 1
            best_fitness = state['best_fitness']
            best_params = state['best_params']
            print(f"Resuming from generation {start_generation} with best fitness {best_fitness:.2f}")
            if config.seed_file:
                 print("Warning: --resume detected. Ignoring --seed_file.")
            if best_params is not None:
                save_parameters(best_params, filenames['param_file_best'])
        else:
             print("Resume requested but no valid checkpoint found. Starting fresh (or using seed if provided).")

    # Initialize population if not loaded from checkpoint
    if population is None:
        # Use the seed here if provided
        seed_params_list = None
        if config.seed_file:
             seed_params_list = load_seed_parameters(config.seed_file)
             
        population = initialize_population(config.population, seed_params=seed_params_list)


    if start_generation == 0:
        if os.path.exists(filenames['history_log']):
            try:
                os.remove(filenames['history_log'])
            except OSError:
                pass


    if start_generation >= config.generations:
        print("Optimization already completed.")
        return

    # Main Loop
    for generation in range(start_generation, config.generations):
        start_time = time.time()
        print(f"\n--- Generation {generation} ---")
        
        # Evaluate
        eval_config = argparse.Namespace(**vars(config))
        fitness_scores = evaluate_population(population, eval_config, filenames)
        
        # Analyze Generation Results
        if len(fitness_scores) == 0 or np.all(np.isnan(fitness_scores)):
             print("Error: All evaluations failed or returned NaN in this generation. Check logs and configuration.")
             break

        # Filter out NaNs for statistical analysis
        valid_fitness = fitness_scores[np.isfinite(fitness_scores)]
        if len(valid_fitness) == 0:
             avg_fitness, std_fitness = 0.0, 0.0
        else:
            avg_fitness = np.mean(valid_fitness)
            std_fitness = np.std(valid_fitness)

        gen_best_idx = np.nanargmax(fitness_scores)
        gen_best_fitness = fitness_scores[gen_best_idx]


        print(f"Avg Fitness: {avg_fitness:.2f} | Std Dev: {std_fitness:.2f} | Best this Gen: {gen_best_fitness:.2f}")

        # Update Overall Best
        if gen_best_fitness > best_fitness:
            best_fitness = gen_best_fitness
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
        if best_params is not None:
            clamped_best_params = prepare_parameters(best_params)
            log_history(filenames['history_log'], generation, stats, clamped_best_params)

        # Create next generation
        next_population = []
        
        # Elitism
        if ELITISM_COUNT > 0:
            # Use argsort prioritizing finite values
            elite_indices = np.argsort(np.where(np.isfinite(fitness_scores), fitness_scores, -np.inf))[-ELITISM_COUNT:]
            for idx in elite_indices:
                next_population.append(prepare_parameters(population[idx]))

        # Selection, Crossover, Mutation
        current_tournament_size = min(DESIRED_TOURNAMENT_SIZE, len(population))
        clamped_population = np.array([prepare_parameters(p) for p in population])

        parents = selection(clamped_population, fitness_scores, tournament_size=current_tournament_size)
        
        if len(parents) == 0:
            print("Warning: Selection failed to produce parents. Skipping generation creation.")
            break

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
    try:
        # Use 'spawn' for safety when launching external processes
        multiprocessing.set_start_method('spawn', force=True)
    except RuntimeError:
        pass

    parser = argparse.ArgumentParser(description="Genetic Algorithm Optimizer for EvoAI")
    parser.add_argument('-g', '--generations', type=int, default=DEFAULT_NUM_GENERATIONS, help='Number of generations')
    parser.add_argument('-p', '--population', type=int, default=DEFAULT_POPULATION_SIZE, help='Population size')
    parser.add_argument('-m', '--mode', choices=['pve', 'pvb', 'self'], default='pve', help='Optimization mode: pve (vs baseline), pvb (vs current best), self (experimental)')
    parser.add_argument('-o', '--opponent_exec', type=str, default=DEFAULT_OPPONENT_EXEC, help='Path to the opponent executable (for pve mode)')
    parser.add_argument('-w', '--workers', type=int, default=0, help='Number of parallel workers (0 = CPU count)')
    parser.add_argument('-n', '--games_per_eval', type=int, default=DEFAULT_GAMES_PER_EVAL, help='Number of games run per individual evaluation')
    parser.add_argument('--resume', action='store_true', help='Resume from the latest checkpoint in output/')
    parser.add_argument('--keep_all_logs', action='store_true', help='Retain logs for all simulations, even successful ones (Warning: fills disk quickly)')
    # NEW: Seed argument
    parser.add_argument('--seed_file', type=str, default=None, help='Path to a parameter file (.txt) to seed the initial population (used for phased training)')

    
    args = parser.parse_args()
    
    # Handle relative paths for opponent executable
    if not os.path.isabs(args.opponent_exec):
        # Check if the path is relative to the SCRIPT_DIR first
        potential_path = os.path.abspath(os.path.join(SCRIPT_DIR, args.opponent_exec))
        if os.path.exists(potential_path):
            args.opponent_exec = potential_path
        else:
            # Otherwise assume it's relative to the current working directory
            args.opponent_exec = os.path.abspath(args.opponent_exec)

    run_optimizer(args)
