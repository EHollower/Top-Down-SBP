#include "../headers/utils/sbp_utils.hpp"

#include <unordered_set>
#include <algorithm>

namespace sbp {

void bottom_up_sbp(
    utils::Graph& G,
    utils::BlockModel& BM,
    utils::ClusterCount target_clusters) {
    
    // Initialize: each vertex in its own cluster
    BM = utils::BlockModel(&G, G.get_vertex_count());
    for (utils::VertexId i = 0; i < static_cast<utils::VertexId>(G.get_vertex_count()); ++i) {
        BM.cluster_assignment[i] = i;
    }
    BM.update_matrix();
    
    // Skip initial MCMC refinement - too expensive with N clusters
    // We'll refine after merges when cluster count is manageable

    while (BM.cluster_count > target_clusters) {
        struct MergeProposal {
            utils::ClusterId c1, c2;
            utils::DescriptionLength deltaH;
        };
        
        std::vector<MergeProposal> proposals;
        bool forced_merge = false;  // Track if we're doing a forced merge
        
        // Parallel merge proposal collection (EDIST Algorithm 4, lines 3-14)
        #pragma omp parallel
        {
            std::vector<MergeProposal> local_proposals;
            
            #pragma omp for schedule(dynamic)
            for (utils::ClusterId c = 0; 
                 c < static_cast<utils::ClusterId>(BM.cluster_count); 
                 ++c) {
                
                if (BM.clusters_sizes[c] == 0) continue;
                
                utils::DescriptionLength best_deltaH = utils::inf;
                utils::ClusterId best_merge_partner = utils::nullCluster;
                
                // Find best merge partner for cluster c
                for (utils::ClusterId c_prime = 0; 
                     c_prime < static_cast<utils::ClusterId>(BM.cluster_count); 
                     ++c_prime) {
                    
                    if (c == c_prime || BM.clusters_sizes[c_prime] == 0) continue;
                    
                    // Only consider merging clusters that have edges between them
                    if (BM.block_matrix[c][c_prime] == 0 && 
                        BM.block_matrix[c_prime][c] == 0) continue;
                    
                    // Calculate MDL-based ΔH (EDIST Algorithm 4, line 9)
                    utils::DescriptionLength deltaH = utils::compute_delta_H_merge(BM, c, c_prime);
                    
                    if (deltaH < best_deltaH) {
                        best_deltaH = deltaH;
                        best_merge_partner = c_prime;
                    }
                }
                
                // Store best merge for this cluster (Algorithm 4, line 11)
                if (best_merge_partner != utils::nullCluster && best_deltaH < 0) {
                    local_proposals.push_back({c, best_merge_partner, best_deltaH});
                }
            }
            
            // Combine results from all threads (simulates MPI_Allgather)
            #pragma omp critical
            {
                proposals.insert(proposals.end(), 
                                local_proposals.begin(), 
                                local_proposals.end());
            }
        }

        // If no beneficial merges found but we're still above target,
        // force the least-bad merge to make progress
        if (proposals.empty() && BM.cluster_count > target_clusters) {
            // Find the single best merge (even if ΔH ≥ 0)
            utils::DescriptionLength best_deltaH = utils::inf;
            utils::ClusterId best_c1 = utils::nullCluster;
            utils::ClusterId best_c2 = utils::nullCluster;
            
            for (utils::ClusterId c1 = 0; c1 < static_cast<utils::ClusterId>(BM.cluster_count); ++c1) {
                if (BM.clusters_sizes[c1] == 0) continue;
                
                for (utils::ClusterId c2 = c1 + 1; c2 < static_cast<utils::ClusterId>(BM.cluster_count); ++c2) {
                    if (BM.clusters_sizes[c2] == 0) continue;
                    
                    // Consider all cluster pairs (not just connected ones) to ensure progress
                    utils::DescriptionLength deltaH = utils::compute_delta_H_merge(BM, c1, c2);
                    if (deltaH < best_deltaH) {
                        best_deltaH = deltaH;
                        best_c1 = c1;
                        best_c2 = c2;
                    }
                }
            }
            
            // Add the best merge found (even if ΔH ≥ 0)
            if (best_c1 != utils::nullCluster && best_c2 != utils::nullCluster) {
                proposals.push_back({best_c1, best_c2, best_deltaH});
                forced_merge = true;  // Mark that this is a forced merge
            }
        }

        if (proposals.empty()) break;
        
        // Sort by ΔH (best merges first - EDIST Algorithm 4, line 16)
        std::sort(proposals.begin(), proposals.end(),
                  [](const MergeProposal& a, const MergeProposal& b) {
                      return a.deltaH < b.deltaH;
                  });
        
        // Select independent merges to prevent conflicts (batch merge strategy)
        std::vector<MergeProposal> independent_merges;
        std::unordered_set<utils::ClusterId> used_clusters;
        
        // Limit merges to not overshoot target
        utils::ClusterCount clusters_to_remove = BM.cluster_count - target_clusters;
        utils::ClusterCount max_merges = std::min(
            static_cast<utils::ClusterCount>(BM.cluster_count * utils::mergeBatchSizeFactor),
            clusters_to_remove  // Don't merge more than needed to reach target
        );
        
        for (const auto& proposal : proposals) {
            // Check if either cluster is already involved in a merge
            if (used_clusters.find(proposal.c1) == used_clusters.end() &&
                used_clusters.find(proposal.c2) == used_clusters.end()) {
                
                independent_merges.push_back(proposal);
                used_clusters.insert(proposal.c1);
                used_clusters.insert(proposal.c2);
                
                // Stop when we've selected enough merges (configurable batch size)
                if (independent_merges.size() >= max_merges) break;
            }
        }
        
        // Apply all independent merges (EDIST Algorithm 4, lines 18-19)
        for (const auto& merge : independent_merges) {
            // Merge cluster c2 into c1
            for (utils::ClusterId& assign : BM.cluster_assignment) {
                if (assign == merge.c2) assign = merge.c1;
            }
        }

        // Renumber clusters to eliminate gaps (optimized version)
        utils::ClusterAssignment old_to_new(BM.cluster_count, utils::nullCluster);
        utils::ClusterCount current_idx = 0;
        
        for (utils::ClusterId c = 0; c < static_cast<utils::ClusterId>(BM.cluster_count); ++c) {
            // Check if cluster c is used
            bool used = std::any_of(
                BM.cluster_assignment.begin(),
                BM.cluster_assignment.end(),
                [c](utils::ClusterId assign) { return assign == c; }
            );
            if (used) old_to_new[c] = current_idx++;
        }
        
        // Remap all assignments
        for (utils::ClusterId& assign : BM.cluster_assignment) {
            assign = old_to_new[assign];
        }
        
        // Update blockmodel structure
        BM.cluster_count = current_idx;
        BM.block_matrix.assign(BM.cluster_count, 
                               std::vector<utils::EdgeCount>(BM.cluster_count, 0));
        BM.clusters_sizes.assign(BM.cluster_count, 0);
        BM.update_matrix();
        
        // Adaptive MCMC refinement based on cluster count and merge type
        // More refinement for: 1) forced merges, 2) when close to target, 3) fewer clusters
        if (BM.cluster_count <= G.get_vertex_count() / utils::mcmcThresholdDivisor) {
            utils::IterationCount base_iters = std::min(
                utils::maxBottomUpMcmcIters,
                utils::bottomUpMcmcMultiplier * BM.cluster_count
            );
            
            // Extra refinement after forced merges (these are risky)
            if (forced_merge) {
                base_iters = std::min(
                    utils::maxBottomUpMcmcIters,
                    utils::forcedMergeMcmcMultiplier * BM.cluster_count
                );
            }
            
            // Even more refinement when very close to target
            if (BM.cluster_count <= target_clusters + 2) {
                base_iters = std::min(
                    utils::maxBottomUpMcmcIters,
                    utils::forcedMergeMcmcMultiplier * BM.cluster_count * 2
                );
            }
            
            utils::mcmc_refine(BM, base_iters);
        }
        
        // Only break if we've reached target (not if we somehow went below)
        if (BM.cluster_count == target_clusters) break;
        
        // Safety check: if we went below target, something went wrong
        if (BM.cluster_count < target_clusters) {
            // This shouldn't happen in bottom-up (we only merge, never split)
            // But break anyway to avoid infinite loop
            break;
        }
    }
    
    // Final intensive refinement pass once we reach target cluster count
    if (BM.cluster_count == target_clusters) {
        utils::IterationCount final_iters = std::min(
            utils::maxBottomUpMcmcIters,
            utils::forcedMergeMcmcMultiplier * BM.cluster_count
        );
        utils::mcmc_refine(BM, final_iters);
    }
}

} // namespace sbp
