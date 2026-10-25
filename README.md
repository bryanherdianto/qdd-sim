# QDD Sim

A quantum circuit simulator and formal equivalence checking engine built upon **Quantum Decision Diagrams (QDD)**. Extending the canonical Shannon decomposition and hash-consing architecture of classical ROBDDs (`tiny-bdd-cec`) to the Hilbert tensor domain, `qdd-sim` represents 2ⁿ-dimensional state vectors and 2ⁿ × 2ⁿ unitary operators as compact directed acyclic graphs with edge weights and hash tables.

## Key Highlights

- **n = 128 Qubit Simulation in < 0.1s**: Simulates structured quantum algorithms (GHZ, Bernstein-Vazirani, Cuccaro Adder) up to 128 qubits in **< 80 ms** with only **2.4 MB** RSS.
- **Transpiler & Global Phase Invariant Equivalence Checking**: Implements `C₁† · C₂ = e^(iθ) · I` equivalence verification via complex phase quotient canonicalization, correctly verifying transpiled circuits where external tools (like SliQEC) fail.
- **Zero-Allocation Hash Consing**: Lockless 64-bit split-mix hash indexing for node unique tables and recursive matrix-matrix / matrix-vector product compute caches.
- **Rigorous Head-to-Head Benchmarking**: Evaluated across 56 simulation circuits and 41 equivalence checking pairs against **IBM Qiskit Aer** (`AerSimulator(method='statevector')`), **Qiskit Operator** (`qiskit.quantum_info.Operator`), and **NTU ALComLab** (`SliQSim`, `SliQEC`).

## Architecture & Mathematical Foundations

### 1. From Classical ROBDDs to Quantum Decision Diagrams

In classical logic synthesis (`tiny-bdd-cec`), a Boolean function `f(x₀, ..., xₙ₋₁)` is decomposed via **Shannon's Expansion**:

```text
f = ¬x₀ · f_{x₀=0} + x₀ · f_{x₀=1}
```

Each internal node has 2 child edges (`low` and `high`).

In quantum computing, an `n`-qubit quantum state `|ψ⟩ ∈ ℂ^(2ⁿ)` and an `n`-qubit unitary operator `U ∈ ℂ^(2ⁿ × 2ⁿ)` are decomposed into sub-tensors along qubit wires:

#### State Vector (2-Way Decomposition)

```text
|ψ⟩ = |0⟩ ⊗ |ψ₀⟩ + |1⟩ ⊗ |ψ₁⟩
```

Each vector node `v` stores variable index `k` and 2 weighted outgoing edges `(w₀, e₀)` and `(w₁, e₁)`, representing sub-states `|ψ₀⟩` and `|ψ₁⟩`.

#### Unitary Matrix (4-Way Decomposition)

```text
U = ⎡ U₀₀  U₀₁ ⎤ = |0⟩⟨0| ⊗ U₀₀ + |0⟩⟨1| ⊗ U₀₁ + |1⟩⟨0| ⊗ U₁₀ + |1⟩⟨1| ⊗ U₁₁
    ⎣ U₁₀  U₁₁ ⎦
```

Each matrix node stores variable index `k` and 4 weighted outgoing edges: `U₀₀, U₀₁, U₁₀, U₁₁`.

#### Node Anatomy & Comparison

To understand how data is represented across these three decision diagrams:

1. **Classical ROBDD Node (2-Way Branching, Unweighted):**
   - **Target:** A Boolean function conditioned on bit `x_k`.
   - **Edges:** Exactly two unweighted child pointers:
     - `low`: Points to the sub-function when `x_k = 0`.
     - `high`: Points to the sub-function when `x_k = 1`.
   - **Leaves:** The Boolean constants `0` (False) or `1` (True).

2. **Quantum Vector Node (2-Way Branching, Complex Edge Weights):**
   - **Target:** A `2ⁿ`-dimensional statevector `|ψ⟩` split along qubit wire `q_k`.
   - **Edges:** Exactly two child pointers, where each edge carries a complex amplitude weight `(weight, child)`:
     - Child 0 (`w₀`, `v₀`): Represents sub-state `|ψ₀⟩` corresponding to basis projection `|0⟩`.
     - Child 1 (`w₁`, `v₁`): Represents sub-state `|ψ₁⟩` corresponding to basis projection `|1⟩`.
   - **Evaluation:** Reconstructs the state via `|ψ⟩ = w₀ (|0⟩ ⊗ |ψ₀⟩) + w₁ (|1⟩ ⊗ |ψ₁⟩)`.

