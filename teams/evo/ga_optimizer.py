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

# Define the parameters (Genome) - UPDATED: Parameters adapted for MagicBag and Trajectory Planning
PARAMETERS = {
    # Heuristics (MagicBag Weights)
    "W_VINYL_VALUE": (5.0, 50.0),
    "W_URANIUM_VALUE": (5.0, 30.0),
    "W_FUEL_BOOST_FACTOR": (2.0, 15.0),
    "W_TIME_PENALTY": (1.0, 20.0),      # Penalty per second of travel time
    "W_FUEL_COST_PENALTY": (0.5, 10.0), # Penalty per unit of Delta-V
    "W_CONFLICT_PENALTY": (50.0, 300.0),
    
    # Thresholds
    "THRESHOLD_RETURN_CARGO": (0.85, 1.0),
    "THRESHOLD_FUEL_TARGET": (50.0, 150.0),
    "THRESHOLD_MAX_SHIELD_BOOST": (15.0, 60.0),
    
    # Dynamic Fuel Management
    "FUEL_COST_PER_DIST_ESTIMATE": (0.05, 0.2), # Used for safety margin calculation
    "FUEL_SAFETY_MARGIN": (20.0, 80.0),

    # Navigation (Trajectory Planning and Alignment)
    "NAV_ALIGNMENT_STRICT_ANGLE": (0.01, 0.15), # Tighter range for precise alignment (0.5 deg to 8.5 deg)
    "NAV_ALIGNMENT_LOOSE_ANGLE": (0.2, 1.2),    # Range for allowing thrust during minor course corrections
    "NAV_INTERCEPT_TIME_HORIZON": (20.0, 90.0), # Max time to search for intercept solutions
    "NAV_STATION_BRAKING_DIST": (20.0, 150.0),  # Distance to start slowing down near the station

    # Safety
    "NAV_AVOIDANCE_HORIZON": (5.0, 20.0),
    "NAV_SHIELD_BOOST_TTC": (0.5, 4.0),
    
    # Tactics
    "TACTICS_LASER_POWER": (500.0, 2000.0),
    "TACTICS_LASER_RANGE": (50.0, 200.0),

    # Configuration
    "SHIP_CARGO_RATIO": (0.1, 0.9)
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
BUILD_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../build/"))

SERVER_EXEC = os.path.join(BUILD_DIR, "mm4serv")
OBSERVER_EXEC = os.path.join(BUILD_DIR, "mm4obs")
EVO_AI_EXEC = os.path.join(BUILD_DIR, "mm4team_evo")
DEFAULT_OPPONENT_EXEC = os.path.join(BUILD_DIR, "mm4team")

GRAPHICS_REG_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "../../team/src/graphics.reg"))

TEAM_NAME = "EvoAI"
GAME_TIMEOUT = 240 # 4 minutes

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

def save_parameters(params, filename):
    """Saves parameters to a specified file."""
    try:
        with open(filename, 'w') as f:
            for i, key in enumerate(PARAM_KEYS):
                # Ensure we save the clamped value
                f.write(f"{key} {params[i]}\n")
    except IOError as e:
        print(f"Warning: Could not save parameters to {filename}: {e}")

