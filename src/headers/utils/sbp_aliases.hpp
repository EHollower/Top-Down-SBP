#ifndef SBP_ALIASES_HPP
#define SBP_ALIASES_HPP

#include <map>
#include <random>
#include <cstddef>
#include <cstdint>

namespace sbp::utils {

using VertexId          = std::int32_t;
using ClusterId         = std::int32_t;
using EdgeScore         = std::int32_t;  

using EdgeCount         = std::size_t;
using VertexCount       = std::size_t;
using ClusterCount      = std::size_t;
using IterationCount    = std::size_t;
using ProposalCount     = std::size_t;

using VertexList        = std::vector<VertexId>;
using ClusterAssignment = std::vector<ClusterId>;
using ClustersSizes     = std::vector<VertexCount>;
using AdjacencyList     = std::vector<VertexList>;
using VertexMapping     = std::vector<VertexId>;
using BlockMatrix       = std::vector<std::vector<EdgeCount>>;

using WeightMap         = std::map<ClusterId, EdgeCount>;
using FrequencyMap      = std::map<ClusterId, EdgeCount>;
using JointFrequencyMap = std::map<std::pair<ClusterId, ClusterId>, EdgeCount>;

using Probability       = double;
using Entropy           = double;
using DescriptionLength = double;
using ToleranceFactor   = double; 

using RandomSeed        = std::uint64_t;
using RandomGenerator   = std::mt19937_64;
using MemorySize        = std::size_t;

} // sbp::utils

#endif // SBP_ALIASES_HPP

