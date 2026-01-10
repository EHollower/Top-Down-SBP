#ifndef SBP_UTILS_HPP
#define SBP_UTILS_HPP

#include <vector>
#include <algorithm>
#include <cmath>
#include <random>
#include <thread>
#include <omp.h>
#include <map>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <psapi.h>
#else
    #include <sys/resource.h>
#endif


namespace sbp {

inline double random_uniform_01() {
    thread_local std::mt19937 gen(std::random_device{}() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
    thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gen);
}

struct Graph {
    int num_vertices{};
    int num_edges{};
    std::vector<std::vector<int>> adj_list;
    Graph() = default;
};

struct Subgraph {
    Graph G;
    std::vector<int> subgraph_mapping;
};

class Blockmodel {
public:
    Graph* G = nullptr;
    int num_clusters{};
    std::vector<std::vector<int>> B;
    std::vector<int> assignment;
    std::vector<int> num_vertices_per_block;

    Blockmodel() = default;
    Blockmodel(Graph* G_ptr, int n_clusters) : G(G_ptr), num_clusters(n_clusters) {
        if (G) {
            int actual_size = std::min(G->num_vertices, (int)G->adj_list.size());
            assignment.assign(actual_size, 0);
            num_vertices_per_block.assign(num_clusters, 0);
            B.assign(num_clusters, std::vector<int>(num_clusters, 0));
        }
    }

    void update_matrix() {
        if (!G || num_clusters <= 0) return;
        
        for (auto& row : B) std::fill(row.begin(), row.end(), 0);
        std::fill(num_vertices_per_block.begin(), num_vertices_per_block.end(), 0);

        for (int i = 0; i < (int)assignment.size(); ++i) {
            int r = assignment[i];
            if (r < 0 || r >= num_clusters) continue;
            if (i >= (int)G->adj_list.size()) continue;
            
            for (int j : G->adj_list[i]) {
                if (j < 0 || j >= (int)assignment.size()) continue;
                int s = assignment[j];
                if (s < 0 || s >= num_clusters) continue;
                if (r >= (int)B.size() || s >= (int)B[r].size()) continue;
                B[r][s]++;
            }
            num_vertices_per_block[r]++;
        }
    }

