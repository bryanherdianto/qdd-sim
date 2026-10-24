#!/usr/bin/env python3
"""Equivalence benchmark pair generator for qdd-sim.
Generates pairs (c1.qasm, c2.qasm) across multiple categories:
1. identical: C1 == C2
2. transpiled: C2 is C1 transpiled to a different basis (e.g., u, cx vs rx, ry, rz, cx) or optimized with Qiskit Level 2/3
3. reordered: Commuting gates swapped in execution order
4. global_phase: C2 has an explicit global phase shift
5. bug_gate_swap: Non-equivalent pair with one gate changed (e.g., T -> S or H -> X)
6. bug_cx_swap: Non-equivalent pair with control/target reversed on one CX
7. bug_missing_gate: Non-equivalent pair with one gate removed
"""

import argparse
import copy
import math
import random
from pathlib import Path
from qiskit import QuantumCircuit, transpile
from qiskit.qasm2 import dump

def gen_ghz(n: int) -> QuantumCircuit:
    qc = QuantumCircuit(n)
    qc.h(0)
    for i in range(n - 1):
        qc.cx(i, i + 1)
    return qc

def gen_bv(n: int) -> QuantumCircuit:
    qc = QuantumCircuit(n)
    anc = n - 1
    qc.x(anc)
    qc.h(anc)
    for i in range(n - 1):
        qc.h(i)
    for i in range(0, n - 1, 2):
        qc.cx(i, anc)
    for i in range(n - 1):
        qc.h(i)
    return qc

def gen_random_clifford_t(n: int, depth: int, seed: int = 42) -> QuantumCircuit:
    import numpy as np
    rng = np.random.default_rng(seed)
    qc = QuantumCircuit(n)
    gates_1q = ['h', 's', 't']
    for _ in range(depth):
        for q in range(n):
            g = rng.choice(gates_1q)
            getattr(qc, g)(q)
        if n > 1:
            perm = rng.permutation(n)
            for i in range(0, n - 1, 2):
                qc.cx(int(perm[i]), int(perm[i + 1]))
    return qc

def mutate_bug(qc: QuantumCircuit, bug_type: str, seed: int = 123) -> QuantumCircuit:
    mutated = qc.copy()
    rng = random.Random(seed)
    # Get all gate instructions
    ops = [instr for instr in mutated.data]
    if not ops:
        return mutated
    
    idx = rng.randint(0, len(ops) - 1)
    target_op = ops[idx]

    if bug_type == "missing_gate":
        # Remove the gate
        del mutated.data[idx]
    elif bug_type == "cx_swap":
        # Find first CX and swap control and target
        cx_indices = [i for i, inst in enumerate(mutated.data) if inst.operation.name == 'cx']
        if cx_indices:
            c_idx = rng.choice(cx_indices)
            inst = mutated.data[c_idx]
            qubits = list(inst.qubits)
            if len(qubits) >= 2:
                from qiskit.circuit.library import CXGate
                mutated.data[c_idx] = inst.replace(qubits=[qubits[1], qubits[0]])
    elif bug_type == "gate_swap":
        # Replace 1Q gate with a different one
        names_1q = {'h': 'x', 'x': 'h', 's': 't', 't': 's', 'z': 's'}
        one_q_indices = [i for i, inst in enumerate(mutated.data) if inst.operation.name in names_1q]
        if one_q_indices:
            q_idx = rng.choice(one_q_indices)
            orig_name = mutated.data[q_idx].operation.name
            new_name = names_1q[orig_name]
            from qiskit.circuit.library import HGate, XGate, SGate, TGate
            gate_map = {'h': HGate(), 'x': XGate(), 's': SGate(), 't': TGate()}
            mutated.data[q_idx] = mutated.data[q_idx].replace(operation=gate_map[new_name])
    return mutated

def main():
    parser = argparse.ArgumentParser(description="Generate equivalence benchmark circuit pairs")
    parser.add_argument("--out", type=str, default="bench/circuits/ec", help="Output directory")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    pairs = [] # (pair_name, qc1, qc2, expected_equivalent)

    # 1. Identical pairs
    for n in [5, 10, 15, 20, 25, 30]:
        c1 = gen_ghz(n)
        c2 = gen_ghz(n)
        pairs.append((f"ghz_identical_n{n}", c1, c2, True))

    for n in [5, 10, 15, 20, 25, 30]:
        c1 = gen_bv(n)
        c2 = gen_bv(n)
        pairs.append((f"bv_identical_n{n}", c1, c2, True))

    for n in [4, 8, 12, 16]:
        c1 = gen_random_clifford_t(n, depth=n*2, seed=n*7)
        c2 = c1.copy()
        pairs.append((f"random_identical_n{n}", c1, c2, True))

    # 2. Transpiled / Optimized pairs (Equivalent)
    for n in [4, 6, 8, 10, 12, 16]:
        c1 = gen_random_clifford_t(n, depth=n*3, seed=n*13)
        # Transpile to u, cx basis with optimization_level=2
        c2 = transpile(c1, basis_gates=['u', 'cx'], optimization_level=2)
        pairs.append((f"random_transpiled_n{n}", c1, c2, True))

    # 3. Global Phase Shift (Equivalent up to phase)
    for n in [4, 8, 12, 16]:
        c1 = gen_random_clifford_t(n, depth=n*2, seed=n*23)
        c2 = c1.copy()
        c2.global_phase += math.pi / 4.0
        pairs.append((f"random_phase_shift_n{n}", c1, c2, True))

    # 4. Bug-injected pairs (NOT Equivalent)
    for n in [4, 8, 12, 16, 20]:
        c1 = gen_random_clifford_t(n, depth=n*2, seed=n*31)
        c2 = mutate_bug(c1, "missing_gate", seed=n)
        pairs.append((f"random_bug_missing_n{n}", c1, c2, False))

    for n in [4, 8, 12, 16, 20]:
        c1 = gen_random_clifford_t(n, depth=n*2, seed=n*41)
        c2 = mutate_bug(c1, "cx_swap", seed=n)
        pairs.append((f"random_bug_cxswap_n{n}", c1, c2, False))

    for n in [4, 8, 12, 16, 20]:
        c1 = gen_random_clifford_t(n, depth=n*2, seed=n*53)
        c2 = mutate_bug(c1, "gate_swap", seed=n)
        pairs.append((f"random_bug_gateswap_n{n}", c1, c2, False))

    # Dump all pairs and manifest
    manifest_lines = ["name,c1_path,c2_path,expected_equivalent,n_qubits\n"]
    for name, c1, c2, expected in pairs:
        c1_path = out_dir / f"{name}_c1.qasm"
        c2_path = out_dir / f"{name}_c2.qasm"
        with open(c1_path, "w") as f:
            dump(c1, f)
        with open(c2_path, "w") as f:
            dump(c2, f)
        manifest_lines.append(f"{name},{c1_path.name},{c2_path.name},{expected},{c1.num_qubits}\n")

    manifest_file = out_dir / "ec_manifest.csv"
    with open(manifest_file, "w") as f:
        f.writelines(manifest_lines)

    print(f"Generated {len(pairs)} equivalence pairs into {out_dir}. Manifest: {manifest_file}")

if __name__ == "__main__":
    main()
