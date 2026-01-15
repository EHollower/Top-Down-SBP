#!/usr/bin/env python3
"""
Run comprehensive SBP benchmark suite.

This script orchestrates the full benchmark pipeline:
1. Generates graph configurations for each scenario
2. Runs benchmarks in both sequential and parallel modes
3. Aggregates results into a master CSV file
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path
import shutil


# Benchmark scenarios
SCENARIOS = [
    {
        'name': 'small_k',
        'description': 'Few clusters (K <= 20), parallel mode',
        'graph_scenario': 'small_k',
        'num_runs': 10,
        'modes': ['parallel'],
    },
    {
        'name': 'small_k_fast',
        'description': 'Few clusters (N <= 1000), sequential mode for quick testing',
        'graph_scenario': 'small_k_fast',
        'num_runs': 10,
        'modes': ['sequential'],
    },
    {
        'name': 'many_k',
        'description': 'Many clusters (K >= N/2), parallel mode',
        'graph_scenario': 'many_k',
        'num_runs': 10,
        'modes': ['parallel'],
    },
    {
        'name': 'many_k_fast',
        'description': 'Many clusters (N <= 200), sequential mode for quick testing',
        'graph_scenario': 'many_k_fast',
        'num_runs': 10,
        'modes': ['sequential'],
    },
    {
        'name': 'parallel_large',
        'description': 'Large graphs (10K-20K vertices), parallel only',
        'graph_scenario': 'parallel_large',
        'num_runs': 10,
        'modes': ['parallel'],
    },
]


def run_command(cmd, description, check=True, capture_output=False):
    """Run a shell command with nice output."""
    print(f"\n{'='*60}")
    print(f"{description}")
    print(f"{'='*60}")
    print(f"Command: {' '.join(cmd)}\n")
    
    try:
        if capture_output:
            result = subprocess.run(cmd, check=check, capture_output=True, text=True)
            return result.stdout
        else:
            subprocess.run(cmd, check=check)
            return None
    except subprocess.CalledProcessError as e:
        print(f"\nError: Command failed with return code {e.returncode}", file=sys.stderr)
        if hasattr(e, 'stderr') and e.stderr:
            print(f"Stderr: {e.stderr}", file=sys.stderr)
        raise


def compile_project():
    """Compile the C++ project."""
    run_command(
        ['make', '-C', 'IDE_project', 'config=release', f'-j{get_num_cores()}'],
        "Compiling C++ project"
    )


def get_num_cores():
    """Get number of CPU cores."""
    try:
        import os
        return os.cpu_count() or 4
    except:
        return 4


def generate_graphs(scenario):
    """Generate graph configurations for a scenario."""
    run_command(
        ['python3', 'scripts/generate_graphs.py', '--scenario', scenario],
        f"Generating graph configs for scenario: {scenario}"
    )


def run_benchmark(scenario_name, mode):
    """Run benchmark for a scenario and execution mode."""
    output_file = f"results/benchmark_{scenario_name}_{mode}.csv"
    
    print(f"\n{'='*60}")
    print(f"Running benchmark: {scenario_name} ({mode} mode)")
    print(f"Output: {output_file}")
    print(f"{'='*60}\n")
    
    start_time = time.time()
    
    # Run the C++ benchmark
    subprocess.run(
        ['./bin/sbp_benchmark', 'standard', mode],
        check=True
    )
    
    elapsed = time.time() - start_time
    
    # Move the output file to the scenario-specific location
    if Path('results/benchmark_results.csv').exists():
        shutil.move('results/benchmark_results.csv', output_file)
        print(f"\n✅ Benchmark completed in {elapsed:.1f} seconds")
        print(f"Results saved to: {output_file}")
    else:
        raise FileNotFoundError("Expected results/benchmark_results.csv not found")
    
    return output_file


def aggregate_results(result_files, append_mode=False):
    """Aggregate all result CSV files into a master file.
    
    Args:
        result_files: List of CSV files to aggregate
        append_mode: If True, append to existing master file instead of overwriting
    """
    import csv
    
    master_file = 'results/benchmark_master.csv'
    master_path = Path(master_file)
    
    print(f"\n{'='*60}")
    print(f"Aggregating {len(result_files)} result files into {master_file}")
    if append_mode and master_path.exists():
        print(f"Mode: APPEND (preserving existing data)")
    else:
        print(f"Mode: OVERWRITE")
    print(f"{'='*60}\n")
    
    all_rows = []
    header = None
    
    # Step 1: Load existing master file if in append mode
    existing_count = 0
    if append_mode and master_path.exists():
        print(f"Loading existing master file: {master_file}")
        with open(master_file, 'r') as f:
            reader = csv.DictReader(f)
            header = reader.fieldnames
            for row in reader:
                all_rows.append(row)
                existing_count += 1
        print(f"  Found {existing_count} existing benchmark runs\n")
    
    # Step 2: Load new result files
    new_count = 0
    for result_file in result_files:
        print(f"Reading: {result_file}")
        with open(result_file, 'r') as f:
            reader = csv.DictReader(f)
            if header is None:
                header = reader.fieldnames
            for row in reader:
                all_rows.append(row)
                new_count += 1
    
    # Step 3: Write aggregated results
    with open(master_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(all_rows)
    
    print(f"\n✅ Aggregation complete:")
    if existing_count > 0:
        print(f"   Existing runs: {existing_count}")
    print(f"   New runs: {new_count}")
    print(f"   Total runs: {len(all_rows)}")
    print(f"   Master file: {master_file}")
    
    return master_file


def print_summary(scenarios_run, total_time):
    """Print final summary."""
    print(f"\n{'='*60}")
    print("BENCHMARK SUITE COMPLETE")
    print(f"{'='*60}")
    print(f"Total scenarios: {len(scenarios_run)}")
    print(f"Total runtime: {total_time/60:.1f} minutes ({total_time:.1f} seconds)")
    print(f"\nResults location: results/")
    print(f"  - benchmark_master.csv (aggregated)")
    for scenario in scenarios_run:
        for mode in scenario['modes']:
            print(f"  - benchmark_{scenario['name']}_{mode}.csv")
    print(f"\nNext steps:")
    print(f"  1. Analyze results: python3 scripts/plot_benchmark_results.py")
    print(f"  2. Or use premake: premake5 analyze-results")
    print(f"{'='*60}\n")


def main():
    parser = argparse.ArgumentParser(
        description='Run comprehensive SBP benchmark suite',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
This script will:
  1. Compile the C++ project (if needed)
  2. Run benchmarks for all scenarios (small_k, many_k, parallel_large)
  3. Run each scenario in sequential and/or parallel mode
  4. Aggregate all results into benchmark_master.csv

Estimated runtime: 48-70 minutes (depending on hardware)

Examples:
  python scripts/run_extensive_benchmark.py
  python scripts/run_extensive_benchmark.py --skip-compile
  python scripts/run_extensive_benchmark.py --scenario small_k --mode parallel
  python scripts/run_extensive_benchmark.py --scenario many_k --append
        """
    )
    parser.add_argument(
        '--skip-compile',
        action='store_true',
        help='Skip compilation step (assume binaries are up to date)'
    )
    parser.add_argument(
        '--scenario',
        choices=[s['name'] for s in SCENARIOS],
        help='Run only a specific scenario (default: run all)'
    )
    parser.add_argument(
        '--mode',
        choices=['sequential', 'parallel'],
        help='Run only a specific execution mode (default: run all applicable)'
    )
    parser.add_argument(
        '--append',
        action='store_true',
        help='Append results to existing benchmark_master.csv (default: overwrite)'
    )
    
    args = parser.parse_args()
    
    # Create results directory
    Path('results').mkdir(exist_ok=True)
    
    # Filter scenarios if requested
    scenarios_to_run = SCENARIOS
    if args.scenario:
        scenarios_to_run = [s for s in SCENARIOS if s['name'] == args.scenario]
    
    print(f"\n{'#'*60}")
    print("SBP COMPREHENSIVE BENCHMARK SUITE")
    print(f"{'#'*60}")
    print(f"Scenarios to run: {', '.join(s['name'] for s in scenarios_to_run)}")
    if args.mode:
        print(f"Execution mode filter: {args.mode}")
    print(f"CPU cores available: {get_num_cores()}")
    print(f"{'#'*60}\n")
    
    start_total = time.time()
    
    # Step 1: Compile (unless skipped)
    if not args.skip_compile:
        try:
            compile_project()
        except subprocess.CalledProcessError:
            print("\nCompilation failed. Exiting.", file=sys.stderr)
            return 1
    else:
        print("\n⚠️  Skipping compilation (--skip-compile)")
    
    # Step 2: Run benchmarks for each scenario
    result_files = []
    
    for scenario in scenarios_to_run:
        print(f"\n{'#'*60}")
        print(f"SCENARIO: {scenario['name']}")
        print(f"Description: {scenario['description']}")
        print(f"{'#'*60}")
        
        # Generate graph configurations
        try:
            generate_graphs(scenario['graph_scenario'])
        except subprocess.CalledProcessError:
            print(f"\nFailed to generate graphs for {scenario['name']}", file=sys.stderr)
            continue
        
        # Determine which modes to run
        modes_to_run = scenario['modes']
        if args.mode:
            if args.mode in modes_to_run:
                modes_to_run = [args.mode]
            else:
                print(f"⚠️  Skipping {scenario['name']}: mode {args.mode} not applicable")
                continue
        
        # Run benchmark for each mode
        for mode in modes_to_run:
            try:
                result_file = run_benchmark(scenario['name'], mode)
                result_files.append(result_file)
            except (subprocess.CalledProcessError, FileNotFoundError) as e:
                print(f"\n❌ Benchmark failed for {scenario['name']} ({mode}): {e}", file=sys.stderr)
                continue
    
    # Step 3: Aggregate results
    if result_files:
        try:
            aggregate_results(result_files, append_mode=args.append)
        except Exception as e:
            print(f"\n❌ Failed to aggregate results: {e}", file=sys.stderr)
            return 1
    else:
        print("\n❌ No benchmark results to aggregate", file=sys.stderr)
        return 1
    
    # Step 4: Print summary
    total_time = time.time() - start_total
    print_summary(scenarios_to_run, total_time)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
