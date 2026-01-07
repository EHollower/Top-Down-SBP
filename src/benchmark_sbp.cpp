#include "sbp_utils.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <string>

namespace sbp {
    void top_down_sbp(Graph&, Blockmodel&, int, int);
    void bottom_up_sbp(Graph&, Blockmodel&, int);
}

struct BenchmarkResult {
    int graph_id;
    int num_vertices;
    int num_edges;
    int target_clusters;
    std::string algorithm;
    int run_number;
    double runtime_seconds;
    size_t memory_mb;
    double nmi;
    double mdl_raw;
    double mdl_normalized;
    int clusters_found;
};

struct GraphConfig {
    int n, k;
    double p_in, p_out;
};

// Generate Stochastic Block Model graph
sbp::Graph generate_sbm_graph(int n, int num_blocks, double p_in, double p_out, 
                               std::vector<int>& true_assignment, int seed) {
    sbp::Graph G;
    G.num_vertices = n;
    G.adj_list.resize(n);
    
    true_assignment.resize(n);
    for(int i = 0; i < n; ++i) {
        true_assignment[i] = i % num_blocks;
    }

    std::mt19937 gen(seed);
    std::uniform_real_distribution<> dist(0.0, 1.0);

    for(int i = 0; i < n; ++i) {
        for(int j = i + 1; j < n; ++j) {
            double p = (true_assignment[i] == true_assignment[j]) ? p_in : p_out;
            if (dist(gen) < p) {
                G.adj_list[i].push_back(j);
                G.adj_list[j].push_back(i);
                G.num_edges++;
            }
        }
    }
    
    // Validate graph
    for(int i = 0; i < n; ++i) {
        for(int j : G.adj_list[i]) {
            if (j < 0 || j >= n) {
                std::cerr << "ERROR: Graph validation failed! Vertex " << i 
                          << " has edge to out-of-bounds vertex " << j 
                          << " (n=" << n << ")" << std::endl;
                std::abort();
            }
        }
    }
    
    return G;
}

// Run single benchmark
BenchmarkResult run_single_benchmark(
    sbp::Graph& G, 
    const std::vector<int>& true_labels,
    int graph_id,
    int target_k,
    const std::string& algorithm, 
    int run_num,
    int proposals_per_split) 
{
    BenchmarkResult result;
    result.graph_id = graph_id;
    result.num_vertices = G.num_vertices;
    result.num_edges = G.num_edges;
    result.target_clusters = target_k;
    result.algorithm = algorithm;
    result.run_number = run_num;
    
    sbp::Blockmodel bm;
    
    // Measure runtime
    auto start = std::chrono::high_resolution_clock::now();
    
    if (algorithm == "TopDown") {
        sbp::top_down_sbp(G, bm, target_k, proposals_per_split);
    } else {
        sbp::bottom_up_sbp(G, bm, target_k);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    
    // Collect metrics
    result.runtime_seconds = elapsed.count();
    result.memory_mb = sbp::get_peak_memory_mb();
    result.nmi = sbp::calculate_nmi(true_labels, bm.assignment);
    result.mdl_raw = sbp::compute_H(bm);
    result.mdl_normalized = sbp::compute_H_normalized(bm);
    result.clusters_found = bm.num_clusters;
    
    return result;
}

// Write CSV header
void write_csv_header(std::ofstream& csv) {
    csv << "graph_id,num_vertices,num_edges,target_clusters,algorithm,run_number,"
        << "runtime_sec,memory_mb,nmi,mdl_raw,mdl_norm,clusters_found\n";
}

// Append result to CSV
void append_result_to_csv(std::ofstream& csv, const BenchmarkResult& result) {
    csv << result.graph_id << ","
        << result.num_vertices << ","
        << result.num_edges << ","
        << result.target_clusters << ","
        << result.algorithm << ","
        << result.run_number << ","
        << std::fixed << std::setprecision(6) << result.runtime_seconds << ","
        << result.memory_mb << ","
        << std::fixed << std::setprecision(6) << result.nmi << ","
        << std::fixed << std::setprecision(2) << result.mdl_raw << ","
        << std::fixed << std::setprecision(6) << result.mdl_normalized << ","
        << result.clusters_found << "\n";
    csv.flush(); // Flush after each write so we can see progress
}

int main() {
    std::cout << "=== SBP Benchmark Suite ===\n";
    std::cout << "Graphs: 1K, 2K, 5K vertices (5 runs each)\n";
    std::cout << "Algorithms: Top-Down SBP, Bottom-Up SBP\n";
    std::cout << "Estimated runtime: ~5-10 minutes\n\n";
    
    // Graph configurations (conservative sizes for stability)
    std::vector<GraphConfig> configs = {
        {200, 5, 0.2, 0.02},     // Small
        {400, 7, 0.2, 0.02},     // Medium  
        {800, 9, 0.2, 0.02}      // Large
    };
    
    const int NUM_RUNS = 5;
    const int PROPOSALS_PER_SPLIT = 50;
    
    // Create results directory if it doesn't exist
    system("mkdir -p results");
    
    // Open CSV file
    std::ofstream csv("results/benchmark_results.csv");
    if (!csv.is_open()) {
        std::cerr << "Error: Could not create results/benchmark_results.csv\n";
        return 1;
    }
    write_csv_header(csv);
    
    int graph_id = 0;
    for (const auto& config : configs) {
        std::cout << "\n=== Graph " << (graph_id + 1) << ": N=" << config.n 
                  << ", K=" << config.k << " ===" << std::endl;
        
        for (int run = 0; run < NUM_RUNS; ++run) {
            std::cout << "  Run " << (run + 1) << "/" << NUM_RUNS << "..." << std::flush;
            
            // Generate graph with unique seed per run
            std::vector<int> true_labels;
            int seed = graph_id * 1000 + run;
            sbp::Graph G = generate_sbm_graph(config.n, config.k, 
                                               config.p_in, config.p_out, 
                                               true_labels, seed);
            
            // Run Top-Down
            auto td_result = run_single_benchmark(
                G, true_labels, graph_id, config.k,
                "TopDown", run, PROPOSALS_PER_SPLIT);
            append_result_to_csv(csv, td_result);
            
            // Run Bottom-Up
            auto bu_result = run_single_benchmark(
                G, true_labels, graph_id, config.k,
                "BottomUp", run, PROPOSALS_PER_SPLIT);
            append_result_to_csv(csv, bu_result);
            
            std::cout << " Done (TD: " << std::fixed << std::setprecision(3) 
                      << td_result.runtime_seconds << "s, BU: " 
                      << bu_result.runtime_seconds << "s)" << std::endl;
        }
        
        graph_id++;
    }
    
    csv.close();
    
    std::cout << "\n✅ Benchmark complete! Results saved to results/benchmark_results.csv\n";
    std::cout << "\nRun './scripts/analyze_results.sh' for quick statistics\n";
    
    return 0;
}
