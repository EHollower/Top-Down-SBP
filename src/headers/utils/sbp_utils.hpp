#ifndef SBP_UTILS_HPP
#define SBP_UTILS_HPP

#include "sbp_rng.hpp"
#include "sbp_graph.hpp"
#include "sbp_aliases.hpp"
#include "sbp_consts.hpp"
#include "sbp_blockmodel.hpp"

#include <omp.h>
#include <cmath>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <psapi.h>
#else
    #include <sys/resource.h>
#endif


namespace sbp::utils {

inline DescriptionLength compute_H(const BlockModel& block_model) {
    if (block_model.graph == nullptr || 
        block_model.cluster_count <= 0) {
        return inf;
    }

    Entropy entropy = 0.0;
    for (ClusterId i = 0; 
         i < static_cast<ClusterId>(block_model.cluster_count); 
        ++i) {

        if (block_model.clusters_sizes[i] == 0) {
            continue;
        }

        for (ClusterId j = 0; 
             j < static_cast<ClusterId>(block_model.cluster_count); 
            ++j) {

            if (block_model.clusters_sizes[j] == 0 || 
                block_model.block_matrix[i][j] <= 0) {
                continue;
            }

            Probability p_ij = (
                static_cast<Probability>(block_model.block_matrix[i][j]) /
                static_cast<Probability>(
                    block_model.clusters_sizes[i] * block_model.clusters_sizes[j]
                )
            );

            entropy += static_cast<Entropy>(
                block_model.block_matrix[i][j] * std::log(p_ij)
            );
        }
    }

    Probability model_complexity = (
        0.5 * block_model.cluster_count *  //NOLINT
        (block_model.cluster_count + 1) * 
        std::log(block_model.graph->get_vertex_count())
    );

    return static_cast <DescriptionLength>(
        -entropy + model_complexity
    );
}

inline Probability calculate_nmi(
    const ClusterAssignment& true_assignment, 
    const ClusterAssignment& output_assingment)  {
    if (true_assignment.size() != output_assingment.size() || 
        true_assignment.empty()) {
        return 0.0;
    }
    
    auto vertex_count = static_cast <VertexCount>(
        true_assignment.size()
    );

    FrequencyMap map_true; 
    FrequencyMap map_output;
    JointFrequencyMap map_joint;
    
    for (VertexId i = 0; 
        i < static_cast<VertexId>(vertex_count); 
        ++i) {
        ++map_true[true_assignment[i]];
        ++map_output[output_assingment[i]];
        ++map_joint[
            std::make_pair(true_assignment[i], output_assingment[i])
        ];
    }
    
    Entropy h_true = 0.0;
    for (const auto& [key, val] : map_true) {
        Probability prob = (
            static_cast<Probability>(val) / 
            static_cast<Probability>(vertex_count)
        );
        h_true -= static_cast<Entropy>(
            prob * std::log(prob)
        );
    }
    
    Entropy h_output = 0.0;
    for (const auto& [key, val] : map_output) {
        Probability prob = (
            static_cast<Probability>(val) / 
            static_cast<Probability>(vertex_count)
        );
        h_output -= prob * std::log(prob);
    }
    
    Entropy mi_ = 0.0;
    for (const auto& [key, val] : map_joint) {
        Probability p_xy = (
            static_cast<Probability>(val) / 
            static_cast<Probability>(vertex_count)
        );

        Probability p_x = (
            static_cast<Probability>(map_true[key.first]) / 
            static_cast<Probability>(vertex_count)
        );

        Probability p_y = (
            static_cast<Probability>(map_output[key.second]) / 
            static_cast<Probability>(vertex_count)
        );

        mi_ += static_cast <Entropy>(
            p_xy * std::log(p_xy / (p_x * p_y))
        );
    }
    
    if (h_true + h_output == 0.0) {
        return 0.0;
    }

    return static_cast<Probability>(
        2.0 * mi_ / // NOLINT
        (h_true +  h_output)
    );
}

inline ClusterId mcmc_proposal(
    const Graph& graph, 
    const BlockModel& block_model, 
    VertexId vertex) {
    
    const auto& neighbors = graph.adjacency_list[vertex];

    if (neighbors.empty()) { // Stay in same cluster
        return block_model.cluster_assignment[vertex]; 
    }
    
    // Choose random neighbor
    VertexId rand_neighbor = neighbors[
        RandomNumerGenerator::random_int(
            0, static_cast<int>(neighbors.size() - 1)
        )
    ];

    WeightMap cluster_weights;
    ClusterId neighbor_cluster = block_model.cluster_assignment[rand_neighbor];
    
    for (ClusterId i = 0; 
         i < static_cast<ClusterId>(block_model.cluster_count); 
         ++i) {

        if (block_model.block_matrix[neighbor_cluster][i] <= 0) {
            continue; 
        }

        cluster_weights[i] = block_model.block_matrix[neighbor_cluster][i];
    }
    
    if (cluster_weights.empty()) {
        return neighbor_cluster;
    }
    
    // Choose cluster weighted by edge counts
    EdgeCount total_weight = 0;
    for (const auto& [cluster, weight] : cluster_weights) {
        total_weight += weight;
    }
    
    EdgeCount rand_weight = RandomNumerGenerator::random_int(
        0, static_cast <int>(total_weight - 1)
    );

    EdgeCount cumulative = 0;

    for (const auto& [cluster, weight] : cluster_weights) {
        cumulative += weight;
        if (rand_weight < cumulative) {
            return cluster;
        }
    }
    
    return neighbor_cluster;
}

// Compute ΔH for merging two clusters (used in bottom-up SBP)
inline DescriptionLength compute_delta_H_merge(
    const BlockModel& block_model, 
    ClusterId c1, 
    ClusterId c2) {
    
    if (block_model.graph == nullptr || 
        c1 < 0 || c2 < 0 || 
        c1 >= static_cast<ClusterId>(block_model.cluster_count) || 
        c2 >= static_cast<ClusterId>(block_model.cluster_count)) {
        return inf;  // Invalid merge
    }
    
    if (c1 == c2) return 0.0;  // No change
    
    VertexCount n1 = block_model.clusters_sizes[c1];
    VertexCount n2 = block_model.clusters_sizes[c2];
    if (n1 == 0 || n2 == 0) return inf;  // Invalid merge
    
    VertexCount n_merged = n1 + n2;
    Entropy delta_entropy = 0.0;
    
    // Step 1: Remove entropy contributions from c1 and c2 separately
    for (ClusterId k = 0; k < static_cast<ClusterId>(block_model.cluster_count); ++k) {
        if (block_model.clusters_sizes[k] == 0) continue;
        VertexCount nk = block_model.clusters_sizes[k];
        
        // Remove c1 -> k edges
        if (block_model.block_matrix[c1][k] > 0) {
            Probability p_1k = static_cast<Probability>(block_model.block_matrix[c1][k]) / 
                              static_cast<Probability>(n1 * nk);
            delta_entropy -= static_cast<Entropy>(
                block_model.block_matrix[c1][k] * std::log(p_1k)
            );
        }
        
        // Remove k -> c1 edges (if k != c1 to avoid double counting diagonal)
        if (k != c1 && block_model.block_matrix[k][c1] > 0) {
            Probability p_k1 = static_cast<Probability>(block_model.block_matrix[k][c1]) / 
                              static_cast<Probability>(nk * n1);
            delta_entropy -= static_cast<Entropy>(
                block_model.block_matrix[k][c1] * std::log(p_k1)
            );
        }
        
        // Remove c2 -> k edges
        if (block_model.block_matrix[c2][k] > 0) {
            Probability p_2k = static_cast<Probability>(block_model.block_matrix[c2][k]) / 
                              static_cast<Probability>(n2 * nk);
            delta_entropy -= static_cast<Entropy>(
                block_model.block_matrix[c2][k] * std::log(p_2k)
            );
        }
        
        // Remove k -> c2 edges (if k != c2)
        if (k != c2 && block_model.block_matrix[k][c2] > 0) {
            Probability p_k2 = static_cast<Probability>(block_model.block_matrix[k][c2]) / 
                              static_cast<Probability>(nk * n2);
            delta_entropy -= static_cast<Entropy>(
                block_model.block_matrix[k][c2] * std::log(p_k2)
            );
        }
    }
    
    // Step 2: Add entropy contributions from merged cluster
    for (ClusterId k = 0; k < static_cast<ClusterId>(block_model.cluster_count); ++k) {
        if (block_model.clusters_sizes[k] == 0) continue;
        if (k == c1 || k == c2) continue;  // Skip the clusters being merged
        
        VertexCount nk = block_model.clusters_sizes[k];
        
        // Add merged -> k edges
        EdgeCount B_merged_k = block_model.block_matrix[c1][k] + block_model.block_matrix[c2][k];
        if (B_merged_k > 0) {
            Probability p_mk = static_cast<Probability>(B_merged_k) / 
                              static_cast<Probability>(n_merged * nk);
            delta_entropy += static_cast<Entropy>(B_merged_k * std::log(p_mk));
        }
        
        // Add k -> merged edges
        EdgeCount B_k_merged = block_model.block_matrix[k][c1] + block_model.block_matrix[k][c2];
        if (B_k_merged > 0) {
            Probability p_km = static_cast<Probability>(B_k_merged) / 
                              static_cast<Probability>(nk * n_merged);
            delta_entropy += static_cast<Entropy>(B_k_merged * std::log(p_km));
        }
    }
    
    // Step 3: Handle self-edges within merged cluster
    EdgeCount B_self = block_model.block_matrix[c1][c1] + 
                       block_model.block_matrix[c2][c2] + 
                       block_model.block_matrix[c1][c2] + 
                       block_model.block_matrix[c2][c1];
    if (B_self > 0) {
        Probability p_self = static_cast<Probability>(B_self) / 
                            static_cast<Probability>(n_merged * n_merged);
        delta_entropy += static_cast<Entropy>(B_self * std::log(p_self));
    }
    
    // Step 4: Model complexity change (one less cluster after merge)
    // Before: K clusters -> After: K-1 clusters
    ClusterCount K = block_model.cluster_count;
    DescriptionLength complexity_before = 
        0.5 * K * (K + 1) * std::log(block_model.graph->get_vertex_count()); // NOLINT
    DescriptionLength complexity_after = 
        0.5 * (K - 1) * K * std::log(block_model.graph->get_vertex_count()); // NOLINT
    DescriptionLength delta_complexity = complexity_after - complexity_before;
    
    // ΔH = -Δentropy + Δcomplexity
    return -delta_entropy + delta_complexity;
}

// MCMC refinement: iteratively propose moves and accept if they improve H
inline void mcmc_refine(
    BlockModel& block_model, 
    IterationCount num_iterations = defaultCount) {

    if (block_model.graph == nullptr || 
        block_model.cluster_count <= 1) {
        return;
    }
    
    for (IterationCount iter = 0; iter < num_iterations; ++iter) {
        VertexId vertex = RandomNumerGenerator::random_int(
            0, static_cast<VertexId>(block_model.graph->get_vertex_count() - 1)
        );

        ClusterId old_cluster = block_model.cluster_assignment[vertex];
        
        // Propose new cluster via MCMC
        ClusterId new_cluster = mcmc_proposal(*block_model.graph, block_model, vertex);
        
        if (new_cluster == old_cluster) {
            continue;
        }
        
        // Calculate delta H for this move
        DescriptionLength h_before = compute_H(block_model);

        block_model.move_vertex(vertex, new_cluster);

        DescriptionLength h_after = compute_H(block_model);
        
        // Accept if improves or with probability based on temperature
        if (h_after >= h_before) { // Reject: revert move
            block_model.move_vertex(vertex, old_cluster);
        }
    }
}

// Compute H_null: description length with all vertices in one cluster
inline DescriptionLength compute_H_null(const Graph& graph) {
    BlockModel null_bm;

    null_bm.graph = const_cast<Graph*>(&graph);
    null_bm.cluster_count = 1;
    null_bm.cluster_assignment.assign(graph.get_vertex_count(), 0);
    null_bm.clusters_sizes.assign(1, graph.get_vertex_count());
    null_bm.block_matrix.assign(1, std::vector<EdgeCount>(1, 0));
    null_bm.update_matrix();

    return compute_H(null_bm);
}

// Compute normalized description length: H_norm = H / H_null
inline Probability compute_H_normalized(const BlockModel& block_model) {
    if (block_model.graph == nullptr) {
        return 0.0;
    }
    DescriptionLength H__ = compute_H(block_model);
    DescriptionLength H_null = compute_H_null(*block_model.graph);

    if (H_null == 0.0) {
        return 0.0;
    }

    return static_cast<Probability>(
        H__ / H_null
    );
}

// Get peak memory usage in MB
inline MemorySize get_peak_memory_mb() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(
        GetCurrentProcess(),
        &info,
        sizeof(info)
    );
    // PeakWorkingSetSize is in bytes
    return static_cast<MemorySize>(
        info.PeakWorkingSetSize / 
        MiB
    );
#else
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

#if defined(__APPLE__)
    // ru_maxrss is in bytes on macOS
    return static_cast<MemorySize>(
        usage.ru_maxrss / 
        MiB
    );
#else
    // ru_maxrss is in kilobytes on Linux
    return static_cast<MemorySize>(
        usage.ru_maxrss / 
       KiB 
    );
#endif

#endif
}

} // sbp::utils

#endif // SBP_UTILS_HPP

