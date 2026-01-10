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
    
    // Initial MCMC refinement to cluster vertices into communities  
    // Moderate iterations to balance quality and speed
    int initial_iters = std::min(1000, 3 * BM.G->num_vertices);
    mcmc_refine(BM, initial_iters);

    while (BM.num_clusters > target_clusters) {
        std::vector<MergeProposal> proposals;
        
        // Collect merge proposals only for connected clusters (parallelized)
        // Always use accurate ΔH calculation for best quality
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
                    
                    // Calculate accurate incremental ΔH for every merge
                    // This ensures maximum quality by choosing MDL-optimal merges
                    double deltaH = compute_delta_H_merge(BM, i, j);
                    
                    // Only propose merges that improve MDL (ΔH < 0)
                    if (deltaH < 0) {
                        local_proposals.push_back({i, j, deltaH});
                    }
                }
            }
            
            #pragma omp critical
            {
                proposals.insert(proposals.end(), local_proposals.begin(), local_proposals.end());
            }
        }

        if (proposals.empty()) break;
        
        // Sort by deltaH (best merges first - most negative ΔH = best MDL improvement)
        std::sort(proposals.begin(), proposals.end(), 
                  [](const MergeProposal& a, const MergeProposal& b) { return a.deltaH < b.deltaH; });
        
        // Small batch size for quality - allows frequent MCMC refinement
        int max_batch = std::min(5, std::max(1, BM.num_clusters / 20));
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
        
        // Apply MCMC refinement after each merge batch
        // Use full Top-Down quality: 10*N iterations for accuracy
        int refine_iters = 10 * BM.G->num_vertices;
        mcmc_refine(BM, refine_iters);
        
        if (BM.num_clusters <= target_clusters) break;
    }
}

} // namespace sbp
