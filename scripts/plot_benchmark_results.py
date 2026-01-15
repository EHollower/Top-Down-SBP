#!/usr/bin/env python3
"""
Generate publication-quality plots from SBP benchmark results.

This script creates visualizations matching the style of the HPDC '25 paper:
- NMI vs Graph Size
- Runtime vs Graph Size  
- MCMC Runtime vs Graph Size
- Memory vs Graph Size
- Parallel Speedup Analysis
"""

import argparse
import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import numpy as np


# Plot style matching the paper
PLOT_STYLE = {
    'bottom_up': {'marker': 'o', 'color': '#1f77b4', 'label': 'Bottom-Up SBP'},
    'top_down': {'marker': 'x', 'color': '#ff7f0e', 'label': 'Top-Down SBP', 'markersize': 10},
}

def setup_plot_style():
    """Configure matplotlib for publication-quality plots."""
    mpl.rcParams['figure.figsize'] = (10, 6)
    mpl.rcParams['font.size'] = 12
    mpl.rcParams['axes.labelsize'] = 14
    mpl.rcParams['axes.titlesize'] = 16
    mpl.rcParams['xtick.labelsize'] = 12
    mpl.rcParams['ytick.labelsize'] = 12
    mpl.rcParams['legend.fontsize'] = 12
    mpl.rcParams['lines.linewidth'] = 2
    mpl.rcParams['lines.markersize'] = 8


def load_results(filepath):
    """Load benchmark results from CSV."""
    df = pd.read_csv(filepath)
    
    # Normalize algorithm names
    df['algorithm'] = df['algorithm'].str.lower().str.replace('topdown', 'top_down').str.replace('bottomup', 'bottom_up')
    
    return df


def aggregate_stats(df):
    """Aggregate results: compute mean and std for each configuration."""
    grouped = df.groupby(['num_vertices', 'target_clusters', 'algorithm', 'execution_mode'])
    
    stats = grouped.agg({
        'runtime_sec': ['mean', 'std'],
        'mcmc_runtime_sec': ['mean', 'std'],
        'memory_mb': ['mean', 'std'],
        'nmi': ['mean', 'std'],
        'mdl_norm': ['mean', 'std'],
    }).reset_index()
    
    # Flatten multi-level columns
    stats.columns = ['_'.join(col).strip('_') if col[1] else col[0] 
                     for col in stats.columns.values]
    
    return stats


def plot_nmi_vs_size(stats, output_dir):
    """Plot NMI vs Graph Size."""
    fig, ax = plt.subplots()
    
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        
        ax.errorbar(
            data['num_vertices'],
            data['nmi_mean'],
            yerr=data['nmi_std'],
            **PLOT_STYLE[algo],
            capsize=5
        )
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('NMI (Normalized Mutual Information)')
    ax.set_title('Clustering Accuracy vs Graph Size')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim([0, 1.05])
    
    plt.tight_layout()
    plt.savefig(output_dir / 'nmi_vs_size.png', dpi=300)
    plt.close()
    print(f"  ✅ nmi_vs_size.png")


def plot_runtime_vs_size(stats, output_dir):
    """Plot Runtime vs Graph Size (log-log scale)."""
    fig, ax = plt.subplots()
    
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        
        ax.errorbar(
            data['num_vertices'],
            data['runtime_sec_mean'],
            yerr=data['runtime_sec_std'],
            **PLOT_STYLE[algo],
            capsize=5
        )
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('Runtime (seconds)')
    ax.set_title('Runtime vs Graph Size')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, which='both')
    ax.legend()
    
    plt.tight_layout()
    plt.savefig(output_dir / 'runtime_vs_size.png', dpi=300)
    plt.close()
    print(f"  ✅ runtime_vs_size.png")


def plot_mcmc_runtime_vs_size(stats, output_dir):
    """Plot MCMC Runtime vs Graph Size."""
    fig, ax = plt.subplots()
    
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        
        ax.errorbar(
            data['num_vertices'],
            data['mcmc_runtime_sec_mean'],
            yerr=data['mcmc_runtime_sec_std'],
            **PLOT_STYLE[algo],
            capsize=5
        )
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('MCMC Runtime (seconds)')
    ax.set_title('MCMC Refinement Time vs Graph Size')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, which='both')
    ax.legend()
    
    plt.tight_layout()
    plt.savefig(output_dir / 'mcmc_runtime_vs_size.png', dpi=300)
    plt.close()
    print(f"  ✅ mcmc_runtime_vs_size.png")


def plot_memory_vs_size(stats, output_dir):
    """Plot Memory Usage vs Graph Size."""
    fig, ax = plt.subplots()
    
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        
        ax.errorbar(
            data['num_vertices'],
            data['memory_mb_mean'],
            yerr=data['memory_mb_std'],
            **PLOT_STYLE[algo],
            capsize=5
        )
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('Peak Memory (MB)')
    ax.set_title('Memory Usage vs Graph Size')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    plt.tight_layout()
    plt.savefig(output_dir / 'memory_vs_size.png', dpi=300)
    plt.close()
    print(f"  ✅ memory_vs_size.png")


