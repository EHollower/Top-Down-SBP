import csv
import subprocess
import sys
import threading
from pathlib import Path
from analyze_results import analyze_benchmark_csv
from utilities import print_indented


# Define graph configurations
graph_configs = {
    "standard" : 
    [   
        {"n": 200, "k": 5, "p_in": 0.2, "p_out": 0.02},     # Small
        # {"n": 400, "k": 7, "p_in": 0.2, "p_out": 0.02},     # Medium  
        # {"n": 800, "k": 9, "p_in": 0.2, "p_out": 0.02},     # Large
    ],
    
    "lfr" :
    [
        {"n": 200, "tau1": 2.3, "tau2": 1.5, "mu": 0.25, "avg_degree": 8, "min_comm_size": 1},
        
    ]
}
    

csv_path = Path("graph_config.csv")
exe_path = Path("../bin/sbp_benchmark.exe")
results_path = Path("results/benchmark_results.csv")

def write_standard_config_to_csv(filePath: Path, config : list):
    # Write CSV file
    with filePath.open(mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["n", "k", "p_in", "p_out"])  # header
        for cfg in config:
            writer.writerow([cfg["n"], cfg["k"], cfg["p_in"], cfg["p_out"]])
            
def write_lfr_config_to_csv(filePath: Path, config : list):
    # Write CSV file
    with filePath.open(mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["n", "tau1", "tau2", "mu", "avg_degree", "min_comm_size"])  # header
        for cfg in config:
            writer.writerow([cfg["n"], cfg["tau1"], cfg["tau2"], cfg["mu"], cfg["avg_degree"], cfg["min_comm_size"]])

def run_process(GraphGenerationMethod : str):
    # Launch the benchmark executable
    if not exe_path.exists():
        print(f"Error: executable not found: {exe_path}")
        sys.exit(1)
    
    def stream_reader(stream, target):
        for line in iter(stream.readline, ''):
            target.write("    " + line)
            target.flush()
        stream.close()
    
    try:
        print(f"Running {exe_path}...")
        process = subprocess.Popen(
            [str(exe_path), GraphGenerationMethod],
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
        
        print_indented(analyze_benchmark_csv, 1, str(results_path))
        print("Benchmark finished successfully.\n\n")
    except subprocess.CalledProcessError as e:
        print(f"Benchmark failed with exit code {e.returncode}")
        sys.exit(e.returncode)

def run_benchmark(graph_configs : dict):
    arg = "standard"
    for k, configs in graph_configs.items():
        if k == "standard" :
            print("Running benchmark with standard-generated graphs")
            write_standard_config_to_csv(csv_path, configs)
            arg = "standard"
        elif k == "lfr" :
            print("Running benchmark with LFR-generated graphs")
            write_lfr_config_to_csv(csv_path, configs)
            arg = "lfr"
        else :
            print("No suitable configurations")
            return
        
        print(f"Wrote {len(configs)} configurations to {csv_path}")
       
        run_process(arg)
        
                
run_benchmark(graph_configs = graph_configs)           







