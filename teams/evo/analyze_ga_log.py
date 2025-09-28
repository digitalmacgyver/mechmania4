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
        r"Gen (\d+), GenBest: ([\d\.]+), AvgFit: ([\d\.]+), StdDev: ([\d\.]+), BestFit: ([\d\.]+), Params: .*"
    )
    
    data = []
    try:
        with open(filepath, 'r') as f:
            for line in f:
                match = pattern.match(line)
                if match:
                    gen, gen_best, avg_fit, std_dev, best_fit = match.groups()
                    data.append({
                        'Generation': int(gen),
                        'GenBest': float(gen_best),
                        'AvgFit': float(avg_fit),
                        'StdDev': float(std_dev),
                        'OverallBest': float(best_fit)
                    })
    except FileNotFoundError:
        print(f"Error: Log file not found at {filepath}")
        return None
    
    if not data:
        print(f"Error: No valid data found in log file. Ensure the log file uses the enhanced format.")
        return None
        
    return pd.DataFrame(data)

def plot_optimization(df, filepath):
    """Generates the plot of the optimization process."""
    plt.figure(figsize=(12, 7))
    
    # Plotting the main fitness metrics
    plt.plot(df['Generation'], df['OverallBest'], label='Overall Best Fitness (Convergence)', marker='o', linestyle='-', color='g')
    plt.plot(df['Generation'], df['AvgFit'], label='Average Fitness (Population Quality)', marker='.', linestyle='-', color='r', alpha=0.7)
    
    # Visualize standard deviation (diversity) using fill_between
    plt.fill_between(df['Generation'], df['AvgFit'] - df['StdDev'], df['AvgFit'] + df['StdDev'], color='r', alpha=0.1, label='Std Dev (Diversity)')

    plt.title(f'Genetic Algorithm Optimization Progress\n({os.path.basename(filepath)})')
    plt.xlabel('Generation')
    plt.ylabel('Fitness (Vinyl Collected)')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.6)
    
    # Save the plot
    output_filename = filepath + '.png'
    plt.savefig(output_filename)
    print(f"\nPlot saved to: {output_filename}")
    # plt.show() # Uncomment if you want to display the plot interactively

def analyze_trends(df):
    """Provides a human-readable interpretation of the trends."""
    print("\n--- Analysis ---")
    
    if len(df) < 5:
        print("Insufficient data for trend analysis (requires 5+ generations).")
        return

    # 1. Convergence Analysis
    final_best = df['OverallBest'].iloc[-1]
    print(f"Final Best Fitness: {final_best:.2f}")

    # Check the slope of the OverallBest in the last 20% of the run
    lookback_period = max(1, int(len(df) * 0.2))
    
    start_val = df['OverallBest'].iloc[-lookback_period]
    improvement_rate = (final_best - start_val) / lookback_period
    
    if improvement_rate > 1.0: # Threshold for "significant improvement" per generation
        print("Interpretation: [Needs More Generations] The 'Overall Best Fitness' was still improving significantly at the end of the run.")
    else:
        print("Interpretation: [Converged] The 'Overall Best Fitness' appears to have plateaued.")
    
    # 2. Diversity Analysis
    final_avg_fit = df['AvgFit'].iloc[-1]
    
    # Check if AvgFit converged close to BestFit
    if final_best > 0 and final_avg_fit < final_best * 0.75:
         print("Interpretation: [Potential Premature Convergence] The 'Average Fitness' is significantly lower than the 'Overall Best'. The population may lack diversity. Consider increasing Population Size (-p).")
    else:
         print("Interpretation: [Good Convergence Quality] The 'Average Fitness' is close to the 'Overall Best', indicating the population converged well.")

def main():
    parser = argparse.ArgumentParser(description="Analyze and visualize EvoAI optimization history logs.")
    parser.add_argument('logfile', type=str, help="Path to the optimization_history_pidXXXX.log file.")
    
    args = parser.parse_args()
    
    df = parse_log_file(args.logfile)
    
    if df is not None:
        plot_optimization(df, args.logfile)
        analyze_trends(df)

if __name__ == '__main__':
    # Ensure required libraries are installed
    try:
        import pandas
        import matplotlib
    except ImportError:
        print("Error: This script requires pandas and matplotlib.")
        print("Please install them: pip install pandas matplotlib")
        sys.exit(1)
    main()