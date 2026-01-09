import csv
from pathlib import Path
from collections import defaultdict

def analyze_benchmark_csv(csv_file: str):
    csv_path = Path(csv_file)

    if not csv_path.exists():
        print(f"Error: {csv_file} not found!")
        print("Run ./bin/sbp_benchmark first to generate results.")
        return

    # Load CSV into list of dicts
    rows = []
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append({
                "graph_id": int(r["graph_id"]),
                "num_vertices": int(r["num_vertices"]),
                "algorithm": r["algorithm"],
                "runtime_sec": float(r["runtime_sec"]),
                "nmi": float(r["nmi"]),
                "memory_mb": float(r["memory_mb"]),
            })

    print("=========================================")
    print("  SBP Benchmark Results Summary")
    print("=========================================\n")

    # === Average Runtime by Algorithm ===
    print("=== Average Runtime by Algorithm ===")
    runtime_by_alg = defaultdict(list)
    for r in rows:
        runtime_by_alg[r["algorithm"]].append(r["runtime_sec"])

    for alg in sorted(runtime_by_alg):
        avg = sum(runtime_by_alg[alg]) / len(runtime_by_alg[alg])
        print(f"{alg}: {avg:.4f} seconds")
    print()

    # === Average NMI (Accuracy) by Algorithm ===
    print("=== Average NMI (Accuracy) by Algorithm ===")
    nmi_by_alg = defaultdict(list)
    for r in rows:
        nmi_by_alg[r["algorithm"]].append(r["nmi"])

    for alg in sorted(nmi_by_alg):
        avg = sum(nmi_by_alg[alg]) / len(nmi_by_alg[alg])
        print(f"{alg}: {avg:.4f} ({avg * 100:.1f}%)")
    print()

    # === Average Memory Usage by Algorithm ===
    print("=== Average Memory Usage by Algorithm ===")
    mem_by_alg = defaultdict(list)
    for r in rows:
        mem_by_alg[r["algorithm"]].append(r["memory_mb"])

    for alg in sorted(mem_by_alg):
        avg = sum(mem_by_alg[alg]) / len(mem_by_alg[alg])
        print(f"{alg}: {avg:.0f} MB")
    print()

    # === Performance by Graph Size ===
    print("=== Performance by Graph Size ===")
    print("Graph | N    | Algorithm | Runtime (s) | NMI    | Memory (MB)")
    print("------|------|-----------|-------------|--------|------------")

    perf_groups = defaultdict(list)
    for r in rows:
        key = (r["graph_id"], r["num_vertices"], r["algorithm"])
        perf_groups[key].append(r)

    perf_table = []
    for (gid, n, alg), items in perf_groups.items():
        avg_runtime = sum(x["runtime_sec"] for x in items) / len(items)
        avg_nmi = sum(x["nmi"] for x in items) / len(items)
        avg_mem = sum(x["memory_mb"] for x in items) / len(items)
        perf_table.append((gid, n, alg, avg_runtime, avg_nmi, avg_mem))

    perf_table.sort(key=lambda x: (x[0], x[1], x[2]))

    for gid, n, alg, rt, nmi, mem in perf_table:
        print(
            f"{gid:5d} | {n:4d} | {alg:<9} | "
            f"{rt:11.4f} | {nmi:.4f} | {mem:10.0f}"
        )
    print()

    # === Speedup: Top-Down vs Bottom-Up ===
    print("=== Speedup: Top-Down vs. Bottom-Up ===")

    speedup_groups = defaultdict(dict)
    for r in rows:
        key = (r["graph_id"], r["num_vertices"])
        speedup_groups[key].setdefault(r["algorithm"], []).append(r["runtime_sec"])

    print("N    | Top-Down (s) | Bottom-Up (s) | Speedup")
    print("-----|--------------|---------------|--------")

    for (_, n), algs in sorted(speedup_groups.items()):
        if "TopDown" in algs and "BottomUp" in algs:
            td = sum(algs["TopDown"]) / len(algs["TopDown"])
            bu = sum(algs["BottomUp"]) / len(algs["BottomUp"])
            if td > 0:
                print(
                    f"{n:4d} | "
                    f"{td:12.4f} | {bu:13.4f} | {bu / td:6.1f}x"
                )

    print()
    print("=========================================")
    print(f"Full results in: {csv_file}")
    print("=========================================")