# UPDATED: Includes comprehensive metrics and logs clamped parameters
def log_history(filename, generation, stats, clamped_params):
    """Logs detailed statistics for the generation."""
    try:
        with open(filename, 'a') as f:
            # Format: Gen X, GenBest: Y.YY, AvgFit: A.AA, StdDev: S.SS, BestFit: B.BB, Params: ...
            f.write(f"Gen {generation}, GenBest: {stats['gen_best']:.2f}, AvgFit: {stats['avg']:.2f}, StdDev: {stats['std']:.2f}, BestFit: {stats['overall_best']:.2f}, Params: ")
            # Log the clamped parameters
            param_strings = [f"{PARAM_KEYS[i]}: {clamped_params[i]:.4f}" for i in range(NUM_PARAMS)]
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
                    pgid = os.getpgid(process.pid)
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
        print(f"\nError: Missing required executables. Please verify paths:")
        for missed in missing:
            print(f"  - {missed}")
        sys.exit(1)
    if not os.path.exists(GRAPHICS_REG_PATH):
        print(f"\nError: Missing required graphics registry file: {GRAPHICS_REG_PATH}")
        sys.exit(1)

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
        clamped_params.append(clamped_val)
    
    # Constraint: NAV_ALIGNMENT_LOOSE_ANGLE must be >= NAV_ALIGNMENT_STRICT_ANGLE
    try:
        strict_idx = PARAM_KEYS.index("NAV_ALIGNMENT_STRICT_ANGLE")
        loose_idx = PARAM_KEYS.index("NAV_ALIGNMENT_LOOSE_ANGLE")
        if clamped_params[loose_idx] < clamped_params[strict_idx]:
            clamped_params[loose_idx] = clamped_params[strict_idx]
            # Re-clip loose angle if it exceeded the max range
            max_val = PARAMETERS["NAV_ALIGNMENT_LOOSE_ANGLE"][1]
            clamped_params[loose_idx] = min(clamped_params[loose_idx], max_val)
    except ValueError:
        pass

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
             print(f"Checkpoint saved: {filename}")
    except Exception as e:
        print(f"Error saving checkpoint {filename}: {e}")

def load_checkpoint(filename):
    """Loads the GA state from a pickle file."""
    if not os.path.exists(filename):
        print(f"Error: Checkpoint file not found: {filename}")
        return None
    try:
        with open(filename, 'rb') as f:
            state = pickle.load(f)
        print(f"Checkpoint loaded successfully from {filename}.")
        return state
    except Exception as e:
        print(f"Error loading checkpoint file {filename}: {e}")
        return None

# --- Game Execution (Worker Functions) ---
# These functions must be top-level to be picklable by the multiprocessing module.

def construct_team_command(executable, port, pid, param_file=None, enable_log=False, log_file_base=None):
    """Builds the command line for launching a team client."""
    cmd = [executable, f"-p{port}", "-hlocalhost"]
    
    if param_file:
        cmd.extend(["--params", param_file])
        
    if enable_log and log_file_base:
        cmd.append("--log")
        # Append main PID and the worker's process ID (os.getpid()) to differentiate concurrent logs
        log_file_unique = f"{log_file_base}_pid{pid}_worker{os.getpid()}.log"
        cmd.extend(["--log-file", log_file_unique])
    return cmd