def plot_speedup(df, output_dir):
    """Plot parallel speedup (Sequential/Parallel runtime ratio)."""
    # Compute speedup for each graph size and algorithm
    speedup_data = []
    
    for (n, k, algo), group in df.groupby(['num_vertices', 'target_clusters', 'algorithm']):
        seq_data = group[group['execution_mode'] == 'sequential']
        par_data = group[group['execution_mode'] == 'parallel']
        
        if len(seq_data) > 0 and len(par_data) > 0:
            seq_mean = seq_data['runtime_sec'].mean()
            par_mean = par_data['runtime_sec'].mean()
            speedup = seq_mean / par_mean if par_mean > 0 else 0
            
            speedup_data.append({
                'num_vertices': n,
                'algorithm': algo,
                'speedup': speedup
            })
    
    if not speedup_data:
        print("  ⚠️  No speedup data available (need both sequential and parallel runs)")
        return
    
    speedup_df = pd.DataFrame(speedup_data)
    
    fig, ax = plt.subplots()
    
    for algo in ['bottom_up', 'top_down']:
        data = speedup_df[speedup_df['algorithm'] == algo]
        
        ax.plot(
            data['num_vertices'],
            data['speedup'],
            **PLOT_STYLE[algo]
        )
    
    ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5, label='No speedup')
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('Speedup (Sequential / Parallel)')
    ax.set_title('Parallel Speedup vs Graph Size')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    plt.tight_layout()
    plt.savefig(output_dir / 'speedup_analysis.png', dpi=300)
    plt.close()
    print(f"  ✅ speedup_analysis.png")


def print_summary_stats(stats, output_file):
    """Print and save summary statistics."""
    summary_lines = []
    
    summary_lines.append("="*60)
    summary_lines.append("BENCHMARK SUMMARY STATISTICS")
    summary_lines.append("="*60)
    summary_lines.append("")
    
    for algo in ['bottom_up', 'top_down']:
        algo_data = stats[stats['algorithm'] == algo]
        algo_name = PLOT_STYLE[algo]['label']
        
        summary_lines.append(f"{algo_name}:")
        summary_lines.append(f"  NMI:          {algo_data['nmi_mean'].mean():.4f} ± {algo_data['nmi_std'].mean():.4f}")
        summary_lines.append(f"  Runtime:      {algo_data['runtime_sec_mean'].mean():.4f}s ± {algo_data['runtime_sec_std'].mean():.4f}s")
        summary_lines.append(f"  MCMC Runtime: {algo_data['mcmc_runtime_sec_mean'].mean():.4f}s ± {algo_data['mcmc_runtime_sec_std'].mean():.4f}s")
        summary_lines.append(f"  Memory:       {algo_data['memory_mb_mean'].mean():.2f}MB ± {algo_data['memory_mb_std'].mean():.2f}MB")
        summary_lines.append("")
    
    summary_lines.append("="*60)
    
    summary_text = "\n".join(summary_lines)
    print(summary_text)
    
    # Save to file
    with open(output_file, 'w') as f:
        f.write(summary_text)
    
    print(f"\n✅ Summary saved to {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate plots from SBP benchmark results',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        '--input',
        default='results/benchmark_master.csv',
        help='Input CSV file (default: results/benchmark_master.csv)'
    )
    parser.add_argument(
        '--output-dir',
        default='results/plots',
        help='Output directory for plots (default: results/plots)'
    )
    parser.add_argument(
        '--execution-mode',
        choices=['sequential', 'parallel'],
        help='Filter by execution mode (default: use parallel if available)'
    )
    
    args = parser.parse_args()
    
    # Check input file
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        return 1
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"\n{'='*60}")
    print("GENERATING BENCHMARK PLOTS")
    print(f"{'='*60}")
    print(f"Input file: {args.input}")
    print(f"Output directory: {args.output_dir}")
    print(f"{'='*60}\n")
    
    # Setup plot style
    setup_plot_style()
    
    # Load data
    print("Loading data...")
    df = load_results(input_path)
    
    # Filter by execution mode if requested
    if args.execution_mode:
        df = df[df['execution_mode'] == args.execution_mode]
        print(f"Filtered to {args.execution_mode} mode only")
    else:
        # Prefer parallel mode if available
        if 'parallel' in df['execution_mode'].values:
            df = df[df['execution_mode'] == 'parallel']
            print("Using parallel execution mode data")
        else:
            print("Using sequential execution mode data")
    
    print(f"Loaded {len(df)} benchmark runs")
    print(f"Graph sizes: {sorted(df['num_vertices'].unique())}")
    print(f"Algorithms: {sorted(df['algorithm'].unique())}\n")
    
    # Aggregate statistics
    print("Computing statistics...")
    stats = aggregate_stats(df)
    
    # Generate plots
    print("\nGenerating plots:")
    plot_nmi_vs_size(stats, output_dir)
    plot_runtime_vs_size(stats, output_dir)
    plot_mcmc_runtime_vs_size(stats, output_dir)
    plot_memory_vs_size(stats, output_dir)
    
    # Speedup requires both modes
    if not args.execution_mode:
        df_all = load_results(input_path)
        plot_speedup(df_all, output_dir)
    
    # Print and save summary
    print("\nSummary Statistics:")
    summary_file = output_dir / 'summary_statistics.txt'
    print_summary_stats(stats, summary_file)
    
    print(f"\n{'='*60}")
    print("PLOTS GENERATED SUCCESSFULLY")
    print(f"{'='*60}")
    print(f"Output location: {output_dir}/")
    print(f"  - nmi_vs_size.png")
    print(f"  - runtime_vs_size.png")
    print(f"  - mcmc_runtime_vs_size.png")
    print(f"  - memory_vs_size.png")
    if not args.execution_mode:
        print(f"  - speedup_analysis.png")
    print(f"  - summary_statistics.txt")
    print(f"{'='*60}\n")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
