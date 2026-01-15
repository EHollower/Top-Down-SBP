#!/usr/bin/env python3

"""
Generate graph configuration files for SBP benchmarking.

Script generates CSV file with graph parameters for different benchmark scenarios:
- small_k: Few clusters (K <= 20), favorable to Top-Down SBP
- many_k: Many clusters (K >= N/2), favorable to Bottom-Up SBP
- parallel_large: Large graphs for parallel-only testing
"""

import argparse
import csv
import sys
from pathlib import Path


# Small K Scenario: Few clusters, favorable to Top-Down
# High inter-block connectivity (higher p_out than typical)
SMALL_K_CONFIGS = [
    {"n": 100, "k": 5, "p_in": 0.3, "p_out": 0.05},
    {"n": 500, "k": 10, "p_in": 0.3, "p_out": 0.05},
    {"n": 1000, "k": 15, "p_in": 0.3, "p_out": 0.05},
    {"n": 2500, "k": 30, "p_in": 0.3, "p_out": 0.05},
]

# Small K Fast: Sequential-friendly subset (lighter parameters)
# For quick sequential vs parallel speedup analysis
SMALL_K_FAST_CONFIGS = [
    {"n": 100, "k": 5, "p_in": 0.3, "p_out": 0.05},
    {"n": 500, "k": 10, "p_in": 0.3, "p_out": 0.05},
    {"n": 1000, "k": 15, "p_in": 0.3, "p_out": 0.05},
]

# Many K Scenario: Lots of tiny clusters, favorable to Bottom-Up
# Even higher inter-block connectivity
MANY_K_CONFIGS = [
    {"n": 100, "k": 50, "p_in": 0.4, "p_out": 0.08},
    {"n": 200, "k": 75, "p_in": 0.4, "p_out": 0.08},
    {"n": 500, "k": 100, "p_in": 0.4, "p_out": 0.08},
]

# Many K Fast: Sequential-friendly subset with fewer clusters
# For quick testing of Bottom-Up with many clusters
MANY_K_FAST_CONFIGS = [
    {"n": 100, "k": 50, "p_in": 0.4, "p_out": 0.08},
    {"n": 200, "k": 75, "p_in": 0.4, "p_out": 0.08},
]

# Parallel-Only: Large graphs (5K, 8K vertices)
PARALLEL_LARGE_CONFIGS = [
    {"n": 5000, "k": 50, "p_in": 0.3, "p_out": 0.05},
    {"n": 8000, "k": 25, "p_in": 0.3, "p_out": 0.05},
]


def write_graph_config(configs, output_file):
    """Write graph configurations to a CSV file."""
    with open(output_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['n', 'k', 'p_in', 'p_out'])
        writer.writeheader()
        for config in configs:
            writer.writerow(config)
    print(f"Generated {len(configs)} configurations -> {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate graph configurations for SBP benchmarking',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python scripts/generate_graphs.py --scenario small_k
  python scripts/generate_graphs.py --scenario small_k_fast
  python scripts/generate_graphs.py --scenario many_k
  python scripts/generate_graphs.py --scenario many_k_fast
  python scripts/generate_graphs.py --scenario parallel_large
  python scripts/generate_graphs.py --scenario all
        """
    )
    parser.add_argument(
        '--scenario',
        choices=['small_k', 'small_k_fast', 'many_k', 'many_k_fast', 'parallel_large', 'all'],
        required=True,
        help='Benchmark scenario to generate configurations for'
    )
    parser.add_argument(
        '--output',
        default='scripts/graph_config.csv',
        help='Output CSV file path (default: scripts/graph_config.csv)'
    )
    
    args = parser.parse_args()
    
    # Determine which configs to generate
    if args.scenario == 'small_k':
        configs = SMALL_K_CONFIGS
    elif args.scenario == 'small_k_fast':
        configs = SMALL_K_FAST_CONFIGS
    elif args.scenario == 'many_k':
        configs = MANY_K_CONFIGS
    elif args.scenario == 'many_k_fast':
        configs = MANY_K_FAST_CONFIGS
    elif args.scenario == 'parallel_large':
        configs = PARALLEL_LARGE_CONFIGS
    elif args.scenario == 'all':
        configs = SMALL_K_CONFIGS + MANY_K_CONFIGS + PARALLEL_LARGE_CONFIGS
    else:
        print(f"Error: Unknown scenario '{args.scenario}'", file=sys.stderr)
        return 1

    # Create output directory if needed
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Write configurations
    write_graph_config(configs, args.output)

    print(f"\nScenario: {args.scenario}")
    print(f"Total configurations: {len(configs)}")
    print(f"Vertex range: {min(c['n'] for c in configs)} - {max(c['n'] for c in configs)}")
    print(f"Cluster range: {min(c['k'] for c in configs)} - {max(c['k'] for c in configs)}")

    return 0


if __name__ == '__main__':
    sys.exit(main())
