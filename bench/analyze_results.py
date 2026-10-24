#!/usr/bin/env python3
import pandas as pd
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

df_a = pd.read_csv(REPO_ROOT / "results" / "bench_a.csv")
print("=== BENCHMARK A SUMMARY ===")
print(df_a.groupby(["engine", "status"]).size())

print("\nqdd-sim max n completed by family:")
for fam, g in df_a[df_a["engine"] == "qdd-sim"].groupby("family"):
    ok_g = g[g["status"] == "OK"]
    if not ok_g.empty:
        max_n = ok_g["n_qubits"].max()
        row = ok_g[ok_g["n_qubits"] == max_n].iloc[0]
        print(f"  {fam:18s}: max_n={max_n:3d}, time={row['runtime_sec']:.4f}s, RSS={row['peak_rss_mb']:.1f}MB, peak_nodes={row['peak_nodes']}")

print("\nQiskit Aer max n completed by family:")
for fam, g in df_a[df_a["engine"] == "qiskit_aer"].groupby("family"):
    ok_g = g[g["status"] == "OK"]
    if not ok_g.empty:
        max_n = ok_g["n_qubits"].max()
        row = ok_g[ok_g["n_qubits"] == max_n].iloc[0]
        print(f"  {fam:18s}: max_n={max_n:3d}, time={row['runtime_sec']:.4f}s, RSS={row['peak_rss_mb']:.1f}MB")

print("\nSliQSim max n completed by family:")
for fam, g in df_a[df_a["engine"] == "sliqsim"].groupby("family"):
    ok_g = g[g["status"] == "OK"]
    if not ok_g.empty:
        max_n = ok_g["n_qubits"].max()
        row = ok_g[ok_g["n_qubits"] == max_n].iloc[0]
        print(f"  {fam:18s}: max_n={max_n:3d}, time={row['runtime_sec']:.4f}s, RSS={row['peak_rss_mb']:.1f}MB")

df_b = pd.read_csv(REPO_ROOT / "results" / "bench_b.csv")
print("\n=== BENCHMARK B SUMMARY ===")
print("Breakdown by engine and correct:")
print(df_b.groupby(["engine", "correct", "status"]).size())

print("\nAccuracy by engine:")
for eng, g in df_b.groupby("engine"):
    correct_count = (g["correct"] == True).sum()
    total = len(g)
    acc = (correct_count / total) * 100
    print(f"  {eng:16s}: {acc:.1f}% ({correct_count} / {total})")

print("\nTranspiled / Phase / Bug breakdown for qdd-sim vs SliQEC:")
for pfx in ["random_transpiled", "random_phase_shift", "random_bug"]:
    sub = df_b[df_b["pair_name"].str.startswith(pfx)]
    for eng in ["qdd-sim", "sliqec", "qiskit_operator"]:
        eng_sub = sub[sub["engine"] == eng]
        c = (eng_sub["correct"] == True).sum()
        t = len(eng_sub)
        print(f"  {pfx:20s} | {eng:15s}: {c}/{t} correct ({c/t*100:.0f}%)")
