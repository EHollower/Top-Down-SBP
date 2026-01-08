#include <unordered_map>
#include <ranges>

#include "headers/sbp_utils.hpp"

namespace sbp {

Blockmodel connectivity_snowball_split(Subgraph& sub, int num_proposals) {
    if (sub.G.num_vertices < 2) return Blockmodel(&(sub.G), 1);
    Blockmodel best_bm;
    double best_h = std::numeric_limits<double>::max();
    #pragma omp parallel
    {
        Blockmodel local_best_bm;
        double local_best_h = std::numeric_limits<double>::max();

        #pragma omp for
        for (int p = 0; p < num_proposals; ++p) {
            Blockmodel current_bm(&(sub.G), 2);
            int n = sub.G.num_vertices;
            
            std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<> dist(0, n - 1);
            int seed1 = dist(gen), seed2 = dist(gen);
            while (seed2 == seed1) seed2 = dist(gen);

            std::vector<int> assignment(n, -1);
            assignment[seed1] = 0, assignment[seed2] = 1;

            auto unassigned = std::views::iota(0, n) 
                            | std::views::filter([&](int i) { return assignment[i] == -1; });
            std::vector<int> unassigned_vec(unassigned.begin(), unassigned.end());
            std::shuffle(unassigned_vec.begin(), unassigned_vec.end(), gen);

            for (int v : unassigned_vec) {
                int score0 = 0, score1 = 0;
                for (int neighbor : sub.G.adj_list[v]) {
                    if (assignment[neighbor] == 0) score0++;
                    else if (assignment[neighbor] == 1) score1++;
                }
                
                if (score0 > score1) assignment[v] = 0;
                else if (score1 > score0) assignment[v] = 1;
                else {
                    // Break ties randomly to avoid bias
                    std::uniform_int_distribution<> tie_dist(0, 1);
                    assignment[v] = tie_dist(gen);
                }
            }

            current_bm.assignment = std::move(assignment);
            current_bm.update_matrix();
            double h = compute_H(current_bm);

            if (h < local_best_h) {
                local_best_h = h;
                local_best_bm = current_bm;
            }
        }

        #pragma omp critical
        {
            if (local_best_h < best_h) {
                best_h = local_best_h;
                best_bm = std::move(local_best_bm);
            }
        }
    }

    return best_bm;
}

void extract_subgraphs_parallel(const Blockmodel& BM, std::vector<Subgraph>& subgraphs) {
    subgraphs.clear();
    subgraphs.resize(BM.num_clusters);
    
    std::vector<std::vector<int>> cluster_members(BM.num_clusters);
    for (size_t i = 0; i < BM.assignment.size(); ++i) {
        cluster_members[BM.assignment[i]].push_back(static_cast <int>(i));
    }

    #pragma omp parallel for
    for (int c = 0; c < BM.num_clusters; ++c) {
        Subgraph& sub = subgraphs[c];
        sub.subgraph_mapping = std::move(cluster_members[c]);
        int n_sub = sub.subgraph_mapping.size();
        sub.G.num_vertices = n_sub;
        sub.G.adj_list.resize(n_sub);
        
        std::unordered_map<int, int> global_to_local;
        for (int i = 0; i < n_sub; ++i) {
            global_to_local[sub.subgraph_mapping[i]] = i;
        }

        for (int i = 0; i < n_sub; ++i) {
            int u_global = sub.subgraph_mapping[i];
            for (int v_global : BM.G->adj_list[u_global]) {
                if (BM.assignment[v_global] == c) {
                    sub.G.adj_list[i].push_back(global_to_local[v_global]);
                    sub.G.num_edges++;
                }
            }
        }
    }
}

void top_down_sbp(Graph& G, Blockmodel& BM, int max_clusters, int proposals_per_split) {
    BM = Blockmodel(&G, 1);
    BM.update_matrix();

    while (BM.num_clusters < max_clusters) {
        std::vector<Subgraph> subgraphs;
        extract_subgraphs_parallel(BM, subgraphs);

        struct SplitResult {
            double deltaH;
            Blockmodel split_bm;
            int cluster_idx;
        };
        std::vector<SplitResult> results;

        for (int i = 0; i < BM.num_clusters; ++i) {
            if (subgraphs[i].G.num_vertices < 2) continue;
            
            // Calculate H for 1-cluster blockmodel of subgraph
            Blockmodel single_bm(&(subgraphs[i].G), 1);
            single_bm.update_matrix();
            double h_before = compute_H(single_bm);
            
            // Get best 2-cluster split
            Blockmodel split = connectivity_snowball_split(subgraphs[i], proposals_per_split);
            double h_after = compute_H(split);
            
            // Accept splits that reduce H or are within a tolerance (less conservative)
            double tolerance = 0.05 * std::abs(h_before);  // 5% tolerance
            if (h_after < h_before + tolerance) {
                SplitResult sr;
                sr.deltaH = h_after - h_before;
                sr.split_bm = std::move(split);
                sr.cluster_idx = i;
                results.push_back(std::move(sr));
            }
        }

        if (results.empty()) break;

        std::ranges::sort(results, {}, &SplitResult::deltaH);

        bool applied = false;
        auto& best = results[0];
        int new_cluster_id = BM.num_clusters;
        Subgraph& sub = subgraphs[best.cluster_idx];
        
        BM.num_clusters++;
        // Resize B matrix: first resize outer dimension, then resize each row
        BM.B.resize(BM.num_clusters);
        for (auto& row : BM.B) {
            row.resize(BM.num_clusters, 0);
        }
        BM.num_vertices_per_block.resize(BM.num_clusters, 0);

        for (int i = 0; i < sub.G.num_vertices; ++i) {
            if (best.split_bm.assignment[i] == 1) {
                BM.assignment[sub.subgraph_mapping[i]] = new_cluster_id;
            }
        }
        
        BM.update_matrix();
        applied = true;
        
        // Apply MCMC refinement after each split (reduced for stability)
        mcmc_refine(BM, 10 * BM.G->num_vertices);

        if (!applied) break;
    }
}

} // namespace sbp
