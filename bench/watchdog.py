#!/usr/bin/env python3
"""Process watchdog for quantum simulator benchmarking.
Guarantees memory limits (default 12 GB) and execution timeouts to prevent
dense statevector simulations from freezing or swapping the OS.
"""

import os
import sys
import time
import psutil
import subprocess
from typing import Dict, Any, List, Optional

def run_with_watchdog(
    cmd: List[str],
    timeout_sec: float = 120.0,
    max_memory_mb: float = 12288.0,  # 12 GB
    poll_interval_sec: float = 0.05,
    cwd: Optional[str] = None,
    env: Optional[Dict[str, str]] = None,
    allow_exit_codes: Optional[List[int]] = None
) -> Dict[str, Any]:
    """Execute command with strict memory and timeout supervision."""
    valid_codes = allow_exit_codes if allow_exit_codes is not None else [0]
    start_time = time.perf_counter()
    peak_rss_bytes = 0
    status = "OK"
    stdout_data = ""
    stderr_data = ""

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=cwd,
            env=env
        )
    except Exception as e:
        return {
            "status": "ERROR",
            "runtime_sec": 0.0,
            "peak_rss_mb": 0.0,
            "stdout": "",
            "stderr": str(e),
            "exit_code": -1
        }

    try:
        p = psutil.Process(proc.pid)
    except psutil.NoSuchProcess:
        p = None

    killed = False
    while True:
        # Check process status
        ret = proc.poll()
        if ret is not None:
            break

        # Check elapsed time
        elapsed = time.perf_counter() - start_time
        if elapsed > timeout_sec:
            status = "TIMEOUT"
            killed = True
            break

        # Check memory
        if p is not None:
            try:
                current_rss = p.memory_info().rss
                for child in p.children(recursive=True):
                    current_rss += child.memory_info().rss
                if current_rss > peak_rss_bytes:
                    peak_rss_bytes = current_rss

                if current_rss > (max_memory_mb * 1024 * 1024):
                    status = "OOM"
                    killed = True
                    break
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass

        time.sleep(poll_interval_sec)

    if killed:
        if os.name == 'nt':
            try:
                subprocess.run(['taskkill', '/F', '/T', '/PID', str(proc.pid)], capture_output=True)
            except Exception:
                pass
        else:
            try:
                if p is not None:
                    for child in p.children(recursive=True):
                        try:
                            child.kill()
                        except psutil.NoSuchProcess:
                            pass
                    p.kill()
                else:
                    proc.kill()
            except Exception:
                pass
        try:
            stdout_data, stderr_data = proc.communicate(timeout=1.0)
        except Exception:
            pass
    else:
        stdout_data, stderr_data = proc.communicate()

    end_time = time.perf_counter()
    runtime = end_time - start_time

    if status == "OK" and proc.returncode not in valid_codes:
        status = "ERROR"

    return {
        "status": status,
        "runtime_sec": runtime,
        "peak_rss_mb": peak_rss_bytes / (1024.0 * 1024.0),
        "stdout": stdout_data,
        "stderr": stderr_data,
        "exit_code": proc.returncode
    }

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: watchdog.py <command> [args...]")
        sys.exit(1)
    res = run_with_watchdog(sys.argv[1:], timeout_sec=5.0, max_memory_mb=1024.0)
    print(f"Status: {res['status']}, Time: {res['runtime_sec']:.3f}s, Peak RSS: {res['peak_rss_mb']:.1f} MB")
