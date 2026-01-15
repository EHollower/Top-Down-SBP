#ifndef SBP_BLOCKMODEL_HPP
#define SBP_BLOCKMODEL_HPP


#include "sbp_graph.hpp"
#include "sbp_consts.hpp"
#include "sbp_aliases.hpp"

#include <omp.h>
#include <algorithm>

#define OMP_CHUNK_SIZE 64

namespace sbp::utils {

struct BlockModel {

    Graph* graph{nullptr};
    ClusterCount cluster_count{0};
    BlockMatrix block_matrix;
    ClusterAssignment cluster_assignment;
    ClustersSizes clusters_sizes;
    double total_mcmc_time{0.0};  // Accumulated MCMC refinement time in seconds

    BlockModel() = default;

    BlockModel(Graph* graph, ClusterCount cluster_count): graph(graph), cluster_count(cluster_count) {
        if (graph == nullptr) { return; }
        
        auto vertex_count = graph->get_vertex_count();

        clusters_sizes.assign(cluster_count, 0);
        cluster_assignment.assign(vertex_count, nullCluster);
        block_matrix.assign(cluster_count, std::vector<EdgeCount>(cluster_count, 0));
    }
    
    void update_matrix() {
        if (graph == nullptr || cluster_count <= 0) { return; }

        for (auto& row : block_matrix) {
            std::ranges::fill(row, 0);
        }

        std::ranges::fill(clusters_sizes, 0);

        #pragma omp parallel for schedule(dynamic, OMP_CHUNK_SIZE) \
                                default(none) \
                                shared(cluster_assignment, block_matrix, clusters_sizes)
        for (VertexId vertex_u = 0;
             vertex_u < static_cast<VertexId>(cluster_assignment.size());
            ++vertex_u) {

            if (vertex_u >= static_cast<VertexId>(graph->get_vertex_count())) {
                continue;
            }

            auto cluster_u = cluster_assignment[vertex_u];
 
            if (cluster_u < 0 ||
                cluster_u >= static_cast <ClusterId>(cluster_count) ||
                cluster_u >= static_cast <ClusterId>(block_matrix.size())) {
                continue;
            }

            for (auto vertex_v : graph->adjacency_list[vertex_u]) {

                if (vertex_v < 0 || 
                    vertex_v >= static_cast<VertexId>(cluster_assignment.size())) {
                    continue;
                }

                auto cluster_v = cluster_assignment[vertex_v];
        
                if (cluster_v < 0 ||
                    cluster_v >= static_cast <ClusterId>(cluster_count) || 
                    cluster_v >= static_cast <ClusterId>(block_matrix[cluster_u].size())) {
                    continue;
                }

                #pragma omp atomic
                ++block_matrix[cluster_u][cluster_v];
            }

            #pragma omp atomic
            ++clusters_sizes[cluster_u];
        }
    } // update_matrix()
    
    void move_vertex(VertexId vertex, ClusterId new_cluster) {
        if (vertex < 0 ||
            vertex >= static_cast<VertexId>(cluster_assignment.size())) {
            return;
        }

        auto old_cluster = cluster_assignment[vertex];

        if (old_cluster == new_cluster) { 
            return; 
        }

        if (old_cluster < 0 || 
            old_cluster >= static_cast<ClusterId>(cluster_count) || 
            old_cluster >= static_cast<ClusterId>(block_matrix.size())) {
                return;
        }

        if (vertex >= static_cast<VertexId>(graph->adjacency_list.size())) {
            return;
        }

        for (auto& neighbour : graph->adjacency_list[vertex]) {
            if (neighbour < 0 || 
                neighbour >= static_cast<VertexId>(cluster_assignment.size())) {
                continue;
            }

            auto neighbour_cluster = cluster_assignment[neighbour];

            if (neighbour_cluster < 0 || 
                neighbour_cluster >= static_cast<ClusterId>(cluster_count)) {
                continue;
            }

            if (new_cluster < 0 || 
                new_cluster >= static_cast<ClusterId>(cluster_count)) {
                continue;
            }

            --block_matrix[old_cluster][neighbour_cluster];
            --block_matrix[neighbour_cluster][old_cluster];

            ++block_matrix[new_cluster][neighbour_cluster];
            ++block_matrix[neighbour_cluster][new_cluster];
        }
        
        --clusters_sizes[old_cluster];
        ++clusters_sizes[new_cluster];
        cluster_assignment[vertex] = new_cluster;

    } // move_vertex()

}; // BlockModel

} // sbp::utils

#endif // SBP_BLOCKMODEL_HPP

