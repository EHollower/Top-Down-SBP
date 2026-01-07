#include "sbp_utils.hpp"

namespace sbp {

void bottom_up_sbp(Graph& G, Blockmodel& BM, int target_clusters) {
    BM = Blockmodel(&G, G.num_vertices);
    for (int i = 0; i < G.num_vertices; ++i) {
        BM.assignment[i] = i;
    }
    BM.update_matrix();
    
    // Skip initial MCMC refinement - too expensive with N clusters
    // We'll refine after merges instead

    while (BM.num_clusters > target_clusters) {
        struct MergeProposal {
            int c1, c2;
            double deltaH;
        };
        
        std::vector<MergeProposal> proposals;
        
        // Collect merge proposals only for connected clusters
        for (int i = 0; i < BM.num_clusters; ++i) {
            if (BM.num_vertices_per_block[i] == 0) { continue; }
            for (int j = i + 1; j < BM.num_clusters; ++j) {
                if (BM.num_vertices_per_block[j] == 0) { continue; }
                
                // Only consider merging clusters that have edges between them
                if (BM.B[i][j] == 0 && BM.B[j][i] == 0) continue;
                
                // Calculate actual Delta H for merging i and j
                // Prefer merges with high edge density between clusters
                int edges_between = BM.B[i][j] + BM.B[j][i];
                int size_product = BM.num_vertices_per_block[i] * BM.num_vertices_per_block[j];
                double merge_score = (double)edges_between / (size_product > 0 ? size_product : 1);
                
                proposals.push_back({i, j, -merge_score}); // Negative so we can sort ascending
            }
        }

        if (proposals.empty()) break;
        
        // Sort by merge score (highest edge density first)
        std::sort(proposals.begin(), proposals.end(), 
                  [](const MergeProposal& a, const MergeProposal& b) { return a.deltaH < b.deltaH; });
        
        auto& best = proposals[0];
        int c1 = best.c1;
        int c2 = best.c2;
        
        // Merge cluster c2 into c1
        for (int& assign : BM.assignment) {
            if (assign == c2) assign = c1;
        }

        // Renumber clusters to eliminate gaps
        std::vector<int> old_to_new(BM.num_clusters, -1);
        int current_idx = 0;
        for (int i = 0; i < BM.num_clusters; ++i) {
            bool used = false;
            for (int assign : BM.assignment) {
                if (assign == i) { used = true; break; }
            }
            if (used) old_to_new[i] = current_idx++;
        }
        
        for (int& assign : BM.assignment) {
            assign = old_to_new[assign];
        }
        
        BM.num_clusters = current_idx;
        BM.B.assign(BM.num_clusters, std::vector<int>(BM.num_clusters, 0));
        BM.num_vertices_per_block.assign(BM.num_clusters, 0);
        BM.update_matrix();
        
        // Apply MCMC refinement after each merge (reduced for performance)
        int refine_iters = std::min(200, 10 * BM.num_clusters);
        mcmc_refine(BM, refine_iters);
        
        if (BM.num_clusters <= target_clusters) break;
    }
}

} // namespace sbp
