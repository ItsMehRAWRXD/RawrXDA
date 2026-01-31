#!/bin/bash
# iOS Development Startup Script

echo "Starting iOS Development Environment..."

# Check for Docker
if command -v docker &> /dev/null; then
    ENGINE="docker"
elif command -v podman &> /dev/null; then
    ENGINE="podman"
else
    echo "Error: No container engine found (Docker or Podman required)"
    exit 1
fi

echo "Using container engine: $ENGINE"

# Start iOS development container
echo "Starting iOS development container..."
$ENGINE run -it --rm \
    -v $(pwd):/workspace \
    -p 8080:8080 \
    -p 8081:8081 \
    -p 8082:8082 \
    -p 8083:8083 \
    -p 8084:8084 \
    --name ios-dev-container \
    mobile-ios-dev

echo "iOS Development Environment started!"
echo "Access IDE at: http://localhost:8080"