def run_single_networked_game(config, filenames, param_file_active):
    """Launches the simulation and waits for the result."""
    port = find_free_port()
    processes = []
    server_process = None
    
    # Setup Popen arguments
    server_popen_kwargs = {'stdout': subprocess.PIPE, 'stderr': subprocess.DEVNULL, 'text': True}
    client_popen_kwargs = {'stdout': subprocess.DEVNULL, 'stderr': subprocess.DEVNULL}

    # Use process groups (os.setsid) for reliable cleanup on Unix/Linux
    if sys.platform != "win32":
        server_popen_kwargs['preexec_fn'] = os.setsid
        client_popen_kwargs['preexec_fn'] = os.setsid

    try:
        # 1. Start Server
        server_cmd = [SERVER_EXEC, f"-p{port}", "-T2"]
        server_process = subprocess.Popen(server_cmd, **server_popen_kwargs)
        processes.append(server_process)
        time.sleep(0.1) # Give server time to bind port

        # 2. Start Team 1 (P1 - Active candidate)
        team1_cmd = construct_team_command(
            EVO_AI_EXEC, port, filenames['pid'], param_file_active, 
            config.log_p1, "EvoAI_P1_active"
        )
        p1 = subprocess.Popen(team1_cmd, **client_popen_kwargs)
        processes.append(p1)

        # 3. Start Team 2 (P2 - Opponent)
        if config.mode == 'pvb':
            # PvB Mode: Opponent uses the shared BEST parameters.
            team2_cmd = construct_team_command(
                EVO_AI_EXEC, port, filenames['pid'], filenames['param_file_best'],
                config.log_p2, "EvoAI_P2_best"
            )
        else:
            # Opponent Mode
            team2_cmd = construct_team_command(
                config.opponent_exec, port, filenames['pid'], config.params_p2,
                config.log_p2, "Opponent_P2"
            )

        p2 = subprocess.Popen(team2_cmd, **client_popen_kwargs)
        processes.append(p2)

        # 4. Start Observer (Headless)
        observer_cmd = [
            OBSERVER_EXEC, f"-p{port}", "-hlocalhost",
            "-G", "-g", GRAPHICS_REG_PATH,
        ]

        observer_kwargs = client_popen_kwargs.copy()
        observer_env = os.environ.copy()
        # Set SDL environment variables for headless operation
        observer_env["SDL_VIDEODRIVER"] = "dummy"
        observer_env["SDL_AUDIODRIVER"] = "dummy"
        observer_kwargs["env"] = observer_env

        p_obs = subprocess.Popen(observer_cmd, **observer_kwargs)
        processes.append(p_obs)

        # 5. Wait for the server process to complete
        try:
            stdout, _ = server_process.communicate(timeout=GAME_TIMEOUT)
            # 6. Parse the score
            score = parse_score(stdout, TEAM_NAME)
            return score
            
        except subprocess.TimeoutExpired:
            # Timeout occurred (game instability)
            return 0.0

    except Exception:
        # Error during execution (e.g., connection refused, simulation crash)
        return 0.0
    finally:
        # 7. Cleanup (Crucial for stability)
        # Terminate the server group first (which should kill clients on Unix)
        terminate_process_group(server_process)
        
        # Cleanup remaining processes (especially important on Windows)
        if sys.platform == "win32":
             for p in processes:
                terminate_process_group(p)
        
        # Final wait to ensure processes are dead
        for p in processes:
            try:
                p.wait(timeout=1)
            except (subprocess.TimeoutExpired, AttributeError):
                pass

# UPDATED: Returns clamped parameters
def worker_evaluate_individual(individual_index, params, config, filenames):
    """Evaluates an individual's fitness. Executed in a worker process."""
    
    # 1. Prepare Parameters (Clamping happens here)
    clamped_params = prepare_parameters(params)

    # Define a unique active parameter file for this worker
    PARAM_FILE_ACTIVE = f"{filenames['active_prefix']}_worker{os.getpid()}.tmp"

    # Save the active parameters
    save_parameters(clamped_params, PARAM_FILE_ACTIVE)
    
    # 2. Run Simulations
    scores = []
    games_run = 0
    try:
        for _ in range(config.games_per_eval):
            score = run_single_networked_game(config, filenames, PARAM_FILE_ACTIVE)
            scores.append(score)
            games_run += 1
    finally:
        # 3. Cleanup worker-specific temporary file
        if os.path.exists(PARAM_FILE_ACTIVE):
            try:
                os.remove(PARAM_FILE_ACTIVE)
            except OSError:
                pass

    avg_score = np.mean(scores) if scores else 0.0
    # Return the score, games run, AND the clamped parameters used
    return avg_score, games_run, clamped_params


# --- Genetic Algorithm Operations (Standard GA logic) ---

def initialize_population(config):
    if config.population_size <= 0:
        return np.zeros((0, NUM_PARAMS))

    population = np.zeros((config.population_size, NUM_PARAMS))
    for i, key in enumerate(PARAM_KEYS):
        min_val, max_val = PARAMETERS[key]
        population[:, i] = np.random.uniform(min_val, max_val, config.population_size)
    return population

