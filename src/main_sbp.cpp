#include "headers/utils/sbp_utils.hpp"
#include <chrono>
#include <iostream>

using namespace sbp;

namespace sbp {
    void top_down_sbp(utils::Graph& G, utils::BlockModel& BM, utils::ClusterCount max_clusters, utils::ProposalCount proposals_per_split);
    void bottom_up_sbp(utils::Graph& G, utils::BlockModel& BM, utils::ClusterCount target_clusters);
}

// Generates SBM graph and returns the ground truth assignments
utils::Graph generate_stochastic_block_model_graph(utils::VertexCount n, utils::ClusterCount num_blocks, utils::Probability p_in, utils::Probability p_out, std::vector<utils::ClusterId>& true_assignment) {
    utils::Graph G;
    G.adjacency_list.resize(n);
    
    true_assignment.resize(n);
    for(utils::VertexId i=0; i<n; ++i) true_assignment[i] = i % num_blocks;

    std::mt19937 gen(42);
    std::uniform_real_distribution<> dist(0.0, 1.0);

    utils::EdgeCount edge_count = 0;
    for(utils::VertexId i=0; i<n; ++i) {
        for(utils::VertexId j=i+1; j<n; ++j) {
            utils::Probability p = (true_assignment[i] == true_assignment[j]) ? p_in : p_out;
            if (dist(gen) < p) {
                G.adjacency_list[i].push_back(j);
                G.adjacency_list[j].push_back(i);
                edge_count++;
            }
        }
    }
    return G;
}

int main() {
    utils::VertexCount n = 200;
    utils::ClusterCount k = 4;
    std::cout << "Generating synthetic SBM graph (N=" << n << ", K=" << k << ")..." << std::endl;
    std::vector<utils::ClusterId> true_labels;
    utils::Graph G = generate_stochastic_block_model_graph(n, k, 0.2, 0.02, true_labels);
    std::cout << "Edges: " << G.get_edge_count() << std::endl;

    {
        std::cout << "\n--- Top-Down SBP ---" << std::endl;
        utils::BlockModel bm;
        auto start = std::chrono::high_resolution_clock::now();
        sbp::top_down_sbp(G, bm, k, 50);  // Increased from 10 to 50 proposals
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        double nmi = utils::calculate_nmi(true_labels, bm.cluster_assignment);
        std::cout << "Finished in " << elapsed.count() << "s, MDL: " << utils::compute_H(bm) 
                  << ", Clusters: " << bm.cluster_count 
                  << ", NMI: " << nmi << std::endl;
    }

    {
        std::cout << "\n--- Bottom-Up SBP ---" << std::endl;
        utils::BlockModel bm;
        auto start = std::chrono::high_resolution_clock::now();
        sbp::bottom_up_sbp(G, bm, k);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        double nmi = utils::calculate_nmi(true_labels, bm.cluster_assignment);
        std::cout << "Finished in " << elapsed.count() << "s, MDL: " << utils::compute_H(bm)
                  << ", Clusters: " << bm.cluster_count
                  << ", NMI: " << nmi << std::endl;
    }

    return 0;
}
