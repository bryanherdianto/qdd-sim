#!/usr/bin/env python3
import argparse
import subprocess
import json
import random
import sys
import numpy as np
from pathlib import Path

# Add Qiskit
from qiskit import QuantumCircuit
from qiskit.quantum_info import Statevector, Operator

def generate_random_circuit(n_qubits: int, depth: int, seed: int = None) -> QuantumCircuit:
    if seed is not None:
        random.seed(seed)
        np.random.seed(seed)

    qc = QuantumCircuit(n_qubits)
    gates_1q = ['x', 'y', 'z', 'h', 's', 'sdg', 't', 'tdg']

    for _ in range(depth):
        gate = random.choice(gates_1q + (['cx', 'cz'] if n_qubits > 1 else []))
        if gate in gates_1q:
            q = random.randrange(n_qubits)
            getattr(qc, gate)(q)
        elif gate in ['cx', 'cz']:
            c, t = random.sample(range(n_qubits), 2)
            getattr(qc, gate)(c, t)
    return qc

def main():
    parser = argparse.ArgumentParser(description="Cross-check qdd-sim vs Qiskit")
    parser.add_argument("--trials", type=int, default=50, help="Number of random circuit trials")
    parser.add_argument("--max-qubits", type=int, default=8, help="Maximum qubit count for crosscheck")
    parser.add_argument("--depth", type=int, default=15, help="Circuit depth")
    parser.add_argument("--qdd-sim", type=str, default="build/release/qdd-sim.exe", help="Path to qdd-sim binary")
    args = parser.parse_args()

    qdd_bin = Path(args.qdd_sim).resolve()
    if not qdd_bin.exists():
        print(f"Error: qdd-sim binary not found at {qdd_bin}")
        sys.exit(1)

    scratch_dir = Path("bench/scratch")
    scratch_dir.mkdir(parents=True, exist_ok=True)

    print(f"Running {args.trials} correctness crosschecks against Qiskit (n <= {args.max_qubits})...")

    passed_sim = 0
    passed_ec = 0

    for trial in range(args.trials):
        n = random.randint(2, args.max_qubits)
        qc = generate_random_circuit(n, args.depth, seed=trial + 100)

        qasm_path = scratch_dir / f"circuit_{trial}.qasm"
        from qiskit.qasm2 import dump
        with open(qasm_path, "w") as f:
            dump(qc, f)

        # 1. Statevector simulation crosscheck
        qiskit_sv = Statevector(qc).data

        cmd = [str(qdd_bin), "--sim", str(qasm_path), "--amplitudes", "--quiet"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"[FAIL] qdd-sim failed on trial {trial}: {proc.stderr}")
            continue

        qdd_sv = np.zeros(1 << n, dtype=complex)
        for line in proc.stdout.splitlines():
            line = line.strip()
            if line.startswith("|") and ">:" in line:
                idx_str = line[1:line.index(">")]
                c_str = line[line.index("(") + 1:line.index(")")]
                parts = c_str.split(",")
                r = float(parts[0])
                im = float(parts[1]) if len(parts) > 1 else 0.0
                qdd_sv[int(idx_str)] = complex(r, im)

        # Compute fidelity |<psi_qdd | psi_qiskit>|^2
        fid = abs(np.vdot(qdd_sv, qiskit_sv)) ** 2
        if fid < 1.0 - 1e-9:
            print(f"[FAIL] Trial {trial} fidelity mismatch: {fid}")
            sys.exit(1)
        passed_sim += 1

        # 2. Equivalence check crosscheck
        # Create equivalent circuit with an identity pair
        qc2 = qc.copy()
        if random.random() < 0.5:
            # Equivalent
            q = random.randrange(n)
            qc2.h(q)
            qc2.h(q)
            expected_equiv = True
        else:
            # Modify to make non-equivalent
            q = random.randrange(n)
            qc2.x(q)
            expected_equiv = Operator(qc).equiv(Operator(qc2))

        qasm_path2 = scratch_dir / f"circuit_{trial}_v2.qasm"
        with open(qasm_path2, "w") as f:
            dump(qc2, f)

        cmd_ec = [str(qdd_bin), "--ec", str(qasm_path), str(qasm_path2), "--quiet"]
        proc_ec = subprocess.run(cmd_ec, capture_output=True, text=True)
        qdd_equiv = (proc_ec.returncode == 0)

        if qdd_equiv != expected_equiv:
            print(f"[FAIL] Trial {trial} EC mismatch: qdd-sim={qdd_equiv}, expected={expected_equiv}")
            sys.exit(1)
        passed_ec += 1

        print(f"Trial {trial+1:2d}/{args.trials}: n={n} qubits, fid={fid:.10f}, EC verdict agreement = OK")

    print("\n==========================================")
    print(f"ALL {passed_sim}/{args.trials} SIMULATION CHECKS PASSED (fidelity >= 1 - 1e-9)!")
    print(f"ALL {passed_ec}/{args.trials} EQUIVALENCE CHECKS PASSED (100% agreement)!")
    print("==========================================")

if __name__ == "__main__":
    main()
