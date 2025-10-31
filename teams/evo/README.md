# EvoAI Genetic Algorithm Optimization Framework

This framework is designed to automatically tune the strategic and tactical parameters of the EvoAI team for the MechMania IV coding contest. It uses a Genetic Algorithm (GA) to explore the parameter space, running thousands of simulated games to evolve behaviors that maximize the team's score (vinyl collected).

## System Overview

The EvoAI C++ code is instrumented with numerous parameters that control its behavior, ranging from navigation thresholds and resource prioritization to combat efficiency and team composition.

The GA optimizer (`ga_optimizer.py`) manages this process:
1. **Initialization**: Creates a population of individuals, optionally seeded with a baseline configuration.
2. **Evaluation**: Runs simulations (games) for each individual against a specified opponent to determine its "fitness" (average score).
3. **Evolution**: Selects the fittest individuals, creating the next generation through crossover (mixing parameters) and mutation (random changes).
4. **Iteration**: Repeats the process, gradually converging on an optimized parameter set.

## The EvoAI Architecture

EvoAI utilizes a hybrid architecture combining centralized planning and decentralized execution with dynamic role switching.

Key features include:

*   **UnifiedBrain:** All ships run a single brain architecture capable of executing both `Gatherer` and `Hunter` behaviors.
*   **Dynamic Role Assignment:** The central AI assesses the world state (resource availability, enemy presence, game time) and dynamically assigns roles to ships each turn.
*   **Configurable Hardware:** The GA optimizes the physical configuration of the ships (Fuel vs. Cargo capacity).
*   **Combat Optimization:** Hunters implement laser efficiency calculations and overkill prevention logic.

## Prerequisites and Setup

1.  **Python 3.8+** with the following libraries:
    ```bash
    pip install numpy pandas matplotlib
    ```
2.  **Compiled MechMania IV Binaries**: The optimizer expects the game server, observer, and team executables to be compiled and available.

### File Structure

The optimizer scripts assume a specific directory structure. The Python scripts are typically located in a subdirectory (e.g., `optimizer/`), and the compiled executables are expected in the `build/` directory two levels up.

When running the optimizer, an `output/` directory will be created in your Current Working Directory (CWD) to store the results.

project_root/ ├── build/ │ ├── mm4serv (Server) │ ├── mm4obs (Observer) │ ├── mm4team_evo (The EvoAI team executable) │ └── mm4team_opponent (Example opponent) ├── optimizer/ │ ├── ga_optimizer.py │ ├── analyze_ga_log.py │ └── (CWD when running script) │ └── output/ (Results, checkpoints, and best parameters are stored here) └── team/ └── src/ ├── graphics.reg (Required dependency for executables) └── ... (C++ source code)


## Running the Optimizer (ga_optimizer.py)

The `ga_optimizer.py` script controls the optimization process.

### Command Line Parameters

| Argument | Description | Default |
| :--- | :--- | :--- |
| `-g`, `--generations` | Total number of generations to run. | `50` |
| `-p`, `--population` | Number of individuals (parameter sets) in each generation. | `30` |
| `-m`, `--mode` | Optimization mode:<br> `pve` (Player vs Environment): Optimize against a specific opponent executable.<br> `pvb` (Player vs Best): Optimize against the best individual found so far.<br> `self` (Self-play): Experimental. | `pve` |
| `-o`, `--opponent_exec` | Path to the opponent executable (used in `pve` mode). | `../../build/mm4team` |
| `-w`, `--workers` | Number of parallel simulations to run. Set to 0 to use all available CPU cores. | `0` |
| `-n`, `--games_per_eval`| Number of games run per individual evaluation. Averaging multiple games reduces noise. | `3` |
| `--resume` | Resumes the optimization from the latest checkpoint file found in the `output/` directory. | `False` |
| `--seed_file` | Path to a parameter file (`.txt`) to seed the initial population. Used for phased training. (Ignored if `--resume` finds a checkpoint). | `None` |
| `--keep_all_logs` | Retain simulation logs even for successful runs (Warning: fills disk quickly). Logs for failed runs are always kept. | `False` |

### Example Invocations