def selection(population, fitness, config):
    # Dynamically adjust tournament size.
    TOURNAMENT_SIZE = min(DESIRED_TOURNAMENT_SIZE, config.population_size)

    if TOURNAMENT_SIZE <= 0:
        return np.array([])

    new_population = []
    
    # Calculate the number of elites
    num_elites = min(ELITISM_COUNT, config.population_size)

    # Elitism
    if num_elites > 0:
        # Use argpartition for efficient selection of top N elements
        elite_indices = np.argpartition(fitness, -num_elites)[-num_elites:]
        for idx in elite_indices:
            new_population.append(population[idx].copy())

    # Tournament
    for _ in range(config.population_size - num_elites):
        indices = np.random.choice(config.population_size, TOURNAMENT_SIZE, replace=False)
        
        valid_fitness = fitness[indices]
        if np.isnan(valid_fitness).all():
             best_idx = indices[0]
        else:
             # Use nanargmax to handle potential NaNs correctly
             best_idx = indices[np.nanargmax(valid_fitness)]
        new_population.append(population[best_idx].copy())
    return np.array(new_population)

def crossover(population, config):
    new_population = population.copy()
    num_elites = min(ELITISM_COUNT, config.population_size)
    
    for i in range(num_elites, config.population_size, 2):
        if i + 1 < config.population_size and np.random.rand() < CROSSOVER_RATE:
            parent1, parent2 = new_population[i], new_population[i+1]
            mask = np.random.rand(NUM_PARAMS) < 0.5
            temp = parent1[mask].copy()
            parent1[mask] = parent2[mask]
            parent2[mask] = temp
    return new_population

def mutation(population, config):
    new_population = population.copy()
    num_elites = min(ELITISM_COUNT, config.population_size)

    for i in range(num_elites, config.population_size):
        for j in range(NUM_PARAMS):
            if np.random.rand() < MUTATION_RATE:
                # Mutation explores outside bounds; clamping happens during evaluation.
                min_val, max_val = PARAMETERS[PARAM_KEYS[j]]
                range_val = max_val - min_val
                noise = np.random.normal(0, MUTATION_STRENGTH * range_val)
                new_population[i, j] += noise
    return new_population

# --- Main Optimization Loop ---

