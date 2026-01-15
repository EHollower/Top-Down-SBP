#include "../headers/utils/sbp_utils.hpp"

#include <unordered_map>

namespace sbp {

utils::BlockModel connectivity_snowball_split( // NOLINT
    utils::SubGraph& subgraph, 
    utils::IterationCount iteration_proposal) {

    if (subgraph.graph.get_vertex_count() < utils::binarySplitCount) {
        utils::BlockModel bm(&(subgraph.graph), utils::minClusterCount);
        // Initialize all vertices to cluster 0
        std::fill(bm.cluster_assignment.begin(), bm.cluster_assignment.end(), 0);
        return bm;
    }

    utils::BlockModel best_bm;
    utils::DescriptionLength best_h = 
        std::numeric_limits<utils::DescriptionLength>::max();

    #pragma omp parallel
    {
        utils::BlockModel local_best_bm;
        utils::DescriptionLength local_best_h = 
            std::numeric_limits<utils::DescriptionLength>::max();

        #pragma omp for
        for (utils::IterationCount iteration = 0; 
            iteration < iteration_proposal; 
            ++iteration) {

            utils::BlockModel current_bm(&(subgraph.graph), utils::binarySplitCount);
            utils::VertexCount vertex_count = subgraph.graph.get_vertex_count();

            // Select two random seed vertices for binary split
            utils::VertexId seed1 = utils::RandomNumerGenerator::random_int(
                0, static_cast<int>(vertex_count) - 1
            );

            utils::VertexId seed2 = utils::RandomNumerGenerator::random_int(
                0, static_cast<int>(vertex_count) - 1
            );

            while (seed2 == seed1) {
                seed2 = utils::RandomNumerGenerator::random_int(
                    0, static_cast<int>(vertex_count) - 1
                );
            }

            utils::ClusterAssignment assignment(vertex_count, utils::nullCluster);

            assignment[seed1] = 0;
            assignment[seed2] = 1;

            // Collect unassigned vertices
            utils::VertexList unassigned_vec;
            for (utils::VertexId i = 0; i < static_cast<utils::VertexId>(vertex_count); ++i) {
                if (assignment[i] == utils::nullCluster) {
                    unassigned_vec.push_back(i);
                }
            }

            std::shuffle(
                unassigned_vec.begin(), 
                unassigned_vec.end(), 
                utils::RandomNumerGenerator::get_generator()
            );

            for (utils::VertexId vertex : unassigned_vec) {
                utils::EdgeScore score0 = 0; 
                utils::EdgeScore score1 = 0;

                for (utils::VertexId neighbor : 
                                    subgraph.graph.adjacency_list[vertex]) {

                    if (assignment[neighbor] == 0) {
                        ++score0;
                    } else if (assignment[neighbor] == 1) {
                        ++score1;
                    }
                }
                
                if (score0 > score1) {
                    assignment[vertex] = 0;
                } else if (score1 > score0) {
                    assignment[vertex] = 1;
                } else {
                    assignment[vertex] = 
                        utils::RandomNumerGenerator::random_int(0, 1);
                }
            }

            current_bm.cluster_assignment = std::move(assignment);
            current_bm.update_matrix();

            utils::DescriptionLength h = utils::compute_H(current_bm); //NOLINT

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

void extract_subgraphs_parallel(
    const utils::BlockModel& block_model, 
    std::vector<utils::SubGraph>& subgraphs)  {

    subgraphs.clear();
    subgraphs.resize(block_model.cluster_count);
    
    std::vector<utils::VertexList> cluster_members(block_model.cluster_count);
    for (utils::VertexId i = 0; 
         i < static_cast<utils::VertexId>(block_model.cluster_assignment.size());
         ++i) {

        cluster_members[
            block_model.cluster_assignment[i]
        ].push_back(i);
    }

    #pragma omp parallel for
    for (utils::ClusterId cluster = 0; 
         cluster < static_cast<utils::ClusterId>(block_model.cluster_count); 
        ++cluster) {

        utils::SubGraph& sub = subgraphs[cluster];
        sub.subgraph_mapping = std::move(cluster_members[cluster]);

        utils::VertexCount n_sub = sub.subgraph_mapping.size();
        sub.graph.adjacency_list.resize(n_sub);
        
        std::unordered_map<utils::VertexId, utils::VertexId> global_to_local;
        for (utils::VertexId i = 0; 
             i < static_cast<utils::VertexId>(n_sub); 
              ++i) {

            global_to_local[sub.subgraph_mapping[i]] = i;
        }

        for (utils::VertexId i = 0; 
             i < static_cast<utils::VertexId>(n_sub); 
             ++i) {

            utils::VertexId u_global = sub.subgraph_mapping[i];

            for (utils::VertexId v_global : 
                        block_model.graph->adjacency_list[u_global]) {

                if (block_model.cluster_assignment[v_global] == cluster) {
                    sub.graph.adjacency_list[i].push_back(global_to_local[v_global]);
                }
            }
        }
    }
}

void top_down_sbp(
    utils::Graph& graph, 
    utils::BlockModel& block_model, 
    utils::ClusterCount max_clusters, 
    utils::IterationCount proposals_per_split) {
    
    block_model = utils::BlockModel(&graph, utils::minClusterCount);
    // Initialize all vertices to cluster 0
    std::fill(block_model.cluster_assignment.begin(), block_model.cluster_assignment.end(), 0);
    block_model.update_matrix();

    while (block_model.cluster_count < max_clusters) {
        std::vector<utils::SubGraph> subgraphs;
        extract_subgraphs_parallel(block_model, subgraphs);

        struct SplitCandidate {
            utils::DescriptionLength deltaH;
            utils::ClusterId cluster_idx;
            utils::BlockModel split_bm;
        };
        std::vector<SplitCandidate> candidates;

        for (utils::ClusterId i = 0; i < static_cast<utils::ClusterId>(block_model.cluster_count); ++i) {
            if (subgraphs[i].graph.get_vertex_count() < utils::binarySplitCount) {
                continue;
            }
            
            // Calculate H for 1-cluster blockmodel of subgraph
            utils::BlockModel single_bm(&(subgraphs[i].graph), utils::minClusterCount);
            std::fill(single_bm.cluster_assignment.begin(), single_bm.cluster_assignment.end(), 0);
            single_bm.update_matrix();
            utils::DescriptionLength h_before = utils::compute_H(single_bm);
            
            // Get best 2-cluster split
            utils::BlockModel split = connectivity_snowball_split(subgraphs[i], proposals_per_split);
            utils::DescriptionLength h_after = utils::compute_H(split);
            
            // Accept splits that reduce H or are within a tolerance (less conservative)
            utils::ToleranceFactor tolerance = utils::splitToleranceFactor * std::abs(h_before);
            if (h_after < h_before + tolerance) {
                SplitCandidate sc; // NOLINT
                sc.deltaH = h_after - h_before;
                sc.cluster_idx = i;
                sc.split_bm = std::move(split);
                candidates.push_back(std::move(sc));
            }
        }

        if (candidates.empty()) {
            break;
        }

        // Find best split (minimum deltaH)
        auto best_it = std::ranges::min_element(candidates,
                            [](const SplitCandidate& a, const SplitCandidate& b) { // NOLINT
                                return a.deltaH < b.deltaH;
                            });
    
        auto& best = *best_it;
        auto new_cluster_id = static_cast<utils::ClusterId>(block_model.cluster_count);
        utils::SubGraph& sub = subgraphs[best.cluster_idx];
        
        block_model.cluster_count++;
        // Resize B matrix: first resize outer dimension, then resize each row
        block_model.block_matrix.resize(block_model.cluster_count);
        for (auto& row : block_model.block_matrix) {
            row.resize(block_model.cluster_count, 0);
        }
        block_model.clusters_sizes.resize(block_model.cluster_count, 0);

        for (utils::VertexId i = 0; i < static_cast<utils::VertexId>(sub.graph.get_vertex_count()); ++i) {
            if (best.split_bm.cluster_assignment[i] == 1) {
                block_model.cluster_assignment[sub.subgraph_mapping[i]] = new_cluster_id;
            }
        }
        
        block_model.update_matrix();
        
        // Apply MCMC refinement after each split (reduced for stability)
        utils::mcmc_refine(block_model, utils::mcmcRefinementMultiplier * block_model.graph->get_vertex_count());
    }
}

} // namespace sbp
