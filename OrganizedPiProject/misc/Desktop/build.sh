#!/bin/bash
echo "Building FPS Game Engine..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
make -j$(nproc)

# Check if build was successful
if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Executable location: build/FPSGameEngine"
    echo ""
    echo "To run the game:"
    echo "cd build"
    echo "./FPSGameEngine"
    echo ""
else
    echo ""
    echo "Build failed!"
    echo "Check the error messages above."
    echo ""
fi