#!/usr/bin/env python3
"""Benchmark A: Quantum Simulation Scalability.
Compares qdd-sim against Qiskit Aer (statevector) and SliQSim.
Saves results to results/bench_a.csv.
"""

import argparse
import csv
import os
import sys
import time
from pathlib import Path

# Add repo root to path
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

from bench.runners import run_qdd_sim, run_aer_sim, run_sliqsim

def parse_circuit_info(qasm_path: Path):
    stem = qasm_path.stem # e.g. ghz_n100, random_clifford_t_n25
    parts = stem.split("_")
    # find n
    n = 0
    family = ""
    for p in parts:
        if p.startswith("n") and p[1:].isdigit():
            n = int(p[1:])
            break
    if "ghz" in stem:
        family = "ghz"
    elif "bv" in stem:
        family = "bv"
    elif "adder" in stem:
        family = "adder"
    elif "grover" in stem:
        family = "grover"
    elif "qft" in stem:
        family = "qft"
    elif "random" in stem:
        family = "random_clifford_t"
    else:
        family = "other"
    return stem, family, n

def main():
    parser = argparse.ArgumentParser(description="Run Benchmark A (Simulation Scalability)")
    parser.add_argument("--circuits-dir", type=str, default="bench/circuits", help="Circuits directory")
    parser.add_argument("--out-csv", type=str, default="results/bench_a.csv", help="Output CSV path")
    parser.add_argument("--timeout", type=float, default=60.0, help="Per-run timeout in seconds")
    parser.add_argument("--max-qubits-aer", type=int, default=29, help="Max qubits to attempt on Aer before predicting OOM")
    args = parser.parse_args()

    circuits_dir = Path(args.circuits_dir)
    out_csv = Path(args.out_csv)
    out_csv.parent.mkdir(parents=True, exist_ok=True)

    qasm_files = sorted(list(circuits_dir.glob("*.qasm")))
    if not qasm_files:
        print(f"No QASM files found in {circuits_dir}!")
        sys.exit(1)

    print(f"Found {len(qasm_files)} benchmark circuits in {circuits_dir}.")
    print(f"Results will be written to {out_csv}.")

    fieldnames = [
        "circuit", "family", "n_qubits", "engine",
        "status", "runtime_sec", "peak_rss_mb",
        "peak_nodes", "final_nodes", "gc_count"
    ]

    records = []

    # Sort files by family, then n_qubits
    parsed_files = [(qf, parse_circuit_info(qf)) for qf in qasm_files]
    parsed_files.sort(key=lambda x: (x[1][1], x[1][2]))

    for qf, (stem, family, n) in parsed_files:
        print(f"\n--- Benchmarking {stem} (family: {family}, n={n}) ---")

        # 1. qdd-sim
        print("  Running qdd-sim...", end="", flush=True)
        res_qdd = run_qdd_sim(str(qf), timeout_sec=args.timeout)
        qdd_stats = res_qdd.get("qdd_stats", {})
        peak_nodes = qdd_stats.get("peak_vector_nodes", qdd_stats.get("peak_matrix_nodes", ""))
        final_nodes = qdd_stats.get("vector_nodes", qdd_stats.get("matrix_nodes", ""))
        gc_count = qdd_stats.get("gc_count", "")
        print(f" {res_qdd['status']} in {res_qdd['runtime_sec']:.3f}s, RSS: {res_qdd['peak_rss_mb']:.1f} MB, peak_nodes: {peak_nodes}")
        records.append({
            "circuit": stem, "family": family, "n_qubits": n, "engine": "qdd-sim",
            "status": res_qdd["status"], "runtime_sec": f"{res_qdd['runtime_sec']:.4f}",
            "peak_rss_mb": f"{res_qdd['peak_rss_mb']:.1f}",
            "peak_nodes": peak_nodes, "final_nodes": final_nodes, "gc_count": gc_count
        })

        # 2. Qiskit Aer
        if n > args.max_qubits_aer:
            print(f"  Skipping Qiskit Aer (n={n} > {args.max_qubits_aer}: 2^{n} dense statevector exceeds 12GB RAM limit)")
            records.append({
                "circuit": stem, "family": family, "n_qubits": n, "engine": "qiskit_aer",
                "status": "PREDICTED_OOM", "runtime_sec": "", "peak_rss_mb": "",
                "peak_nodes": "", "final_nodes": "", "gc_count": ""
            })
        else:
            print("  Running Qiskit Aer...", end="", flush=True)
            res_aer = run_aer_sim(str(qf), timeout_sec=args.timeout)
            print(f" {res_aer['status']} in {res_aer['runtime_sec']:.3f}s, RSS: {res_aer['peak_rss_mb']:.1f} MB")
            records.append({
                "circuit": stem, "family": family, "n_qubits": n, "engine": "qiskit_aer",
                "status": res_aer["status"], "runtime_sec": f"{res_aer['runtime_sec']:.4f}",
                "peak_rss_mb": f"{res_aer['peak_rss_mb']:.1f}",
                "peak_nodes": "", "final_nodes": "", "gc_count": ""
            })

        # 3. SliQSim
        res_sliq = run_sliqsim(str(qf), timeout_sec=args.timeout)
        if res_sliq["status"] != "UNAVAILABLE":
            print(f"  Running SliQSim... {res_sliq['status']} in {res_sliq['runtime_sec']:.3f}s, RSS: {res_sliq['peak_rss_mb']:.1f} MB")
            records.append({
                "circuit": stem, "family": family, "n_qubits": n, "engine": "sliqsim",
                "status": res_sliq["status"], "runtime_sec": f"{res_sliq['runtime_sec']:.4f}",
                "peak_rss_mb": f"{res_sliq['peak_rss_mb']:.1f}",
                "peak_nodes": "", "final_nodes": "", "gc_count": ""
            })

        # Flush to CSV after each circuit so progress is saved
        with open(out_csv, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(records)

    print(f"\nBenchmark A complete! Saved {len(records)} records to {out_csv}")

if __name__ == "__main__":
    main()
