#include "headers/utils/sbp_utils.hpp"
#include "headers/graph_generation.hpp"

#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <omp.h>

using namespace sbp;

namespace sbp {
    void top_down_sbp(utils::Graph&, utils::BlockModel&, utils::ClusterCount, utils::ProposalCount);
    void bottom_up_sbp(utils::Graph&, utils::BlockModel&, utils::ClusterCount);
}

struct BenchmarkResult {
    int graph_id;
    int num_vertices;
    int num_edges;
    int target_clusters;
    std::string algorithm;
    std::string execution_mode;
    int run_number;
    double runtime_seconds;
    double mcmc_runtime_seconds;
    size_t memory_mb;
    double nmi;
    double mdl_raw;
    double mdl_normalized;
    int clusters_found;
};


// Run single benchmark
BenchmarkResult run_single_benchmark(
    utils::Graph& G, 
    const std::vector<utils::ClusterId>& true_labels,
    int graph_id,
    utils::ClusterCount target_k,
    const std::string& algorithm,
    const std::string& execution_mode,
    int run_num,
    utils::ProposalCount proposals_per_split) 
{
    BenchmarkResult result;
    result.graph_id = graph_id;
    result.num_vertices = G.get_vertex_count();
    result.num_edges = G.get_edge_count();
    result.target_clusters = target_k;
    result.algorithm = algorithm;
    result.execution_mode = execution_mode;
    result.run_number = run_num;
    
    utils::BlockModel bm;
    
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
    result.mcmc_runtime_seconds = bm.total_mcmc_time;
    result.memory_mb = utils::get_peak_memory_mb();
    result.nmi = utils::calculate_nmi(true_labels, bm.cluster_assignment);
    result.mdl_raw = utils::compute_H(bm);
    result.mdl_normalized = utils::compute_H_normalized(bm);
    result.clusters_found = bm.cluster_count;
    
    return result;
}

// Write CSV header
void write_csv_header(std::ofstream& csv) {
    csv << "graph_id,num_vertices,num_edges,target_clusters,algorithm,execution_mode,run_number,"
        << "runtime_sec,mcmc_runtime_sec,memory_mb,nmi,mdl_raw,mdl_norm,clusters_found\n";
}

// Append result to CSV
void append_result_to_csv(std::ofstream& csv, const BenchmarkResult& result) {
    csv << result.graph_id << ","
        << result.num_vertices << ","
        << result.num_edges << ","
        << result.target_clusters << ","
        << result.algorithm << ","
        << result.execution_mode << ","
        << result.run_number << ","
        << std::fixed << std::setprecision(6) << result.runtime_seconds << ","
        << std::fixed << std::setprecision(6) << result.mcmc_runtime_seconds << ","
        << result.memory_mb << ","
        << std::fixed << std::setprecision(6) << result.nmi << ","
        << std::fixed << std::setprecision(2) << result.mdl_raw << ","
        << std::fixed << std::setprecision(6) << result.mdl_normalized << ","
        << result.clusters_found << "\n";
    csv.flush(); // Flush after each write so we can see progress
}

int main(int argc, char* argv[]) {
    GraphGenerationMethod graphGenerationMethod = GraphGenerationMethod::STANDARD;
    std::string execution_mode = "parallel"; // default to parallel

    if (argc > 1) {
        std::string arg = argv[1];
        if(arg == "standard")
            graphGenerationMethod = GraphGenerationMethod::STANDARD;
        if (arg == "lfr")
            graphGenerationMethod = GraphGenerationMethod::LFR;
    }
    
    if (argc > 2) {
        execution_mode = argv[2];
        if (execution_mode == "sequential") {
            omp_set_num_threads(1);
        }
        // For "parallel" mode, use default (all available threads)
    }

    std::cout << "=== SBP Benchmark Suite ===\n";
    std::cout << "Graphs: 1K, 2K, 5K vertices (5 runs each)\n";
    std::cout << "Algorithms: Top-Down SBP, Bottom-Up SBP\n";

    if (graphGenerationMethod == GraphGenerationMethod::STANDARD) {
        std::cout << "Graph generation method: standard\n";
    }
    else {
        if (graphGenerationMethod == GraphGenerationMethod::LFR) {
            std::cout << "Graph generation method: lfr\n";
        }
    }
    
    std::cout << "Execution mode: " << execution_mode << "\n";
    if (execution_mode == "sequential") {
        std::cout << "Threads: 1\n";
    } else {
        std::cout << "Threads: " << omp_get_max_threads() << "\n";
    }

    std::cout << "Estimated runtime: ~5-10 minutes\n\n";
    
    // Graph configurations (conservative sizes for stability)
    std::vector<GraphConfigBase*> configs = read_graph_configs_from_csv("scripts/graph_config.csv", graphGenerationMethod);
    
    const int NUM_RUNS = 5;
    const int PROPOSALS_PER_SPLIT = 50;
    
    // Create results directory if it doesn't exist
    std::filesystem::create_directories("results");
    
    // Open CSV file
    std::ofstream csv("results/benchmark_results.csv");
    if (!csv.is_open()) {
        std::cerr << "Error: Could not create results/benchmark_results.csv\n";
        return 1;
    }
    write_csv_header(csv);
    
    int graph_id = 0;
    for (auto& config : configs) {
        std::cout << "\n=== Graph " << (graph_id + 1) << ": N=" << config->n;
        if (graphGenerationMethod == GraphGenerationMethod::STANDARD)
            std::cout << ", K=" << config->k;
        std::cout << " ===" << std::endl;
        
        for (int run = 0; run < NUM_RUNS; ++run) {
            std::cout << "  Run " << (run + 1) << "/" << NUM_RUNS << "..." << std::flush;
            
            // Generate graph with unique seed per run
            std::vector<utils::ClusterId> true_labels;
            int seed = graph_id * 1000 + run;

            utils::Graph G = config->generateGraph(true_labels, seed);
            
            // Run Top-Down
            auto td_result = run_single_benchmark(
                G, true_labels, graph_id, config->k,
                "TopDown", execution_mode, run, PROPOSALS_PER_SPLIT);
            append_result_to_csv(csv, td_result);
            
            // Run Bottom-Up
            auto bu_result = run_single_benchmark(
                G, true_labels, graph_id, config->k,
                "BottomUp", execution_mode, run, PROPOSALS_PER_SPLIT);
            append_result_to_csv(csv, bu_result);
            
            std::cout << " Done (TD: " << std::fixed << std::setprecision(3)
                << td_result.runtime_seconds << "s, BU: "
                << bu_result.runtime_seconds << "s)";
            
            if (graphGenerationMethod == GraphGenerationMethod::LFR) {
                std::cout << "  K=" << config->k;
            }
            std::cout << std::endl;
        }
        
        graph_id++;
    }
    
    csv.close();
    clear_configs(configs);
    
    std::cout << "\n✅ Benchmark complete! Results saved to results/benchmark_results.csv\n";
    std::cout << "\nRun './scripts/analyze_results.sh' for quick statistics\n";
    
    return 0;
}