def genetic_algorithm(config):
    # Initialize state variables
    start_generation = 0
    population = None
    best_fitness_overall = -np.inf
    best_params_overall = None
    GAMES_COMPLETED = 0
    FILENAMES = None

    # 1. Initialization or Resume
    if config.resume:
        checkpoint = load_checkpoint(config.resume)
        if checkpoint:
            # Restore state
            config = checkpoint['config']
            FILENAMES = initialize_filenames(checkpoint['pid'])
            start_generation = checkpoint['generation'] 
            population = checkpoint['population']
            best_fitness_overall = checkpoint['best_fitness']
            best_params_overall = checkpoint['best_params']
            GAMES_COMPLETED = checkpoint['games_completed']
            np.random.set_state(checkpoint['random_state'])
            
            check_executables(config)
            
            if start_generation >= config.num_generations:
                print("Checkpoint indicates optimization is already complete.")
                return best_params_overall, best_fitness_overall, config, FILENAMES

            print(f"Resuming optimization from Generation {start_generation + 1}.")
        else:
            return None, None, config, None
    else:
        # Fresh start
        PID = os.getpid()
        FILENAMES = initialize_filenames(PID)
        check_executables(config)
        
        # Clean up previous logs/temps/checkpoints for this specific PID
        for filename in os.listdir('.'):
            if filename.startswith(FILENAMES['active_prefix']):
                try:
                    os.remove(filename)
                except OSError:
                    pass
        
        for filepath in [FILENAMES['history_log'], FILENAMES['checkpoint_file']]:
             if os.path.exists(filepath):
                try:
                    os.remove(filepath)
                except OSError:
                    pass
        
        population = initialize_population(config)
        if population.size == 0:
            print("Error: Population size must be greater than 0.")
            return None, None, config, FILENAMES
        
        # --- SEEDING LOGIC ---
        if config.seed:
            if os.path.exists(config.seed):
                print(f"Seeding initial 'Best' parameters from: {config.seed}")
                shutil.copy(config.seed, FILENAMES['param_file_best'])
                # Attempt to load the seed file to initialize best_params_overall internally
                try:
                    # A functional way to load the key-value pairs back into an array in the correct order
                    seed_params_dict = {}
                    with open(config.seed, 'r') as f:
                        for line in f:
                            parts = line.strip().split()
                            if len(parts) == 2:
                                seed_params_dict[parts[0]] = float(parts[1])
                    
                    seed_data = [seed_params_dict[key] for key in PARAM_KEYS if key in seed_params_dict]
                    
                    if len(seed_data) == NUM_PARAMS:
                        best_params_overall = np.array(seed_data)
                        # We don't know the fitness of the seed yet, so best_fitness_overall remains -inf
                    else:
                         raise ValueError(f"Seed file parameter mismatch. Expected {NUM_PARAMS}, found {len(seed_data)}.")
                except Exception as e:
                    print(f"Warning: Could not load seed file correctly ({e}). Falling back to random initialization.")
                    best_params_overall = population[0].copy()
                    initial_best_clamped = prepare_parameters(best_params_overall)
                    save_parameters(initial_best_clamped, FILENAMES['param_file_best'])

            else:
                print(f"Warning: Seed file not found: {config.seed}. Initializing randomly.")
                best_params_overall = population[0].copy()
                initial_best_clamped = prepare_parameters(best_params_overall)
                save_parameters(initial_best_clamped, FILENAMES['param_file_best'])
        else:
            print(f"Initializing {FILENAMES['param_file_best']} with random parameters.")
            best_params_overall = population[0].copy()
            initial_best_clamped = prepare_parameters(best_params_overall)
            save_parameters(initial_best_clamped, FILENAMES['param_file_best'])


    # Calculate total games
    TOTAL_GAMES = config.num_generations * config.population_size * config.games_per_eval
    
    # Determine the number of workers
    num_workers = config.jobs
    if num_workers > config.population_size:
        print(f"Note: Reducing parallel jobs from {num_workers} to match population size {config.population_size}.")
        num_workers = config.population_size


    print(f"\nStarting GA optimization (Mode: {config.mode.upper()}) [PID: {FILENAMES['pid']}]")
    print(f"Parallel Jobs: {num_workers}")
    print(f"Generations: {config.num_generations}, Population: {config.population_size}, Games/Eval: {config.games_per_eval}")
    print(f"Total Games: {TOTAL_GAMES} (Completed so far: {GAMES_COMPLETED})\n")

    # 2. Main Loop
    # Initialize the Process Pool Executor
    with concurrent.futures.ProcessPoolExecutor(max_workers=num_workers) as executor:
        for generation in range(start_generation, config.num_generations):
            start_time = time.time()
            
            # Evaluation (Parallel)
            fitness = np.zeros(config.population_size)
            # Store clamped parameters for logging
            clamped_params_gen = [None] * config.population_size
            futures_to_index = {}
            
            print(f"--- Generation {generation+1}/{config.num_generations} ---")
            print(f"Submitting {config.population_size} individuals to {num_workers} workers...")

            # Submit tasks
            for i in range(config.population_size):
                # Pass the configuration and filenames explicitly to the worker
                future = executor.submit(worker_evaluate_individual, i, population[i], config, FILENAMES)
                futures_to_index[future] = i

            # Collect results as they complete
            for future in concurrent.futures.as_completed(futures_to_index):
                index = futures_to_index[future]
                try:
                    # Set a generous timeout for results collection
                    timeout_val = (GAME_TIMEOUT * config.games_per_eval) + 60
                    # Collect the score, games run, AND clamped parameters
                    avg_score, games_run, clamped_params = future.result(timeout=timeout_val)
                    fitness[index] = avg_score
                    clamped_params_gen[index] = clamped_params
                    GAMES_COMPLETED += games_run
                    
                    # Progress update
                    print(f"  Individual {index+1} completed. Score: {avg_score:.2f}. Progress: {GAMES_COMPLETED}/{TOTAL_GAMES} games.")

                except concurrent.futures.TimeoutError:
                    print(f"  Warning: Individual {index+1} timed out during result collection. Assigning score 0.")
                    fitness[index] = 0.0
                    clamped_params_gen[index] = prepare_parameters(population[index]) # Fallback
                    future.cancel()
                except Exception as e:
                    print(f"  Error: Individual {index+1} raised an exception: {e}. Assigning score 0.")
                    fitness[index] = 0.0
                    clamped_params_gen[index] = prepare_parameters(population[index]) # Fallback

            print("Generation evaluation complete.")
            
            # --- Sequential GA steps ---
            
            # Handle potential NaNs
            fitness[np.isnan(fitness)] = 0.0

            if fitness.size == 0:
                print("Warning: Fitness array is empty.")
                continue

            # Track statistics
            gen_best_idx = np.argmax(fitness)
            gen_best_fitness = fitness[gen_best_idx]
            gen_avg_fitness = np.mean(fitness)
            gen_std_fitness = np.std(fitness)
            
            # Get the clamped parameters used by the best individual
            gen_best_params_clamped = clamped_params_gen[gen_best_idx]

            # Track overall best
            if gen_best_fitness > best_fitness_overall:
                best_fitness_overall = gen_best_fitness
                # Update overall best using the clamped parameters
                best_params_overall = gen_best_params_clamped
                print(f"  *** New overall best fitness: {best_fitness_overall:.2f} ***")
                
                # Save the best parameters (already clamped)
                save_parameters(best_params_overall, FILENAMES['param_file_best'])

                if config.mode == 'pvb':
                    print("  Updated 'Best' parameters for opponent.")

            # Log history using the clamped parameters and enhanced metrics
            stats = {
                'gen_best': gen_best_fitness,
                'avg': gen_avg_fitness,
                'std': gen_std_fitness,
                'overall_best': best_fitness_overall
            }
            log_history(FILENAMES['history_log'], generation + 1, stats, gen_best_params_clamped)

            # Evolution (Prepare for next generation)
            if generation < config.num_generations - 1:
                # Note: Evolution operates on the raw (potentially unclamped) population parameters
                population = selection(population, fitness, config)
                if population.size == 0:
                    print("Error: Population extinguished. Stopping optimization.")
                    break

                population = crossover(population, config)
                population = mutation(population, config)
            
            duration = time.time() - start_time
            print(f"Generation {generation+1} Summary:")
            # Updated console summary
            print(f"  Time: {duration:.2f}s | GenBest: {gen_best_fitness:.2f} | AvgFit: {gen_avg_fitness:.2f} | StdDev: {gen_std_fitness:.2f} | BestFit: {best_fitness_overall:.2f}\n")


            # Save Checkpoint
            checkpoint_state = {
                'pid': FILENAMES['pid'],
                'config': config,
                'generation': generation + 1, # Save the next generation number
                'population': population,
                'best_fitness': best_fitness_overall,
                'best_params': best_params_overall,
                'games_completed': GAMES_COMPLETED,
                'random_state': np.random.get_state()
            }
            save_checkpoint(FILENAMES['checkpoint_file'], checkpoint_state)

    # Executor shuts down automatically here due to 'with' block

    # Final cleanup of temporary files
    for filename in os.listdir('.'):
        if filename.startswith(FILENAMES['active_prefix']):
            try:
                os.remove(filename)
            except OSError:
                pass

    return best_params_overall, best_fitness_overall, config, FILENAMES

