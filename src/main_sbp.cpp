#include "headers/sbp_utils.hpp"
#include <chrono>
#include <iostream>

namespace sbp {
    void top_down_sbp(Graph& G, Blockmodel& BM, int max_clusters, int proposals_per_split);
    void bottom_up_sbp(Graph& G, Blockmodel& BM, int target_clusters);
}

// Generates SBM graph and returns the ground truth assignments
sbp::Graph generate_stochastic_block_model_graph(int n, int num_blocks, double p_in, double p_out, std::vector<int>& true_assignment) {
    sbp::Graph G;
    G.num_vertices = n;
    G.adj_list.resize(n);
    
    true_assignment.resize(n);
    for(int i=0; i<n; ++i) true_assignment[i] = i % num_blocks;

    std::mt19937 gen(42);
    std::uniform_real_distribution<> dist(0.0, 1.0);

    for(int i=0; i<n; ++i) {
        for(int j=i+1; j<n; ++j) {
            double p = (true_assignment[i] == true_assignment[j]) ? p_in : p_out;
            if (dist(gen) < p) {
                G.adj_list[i].push_back(j);
                G.adj_list[j].push_back(i);
                G.num_edges++;
            }
        }
    }
    return G;
}

int main() {
    int n = 200;
    int k = 4;
    std::cout << "Generating synthetic SBM graph (N=" << n << ", K=" << k << ")..." << std::endl;
    std::vector<int> true_labels;
    sbp::Graph G = generate_stochastic_block_model_graph(n, k, 0.2, 0.02, true_labels);
    std::cout << "Edges: " << G.num_edges << std::endl;

    {
        std::cout << "\n--- Top-Down SBP ---" << std::endl;
        sbp::Blockmodel bm;
        auto start = std::chrono::high_resolution_clock::now();
        sbp::top_down_sbp(G, bm, k, 50);  // Increased from 10 to 50 proposals
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        double nmi = sbp::calculate_nmi(true_labels, bm.assignment);
        std::cout << "Finished in " << elapsed.count() << "s, MDL: " << sbp::compute_H(bm) 
                  << ", Clusters: " << bm.num_clusters 
                  << ", NMI: " << nmi << std::endl;
    }

    {
        std::cout << "\n--- Bottom-Up SBP ---" << std::endl;
        sbp::Blockmodel bm;
        auto start = std::chrono::high_resolution_clock::now();
        sbp::bottom_up_sbp(G, bm, k);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        double nmi = sbp::calculate_nmi(true_labels, bm.assignment);
        std::cout << "Finished in " << elapsed.count() << "s, MDL: " << sbp::compute_H(bm)
                  << ", Clusters: " << bm.num_clusters
                  << ", NMI: " << nmi << std::endl;
    }

    return 0;
}