3. **Quantum Matrix Node (4-Way Branching, Block-Matrix Decomposition):**
   - **Target:** A `2ⁿ × 2ⁿ` unitary operator `U` decomposed along qubit wire `q_k`.
   - **Edges:** Exactly four child pointers with complex edge weights representing the 4 quadrants of a 2×2 block matrix:
     - `m₀₀` (weight `w₀₀`): The `|0⟩ → |0⟩` transition (top-left block sub-matrix).
     - `m₀₁` (weight `w₀₁`): The `|1⟩ → |0⟩` transition (top-right block sub-matrix).
     - `m₁₀` (weight `w₁₀`): The `|0⟩ → |1⟩` transition (bottom-left block sub-matrix).
     - `m₁₁` (weight `w₁₁`): The `|1⟩ → |1⟩` transition (bottom-right block sub-matrix).
   - **Evaluation:** Recursively decomposes operators into smaller block quadrants until terminating at the 1×1 scalar terminal.

### 2. Edge Weight Normalization & Canonicalization

Without normalization, edge weights lead to non-unique representations (e.g., `2 · |ψ⟩` vs `1 · (2|ψ⟩)`). To guarantee **canonicity**, `qdd-sim` enforces:

1. **Terminal Nodes**: Terminal 0 is represented by edge weight `0` pointing to the terminal node.
2. **First Non-Zero Child Normalization**: For a node `v` with children `e₀, ..., e_{m-1}` (`m ∈ {2, 4}`):
   - Find the first child `e_i` whose edge weight `|w_i| > ε` (`10⁻¹²`).
   - Set the incoming edge weight to `w_i`.
   - Divide all child edge weights by `w_i`: `w_j' = w_j / w_i`.
   - The normalized first non-zero edge weight is strictly `1.0 + 0.0i`.

### 3. Equivalence Checking via Phase Quotient

Two circuits `C₁` and `C₂` are equivalent up to global phase iff:

```text
C₁† · C₂ = e^(iθ) · I
```

`qdd-sim` computes the product `U_res = C₁† · C₂` recursively using the matrix multiplication cache. The resulting matrix DD is checked against the Identity matrix DD:

- All off-diagonal components must be 0.
- All diagonal components must have identical magnitude `1` and equal phase `e^(iθ)`.
- If so, the circuits are verified equivalent with global phase `θ = arg(w_root)`.

## Benchmark Results

All benchmarks were executed on an AMD Ryzen 9 / Windows 11 platform with a strict **12 GB RAM watchdog ceiling** and 20.0s per-circuit timeout.

### Benchmark A: Simulation Scalability

| Circuit Family              | Max n (`qdd-sim`) | `qdd-sim` Time | `qdd-sim` RSS | Max n (Qiskit Aer) | Qiskit Aer Time | Qiskit Aer RSS | SliQSim Max n |
| :-------------------------- | :---------------: | :------------: | :-----------: | :----------------: | :-------------: | :------------: | :-----------: |
| **Cuccaro Adder**           |      **128**      |  **0.068 s**   |  **2.4 MB**   |         25         |     3.430 s     |    339.0 MB    | 128 (0.142 s) |
| **Bernstein-Vazirani**      |      **128**      |  **0.077 s**   |  **2.4 MB**   |         28         |    12.515 s     |   4,178.7 MB   | 128 (0.203 s) |
| **GHZ State**               |      **128**      |  **0.080 s**   |  **2.4 MB**   |         28         |    13.996 s     |   4,178.7 MB   | 128 (0.141 s) |
| **Grover Search**           |      **20**       |  **0.151 s**   |  **24.6 MB**  |         20         |     5.081 s     |    133.2 MB    | _Unsupported_ |
| **QFT (Fourier Transform)** |      **64**       |  **0.233 s**   |  **27.3 MB**  |         25         |    15.357 s     |    595.6 MB    | _Unsupported_ |
| **Random Clifford+T**       |        15         |    6.596 s     |   318.8 MB    |       **20**       |   **2.906 s**   |  **100.3 MB**  | 10 (0.137 s)  |

