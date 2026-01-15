#ifndef GRAPH_GENERATION_HPP
#define GRAPH_GENERATION_HPP

#include "utils/sbp_utils.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <unordered_set>
#include <cmath>
#include <filesystem>


int sample_powerlaw(double xmin, double tau, std::mt19937& rng) {
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    double r = uni(rng);
    return static_cast<int>(xmin * std::pow(1.0 - r, -1.0 / (tau - 1.0)));
}

enum class GraphGenerationMethod {
    STANDARD, LFR
};

struct GraphConfigBase {
    int n, k;
public:
    virtual sbp::utils::Graph generateGraph(std::vector<sbp::utils::ClusterId>& true_assignment, int seed) = 0;
    virtual ~GraphConfigBase() = default;  // Add virtual destructor
};

struct GraphConfig : public GraphConfigBase {
    double p_in, p_out;

public:
    virtual sbp::utils::Graph generateGraph(std::vector<sbp::utils::ClusterId>& true_assignment, int seed) override {
        sbp::utils::Graph G;
        G.adjacency_list.resize(n);

        true_assignment.resize(n);
        for (int i = 0; i < n; ++i) {
            true_assignment[i] = i % k;
        }

        std::mt19937 gen(seed);
        std::uniform_real_distribution<> dist(0.0, 1.0);

        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                double p = (true_assignment[i] == true_assignment[j]) ? p_in : p_out;
                if (dist(gen) < p) {
                    G.adjacency_list[i].push_back(j);
                    G.adjacency_list[j].push_back(i);
                }
            }
        }

        // Validate graph
        for (int i = 0; i < n; ++i) {
            for (int j : G.adjacency_list[i]) {
                if (j < 0 || j >= n) {
                    std::cerr << "ERROR: Graph validation failed! Vertex " << i
                        << " has edge to out-of-bounds vertex " << j
                        << " (n=" << n << ")" << std::endl;
                    std::abort();
                }
            }
        }

        return G;
    }
};

struct GraphConfigLFR : public GraphConfigBase {
    double tau1;            // degree exponent
    double tau2;            // community size exponent
    double mu;              // mixing parameter
    int avg_degree;         // target average degree
    int min_comm_size;      // minimum community size

public:
    virtual sbp::utils::Graph generateGraph(std::vector<sbp::utils::ClusterId>& true_assignment, int seed) override {
        int N = this->n;
        k = 0;

        std::mt19937 rng(seed);

        sbp::utils::Graph G;
        G.adjacency_list.resize(N);

        /* --------------------------------------------------
           1. Generate degree sequence (power law)
        -------------------------------------------------- */
        std::vector<int> degree(N);
        for (int i = 0; i < N; ++i)
            degree[i] = std::max(1, sample_powerlaw(1.0, tau1, rng));

        // Rescale degrees to match average degree
        double mean_deg = std::accumulate(degree.begin(), degree.end(), 0.0) / N;
        double scale = avg_degree / mean_deg;
        for (int i = 0; i < N; ++i)
            degree[i] = std::max(1, int(degree[i] * scale));

        /* --------------------------------------------------
           2. Generate community sizes (power law)
        -------------------------------------------------- */
        std::vector<int> comm_sizes;
        int total = 0;
        while (total < N) {
            int s = std::max(min_comm_size,
                sample_powerlaw(min_comm_size, tau2, rng));
            comm_sizes.push_back(s);
            total += s;
        }
        comm_sizes.back() -= (total - N);

        this->k = (int)comm_sizes.size();

        /* --------------------------------------------------
           3. Assign nodes to communities
        -------------------------------------------------- */
        true_assignment.resize(N);
        int node = 0;
        for (int c = 0; c < (int)comm_sizes.size(); ++c) {
            for (int i = 0; i < comm_sizes[c]; ++i)
                true_assignment[node++] = c;
        }

        /* --------------------------------------------------
           4. Split degrees into internal / external
        -------------------------------------------------- */
        std::vector<std::vector<int>> internal_stubs(comm_sizes.size());
        std::vector<int> external_stubs;

        for (int i = 0; i < N; ++i) {
            int kin = int((1.0 - mu) * degree[i]);
            int kout = degree[i] - kin;

            internal_stubs[true_assignment[i]].insert(
                internal_stubs[true_assignment[i]].end(), kin, i
            );
            external_stubs.insert(external_stubs.end(), kout, i);
        }

        /* --------------------------------------------------
           5. Wire internal edges (within communities)
        -------------------------------------------------- */
        for (auto& stubs : internal_stubs) {
            std::shuffle(stubs.begin(), stubs.end(), rng);
            for (size_t i = 0; i + 1 < stubs.size(); i += 2) {
                int u = stubs[i];
                int v = stubs[i + 1];
                if (u != v) {
                    G.adjacency_list[u].push_back(v);
                    G.adjacency_list[v].push_back(u);
                }
            }
        }

        /* --------------------------------------------------
           6. Wire external edges (between communities)
        -------------------------------------------------- */
        std::shuffle(external_stubs.begin(), external_stubs.end(), rng);

        for (size_t i = 0; i + 1 < external_stubs.size(); i += 2) {
            int u = external_stubs[i];
            int v = external_stubs[i + 1];

            if (u != v && true_assignment[u] != true_assignment[v]) {
                G.adjacency_list[u].push_back(v);
                G.adjacency_list[v].push_back(u);
            }
        }

        return G;
    }
};

