#!/usr/bin/env python3
"""Plot benchmark results for qdd-sim.
Generates publication-quality charts saved to docs/images/:
1. runtime_scaling.png (Runtime vs n across circuit families)
2. memory_scaling.png (Peak RSS vs n: dense statevector vs QDD)
3. dd_nodes_scaling.png (Peak DD nodes vs n)
4. ec_runtime_comparison.png (Equivalence checking runtime comparison)
"""

import argparse
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# Setup clean style
plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
plt.rcParams['font.sans-serif'] = 'Arial'
plt.rcParams['font.size'] = 11
plt.rcParams['axes.labelsize'] = 12
plt.rcParams['axes.titlesize'] = 13
plt.rcParams['legend.fontsize'] = 10
plt.rcParams['xtick.labelsize'] = 10
plt.rcParams['ytick.labelsize'] = 10

def plot_simulation_scaling(bench_a_csv: Path, out_dir: Path):
    if not bench_a_csv.exists():
        print(f"File {bench_a_csv} does not exist, skipping simulation plots.")
        return

    df = pd.read_csv(bench_a_csv)
    df["n_qubits"] = pd.to_numeric(df["n_qubits"], errors="coerce")
    df["runtime_sec"] = pd.to_numeric(df["runtime_sec"], errors="coerce")
    df["peak_rss_mb"] = pd.to_numeric(df["peak_rss_mb"], errors="coerce")
    df["peak_nodes"] = pd.to_numeric(df["peak_nodes"], errors="coerce")

    # --- Plot 1: Runtime Scaling ---
    fig, ax = plt.subplots(figsize=(10, 6), dpi=300)
    
    # Qiskit Aer curve
    aer_df = df[(df["engine"] == "qiskit_aer") & (df["status"] == "OK")].groupby("n_qubits")["runtime_sec"].mean().reset_index()
    if not aer_df.empty:
        ax.plot(aer_df["n_qubits"], aer_df["runtime_sec"], 'o-', color='#e74c3c', linewidth=2.5, label='Qiskit Aer (Dense Statevector)')

    # qdd-sim curves by family
    qdd_df = df[(df["engine"] == "qdd-sim") & (df["status"] == "OK")]
    family_colors = {
        'ghz': '#2ecc71',
        'bv': '#3498db',
        'adder': '#9b59b6',
        'qft': '#f39c12',
        'random_clifford_t': '#e67e22',
        'grover': '#1abc9c'
    }
    family_labels = {
        'ghz': 'qdd-sim (GHZ)',
        'bv': 'qdd-sim (Bernstein-Vazirani)',
        'adder': 'qdd-sim (Cuccaro Adder)',
        'qft': 'qdd-sim (QFT)',
        'random_clifford_t': 'qdd-sim (Random Clifford+T)',
        'grover': 'qdd-sim (Grover)'
    }

    for fam, col in family_colors.items():
        fam_df = qdd_df[qdd_df["family"] == fam].sort_values("n_qubits")
        if not fam_df.empty:
            ax.plot(fam_df["n_qubits"], fam_df["runtime_sec"], 's--', color=col, linewidth=2.0, label=family_labels.get(fam, fam))

    # SliQSim if available
    sliq_df = df[(df["engine"] == "sliqsim") & (df["status"] == "OK")].groupby("n_qubits")["runtime_sec"].mean().reset_index()
    if not sliq_df.empty:
        ax.plot(sliq_df["n_qubits"], sliq_df["runtime_sec"], '^:', color='#34495e', linewidth=2.0, label='SliQSim')

    # Mark Aer OOM cliff
    ax.axvline(x=29.5, color='#c0392b', linestyle=':', alpha=0.8)
    ax.text(30.5, 0.05, 'Dense Statevector\nRAM Cliff (>=30 qubits OOM)', color='#c0392b', fontsize=10, fontweight='bold', rotation=90, verticalalignment='bottom')

    ax.set_yscale('log')
    ax.set_xlabel('Number of Qubits ($n$)')
    ax.set_ylabel('Execution Time (seconds, log scale)')
    ax.set_title('Simulation Scalability: qdd-sim vs. Qiskit Aer Statevector')
    ax.grid(True, which="both", ls="-", alpha=0.2)
    ax.legend(loc='upper left', frameon=True)
    plt.tight_layout()
    plt.savefig(out_dir / "runtime_scaling.png")
    plt.close()
    print(f"Saved {out_dir / 'runtime_scaling.png'}")

    # --- Plot 2: Peak Memory Scaling ---
    fig, ax = plt.subplots(figsize=(10, 6), dpi=300)

    # Theoretical dense statevector line: 2^n * 16 bytes / (1024^2) MB
    n_dense = np.arange(10, 31)
    dense_mb = (2.0 ** n_dense) * 16.0 / (1024.0 * 1024.0)
    ax.plot(n_dense, dense_mb, 'k:', linewidth=1.5, label='Theoretical Dense Statevector ($2^n \\times 16$ B)')

    if not aer_df.empty:
        aer_mem = df[(df["engine"] == "qiskit_aer") & (df["status"] == "OK")].groupby("n_qubits")["peak_rss_mb"].mean().reset_index()
        ax.plot(aer_mem["n_qubits"], aer_mem["peak_rss_mb"], 'o-', color='#e74c3c', linewidth=2.5, label='Qiskit Aer Peak RSS')

    for fam, col in family_colors.items():
        fam_df = qdd_df[qdd_df["family"] == fam].sort_values("n_qubits")
        if not fam_df.empty:
            ax.plot(fam_df["n_qubits"], fam_df["peak_rss_mb"], 's--', color=col, linewidth=2.0, label=f"qdd-sim ({fam})")

    # Safety limit line
    ax.axhline(y=12288, color='red', linestyle='--', linewidth=1.5, label='Watchdog OOM Threshold (12 GB)')

    ax.set_yscale('log')
    ax.set_xlabel('Number of Qubits ($n$)')
    ax.set_ylabel('Peak Memory Consumption (MB, log scale)')
    ax.set_title('Peak Memory: Decision Diagrams vs. Dense Statevectors')
    ax.grid(True, which="both", ls="-", alpha=0.2)
    ax.legend(loc='lower right', frameon=True)
    plt.tight_layout()
    plt.savefig(out_dir / "memory_scaling.png")
    plt.close()
    print(f"Saved {out_dir / 'memory_scaling.png'}")

    # --- Plot 3: Decision Diagram Peak Nodes ---
    fig, ax = plt.subplots(figsize=(10, 6), dpi=300)
    for fam, col in family_colors.items():
        fam_df = qdd_df[(qdd_df["family"] == fam) & (qdd_df["peak_nodes"].notna()) & (qdd_df["peak_nodes"] > 0)].sort_values("n_qubits")
        if not fam_df.empty:
            ax.plot(fam_df["n_qubits"], fam_df["peak_nodes"], 's-', color=col, linewidth=2.0, label=family_labels.get(fam, fam))

    ax.set_yscale('log')
    ax.set_xlabel('Number of Qubits ($n$)')
    ax.set_ylabel('Peak Decision Diagram Nodes (log scale)')
    ax.set_title('Decision Diagram Compactness: Peak Unique Nodes across Circuit Families')
    ax.grid(True, which="both", ls="-", alpha=0.2)
    ax.legend(loc='upper left', frameon=True)
    plt.tight_layout()
    plt.savefig(out_dir / "dd_nodes_scaling.png")
    plt.close()
    print(f"Saved {out_dir / 'dd_nodes_scaling.png'}")

