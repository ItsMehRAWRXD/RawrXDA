#!/bin/bash
# build_tests.sh - Build and run AgentHotPatcher tests

echo "=== Building AgentHotPatcher Test Suite ==="

# Create build directory
mkdir -p build_tests
cd build_tests

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
echo "Building tests..."
make -j$(nproc)

echo "=== Running Tests ==="

# Run unit tests
echo "Running unit tests..."
./test_agent_hot_patcher

# Run integration tests
echo "Running integration tests..."
./test_agent_hot_patcher_integration

echo "=== Test Suite Completed ==="