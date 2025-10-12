#!/usr/bin/env python3
"""
Pathfinding Log Analysis Script
Analyzes pathfinding case distribution and thrust patterns from verbose log files.

Usage: python3 analyze_pathfinding.py <path_to_log_file>
"""

import sys
import re
from collections import defaultdict


def analyze_log_file(filepath):
    """Analyze pathfinding patterns in the log file."""

    # Initialize counters
    case_counts = defaultdict(int)
    thrust_counts = defaultdict(lambda: {"positive": 0, "negative": 0})
    case_2aii_orientation = defaultdict(int)
    case_2aii_thrust_by_orientation = defaultdict(lambda: {"positive": 0, "negative": 0})
    case_2aii_speeds = defaultdict(list)

    # Regex patterns
    pathfinding_pattern = re.compile(r'\[Pathfinding\] Case (\w+): (\w+) ([-0-9.]+)')
    case_2aii_analysis_pattern = re.compile(
        r'\[Case 2aii Analysis\] Thrust: ([-0-9.]+), (?:Facing: (\w+), Speed: ([0-9.]+)/([0-9.]+)|LOW_VELOCITY \(([0-9.]+)\))'
    )

    # Process file
    with open(filepath, 'r') as f:
        for line in f:
            # Check for pathfinding cases
            match = pathfinding_pattern.search(line)
            if match:
                case_label = match.group(1)
                order_type = match.group(2)
                magnitude = float(match.group(3))

                case_counts[case_label] += 1

                # Track thrust direction for thrust orders
                if order_type == "THRUST":
                    if magnitude < 0:
                        thrust_counts[case_label]["negative"] += 1
                    else:
                        thrust_counts[case_label]["positive"] += 1

            # Check for Case 2aii analysis
            match = case_2aii_analysis_pattern.search(line)
            if match:
                thrust = float(match.group(1))

                if match.group(2):  # Has facing direction
                    facing = match.group(2)
                    speed = float(match.group(3))
                    case_2aii_orientation[facing] += 1

                    if thrust < 0:
                        case_2aii_thrust_by_orientation[facing]["negative"] += 1
                        if facing == "FORWARD":
                            case_2aii_speeds["forward_negative"].append(speed)
                    else:
                        case_2aii_thrust_by_orientation[facing]["positive"] += 1
                        if facing == "FORWARD":
                            case_2aii_speeds["forward_positive"].append(speed)
                else:  # LOW_VELOCITY
                    case_2aii_orientation["LOW_VELOCITY"] += 1
                    if thrust < 0:
                        case_2aii_thrust_by_orientation["LOW_VELOCITY"]["negative"] += 1
                    else:
                        case_2aii_thrust_by_orientation["LOW_VELOCITY"]["positive"] += 1

    return case_counts, thrust_counts, case_2aii_orientation, case_2aii_thrust_by_orientation, case_2aii_speeds


def print_report(case_counts, thrust_counts, case_2aii_orientation, case_2aii_thrust_by_orientation, case_2aii_speeds):
    """Print formatted analysis report."""

    # Calculate total pathfinding calls
    total_calls = sum(case_counts.values())

    print("=" * 60)
    print("PATHFINDING LOG ANALYSIS REPORT")
    print("=" * 60)

    # Overall case distribution
    print(f"\n### Overall Case Distribution ({total_calls:,} total calls)")
    print("-" * 50)

    case_names = {
        "1a": "drift",
        "1b": "aligned thrust",
        "1c": "turn to align",
        "2ai": "thrust and drift",
        "2aii": "thrust-turn-thrust",
        "2b": "turn then thrust"
    }

    for case in ["1a", "1b", "1c", "2ai", "2aii", "2b"]:
        count = case_counts.get(case, 0)
        pct = (count / total_calls * 100) if total_calls > 0 else 0
        print(f"Case {case:4} ({case_names[case]:20}): {count:5,} ({pct:5.1f}%)")

    # Thrust direction analysis
    print("\n### Thrust Direction Analysis")
    print("-" * 50)

    for case in ["1b", "2ai", "2aii"]:
        if case in thrust_counts:
            pos = thrust_counts[case]["positive"]
            neg = thrust_counts[case]["negative"]
            total = pos + neg
            if total > 0:
                print(f"\nCase {case} ({case_names[case]}):")
                print(f"  Positive thrust: {pos:5,} ({pos/total*100:5.1f}%)")
                print(f"  Negative thrust: {neg:5,} ({neg/total*100:5.1f}%)")

    # Case 2aii orientation analysis
    if case_2aii_orientation:
        print("\n### Case 2aii Orientation Analysis")
        print("-" * 50)

        total_2aii = sum(case_2aii_orientation.values())
        print(f"\nTotal Case 2aii Analysis logs: {total_2aii:,}")
        print("\nFacing direction breakdown:")

        for facing in ["FORWARD", "BACKWARD", "SIDEWAYS", "LOW_VELOCITY"]:
            if facing in case_2aii_orientation:
                count = case_2aii_orientation[facing]
                pct = (count / total_2aii * 100) if total_2aii > 0 else 0
                print(f"  {facing:12}: {count:5,} ({pct:5.1f}%)")

        print("\n### Case 2aii: Thrust by Facing Direction")
        print("-" * 50)

        for facing in ["FORWARD", "BACKWARD", "SIDEWAYS", "LOW_VELOCITY"]:
            if facing in case_2aii_thrust_by_orientation:
                pos = case_2aii_thrust_by_orientation[facing]["positive"]
                neg = case_2aii_thrust_by_orientation[facing]["negative"]
                total = pos + neg
                if total > 0:
                    print(f"\n{facing} facing ships:")
                    print(f"  Positive thrust: {pos:5,} ({pos/total*100:5.1f}%)")
                    print(f"  Negative thrust: {neg:5,} ({neg/total*100:5.1f}%)")

        # Average speed analysis for forward-facing ships
        if case_2aii_speeds.get("forward_negative") or case_2aii_speeds.get("forward_positive"):
            print("\n### Speed Analysis for Forward-Facing Case 2aii")
            print("-" * 50)

            if case_2aii_speeds.get("forward_negative"):
                speeds = case_2aii_speeds["forward_negative"]
                avg_speed = sum(speeds) / len(speeds)
                print(f"Forward-facing with negative thrust:")
                print(f"  Average speed: {avg_speed:.1f}/30.0 ({avg_speed/30*100:.0f}% of max)")

            if case_2aii_speeds.get("forward_positive"):
                speeds = case_2aii_speeds["forward_positive"]
                avg_speed = sum(speeds) / len(speeds)
                print(f"Forward-facing with positive thrust:")
                print(f"  Average speed: {avg_speed:.1f}/30.0 ({avg_speed/30*100:.0f}% of max)")

    print("\n" + "=" * 60)
    print("END OF REPORT")
    print("=" * 60)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        print(f"Error: Expected 1 argument, got {len(sys.argv) - 1}")
        sys.exit(1)

    log_file = sys.argv[1]

    try:
        print(f"Analyzing log file: {log_file}")
        case_counts, thrust_counts, case_2aii_orientation, case_2aii_thrust_by_orientation, case_2aii_speeds = analyze_log_file(log_file)
        print_report(case_counts, thrust_counts, case_2aii_orientation, case_2aii_thrust_by_orientation, case_2aii_speeds)
    except FileNotFoundError:
        print(f"Error: File '{log_file}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error analyzing log file: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()