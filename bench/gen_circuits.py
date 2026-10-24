#!/usr/bin/env python3
"""Circuit generator for qdd-sim benchmarking.
Generates OpenQASM 2.0 files for standard circuit families:
- GHZ(n)
- Bernstein-Vazirani(n)
- Grover(n)
- Cuccaro Adder(n)
- QFT(n)
- Random Clifford+T(n)
"""

import argparse
import math
import numpy as np
from pathlib import Path
from qiskit import QuantumCircuit
from qiskit.qasm2 import dump

def gen_ghz(n: int) -> QuantumCircuit:
    qc = QuantumCircuit(n)
    qc.h(0)
    for i in range(n - 1):
        qc.cx(i, i + 1)
    return qc

def gen_bv(n: int) -> QuantumCircuit:
    # n qubits: n-1 query qubits + 1 ancilla at n-1
    qc = QuantumCircuit(n)
    anc = n - 1
    qc.x(anc)
    qc.h(anc)
    for i in range(n - 1):
        qc.h(i)
    # Secret: alternating 10101...
    for i in range(0, n - 1, 2):
        qc.cx(i, anc)
    for i in range(n - 1):
        qc.h(i)
    return qc

def gen_grover(n: int) -> QuantumCircuit:
    # 1 marked state: |1...1>
    qc = QuantumCircuit(n)
    for i in range(n):
        qc.h(i)

    # Number of iterations ~ pi/4 * sqrt(2^n)
    # Cap iterations to 20 so runtime remains benchmarkable
    max_iters = min(20, max(1, int(math.floor(math.pi / 4.0 * math.sqrt(2**n)))))

    for _ in range(max_iters):
        # Oracle: flip phase of |1...1>
        # Use H, MCX, H on last qubit
        if n == 1:
            qc.z(0)
        elif n == 2:
            qc.cz(0, 1)
        else:
            qc.h(n - 1)
            qc.mcx(list(range(n - 1)), n - 1)
            qc.h(n - 1)

        # Diffusion operator: 2|s><s| - I = H (2|0><0| - I) H
        for i in range(n):
            qc.h(i)
            qc.x(i)
        if n == 1:
            qc.z(0)
        elif n == 2:
            qc.cz(0, 1)
        else:
            qc.h(n - 1)
            qc.mcx(list(range(n - 1)), n - 1)
            qc.h(n - 1)
        for i in range(n):
            qc.x(i)
            qc.h(i)
    return qc

def gen_cuccaro_adder(n: int) -> QuantumCircuit:
    k = max(1, (n - 2) // 2)
    actual_n = 2 * k + 2
    qc = QuantumCircuit(actual_n)

    cin = 0
    a = [1 + 2 * i for i in range(k)]
    b = [2 + 2 * i for i in range(k)]
    cout = 2 * k + 1

    def maj(c_in, b_i, a_i):
        qc.cx(a_i, b_i)
        qc.cx(a_i, c_in)
        qc.ccx(c_in, b_i, a_i)

    def uma(c_in, b_i, a_i):
        qc.ccx(c_in, b_i, a_i)
        qc.cx(a_i, c_in)
        qc.cx(c_in, b_i)

    # Carry ripple forward
    maj(cin, b[0], a[0])
    for i in range(1, k):
        maj(a[i - 1], b[i], a[i])

    qc.cx(a[k - 1], cout)

    # Ripple backward
    for i in range(k - 1, 0, -1):
        uma(a[i - 1], b[i], a[i])
    uma(cin, b[0], a[0])

    return qc

def gen_qft(n: int) -> QuantumCircuit:
    qc = QuantumCircuit(n)
    for j in range(n):
        qc.h(j)
        for k in range(j + 1, n):
            lam = math.pi / (2 ** (k - j))
            qc.cp(lam, k, j)
    # Optional swaps
    for i in range(n // 2):
        qc.swap(i, n - 1 - i)
    return qc

def gen_random_clifford_t(n: int, depth: int, seed: int = 42) -> QuantumCircuit:
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

def main():
    parser = argparse.ArgumentParser(description="Generate benchmark quantum circuits")
    parser.add_argument("--out", type=str, default="bench/circuits", help="Output directory")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Suite of qubit counts
    qubit_counts = [10, 15, 20, 25, 28, 29, 30, 40, 50, 64, 100, 128]

    print(f"Generating benchmark circuits into {out_dir}...")

    for n in qubit_counts:
        # 1. GHZ
        qc = gen_ghz(n)
        with open(out_dir / f"ghz_n{n}.qasm", "w") as f:
            dump(qc, f)

        # 2. Bernstein-Vazirani
        qc = gen_bv(n)
        with open(out_dir / f"bv_n{n}.qasm", "w") as f:
            dump(qc, f)

        # 3. Grover (up to n=24)
        if n <= 24:
            qc = gen_grover(n)
            with open(out_dir / f"grover_n{n}.qasm", "w") as f:
                dump(qc, f)

        # 4. Cuccaro Adder
        qc = gen_cuccaro_adder(n)
        with open(out_dir / f"adder_n{n}.qasm", "w") as f:
            dump(qc, f)

        # 5. QFT (up to n=64)
        if n <= 64:
            qc = gen_qft(n)
            with open(out_dir / f"qft_n{n}.qasm", "w") as f:
                dump(qc, f)

        # 6. Random Clifford+T (depth=n, up to n=30)
        if n <= 30:
            qc = gen_random_clifford_t(n, depth=n, seed=n * 17)
            with open(out_dir / f"random_clifford_t_n{n}.qasm", "w") as f:
                dump(qc, f)

    print("Generation complete!")

if __name__ == "__main__":
    main()
