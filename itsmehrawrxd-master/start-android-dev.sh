#!/bin/bash
# Android Development Startup Script

echo "Starting Android Development Environment..."

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

# Start Android development container
echo "Starting Android development container..."
$ENGINE run -it --rm \
    -v $(pwd):/workspace \
    -p 8080:8080 \
    -p 8081:8081 \
    -p 8082:8082 \
    -p 8083:8083 \
    -p 8084:8084 \
    -p 5555:5555 \
    -p 5556:5556 \
    --name android-dev-container \
    mobile-android-dev

echo "Android Development Environment started!"
echo "Access IDE at: http://localhost:8080"
