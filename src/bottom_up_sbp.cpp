#include "headers/sbp_utils.hpp"
#include <unordered_set>

namespace sbp {

struct MergeProposal {
    int c1, c2;
    double deltaH;
};

// Find set of independent (non-conflicting) merges that can be done in parallel
std::vector<MergeProposal> select_independent_merges(
    const std::vector<MergeProposal>& sorted_proposals, 
    int max_batch_size = 100) 
{
    std::vector<MergeProposal> independent;
    std::unordered_set<int> used_clusters;
    
    for (const auto& proposal : sorted_proposals) {
        // Check if either cluster is already used in batch
        if (used_clusters.find(proposal.c1) == used_clusters.end() &&
            used_clusters.find(proposal.c2) == used_clusters.end()) {
            independent.push_back(proposal);
            used_clusters.insert(proposal.c1);
            used_clusters.insert(proposal.c2);
            
            if ((int)independent.size() >= max_batch_size) break;
        }
    }
    
    return independent;
}

void bottom_up_sbp(Graph& G, Blockmodel& BM, int target_clusters) {
    BM = Blockmodel(&G, G.num_vertices);
    for (int i = 0; i < G.num_vertices; ++i) {
        BM.assignment[i] = i;
    }
    BM.update_matrix();
    
    // Skip initial MCMC refinement - too expensive with N clusters
    // We'll refine after merges instead

    while (BM.num_clusters > target_clusters) {
        std::vector<MergeProposal> proposals;
        
        // Collect merge proposals only for connected clusters (parallelized)
        #pragma omp parallel
        {
            std::vector<MergeProposal> local_proposals;
            
            #pragma omp for nowait
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
                    
                    local_proposals.push_back({i, j, -merge_score}); // Negative so we can sort ascending
                }
            }
            
            #pragma omp critical
            {
                proposals.insert(proposals.end(), local_proposals.begin(), local_proposals.end());
            }
        }

        if (proposals.empty()) break;
        
        // Sort by merge score (highest edge density first)
        std::sort(proposals.begin(), proposals.end(), 
                  [](const MergeProposal& a, const MergeProposal& b) { return a.deltaH < b.deltaH; });
        
        // Select independent merges that can be done in parallel
        int max_batch = std::min(100, BM.num_clusters / 2);  // Don't merge too many at once
        auto independent_merges = select_independent_merges(proposals, max_batch);
        
        // Apply all independent merges in parallel
        #pragma omp parallel for
        for (size_t idx = 0; idx < independent_merges.size(); ++idx) {
            auto& merge = independent_merges[idx];
            int c1 = merge.c1;
            int c2 = merge.c2;
            
            // Merge cluster c2 into c1 (thread-safe since clusters are independent)
            #pragma omp critical
            {
                for (int& assign : BM.assignment) {
                    if (assign == c2) assign = c1;
                }
            }
        }

        // Renumber clusters to eliminate gaps (sequential - needs full view)
        std::vector<int> old_to_new(BM.num_clusters, -1);
        int current_idx = 0;
        for (int i = 0; i < BM.num_clusters; ++i) {
            bool used = false;
            for (int assign : BM.assignment) {
                if (assign == i) { used = true; break; }
            }
            if (used) old_to_new[i] = current_idx++;
        }
        
        // Update assignments with new cluster IDs
        for (int& assign : BM.assignment) {
            assign = old_to_new[assign];
        }
        
        BM.num_clusters = current_idx;
        BM.B.assign(BM.num_clusters, std::vector<int>(BM.num_clusters, 0));
        BM.num_vertices_per_block.assign(BM.num_clusters, 0);
        BM.update_matrix();
        
        // Apply MCMC refinement after each merge (reduced for performance)
        // Now parallelized for much better performance
        int refine_iters = std::min(200, 10 * BM.num_clusters);
        mcmc_refine(BM, refine_iters);
        
        if (BM.num_clusters <= target_clusters) break;
    }
}

} // namespace sbp
