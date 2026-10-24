#!/usr/bin/env python3
"""Runner abstractions for quantum simulation and equivalence checking benchmarks.
Dispatches commands through watchdog with timeout and memory supervision.
"""

import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Dict, Any, Optional

from bench.watchdog import run_with_watchdog

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_BIN = REPO_ROOT / "build" / "release" / "qdd-sim.exe"
if not BUILD_BIN.exists():
    BUILD_BIN = REPO_ROOT / "build" / "qdd-sim.exe"

PYTHON_BIN = sys.executable

SLIQSIM_BIN = REPO_ROOT / "third_party" / "SliQSim" / "SliQSim.exe"
if not SLIQSIM_BIN.exists():
    SLIQSIM_BIN = REPO_ROOT / "third_party" / "SliQSim" / "SliQSim"

SLIQEC_BIN = REPO_ROOT / "third_party" / "SliQEC" / "SliQEC.exe"
if not SLIQEC_BIN.exists():
    SLIQEC_BIN = REPO_ROOT / "third_party" / "SliQEC" / "SliQEC"

def run_qdd_sim(
    qasm_path: str,
    timeout_sec: float = 120.0,
    max_memory_mb: float = 12288.0
) -> Dict[str, Any]:
    """Run qdd-sim simulation."""
    if not BUILD_BIN.exists():
        return {"status": "ERROR", "error": f"Binary not found: {BUILD_BIN}"}

    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as f:
        stats_path = f.name

    try:
        cmd = [
            str(BUILD_BIN),
            "--sim", str(qasm_path),
            "--stats-json", stats_path,
            "--quiet"
        ]
        res = run_with_watchdog(cmd, timeout_sec=timeout_sec, max_memory_mb=max_memory_mb)

        stats = {}
        if Path(stats_path).exists() and Path(stats_path).stat().st_size > 0:
            try:
                with open(stats_path, "r") as sf:
                    stats = json.load(sf)
            except Exception:
                pass

        return {
            "engine": "qdd-sim",
            "status": res["status"],
            "runtime_sec": res["runtime_sec"],
            "peak_rss_mb": res["peak_rss_mb"],
            "qdd_stats": stats,
            "stdout": res["stdout"],
            "stderr": res["stderr"]
        }
    finally:
        if Path(stats_path).exists():
            try:
                os.remove(stats_path)
            except OSError:
                pass

def run_aer_sim(
    qasm_path: str,
    timeout_sec: float = 120.0,
    max_memory_mb: float = 12288.0
) -> Dict[str, Any]:
    """Run Qiskit Aer statevector simulation."""
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as f:
        stats_path = f.name

    try:
        script = REPO_ROOT / "bench" / "run_aer_sim.py"
        cmd = [
            PYTHON_BIN,
            str(script),
            str(qasm_path),
            "--stats-json", stats_path
        ]
        res = run_with_watchdog(cmd, timeout_sec=timeout_sec, max_memory_mb=max_memory_mb)

        stats = {}
        if Path(stats_path).exists() and Path(stats_path).stat().st_size > 0:
            try:
                with open(stats_path, "r") as sf:
                    stats = json.load(sf)
            except Exception:
                pass

        return {
            "engine": "qiskit_aer",
            "status": res["status"],
            "runtime_sec": res["runtime_sec"],
            "peak_rss_mb": res["peak_rss_mb"],
            "aer_stats": stats,
            "stdout": res["stdout"],
            "stderr": res["stderr"]
        }
    finally:
        if Path(stats_path).exists():
            try:
                os.remove(stats_path)
            except OSError:
                pass

def run_sliqsim(
    qasm_path: str,
    timeout_sec: float = 120.0,
    max_memory_mb: float = 12288.0
) -> Dict[str, Any]:
    """Run SliQSim if binary is built."""
    if not SLIQSIM_BIN.exists():
        return {
            "engine": "sliqsim",
            "status": "UNAVAILABLE",
            "runtime_sec": 0.0,
            "peak_rss_mb": 0.0,
            "stdout": "",
            "stderr": f"SliQSim binary not found at {SLIQSIM_BIN}"
        }

    with open(qasm_path, "r") as f:
        qasm_text = f.read()

    if "cp(" in qasm_text:
        return {
            "engine": "sliqsim",
            "status": "UNSUPPORTED_GATE",
            "runtime_sec": 0.0,
            "peak_rss_mb": 0.0,
            "stdout": "",
            "stderr": "SliQSim does not support cp gate"
        }

    needs_temp = ("measure" not in qasm_text)
    temp_path = None
    if needs_temp:
        with tempfile.NamedTemporaryFile(suffix=".qasm", delete=False, mode="w") as tf:
            tf.write(qasm_text)
            tf.write("\ncreg c_sliq[1];\nmeasure q[0] -> c_sliq[0];\n")
            temp_path = tf.name
        sim_file = temp_path
    else:
        sim_file = str(qasm_path)

    try:
        cmd = [str(SLIQSIM_BIN), "--sim_qasm", sim_file, "--type", "0", "--shots", "1", "--print_info"]
        res = run_with_watchdog(cmd, timeout_sec=timeout_sec, max_memory_mb=max_memory_mb)
        return {
            "engine": "sliqsim",
            "status": res["status"],
            "runtime_sec": res["runtime_sec"],
            "peak_rss_mb": res["peak_rss_mb"],
            "stdout": res["stdout"],
            "stderr": res["stderr"]
        }
    finally:
        if temp_path and os.path.exists(temp_path):
            try:
                os.remove(temp_path)
            except OSError:
                pass

