#ifndef SBP_GRAPH_HPP
#define SBP_GRAPH_HPP

#include "sbp_aliases.hpp"

namespace sbp::utils {

struct Graph {
    AdjacencyList adjacency_list;

    Graph() = default;

    [[nodiscard]] VertexCount get_vertex_count() const {
        return static_cast <VertexCount> (
            adjacency_list.size()
        );
    }

    [[nodiscard]] EdgeCount get_edge_count() const {
        EdgeCount count = 0;
        for (const auto& neighbors : adjacency_list) {
            count += neighbors.size();
        }
        return count / 2;  // Each edge is counted twice in undirected graph
    }

}; // Graph

struct SubGraph {

    Graph graph;
    VertexMapping subgraph_mapping;

}; // SubGraph

} // sbp::utils

#endif // SBP_GRAPH_HPP