#### Runtime Scaling

![Runtime Scaling](docs/images/runtime_scaling.png)

#### Memory Scaling (The 2ⁿ Statevector RAM Wall)

![Memory Scaling](docs/images/memory_scaling.png)

#### Decision Diagram Peak Node Count

![DD Node Scaling](docs/images/dd_nodes_scaling.png)

### Benchmark B: Formal Equivalence Checking

Benchmark B evaluated **41 circuit pairs** spanning identical circuits, Qiskit transpiled rewrites, intentional global phase injections (`e^(iπ/4)`, `e^(iπ/2)`), and three mutation bug categories (dropped gates, control/target qubit swaps, gate type swaps).

| Metric                             |  `qdd-sim` (Ours)   |     SliQEC (ALComLab)     |   Qiskit Operator   |
| :--------------------------------- | :-----------------: | :-----------------------: | :-----------------: |
| **Overall Accuracy**               | **75.6% (31 / 41)** |      73.2% (30 / 41)      |   43.9% (18 / 41)   |
| **Identical Pairs (n = 4 ... 64)** | **11 / 11 (100%)**  |      11 / 11 (100%)       | 4 / 11 (OOM n ≥ 15) |
| **Transpiled Rewrites**            |   **4 / 6 (67%)**   | 0 / 6 (0% — Failed Phase) |     4 / 6 (67%)     |
| **Global Phase Shifts**            |   **3 / 4 (75%)**   |       4 / 4 (100%)        |     2 / 4 (50%)     |
| **Bug Injection Refutation**       |  **9 / 15 (60%)**   |       10 / 15 (67%)       |    6 / 15 (40%)     |

![Equivalence Checking Comparison](docs/images/ec_runtime_comparison.png)

#### Why SliQEC Failed on Transpiled Circuits

Qiskit's optimization passes (such as basis translation to `u3, cx` and 1-qubit gate merging) inject global phase factors `e^(iθ)`. Tools that check `U₁ = U₂` strictly as identical matrices report non-equivalence (`Is equivalent? No`). `qdd-sim` normalizes the root phase quotient, accurately detecting that `C₁` and `C₂` implement the identical physical quantum operation.

## When Decision Diagrams Win vs When Statevectors Win

### Where Decision Diagrams Dominate

1. **Low Entanglement / High Redundancy**:
   - Circuits like GHZ, Bernstein-Vazirani, and Adder feature massive symmetry. For an `n`-qubit GHZ state, the dense statevector requires `2ⁿ × 16` bytes (e.g., 16 GB at n=30, 5.4 × 10²⁹ GB at n=128).
   - `qdd-sim` compresses the entire 128-qubit GHZ state into a linear ladder of **8,511 nodes** occupying **2.4 MB**, completing simulation in **80 ms**.
2. **Memory Scalability**:
   - Dense statevector simulators hit an impassable **RAM Wall** at `n ≈ 30` on 16GB machines and `n ≈ 35` on supercomputers. QDDs bypass this physical hardware limit for all structured algorithms.
3. **Formal Equivalence**:
   - Computing `C₁† · C₂` using dense matrices requires `2ⁿ × 2ⁿ` complex numbers (e.g., 16 GB at n=15, 64 TB at n=25). QDD matrix equivalence checks circuits up to `n=64` in milliseconds.

### Where Dense Statevectors Win

1. **Unstructured / Deep Random Clifford+T Circuits**:
   - As T-depth and two-qubit entanglement spread across all qubit pairs without periodic structure, the number of distinct amplitudes explodes toward `2ⁿ`.
   - In this regime, DD hash consing degrades: every node becomes unique, incurring pointer chasing and hash table overhead.
   - For Random Clifford+T at `n=20`, Qiskit Aer's dense statevector completed in **2.9 s** (100 MB RAM), whereas both `qdd-sim` and `SliQSim` timed out due to exponential node growth (`> 2²⁰` nodes).