1. **Standard optimization against the default opponent using all cores:**
   ```bash
   python3 ga_optimizer.py -g 100 -p 40
Optimization against a specific strong team (e.g., Team Groogroo) using 8 workers:

Bash

python3 ga_optimizer.py -m pve -o ../../build/mm4team_groogroo -w 8
Phased Training (Seeding): Start a new optimization using the results of a previous run.

Bash

python3 ga_optimizer.py -m pve -o ../../build/mm4team_medium_opponent --seed_file output/EvoAI_params_best_pid12345.txt
Resuming a previous optimization run:

Bash

python3 ga_optimizer.py --resume
Output Files
The optimizer generates results in the output/ directory (relative to where the script is run). Simulation logs (e.g., crashes) are stored separately (default /tmp/evo_ga_logs/).

output/optimization_history_pidXXXX.log: A detailed log of the metrics and the best parameters found in each generation. Used for analysis.

output/EvoAI_params_best_pidXXXX.txt: The parameter file containing the best configuration found so far. This is the final output of the optimization.

output/EvoAI_checkpoint_pidXXXX.pkl: Checkpoint data used for resuming the optimization.

Training Strategy Recommendations
The goal is to evolve an AI capable of regularly defeating a "good" enemy team. This requires a phased approach to training.

Utilizing Phased Training (Seeding)
The --seed_file argument is crucial for effective phased training. When moving from one phase to the next (e.g., from Phase 1 vs. No Opposition to Phase 2 vs. Mediocre Opponent), you should seed the new optimization run with the best results from the previous phase.

Complete Phase 1. Identify the best parameter file (e.g., output/EvoAI_params_best_pidXXXX.txt).

Start Phase 2, using the --seed_file argument pointing to that file, and changing the opponent (-o) or mode (-m).

This ensures the GA begins the new phase with established competence, rather than starting randomly.

Phase 1: Establishing Baseline Competence (Bootstrapping)
Initially, the GA knows nothing. The priority is to evolve fundamental skills: navigation and resource gathering.

Vs. No Opposition / Passive Team: Highly Recommended. Training against a team that takes no action (a "drifter" or "noop" team) allows the GA to optimize navigation and gathering efficiency without interference.

Vs. A Good Team: Not Recommended. If the opponent is too strong, the initial population may consistently score 0, giving the GA no signal to improve.

Phase 2: Introducing Pressure
Once the AI is a competent gatherer, introduce pressure to evolve defensive behaviors and efficient resource management.

Recommended Opponent: A mediocre enemy team.

Mode: pve

Seeding: Use the --seed_file argument with the best parameters from Phase 1.

Phase 3: Targeted Optimization
To defeat a specific "good" enemy team, the GA must optimize specifically against that team's strategies.

Recommended Opponent: The target "good" team.

Mode: pve

Seeding: Use the --seed_file argument with the best parameters from Phase 2.

Phase 4: Refinement and Generalization (PvB)
Successive training using pvb (Player vs. Best) can rapidly improve performance.

Mode: pvb

Goal: The opponent constantly improves (as it is the best previous version of EvoAI), forcing the current generation to adapt.

Caution: PvB can lead to overfitting (becoming excellent at beating itself but worse against others). Periodically validate the best PvB result against the target "good" team from Phase 3.

Analyzing the Results (analyze_ga_log.py)
The analyze_ga_log.py script visualizes the optimization process by parsing the optimization_history_pidXXXX.log file found in the output/ directory.

Usage
Bash

python3 analyze_ga_log.py <path_to_log_file>
Example:

Bash

python3 analyze_ga_log.py output/optimization_history_pid12345.log
This will generate a plot (e.g., output/optimization_history_pid12345.png) visualizing the fitness metrics over the generations.

Interpreting the Results
The plot displays several key metrics:

BestFit (Overall Best Fitness): The highest fitness score achieved across all generations up to that point.

GenBest (This Generation): The highest fitness score achieved within that specific generation.

AvgFit (Population Average): The average fitness of the entire population in that generation.

StdDev Range: The diversity of fitness scores within the population.

Is the GA Working Well?
Signs of Healthy Convergence:

Upward Trend: BestFit and AvgFit should generally trend upwards.

Average Approaches Best: As the GA converges, the AvgFit line should rise to meet the BestFit line.

Managed Diversity: The StdDev Range should generally decrease, but if it collapses too quickly, the GA may get stuck in a local optimum (premature convergence).

Signs of Problems (Drifting or Stagnation):

Random Drift / Stagnation:

BestFit and AvgFit are flat or erratic with no sustained upward trend.

Action: The GA might be stuck, or the fitness evaluation might be too noisy. Increase games_per_eval (-n). If BestFit is near zero, the opponent might be too difficult (see Phase 1).

Premature Convergence:

AvgFit rapidly meets BestFit, but the score is low.

Action: The population lost diversity too soon. Increase the mutation rate (in the Python script configuration) or population size (-p).
