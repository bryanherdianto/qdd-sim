#!/usr/bin/env python3
"""Run Qiskit Aer statevector simulation in an isolated process."""

import argparse
import json
import sys
import time
from qiskit import QuantumCircuit
from qiskit.qasm2 import load
from qiskit_aer import AerSimulator

def main():
    parser = argparse.ArgumentParser(description="Run Qiskit Aer statevector simulation")
    parser.add_argument("qasm", type=str, help="Path to QASM file")
    parser.add_argument("--stats-json", type=str, default="", help="Path to write JSON stats")
    args = parser.parse_args()

    t_start = time.perf_counter()
    
    import qiskit.qasm2
    qc = load(
        args.qasm,
        custom_instructions=qiskit.qasm2.LEGACY_CUSTOM_INSTRUCTIONS,
        include_path=qiskit.qasm2.LEGACY_INCLUDE_PATH
    )
    # Remove measurements if any for statevector simulation
    qc.remove_final_measurements(inplace=True)
    qc.save_statevector()

    t_load = time.perf_counter()

    # Run statevector simulation
    backend = AerSimulator(method="statevector")
    job = backend.run(qc)
    result = job.result()
    
    t_end = time.perf_counter()

    if not result.success:
        print(f"Aer simulation failed: {result.status}", file=sys.stderr)
        sys.exit(1)

    sim_time = t_end - t_load
    total_time = t_end - t_start

    stats = {
        "engine": "qiskit_aer",
        "n_qubits": qc.num_qubits,
        "n_gates": len(qc.data),
        "sim_time_sec": sim_time,
        "total_time_sec": total_time,
        "success": True
    }

    if args.stats_json:
        with open(args.stats_json, "w") as f:
            json.dump(stats, f, indent=2)

    print(f"Aer finished: {qc.num_qubits} qubits, {sim_time:.4f}s sim time")

if __name__ == "__main__":
    main()
