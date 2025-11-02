#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import argparse
import re
import os
import sys

def parse_log_file(filepath):
    """Parses the enhanced optimization history log file."""
    
    # Define the regex pattern to capture the metrics
    # Gen X, GenBest: Y.YY, AvgFit: A.AA, StdDev: S.SS, BestFit: B.BB, Params: ...
    pattern = re.compile(
        r"Gen (\d+), GenBest: ([\d\.]+), AvgFit: ([\d\.]+), StdDev: ([\d\.]+), BestFit: ([\d\.]+), Params: (.*)"
    )
    
    data = []
    
    try:
        with open(filepath, 'r') as f:
            for line in f:
                match = pattern.match(line)
                if match:
                    gen, gen_best, avg_fit, std_dev, best_fit, params_str = match.groups()
                    
                    # Parse the parameters string into a dictionary
                    params = {}
                    # Handle both integer and float parameter formats
                    param_parts = params_str.split(', ')
                    for part in param_parts:
                        try:
                            key, value_str = part.split(': ')
                            # Attempt to convert to float, then check if it looks like an integer
                            value = float(value_str)
                            if value_str.isdigit() or ('.' not in value_str and 'e' not in value_str.lower()):
                                 try:
                                     value = int(value_str)
                                 except ValueError:
                                     pass # Keep as float if int conversion fails unexpectedly
                            params[key] = value
                        except ValueError:
                            # Handle potential parsing issues gracefully
                            continue

                    
                    entry = {
                        'Generation': int(gen),
                        'GenBest': float(gen_best),
                        'AvgFit': float(avg_fit),
                        'StdDev': float(std_dev),
                        'BestFit': float(best_fit),
                        'Params': params
                    }
                    data.append(entry)
                    
    except FileNotFoundError:
        print(f"Error: Log file not found at {filepath}")
        sys.exit(1)
        
    if not data:
        print("Error: No valid data found in the log file. Check the file format.")
        sys.exit(1)
        
    return pd.DataFrame(data)

def check_convergence_health(data):
    """Analyzes and reports on the health/quality of the GA convergence."""

    print("\n--- Convergence Health Analysis ---")

    total_gens = data['Generation'].max() + 1

    # 1. Check if fitness is improving
    first_10_avg = data.head(10)['BestFit'].mean() if len(data) >= 10 else data['BestFit'].mean()
    last_10_avg = data.tail(10)['BestFit'].mean() if len(data) >= 10 else data['BestFit'].mean()
    improvement = last_10_avg - first_10_avg
    improvement_pct = (improvement / first_10_avg * 100) if first_10_avg > 0 else 0

    if improvement > 0:
        print(f"✓ Fitness is improving: {improvement:.2f} point gain ({improvement_pct:.1f}% improvement)")
    else:
        print(f"✗ Fitness is NOT improving: {improvement:.2f} point change ({improvement_pct:.1f}% change)")
        print(f"  Warning: No improvement from early to late generations")

    # 2. Check for convergence (low standard deviation in later generations)
    last_gen_std = data.iloc[-1]['StdDev']
    avg_std = data['StdDev'].mean()

    if last_gen_std < avg_std * 0.5:
        print(f"✓ Population is converging: StdDev={last_gen_std:.2f} (well below average of {avg_std:.2f})")
    else:
        print(f"✗ Population is NOT converging: StdDev={last_gen_std:.2f} (not below average of {avg_std:.2f})")
        print(f"  Warning: High diversity may indicate lack of convergence or unstable fitness landscape")

    # 3. Check for premature convergence (early low diversity)
    if len(data) >= 20:
        early_std = data.head(20)['StdDev'].mean()
        if early_std < avg_std * 0.3:
            print(f"✗ Possible premature convergence: Early StdDev={early_std:.2f} is very low")
            print(f"  Warning: Population may have converged too quickly, limiting exploration")
        else:
            print(f"✓ Good early diversity: Early StdDev={early_std:.2f} shows healthy exploration")

    # 4. Check for plateau (no improvement in later generations)
    if len(data) >= 20:
        last_20 = data.tail(20)
        best_improvement = last_20['BestFit'].max() - last_20['BestFit'].min()

        if best_improvement < 0.01 * data['BestFit'].max():  # Less than 1% improvement
            print(f"✗ Fitness has plateaued: Only {best_improvement:.4f} improvement in last 20 generations")
            print(f"  Warning: May need more generations, different parameters, or restart")
        else:
            print(f"✓ Fitness still improving: {best_improvement:.4f} gain in last 20 generations")

    # 5. Check fitness trajectory
    max_fitness = data['BestFit'].max()
    final_fitness = data.iloc[-1]['BestFit']

    if final_fitness >= max_fitness * 0.95:  # Within 5% of best
        print(f"✓ Maintaining near-peak fitness: {final_fitness:.2f} (max: {max_fitness:.2f})")
    else:
        print(f"✗ NOT at peak fitness: {final_fitness:.2f} vs max: {max_fitness:.2f}")
        print(f"  Warning: Current best is {max_fitness - final_fitness:.2f} points below peak")

    # 6. Check generation count adequacy
    if total_gens < 50:
        print(f"✗ Short run: Only {total_gens} generations may be insufficient for convergence")
        print(f"  Recommendation: Consider running for more generations")
    else:
        print(f"✓ Adequate generation count: {total_gens} generations provide good optimization time")

    # 7. Overall assessment
    print("\n--- Overall Assessment ---")
    checks_passed = 0
    total_checks = 0

    if improvement > 0:
        checks_passed += 1
    total_checks += 1

    if last_gen_std < avg_std * 0.5:
        checks_passed += 1
    total_checks += 1

    if len(data) >= 20:
        if early_std >= avg_std * 0.3:
            checks_passed += 1
        total_checks += 1

        if best_improvement >= 0.01 * data['BestFit'].max():
            checks_passed += 1
        total_checks += 1

    if final_fitness >= max_fitness * 0.95:
        checks_passed += 1
    total_checks += 1

    if total_gens >= 50:
        checks_passed += 1
    total_checks += 1

    health_pct = (checks_passed / total_checks * 100) if total_checks > 0 else 0

    if health_pct >= 80:
        print(f"✓ HEALTHY: {checks_passed}/{total_checks} health checks passed ({health_pct:.0f}%)")
        print(f"  The optimization appears to be converging well.")
    elif health_pct >= 50:
        print(f"⚠ MARGINAL: {checks_passed}/{total_checks} health checks passed ({health_pct:.0f}%)")
        print(f"  The optimization shows mixed results. Review warnings above.")
    else:
        print(f"✗ UNHEALTHY: Only {checks_passed}/{total_checks} health checks passed ({health_pct:.0f}%)")
        print(f"  The optimization may not be converging properly. Review warnings above.")

