#!/usr/bin/env python3
"""Run Qiskit Operator equivalence check in an isolated process."""

import argparse
import json
import sys
import time
from qiskit import QuantumCircuit
from qiskit.qasm2 import load
from qiskit.quantum_info import Operator

def main():
    parser = argparse.ArgumentParser(description="Run Qiskit Operator equivalence check")
    parser.add_argument("qasm1", type=str, help="Path to first QASM file")
    parser.add_argument("qasm2", type=str, help="Path to second QASM file")
    parser.add_argument("--stats-json", type=str, default="", help="Path to write JSON stats")
    args = parser.parse_args()

    t_start = time.perf_counter()

    import qiskit.qasm2
    qc1 = load(args.qasm1, custom_instructions=qiskit.qasm2.LEGACY_CUSTOM_INSTRUCTIONS, include_path=qiskit.qasm2.LEGACY_INCLUDE_PATH)
    qc2 = load(args.qasm2, custom_instructions=qiskit.qasm2.LEGACY_CUSTOM_INSTRUCTIONS, include_path=qiskit.qasm2.LEGACY_INCLUDE_PATH)

    if qc1.num_qubits != qc2.num_qubits:
        res = False
    elif qc1.num_qubits > 14:
        # 15+ qubits requires 2^15 x 2^15 = 32768 x 32768 complex128 matrix = 16 GB dense matrix!
        print(f"Skipping dense Operator: {qc1.num_qubits} qubits exceeds RAM limit for dense matrix", file=sys.stderr)
        sys.exit(2)
    else:
        op1 = Operator(qc1)
        op2 = Operator(qc2)
        res = op1.equiv(op2)

    t_end = time.perf_counter()

    stats = {
        "engine": "qiskit_operator",
        "n_qubits": qc1.num_qubits,
        "equivalent": bool(res),
        "total_time_sec": t_end - t_start,
        "success": True
    }

    if args.stats_json:
        with open(args.stats_json, "w") as f:
            json.dump(stats, f, indent=2)

    print(f"Qiskit Operator EC finished: {res}, {t_end - t_start:.4f}s")

if __name__ == "__main__":
    main()
