#ifndef SBP_CONST_HPP
#define SBP_CONST_HPP

#include "sbp_aliases.hpp"

namespace sbp::utils {
   
constexpr ClusterId nullCluster = -1;
constexpr DescriptionLength inf = 1e18;
constexpr IterationCount defaultCount = 100;

constexpr MemorySize KiB = 1024;
constexpr MemorySize MiB = KiB * KiB;

// Cluster configuration
constexpr ClusterCount minClusterCount = 1;
constexpr ClusterCount binarySplitCount = 2;

// Algorithm tuning parameters
constexpr ToleranceFactor splitToleranceFactor = 0.05;  // 5% tolerance for split acceptance
constexpr IterationCount mcmcRefinementMultiplier = 10;  // 10*N iterations per split

// Bottom-up SBP parameters (tuned for accuracy over speed)
constexpr IterationCount bottomUpMcmcMultiplier = 50;   // Iterations per cluster count (increased from 10)
constexpr IterationCount maxBottomUpMcmcIters = 2000;   // Cap for performance (increased from 200)
constexpr Probability mergeBatchSizeFactor = 0.5;       // Merge up to 50% of clusters per iteration // NOLINT
constexpr VertexCount mcmcThresholdDivisor = 5;         // Start MCMC when clusters < N/5 (was N/10)
constexpr ToleranceFactor mergeToleranceFactor = 0.01;  // 1% tolerance for merge acceptance
constexpr IterationCount forcedMergeMcmcMultiplier = 100; // Extra MCMC after forced merges

}; // sbp::utils

#endif // SBP_CONST_HPP