def analyze_results(data):
    """Prints summary statistics and the best parameters found."""

    print("--- Optimization Analysis Summary ---")
    print(f"Total Generations: {data['Generation'].max() + 1}")
    print(f"Maximum Fitness Achieved: {data['BestFit'].max():.4f}")

    # Find the row corresponding to the final best fitness
    # We use the parameters recorded in the very last generation's log entry
    best_row = data.iloc[-1]

    print("\nBest Parameters Found (as of Generation {}):".format(best_row['Generation']))
    # Ensure Params column is correctly accessed
    if 'Params' in best_row and isinstance(best_row['Params'], dict):
        for key, value in best_row['Params'].items():
            if isinstance(value, int):
                print(f"  {key}: {value}")
            else:
                # Handle potential float representations of integers
                if isinstance(value, float) and value.is_integer():
                     print(f"  {key}: {int(value)}")
                else:
                    print(f"  {key}: {value:.4f}")
    else:
        print("  Could not retrieve best parameters.")

def plot_fitness(data, filepath):
    """Generates a plot of the fitness progression."""
    plt.figure(figsize=(12, 7))
    
    plt.plot(data['Generation'], data['BestFit'], label='BestFit (Overall)', linestyle='--', color='green', linewidth=2)
    plt.plot(data['Generation'], data['GenBest'], label='GenBest (This Generation)', color='blue', alpha=0.7)
    plt.plot(data['Generation'], data['AvgFit'], label='AvgFit (Population Average)', color='orange', alpha=0.7)
    
    # Adding StdDev visualization
    plt.fill_between(data['Generation'], data['AvgFit'] - data['StdDev'], data['AvgFit'] + data['StdDev'], color='orange', alpha=0.2, label='StdDev Range')

    
    plt.title('Genetic Algorithm Fitness Progression')
    plt.xlabel('Generation')
    plt.ylabel('Fitness (Score)')
    plt.legend()
    plt.grid(True, linestyle=':', alpha=0.6)
    
    # Determine the output filename
    # FIX: Use os.path.splitext to correctly replace the extension
    base_filename, _ = os.path.splitext(filepath)
    output_path = base_filename + ".png"
        
    plt.tight_layout()
    plt.savefig(output_path)
    print(f"\nPlot saved to {output_path}")
    # plt.show() # Uncomment if you want the plot to display interactively

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analyze GA Optimization Log File")
    parser.add_argument('logfile', type=str, help='Path to the optimization history log file (e.g., output/optimization_history_pidXXXX.log)')

    args = parser.parse_args()

    data = parse_log_file(args.logfile)
    analyze_results(data)
    check_convergence_health(data)
    plot_fitness(data, args.logfile)