def run_qdd_ec(
    qasm1: str,
    qasm2: str,
    timeout_sec: float = 120.0,
    max_memory_mb: float = 12288.0
) -> Dict[str, Any]:
    """Run qdd-sim equivalence check."""
    if not BUILD_BIN.exists():
        return {"status": "ERROR", "error": f"Binary not found: {BUILD_BIN}"}

    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as f:
        stats_path = f.name

    try:
        cmd = [
            str(BUILD_BIN),
            "--ec", str(qasm1), str(qasm2),
            "--stats-json", stats_path
        ]
        res = run_with_watchdog(cmd, timeout_sec=timeout_sec, max_memory_mb=max_memory_mb, allow_exit_codes=[0, 1])

        stats = {}
        equivalent = None
        if Path(stats_path).exists() and Path(stats_path).stat().st_size > 0:
            try:
                with open(stats_path, "r") as sf:
                    stats = json.load(sf)
            except Exception:
                pass

        if res["status"] == "OK":
            if res["exit_code"] == 0:
                equivalent = True
            elif res["exit_code"] == 1:
                equivalent = False
        if equivalent is None and "EQUIVALENT" in res["stdout"]:
            equivalent = not ("NOT EQUIVALENT" in res["stdout"])

        return {
            "engine": "qdd-sim",
            "status": res["status"],
            "equivalent": equivalent,
            "runtime_sec": res["runtime_sec"],
            "peak_rss_mb": res["peak_rss_mb"],
            "qdd_stats": stats,
            "stdout": res["stdout"],
            "stderr": res["stderr"]
        }
    finally:
        if Path(stats_path).exists():
            try:
                os.remove(stats_path)
            except OSError:
                pass

def run_qiskit_ec(
    qasm1: str,
    qasm2: str,
    timeout_sec: float = 120.0,
    max_memory_mb: float = 12288.0
) -> Dict[str, Any]:
    """Run Qiskit Operator equivalence check."""
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as f:
        stats_path = f.name

    try:
        script = REPO_ROOT / "bench" / "run_qiskit_ec.py"
        cmd = [
            PYTHON_BIN,
            str(script),
            str(qasm1),
            str(qasm2),
            "--stats-json", stats_path
        ]
        res = run_with_watchdog(cmd, timeout_sec=timeout_sec, max_memory_mb=max_memory_mb)

        stats = {}
        equivalent = None
        if Path(stats_path).exists() and Path(stats_path).stat().st_size > 0:
            try:
                with open(stats_path, "r") as sf:
                    stats = json.load(sf)
                    equivalent = stats.get("equivalent", None)
            except Exception:
                pass

        # Exit code 2 was skip because n > 14
        if res.get("exit_code") == 2:
            status = "SKIPPED_DENSE_OOM"
        else:
            status = res["status"]

        return {
            "engine": "qiskit_operator",
            "status": status,
            "equivalent": equivalent,
            "runtime_sec": res["runtime_sec"],
            "peak_rss_mb": res["peak_rss_mb"],
            "operator_stats": stats,
            "stdout": res["stdout"],
            "stderr": res["stderr"]
        }
    finally:
        if Path(stats_path).exists():
            try:
                os.remove(stats_path)
            except OSError:
                pass

def run_sliqec(
    qasm1: str,
    qasm2: str,
    timeout_sec: float = 120.0,
    max_memory_mb: float = 12288.0
) -> Dict[str, Any]:
    """Run SliQEC if binary is built."""
    if not SLIQEC_BIN.exists():
        return {
            "engine": "sliqec",
            "status": "UNAVAILABLE",
            "runtime_sec": 0.0,
            "peak_rss_mb": 0.0,
            "stdout": "",
            "stderr": f"SliQEC binary not found at {SLIQEC_BIN}"
        }

    cmd = [str(SLIQEC_BIN), "--circuit1", str(qasm1), "--circuit2", str(qasm2)]
    res = run_with_watchdog(cmd, timeout_sec=timeout_sec, max_memory_mb=max_memory_mb, allow_exit_codes=[0])
    equiv = None
    if "Is equivalent? Yes" in res["stdout"]:
        equiv = True
    elif "Is equivalent? No" in res["stdout"]:
        equiv = False
    return {
        "engine": "sliqec",
        "status": res["status"],
        "equivalent": equiv,
        "runtime_sec": res["runtime_sec"],
        "peak_rss_mb": res["peak_rss_mb"],
        "stdout": res["stdout"],
        "stderr": res["stderr"]
    }