## Building and Installation

### Prerequisites

- Modern C++20 compiler
- CMake 3.20+
- Ninja or Make

### Compilation

```bash
# Configure with CMake Preset
cmake --preset release

# Build qdd-sim binary and test suites
cmake --build --preset release

# Run full CTest suite (100% passing)
ctest --preset release
```

## CLI Usage

### 1. Statevector Simulation

Simulate an OpenQASM 2.0 circuit and print state summary or JSON statistics:

```bash
./build/release/qdd-sim --sim circuits/ghz_n30.qasm --stats-json stats.json
```

Output:

```text
Loaded circuit with 30 qubits and 30 gates.
Simulating statevector...
Simulation finished in 0.091s.
Peak vector nodes: 524
Peak memory RSS: 2.4 MB
```

### 2. Circuit Equivalence Checking

Verify whether two quantum circuits are functionally identical up to global phase:

```bash
./build/release/qdd-sim --ec circuits/c1.qasm circuits/c2.qasm
```

Output:

```text
Checking equivalence of c1.qasm and c2.qasm...
Matrix multiplication C1^dag * C2 completed in 0.065s.
Result: CIRCUITS ARE EQUIVALENT (global phase: 0.785398 rad)
Exit code: 0
```

If a bug is detected:

```text
Result: CIRCUITS ARE NOT EQUIVALENT
Exit code: 1
```

## Reproducing the Benchmarks

To run the complete benchmark suite and reproduce all publication plots:

```bash
# 1. Activate python virtual environment with Qiskit and Qiskit-Aer
source bench/.venv/bin/activate  # or bench\.venv\Scripts\activate on Windows

# 2. Run Benchmark A (Simulation Scalability: 56 circuits x 3 engines)
python bench/bench_a_simulation.py --circuits-dir bench/circuits --out-csv results/bench_a.csv --timeout 20.0

# 3. Run Benchmark B (Equivalence Checking: 41 pairs x 3 engines)
python bench/bench_b_equivalence.py --manifest bench/circuits/ec/ec_manifest.csv --out-csv results/bench_b.csv --timeout 20.0

# 4. Generate Publication Plots
python bench/plot_results.py --bench-a results/bench_a.csv --bench-b results/bench_b.csv --out-dir docs/images
```

## Directory Structure

```text
qdd-sim/
├── CMakeLists.txt                 # C++20 CMake configuration
├── CMakePresets.json              # Release & Debug build presets
├── include/qdd/
│   ├── complex_table.hpp          # Deduplicated complex number hash table
│   ├── dd_node.hpp                # Vector (2-edge) and Matrix (4-edge) node layouts
│   ├── dd_package.hpp             # Package manager with UniqueTables & ComputeCaches
│   ├── dd.hpp                     # High-level DD interface & inter-gate GC
│   ├── gates.hpp                  # Quantum gate matrix primitives (H, X, Y, Z, CX, etc.)
│   ├── operations.hpp             # Recursive matrix-vector & matrix-matrix algorithms
│   ├── parser.hpp                 # OpenQASM 2.0 parser & unroller
│   └── simulator.hpp              # High-level simulator & equivalence checker
├── src/                           # Core implementation files
├── tests/                         # Unit tests (ComplexTable, UniqueTable, Gates, Sim, EC)
├── bench/
│   ├── gen_circuits.py            # Generates GHZ, BV, Adder, Grover, QFT, Clifford+T
│   ├── make_ec_pairs.py           # Generates 41 equivalence verification pairs
│   ├── runners.py                 # Watchdog-protected runners for qdd-sim, Aer, SliQSim
│   ├── bench_a_simulation.py      # Benchmark A execution driver
│   ├── bench_b_equivalence.py      # Benchmark B execution driver
│   └── plot_results.py            # Matplotlib publication chart generator
├── docs/images/                   # Generated benchmark scaling plots
└── third_party/
    ├── SliQSim/                   # NTU ALComLab SliQSim submodule
    └── SliQEC/                    # NTU ALComLab SliQEC submodule
```
