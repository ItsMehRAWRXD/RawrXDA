#!/bin/bash
# PE Writer Production Build Script
# Supports Windows, Linux, and macOS builds

echo "=== PE Writer Production Build Script ==="

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found. Please install CMake 3.16 or later."
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure build
echo "Configuring build..."
cmake .. -DBUILD_TESTS=ON -DBUILD_IDE_INTEGRATION=ON
if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed."
    cd ..
    exit 1
fi

# Build project
echo "Building project..."
cmake --build . --config Release
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed."
    cd ..
    exit 1
fi

# Run tests
echo "Running tests..."
ctest --output-on-failure
if [ $? -ne 0 ]; then
    echo "WARNING: Some tests failed."
fi

# Install (optional)
echo "Installing..."
cmake --install . --prefix ../install
if [ $? -ne 0 ]; then
    echo "WARNING: Installation failed."
fi

cd ..
echo
echo "=== Build Complete ==="
echo
echo "Build artifacts in: build/"
echo "Install location: install/"
echo
echo "To run tests manually: cd build && ctest"
echo "To build examples: cd build && cmake --build . --target examples"