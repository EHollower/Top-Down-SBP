#!/bin/bash

# Build script for SBP Experiment
# Usage: ./build.sh [debug|release] [run]

set -e

CONFIG=${1:-release}
BUILD_DIR="build"
BIN_DIR="bin"

echo "🔨 Building SBP Experiment (${CONFIG})..."

# Create directories
mkdir -p ${BIN_DIR}

# Compile
if [ "$CONFIG" == "debug" ]; then
    # Main experiment (single demo)
    g++-14 -g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wno-unknown-pragmas -fopenmp -std=c++20 \
        src/top_down_sbp.cpp src/bottom_up_sbp.cpp src/main_sbp.cpp \
        -o ${BIN_DIR}/sbp_experiment
    
    # Benchmark harness (full suite)
    g++-14 -g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wno-unknown-pragmas -fopenmp -std=c++20 \
        src/top_down_sbp.cpp src/bottom_up_sbp.cpp src/benchmark_sbp.cpp \
        -o ${BIN_DIR}/sbp_benchmark
else
    # Main experiment (single demo)
    g++-14 -O3 -Wall -Wextra -Wno-unknown-pragmas -fopenmp -std=c++20 \
        src/top_down_sbp.cpp src/bottom_up_sbp.cpp src/main_sbp.cpp \
        -o ${BIN_DIR}/sbp_experiment
    
    # Benchmark harness (full suite)
    g++-14 -O3 -Wall -Wextra -Wno-unknown-pragmas -fopenmp -std=c++20 \
        src/top_down_sbp.cpp src/bottom_up_sbp.cpp src/benchmark_sbp.cpp \
        -o ${BIN_DIR}/sbp_benchmark
fi

echo "✅ Build complete:"
echo "   - ${BIN_DIR}/sbp_experiment (single demo)"
echo "   - ${BIN_DIR}/sbp_benchmark (full benchmark suite)"

# Run if requested
if [ "$2" == "run" ]; then
    echo ""
    echo "🚀 Running experiment..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    ./${BIN_DIR}/sbp_experiment
fi