def parse_arguments():
    parser = argparse.ArgumentParser(description="EvoAI Genetic Algorithm Optimizer for MechMania 4")
    
    # Parallelism
    try:
        default_jobs = multiprocessing.cpu_count()
    except NotImplementedError:
        default_jobs = 1

    parser.add_argument('-j', '--jobs', type=int, default=default_jobs,
                        help=f"Number of parallel jobs (workers) to run. Default: {default_jobs} (CPU count).")

    # Resume and Seeding
    parser.add_argument('--resume', type=str, default=None, metavar="FILE.pkl",
                        help="Resume optimization from a specified checkpoint file.")
    parser.add_argument('--seed', type=str, default=None, metavar="FILE.txt",
                        help="Seed the initial 'Best' parameters from a file (useful for iterative PvB).")


    # Training Mode
    parser.add_argument('--mode', type=str, choices=['opponent', 'pvb'], default='opponent',
                        help="Optimization mode: 'opponent' (default) or 'pvb' (Population vs. Best self-play).")
    
    # GA Settings
    parser.add_argument('-g', '--generations', dest='num_generations', type=int, default=DEFAULT_NUM_GENERATIONS,
                        help=f"Number of generations (default: {DEFAULT_NUM_GENERATIONS}).")
    parser.add_argument('-p', '--population', dest='population_size', type=int, default=DEFAULT_POPULATION_SIZE,
                        help=f"Population size per generation (default: {DEFAULT_POPULATION_SIZE}).")
    parser.add_argument('-e', '--evals', dest='games_per_eval', type=int, default=DEFAULT_GAMES_PER_EVAL,
                        help=f"Number of games per individual evaluation (default: {DEFAULT_GAMES_PER_EVAL}).")

    # Configuration
    parser.add_argument('--opponent-exec', type=str, default=DEFAULT_OPPONENT_EXEC,
                        help=f"Path to the opponent executable for 'opponent' mode. (Default: {DEFAULT_OPPONENT_EXEC})")
    
    # Logging and Parameters
    parser.add_argument('--log-p1', action='store_true',
                        help="Enable detailed logging for Player 1 (EvoAI Candidate).")
    parser.add_argument('--log-p2', action='store_true',
                        help="Enable detailed logging for Player 2 (Opponent or EvoAI Best).")
    parser.add_argument('--params-p2', type=str, default=None,
                        help="Specify a fixed parameter file for Player 2 (only in 'opponent' mode).")

    args = parser.parse_args()
    
    # Validation
    if args.jobs < 1:
        args.jobs = 1

    if args.resume and args.seed:
        parser.error("--resume and --seed cannot be used together.")
        
    return args