def plot_equivalence_comparison(bench_b_csv: Path, out_dir: Path):
    if not bench_b_csv.exists():
        print(f"File {bench_b_csv} does not exist, skipping EC plots.")
        return

    df = pd.read_csv(bench_b_csv)
    df["n_qubits"] = pd.to_numeric(df["n_qubits"], errors="coerce")
    df["runtime_sec"] = pd.to_numeric(df["runtime_sec"], errors="coerce")

    fig, ax = plt.subplots(figsize=(11, 6), dpi=300)

    # Filter for comparable pairs
    pairs = df["pair_name"].unique()
    # Group by pair_name and engine
    qdd_data = df[df["engine"] == "qdd-sim"].set_index("pair_name")
    qiskit_data = df[df["engine"] == "qiskit_operator"].set_index("pair_name")

    valid_pairs = [p for p in pairs if p in qdd_data.index and p in qiskit_data.index]
    # Sample subset for clean visualization
    sample_pairs = valid_pairs[:18] # clean readable count

    x = np.arange(len(sample_pairs))
    width = 0.35

    qdd_times = [qdd_data.loc[p, "runtime_sec"] if qdd_data.loc[p, "status"] == "OK" else 0.001 for p in sample_pairs]
    qiskit_times = []
    for p in sample_pairs:
        st = qiskit_data.loc[p, "status"]
        if st == "OK":
            qiskit_times.append(qiskit_data.loc[p, "runtime_sec"])
        elif "OOM" in st or "SKIP" in st:
            qiskit_times.append(60.0) # mark as capped at timeout/OOM
        else:
            qiskit_times.append(0.001)

    rects1 = ax.bar(x - width/2, qdd_times, width, label='qdd-sim (Alternating EC)', color='#2ecc71', alpha=0.9)
    rects2 = ax.bar(x + width/2, qiskit_times, width, label='Qiskit Operator (Dense Matrix)', color='#e74c3c', alpha=0.9)

    ax.set_yscale('log')
    ax.set_ylabel('Verification Time (seconds, log scale)')
    ax.set_title('Equivalence Checking: qdd-sim Alternating DD vs. Qiskit Operator Matrix Equiv')
    ax.set_xticks(x)
    ax.set_xticklabels(sample_pairs, rotation=45, ha='right', fontsize=9)
    ax.legend(loc='upper right', frameon=True)
    ax.grid(True, which="both", ls="-", alpha=0.2)

    plt.tight_layout()
    plt.savefig(out_dir / "ec_runtime_comparison.png")
    plt.close()
    print(f"Saved {out_dir / 'ec_runtime_comparison.png'}")

def main():
    parser = argparse.ArgumentParser(description="Plot benchmark charts")
    parser.add_argument("--bench-a", type=str, default="results/bench_a.csv", help="Benchmark A CSV")
    parser.add_argument("--bench-b", type=str, default="results/bench_b.csv", help="Benchmark B CSV")
    parser.add_argument("--out-dir", type=str, default="docs/images", help="Output directory for PNG charts")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    plot_simulation_scaling(Path(args.bench_a), out_dir)
    plot_equivalence_comparison(Path(args.bench_b), out_dir)

if __name__ == "__main__":
    main()
