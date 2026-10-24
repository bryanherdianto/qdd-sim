#!/usr/bin/env python3
"""Benchmark B: Quantum Circuit Equivalence Checking.
Compares qdd-sim against Qiskit Operator equivalence checking and SliQEC.
Saves results to results/bench_b.csv.
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

from bench.runners import run_qdd_ec, run_qiskit_ec, run_sliqec

def main():
    parser = argparse.ArgumentParser(description="Run Benchmark B (Equivalence Checking)")
    parser.add_argument("--manifest", type=str, default="bench/circuits/ec/ec_manifest.csv", help="EC Manifest CSV")
    parser.add_argument("--out-csv", type=str, default="results/bench_b.csv", help="Output CSV path")
    parser.add_argument("--timeout", type=float, default=60.0, help="Per-run timeout in seconds")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    out_csv = Path(args.out_csv)
    out_csv.parent.mkdir(parents=True, exist_ok=True)

    if not manifest_path.exists():
        print(f"Manifest not found at {manifest_path}!")
        sys.exit(1)

    with open(manifest_path, "r") as f:
        reader = csv.DictReader(f)
        pairs = list(reader)

    print(f"Found {len(pairs)} equivalence pairs in {manifest_path}.")
    print(f"Results will be written to {out_csv}.")

    ec_dir = manifest_path.parent

    fieldnames = [
        "pair_name", "n_qubits", "expected_equivalent", "engine",
        "status", "reported_equivalent", "correct",
        "runtime_sec", "peak_rss_mb", "final_nodes"
    ]

    records = []

    for p in pairs:
        name = p["name"]
        n_qubits = int(p["n_qubits"])
        expected = p["expected_equivalent"].strip().lower() == "true"
        c1_path = ec_dir / p["c1_path"]
        c2_path = ec_dir / p["c2_path"]

        print(f"\n--- Checking pair: {name} (n={n_qubits}, expected={expected}) ---")

        # 1. qdd-sim
        print("  Running qdd-sim...", end="", flush=True)
        res_qdd = run_qdd_ec(str(c1_path), str(c2_path), timeout_sec=args.timeout)
        rep_qdd = res_qdd.get("equivalent")
        correct_qdd = (rep_qdd == expected) if rep_qdd is not None else False
        qdd_stats = res_qdd.get("qdd_stats", {})
        final_nodes = qdd_stats.get("final_nodes", "")
        print(f" {res_qdd['status']}, eq={rep_qdd} (correct={correct_qdd}) in {res_qdd['runtime_sec']:.3f}s, RSS: {res_qdd['peak_rss_mb']:.1f} MB")
        records.append({
            "pair_name": name, "n_qubits": n_qubits, "expected_equivalent": expected,
            "engine": "qdd-sim", "status": res_qdd["status"],
            "reported_equivalent": rep_qdd, "correct": correct_qdd,
            "runtime_sec": f"{res_qdd['runtime_sec']:.4f}",
            "peak_rss_mb": f"{res_qdd['peak_rss_mb']:.1f}",
            "final_nodes": final_nodes
        })

        # 2. Qiskit Operator
        print("  Running Qiskit Operator...", end="", flush=True)
        res_qiskit = run_qiskit_ec(str(c1_path), str(c2_path), timeout_sec=args.timeout)
        rep_qiskit = res_qiskit.get("equivalent")
        correct_qiskit = (rep_qiskit == expected) if rep_qiskit is not None else False
        print(f" {res_qiskit['status']}, eq={rep_qiskit} (correct={correct_qiskit}) in {res_qiskit['runtime_sec']:.3f}s, RSS: {res_qiskit['peak_rss_mb']:.1f} MB")
        records.append({
            "pair_name": name, "n_qubits": n_qubits, "expected_equivalent": expected,
            "engine": "qiskit_operator", "status": res_qiskit["status"],
            "reported_equivalent": rep_qiskit, "correct": correct_qiskit,
            "runtime_sec": f"{res_qiskit['runtime_sec']:.4f}",
            "peak_rss_mb": f"{res_qiskit['peak_rss_mb']:.1f}",
            "final_nodes": ""
        })

        # 3. SliQEC
        res_sliq = run_sliqec(str(c1_path), str(c2_path), timeout_sec=args.timeout)
        if res_sliq["status"] != "UNAVAILABLE":
            rep_sliq = res_sliq.get("equivalent")
            correct_sliq = (rep_sliq == expected) if rep_sliq is not None else False
            print(f"  Running SliQEC... {res_sliq['status']}, eq={rep_sliq} (correct={correct_sliq}) in {res_sliq['runtime_sec']:.3f}s, RSS: {res_sliq['peak_rss_mb']:.1f} MB")
            records.append({
                "pair_name": name, "n_qubits": n_qubits, "expected_equivalent": expected,
                "engine": "sliqec", "status": res_sliq["status"],
                "reported_equivalent": rep_sliq, "correct": correct_sliq,
                "runtime_sec": f"{res_sliq['runtime_sec']:.4f}",
                "peak_rss_mb": f"{res_sliq['peak_rss_mb']:.1f}",
                "final_nodes": ""
            })

        # Flush to CSV after each pair
        with open(out_csv, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(records)

    print(f"\nBenchmark B complete! Saved {len(records)} records to {out_csv}")

if __name__ == "__main__":
    main()