    void move_vertex(int vertex, int new_cluster) {
        int old_cluster = assignment[vertex];
        if (old_cluster == new_cluster) return;

        for (int neighbour : G->adj_list[vertex]) {
            // Bounds check: ensure neighbour is within assignment vector
            if (neighbour < 0 || neighbour >= (int)assignment.size()) continue;
            int neighbour_cluster = assignment[neighbour];
            if (neighbour_cluster < 0 || neighbour_cluster >= num_clusters) continue;
            if (old_cluster < 0 || old_cluster >= num_clusters) continue;
            if (new_cluster < 0 || new_cluster >= num_clusters) continue;
            
            B[old_cluster][neighbour_cluster]--;
            B[new_cluster][neighbour_cluster]++;
            B[neighbour_cluster][old_cluster]--;
            B[neighbour_cluster][new_cluster]++;
        }

        assignment[vertex] = new_cluster;
        num_vertices_per_block[old_cluster]--;
        num_vertices_per_block[new_cluster]++;
    }
};

inline double compute_H(const Blockmodel& BM) {
    if (!BM.G || BM.num_clusters <= 0) return 1e18;

    double entropy = 0.0;
    for (int i = 0; i < BM.num_clusters; ++i) {
        if (BM.num_vertices_per_block[i] == 0) continue;
        for (int j = 0; j < BM.num_clusters; ++j) {
            if (BM.num_vertices_per_block[j] == 0 || BM.B[i][j] <= 0) continue;
            double p_ij = (double)BM.B[i][j] / ((double)BM.num_vertices_per_block[i] * BM.num_vertices_per_block[j]);
            entropy += BM.B[i][j] * std::log(p_ij);
        }
    }
    double model_complexity = 0.5 * BM.num_clusters * (BM.num_clusters + 1) * std::log(BM.G->num_vertices);
    return -entropy + model_complexity;
}

// Compute incremental change in H (MDL) when merging clusters c1 and c2
// Returns ΔH (negative = improvement, positive = degradation)
inline double compute_delta_H_merge(const Blockmodel& BM, int c1, int c2) {
    if (!BM.G || c1 < 0 || c2 < 0 || c1 >= BM.num_clusters || c2 >= BM.num_clusters) {
        return 1e18;  // Invalid merge
    }
    if (c1 == c2) return 0.0;  // No change
    
    int n1 = BM.num_vertices_per_block[c1];
    int n2 = BM.num_vertices_per_block[c2];
    if (n1 == 0 || n2 == 0) return 1e18;  // Invalid merge
    
    int n_merged = n1 + n2;
    double delta_entropy = 0.0;
    
    // Step 1: Remove entropy contributions from c1 and c2 separately
    for (int k = 0; k < BM.num_clusters; ++k) {
        if (BM.num_vertices_per_block[k] == 0) continue;
        int nk = BM.num_vertices_per_block[k];
        
        // Remove c1 -> k edges
        if (BM.B[c1][k] > 0) {
            double p_1k = (double)BM.B[c1][k] / ((double)n1 * nk);
            delta_entropy -= BM.B[c1][k] * std::log(p_1k);
        }
        
        // Remove k -> c1 edges (if k != c1 to avoid double counting diagonal)
        if (k != c1 && BM.B[k][c1] > 0) {
            double p_k1 = (double)BM.B[k][c1] / ((double)nk * n1);
            delta_entropy -= BM.B[k][c1] * std::log(p_k1);
        }
        
        // Remove c2 -> k edges
        if (BM.B[c2][k] > 0) {
            double p_2k = (double)BM.B[c2][k] / ((double)n2 * nk);
            delta_entropy -= BM.B[c2][k] * std::log(p_2k);
        }
        
        // Remove k -> c2 edges (if k != c2)
        if (k != c2 && BM.B[k][c2] > 0) {
            double p_k2 = (double)BM.B[k][c2] / ((double)nk * n2);
            delta_entropy -= BM.B[k][c2] * std::log(p_k2);
        }
    }
    
    // Step 2: Add entropy contributions from merged cluster
    for (int k = 0; k < BM.num_clusters; ++k) {
        if (BM.num_vertices_per_block[k] == 0) continue;
        if (k == c1 || k == c2) continue;  // Skip the clusters being merged
        
        int nk = BM.num_vertices_per_block[k];
        
        // Add merged -> k edges
        int B_merged_k = BM.B[c1][k] + BM.B[c2][k];
        if (B_merged_k > 0) {
            double p_mk = (double)B_merged_k / ((double)n_merged * nk);
            delta_entropy += B_merged_k * std::log(p_mk);
        }
        
        // Add k -> merged edges
        int B_k_merged = BM.B[k][c1] + BM.B[k][c2];
        if (B_k_merged > 0) {
            double p_km = (double)B_k_merged / ((double)nk * n_merged);
            delta_entropy += B_k_merged * std::log(p_km);
        }
    }
    
    // Step 3: Handle self-edges within merged cluster
    int B_self = BM.B[c1][c1] + BM.B[c2][c2] + BM.B[c1][c2] + BM.B[c2][c1];
    if (B_self > 0) {
        double p_self = (double)B_self / ((double)n_merged * n_merged);
        delta_entropy += B_self * std::log(p_self);
    }
    
    // Step 4: Model complexity change (one less cluster after merge)
    // Before: K clusters -> After: K-1 clusters
    int K = BM.num_clusters;
    double complexity_before = 0.5 * K * (K + 1) * std::log(BM.G->num_vertices);
    double complexity_after = 0.5 * (K - 1) * K * std::log(BM.G->num_vertices);
    double delta_complexity = complexity_after - complexity_before;
    
    // ΔH = -Δentropy + Δcomplexity
    return -delta_entropy + delta_complexity;
}

inline double calculate_nmi(const std::vector<int>& assignment_a, const std::vector<int>& assignment_b) {
    if (assignment_a.size() != assignment_b.size() || assignment_a.empty()) return 0.0;
    
    int n = assignment_a.size();
    std::map<int, int> map_a, map_b;
    std::map<std::pair<int, int>, int> map_ab;
    
    for (int i = 0; i < n; ++i) {
        map_a[assignment_a[i]]++;
        map_b[assignment_b[i]]++;
        map_ab[{assignment_a[i], assignment_b[i]}]++;
    }
    
    double h_a = 0.0;
    for (auto const& [key, val] : map_a) {
        double p = (double)val / n;
        h_a -= p * std::log(p);
    }
    
    double h_b = 0.0;
    for (auto const& [key, val] : map_b) {
        double p = (double)val / n;
        h_b -= p * std::log(p);
    }
    
    double mi = 0.0;
    for (auto const& [key, val] : map_ab) {
        double p_xy = (double)val / n;
        double p_x = (double)map_a[key.first] / n;
        double p_y = (double)map_b[key.second] / n;
        mi += p_xy * std::log(p_xy / (p_x * p_y));
    }
    
    if (h_a + h_b == 0.0) return 0.0;
    return 2.0 * mi / (h_a + h_b);
}

// Algorithm 4: MCMC_PROPOSAL
inline int mcmc_proposal(const Graph& G, const Blockmodel& BM, int v) {
    thread_local std::mt19937 gen(std::random_device{}() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
    
    // Get neighbors of vertex v
    const auto& neighbors = G.adj_list[v];
    if (neighbors.empty()) return BM.assignment[v]; // Stay in same cluster
    
    // Choose random neighbor
    std::uniform_int_distribution<int> neighbor_dist(0, neighbors.size() - 1);
    int rand_neighbor = neighbors[neighbor_dist(gen)];
    int neighbor_cluster = BM.assignment[rand_neighbor];
    
    // Get all clusters that are neighbors to neighbor_cluster with weights
    std::map<int, int> cluster_weights;
    
    for (int i = 0; i < BM.num_clusters; ++i) {
        if (BM.B[neighbor_cluster][i] > 0) {
            cluster_weights[i] = BM.B[neighbor_cluster][i];
        }
    }
    
    if (cluster_weights.empty()) return neighbor_cluster;
    
    // Choose cluster weighted by edge counts
    int total_weight = 0;
    for (const auto& [cluster, weight] : cluster_weights) {
        total_weight += weight;
    }
    
    std::uniform_int_distribution<int> weight_dist(0, total_weight - 1);
    int rand_weight = weight_dist(gen);
    int cumulative = 0;
    
    for (const auto& [cluster, weight] : cluster_weights) {
        cumulative += weight;
        if (rand_weight < cumulative) {
            return cluster;
        }
    }
    
    return neighbor_cluster;
}

// MCMC refinement: iteratively propose moves and accept if they improve H
// Parallel version using per-thread B matrices for maximum performance
inline void mcmc_refine(Blockmodel& BM, int num_iterations = 100) {
    if (!BM.G || BM.num_clusters <= 1) return;
    
    // Store best result found across all threads
    double best_H = compute_H(BM);
    std::vector<int> best_assignment = BM.assignment;
    
    #pragma omp parallel
    {
        // Thread-local RNG (already thread-safe)
        thread_local std::mt19937 gen(std::random_device{}() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::uniform_int_distribution<int> vertex_dist(0, BM.G->num_vertices - 1);
        
        // Thread-local copy of blockmodel
        Blockmodel local_BM;
        local_BM.G = BM.G;  // Graph is read-only, safe to share
        local_BM.num_clusters = BM.num_clusters;
        local_BM.assignment = BM.assignment;  // Deep copy
        local_BM.num_vertices_per_block = BM.num_vertices_per_block;  // Deep copy
        local_BM.B.assign(BM.num_clusters, std::vector<int>(BM.num_clusters, 0));
        for (int i = 0; i < BM.num_clusters; ++i) {
            for (int j = 0; j < BM.num_clusters; ++j) {
                local_BM.B[i][j] = BM.B[i][j];  // Deep copy B matrix
            }
        }
        
        // Each thread performs iterations independently
        #pragma omp for nowait
        for (int iter = 0; iter < num_iterations; ++iter) {
            // Pick random vertex
            int v = vertex_dist(gen);
            int old_cluster = local_BM.assignment[v];
            
            // Propose new cluster via MCMC
            int new_cluster = mcmc_proposal(*local_BM.G, local_BM, v);
            
            if (new_cluster == old_cluster) continue;
            
            // Calculate delta H for this move
            double h_before = compute_H(local_BM);
            local_BM.move_vertex(v, new_cluster);
            double h_after = compute_H(local_BM);
            
            // Accept if improves
            if (h_after < h_before) {
                // Accept move (already applied)
            } else {
                // Reject: revert move
                local_BM.move_vertex(v, old_cluster);
            }
        }
        
        // After all iterations, check if this thread found better solution
        double local_H = compute_H(local_BM);
        
        #pragma omp critical
        {
            if (local_H < best_H) {
                best_H = local_H;
                best_assignment = local_BM.assignment;
            }
        }
    }
    
    // Apply best solution found
    BM.assignment = best_assignment;
    BM.update_matrix();
}

// Compute H_null: description length with all vertices in one cluster
inline double compute_H_null(const Graph& G) {
    Blockmodel null_bm;
    null_bm.G = const_cast<Graph*>(&G);
    null_bm.num_clusters = 1;
    null_bm.assignment.assign(G.num_vertices, 0);
    null_bm.num_vertices_per_block.assign(1, G.num_vertices);
    null_bm.B.assign(1, std::vector<int>(1, 0));
    null_bm.update_matrix();
    return compute_H(null_bm);
}

// Compute normalized description length: H_norm = H / H_null
inline double compute_H_normalized(const Blockmodel& BM) {
    if (!BM.G) return 0.0;
    double H = compute_H(BM);
    double H_null = compute_H_null(*BM.G);
    if (H_null == 0.0) return 0.0;
    return H / H_null;
}

// Get peak memory usage in MB
inline std::size_t get_peak_memory_mb()
{
#if defined(_WIN32)

    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(
        GetCurrentProcess(),
        &info,
        sizeof(info)
    );

    // PeakWorkingSetSize is in bytes
    return static_cast<std::size_t>(info.PeakWorkingSetSize / (1024 * 1024));

#else

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

#if defined(__APPLE__)
    // ru_maxrss is in bytes on macOS
    return static_cast<std::size_t>(usage.ru_maxrss / (1024 * 1024));
#else
    // ru_maxrss is in kilobytes on Linux
    return static_cast<std::size_t>(usage.ru_maxrss / 1024);
#endif

#endif
}


} // namespace sbp

#endif // SBP_UTILS_HPP
