#!/usr/bin/env python3
"""
Generate publication-quality plots suitable for Beamer presentations.

This script creates separate comparison plots for:
1. Sequential mode: Top-Down vs Bottom-Up
2. Parallel mode: Top-Down vs Bottom-Up
3. Speedup analysis: Sequential vs Parallel (for each algorithm)

All plots are styled for presentation clarity with larger fonts and markers.
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import numpy as np
import sys
from pathlib import Path


# Enhanced plot style for presentations
PLOT_STYLE = {
    'bottom_up': {'marker': 'o', 'color': '#1f77b4', 'label': 'Bottom-Up SBP', 'linewidth': 3, 'markersize': 12},
    'top_down': {'marker': 'x', 'color': '#ff7f0e', 'label': 'Top-Down SBP', 'linewidth': 3, 'markersize': 15},
}


def setup_beamer_style():
    """Configure matplotlib for Beamer presentation plots."""
    mpl.rcParams['figure.figsize'] = (12, 7)
    mpl.rcParams['font.size'] = 16
    mpl.rcParams['axes.labelsize'] = 18
    mpl.rcParams['axes.titlesize'] = 20
    mpl.rcParams['xtick.labelsize'] = 16
    mpl.rcParams['ytick.labelsize'] = 16
    mpl.rcParams['legend.fontsize'] = 16
    mpl.rcParams['lines.linewidth'] = 3
    mpl.rcParams['lines.markersize'] = 12


def load_and_prepare_data(filepath):
    """Load and normalize benchmark data."""
    df = pd.read_csv(filepath)
    df['algorithm'] = df['algorithm'].str.lower().str.replace('topdown', 'top_down').str.replace('bottomup', 'bottom_up')
    return df


def aggregate_stats(df, mode=None):
    """Aggregate results for a specific execution mode."""
    if mode:
        df = df[df['execution_mode'] == mode]
    
    grouped = df.groupby(['num_vertices', 'target_clusters', 'algorithm', 'execution_mode'])
    
    stats = grouped.agg({
        'runtime_sec': ['mean', 'std'],
        'mcmc_runtime_sec': ['mean', 'std'],
        'memory_mb': ['mean', 'std'],
        'nmi': ['mean', 'std'],
    }).reset_index()
    
    stats.columns = ['_'.join(col).strip('_') if col[1] else col[0] 
                     for col in stats.columns.values]
    
    return stats


def plot_sequential_comparison(df, output_dir):
    """Plot Sequential mode comparison: Top-Down vs Bottom-Up."""
    stats = aggregate_stats(df, mode='sequential')
    
    if len(stats) == 0:
        print("  ⚠️  No sequential data available")
        return
    
    # Runtime comparison
    fig, ax = plt.subplots()
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        ax.errorbar(data['num_vertices'], data['runtime_sec_mean'], 
                   yerr=data['runtime_sec_std'], **PLOT_STYLE[algo], capsize=8)
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('Runtime (seconds)')
    ax.set_title('Sequential Mode: Runtime Comparison')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, which='both')
    ax.legend(loc='best')
    plt.tight_layout()
    plt.savefig(output_dir / 'sequential_runtime.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✅ sequential_runtime.png")
    
    # NMI comparison
    fig, ax = plt.subplots()
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        ax.errorbar(data['num_vertices'], data['nmi_mean'],
                   yerr=data['nmi_std'], **PLOT_STYLE[algo], capsize=8)
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('NMI (Clustering Accuracy)')
    ax.set_title('Sequential Mode: Accuracy Comparison')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend(loc='best')
    ax.set_ylim([0, 1.05])
    plt.tight_layout()
    plt.savefig(output_dir / 'sequential_nmi.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✅ sequential_nmi.png")


def plot_parallel_comparison(df, output_dir):
    """Plot Parallel mode comparison: Top-Down vs Bottom-Up."""
    stats = aggregate_stats(df, mode='parallel')
    
    if len(stats) == 0:
        print("  ⚠️  No parallel data available")
        return
    
    # Runtime comparison
    fig, ax = plt.subplots()
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        ax.errorbar(data['num_vertices'], data['runtime_sec_mean'],
                   yerr=data['runtime_sec_std'], **PLOT_STYLE[algo], capsize=8)
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('Runtime (seconds)')
    ax.set_title('Parallel Mode: Runtime Comparison')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, which='both')
    ax.legend(loc='best')
    plt.tight_layout()
    plt.savefig(output_dir / 'parallel_runtime.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✅ parallel_runtime.png")
    
    # NMI comparison
    fig, ax = plt.subplots()
    for algo in ['bottom_up', 'top_down']:
        data = stats[stats['algorithm'] == algo]
        ax.errorbar(data['num_vertices'], data['nmi_mean'],
                   yerr=data['nmi_std'], **PLOT_STYLE[algo], capsize=8)
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('NMI (Clustering Accuracy)')
    ax.set_title('Parallel Mode: Accuracy Comparison')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend(loc='best')
    ax.set_ylim([0, 1.05])
    plt.tight_layout()
    plt.savefig(output_dir / 'parallel_nmi.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✅ parallel_nmi.png")


def plot_speedup_comparison(df, output_dir):
    """Plot Sequential vs Parallel speedup for each algorithm."""
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
        print("  ⚠️  No speedup data available")
        return
    
    speedup_df = pd.DataFrame(speedup_data)
    
    fig, ax = plt.subplots()
    
    for algo in ['bottom_up', 'top_down']:
        data = speedup_df[speedup_df['algorithm'] == algo]
        if len(data) > 0:
            ax.plot(data['num_vertices'], data['speedup'], **PLOT_STYLE[algo])
    
    ax.axhline(y=1, color='gray', linestyle='--', linewidth=2, alpha=0.7, label='No speedup (1x)')
    
    ax.set_xlabel('Graph Size (vertices)')
    ax.set_ylabel('Speedup (Sequential / Parallel)')
    ax.set_title('Parallelization Speedup Analysis')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend(loc='best')
    plt.tight_layout()
    plt.savefig(output_dir / 'speedup_comparison.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✅ speedup_comparison.png")


def plot_combined_runtime(df, output_dir):
    """Plot combined runtime showing all modes."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 7))
    
    # Sequential
    seq_stats = aggregate_stats(df, mode='sequential')
    if len(seq_stats) > 0:
        for algo in ['bottom_up', 'top_down']:
            data = seq_stats[seq_stats['algorithm'] == algo]
            ax1.errorbar(data['num_vertices'], data['runtime_sec_mean'],
                        yerr=data['runtime_sec_std'], **PLOT_STYLE[algo], capsize=8)
        ax1.set_xlabel('Graph Size (vertices)')
        ax1.set_ylabel('Runtime (seconds)')
        ax1.set_title('Sequential Mode')
        ax1.set_xscale('log')
        ax1.set_yscale('log')
        ax1.grid(True, alpha=0.3, which='both')
        ax1.legend(loc='best')
    
    # Parallel
    par_stats = aggregate_stats(df, mode='parallel')
    if len(par_stats) > 0:
        for algo in ['bottom_up', 'top_down']:
            data = par_stats[par_stats['algorithm'] == algo]
            ax2.errorbar(data['num_vertices'], data['runtime_sec_mean'],
                        yerr=data['runtime_sec_std'], **PLOT_STYLE[algo], capsize=8)
        ax2.set_xlabel('Graph Size (vertices)')
        ax2.set_ylabel('Runtime (seconds)')
        ax2.set_title('Parallel Mode (24 threads)')
        ax2.set_xscale('log')
        ax2.set_yscale('log')
        ax2.grid(True, alpha=0.3, which='both')
        ax2.legend(loc='best')
    
    plt.tight_layout()
    plt.savefig(output_dir / 'combined_runtime.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✅ combined_runtime.png")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Generate Beamer-ready plots')
    parser.add_argument('--input', default='results/benchmark_master.csv',
                       help='Input CSV file with benchmark results')
    parser.add_argument('--output-dir', default='results/plots',
                       help='Output directory for plots')
    
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_dir = Path(args.output_dir)
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}", file=sys.stderr)
        return 1
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("\n" + "="*60)
    print("GENERATING BEAMER-READY PLOTS")
    print("="*60)
    print(f"Input: {input_path}")
    print(f"Output: {output_dir}")
    print("="*60 + "\n")
    
    setup_beamer_style()
    df = load_and_prepare_data(input_path)
    
    print("Generating plots:")
    plot_sequential_comparison(df, output_dir)
    plot_parallel_comparison(df, output_dir)
    plot_speedup_comparison(df, output_dir)
    plot_combined_runtime(df, output_dir)
    
    print("\n" + "="*60)
    print("PLOTS GENERATED SUCCESSFULLY")
    print("="*60)
    print(f"Location: {output_dir}/")
    print("Files:")
    print("  - sequential_runtime.png")
    print("  - sequential_nmi.png")
    print("  - parallel_runtime.png")
    print("  - parallel_nmi.png")
    print("  - speedup_comparison.png")
    print("  - combined_runtime.png")
    print("="*60 + "\n")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
