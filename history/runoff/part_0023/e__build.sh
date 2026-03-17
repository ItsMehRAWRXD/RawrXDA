#!/bin/bash
# build.sh - Build script for AgentHotPatcher

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

echo "Build completed successfully!"
echo "Run tests with:"
echo "  ./test/TestAgentHotPatcher"
echo "  ./test_qt/TestAgentHotPatcherQtTest"