std::vector<GraphConfigBase*> read_standard_graph_configs_from_csv(const std::string& path) {
    std::vector<GraphConfigBase*> configs;

    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: Could not open the configuration file: " << path << "\n";
        std::exit(1);
    }

    std::string line;

    // Skip header line
    if (!std::getline(f, line)) {
        return configs; // empty file
    }

    while (std::getline(f, line)) {
        // Skip empty lines
        if (line.empty())
            continue;

        std::stringstream ss(line);
        std::string token;
        GraphConfig* cfg = new GraphConfig();

        try {
            // Parse n
            if (!std::getline(ss, token, ',')) continue;
            cfg->n = std::stoi(token);

            // Parse k
            if (!std::getline(ss, token, ',')) continue;
            cfg->k = std::stoi(token);

            // Parse p_in
            if (!std::getline(ss, token, ',')) continue;
            cfg->p_in = std::stod(token);

            // Parse p_out
            if (!std::getline(ss, token, ',')) continue;
            cfg->p_out = std::stod(token);

            configs.push_back(cfg);
        }
        catch (const std::invalid_argument& e) {

        }
        catch (const std::out_of_range& e) {

        }
    }

    return configs;
}

std::vector<GraphConfigBase*> read_lfr_graph_configs_from_csv(const std::string& path) {
    std::vector<GraphConfigBase*> configs;

    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: Could not open the configuration file: " << path << "\n";
        std::exit(1);
    }

    std::string line;

    // Skip header line
    if (!std::getline(f, line)) {
        return configs; // empty file
    }

    while (std::getline(f, line)) {
        // Skip empty lines
        if (line.empty())
            continue;

        std::stringstream ss(line);
        std::string token;
        GraphConfigLFR* cfg = new GraphConfigLFR();

        try {
            // Parse N
            if (!std::getline(ss, token, ',')) continue;
            cfg->n = std::stoi(token);

            // Parse tau1
            if (!std::getline(ss, token, ',')) continue;
            cfg->tau1 = std::stod(token);

            // Parse tau2
            if (!std::getline(ss, token, ',')) continue;
            cfg->tau2 = std::stod(token);

            // Parse mu
            if (!std::getline(ss, token, ',')) continue;
            cfg->mu = std::stod(token);

            // Parse avg_degree
            if (!std::getline(ss, token, ',')) continue;
            cfg->avg_degree = std::stoi(token);

            // Parse min_comm_size
            if (!std::getline(ss, token, ',')) continue;
            cfg->min_comm_size = std::stoi(token);

            configs.push_back(cfg);
        }
        catch (const std::invalid_argument&) {
            // malformed number, skip line
        }
        catch (const std::out_of_range&) {
            // number out of range, skip line
        }
    }

    return configs;
}

std::vector<GraphConfigBase*> read_graph_configs_from_csv(const std::string& path, const GraphGenerationMethod& method) {
    if (method == GraphGenerationMethod::STANDARD) {
        return read_standard_graph_configs_from_csv(path);
    }
    if (method == GraphGenerationMethod::LFR) {
        return read_lfr_graph_configs_from_csv(path);
    }
    return {};  // Return empty vector if no method matches
}

void clear_configs(std::vector<GraphConfigBase*>& configs) {
    for (size_t i = 0; i < configs.size(); ++i) {
        delete configs[i];
        configs[i] = nullptr;
    }
}

#endif // GRAPH_GENERATION_HPP
