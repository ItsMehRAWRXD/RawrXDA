#!/bin/bash

echo "π Native C++ Compiler Build Script"
echo "==================================="

# Build with Gradle
echo "Building with Gradle..."
./gradlew build --no-daemon

# Build Docker image
echo "Building Docker image..."
docker build -t native-cpp-compiler .

echo "✓ Build complete!"
echo ""
echo "To run:"
echo "docker run -v \$(pwd)/src/main/cpp:/cpp native-cpp-compiler /cpp/hello.cpp /tmp/hello"