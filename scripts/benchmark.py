import csv
import subprocess
import sys
import threading
from pathlib import Path
from analyze_results import analyze_benchmark_csv


# Define graph configurations
graph_configs = [   
    {"n": 200, "k": 5, "p_in": 0.2, "p_out": 0.02},     # Small
    {"n": 400, "k": 7, "p_in": 0.2, "p_out": 0.02},     # Medium  
    {"n": 800, "k": 9, "p_in": 0.2, "p_out": 0.02},     # Large
]

csv_path = Path("graph_config.csv")
exe_path = Path("../bin/sbp_benchmark.exe")
results_path = Path("results/benchmark_results.csv")



# Write CSV file
with csv_path.open(mode="w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["n", "k", "p_in", "p_out"])  # header
    for cfg in graph_configs:
        writer.writerow([cfg["n"], cfg["k"], cfg["p_in"], cfg["p_out"]])

print(f"Wrote {len(graph_configs)} configurations to {csv_path}")

# Launch the benchmark executable
if not exe_path.exists():
    print(f"Error: executable not found: {exe_path}")
    sys.exit(1)

def stream_reader(stream, target):
    for line in iter(stream.readline, ''):
        target.write(line)
        target.flush()
    stream.close()

try:
    print(f"Running {exe_path}...")
    process = subprocess.Popen(
        [str(exe_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1   # line-buffered
    )
    
    threads = [
        threading.Thread(target=stream_reader, args=(process.stdout, sys.stdout)),
        threading.Thread(target=stream_reader, args=(process.stderr, sys.stderr)),
    ]
    
    for t in threads:
        t.start()
    
    process.wait()
    
    for t in threads:
        t.join()
        
    analyze_benchmark_csv(str(results_path))
    print("Benchmark finished successfully.")
except subprocess.CalledProcessError as e:
    print(f"Benchmark failed with exit code {e.returncode}")
    sys.exit(e.returncode)









