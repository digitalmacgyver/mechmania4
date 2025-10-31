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
    plot_fitness(data, args.logfile)