if __name__ == '__main__':
    # Set the global configuration
    CONFIG = parse_arguments()
    
    # Set the multiprocessing start method to 'fork' if available (Unix/Linux)
    # This is generally preferred for this type of application over 'spawn' for stability and efficiency.
    if sys.platform != "win32":
        try:
            # Check if multiprocessing context is available and set method
            if multiprocessing.get_start_method(allow_none=True) != 'fork':
                multiprocessing.set_start_method('fork', force=True)
        except (RuntimeError, AttributeError, ValueError):
            pass # Method might already be set or context unavailable

    best_params, best_fitness, final_config, filenames = genetic_algorithm(CONFIG)
    
    if best_params is not None:
        print("\nOptimization finished.")
        print(f"Best fitness achieved: {best_fitness:.2f}")
        print(f"Best parameters saved to: {filenames['param_file_best']}")
        print(f"History log saved to: {filenames['history_log']}")
        print(f"Final checkpoint saved to: {filenames['checkpoint_file']}")

    else:
         print("\nOptimization failed or stopped.")
         if filenames and 'checkpoint_file' in filenames and os.path.exists(filenames['checkpoint_file']):
            print(f"If optimization stopped prematurely, resume using: python3 ga_optimizer.py --resume {filenames['checkpoint_file']}")