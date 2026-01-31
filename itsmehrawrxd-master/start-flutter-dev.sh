#!/bin/bash
# Flutter Development Startup Script

echo "Starting Flutter Development Environment..."

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

# Start Flutter development container
echo "Starting Flutter development container..."
$ENGINE run -it --rm \
    -v $(pwd):/workspace \
    -p 8080:8080 \
    -p 8081:8081 \
    -p 8082:8082 \
    -p 8083:8083 \
    -p 8084:8084 \
    -p 8095:5555 \
    --name flutter-dev-container \
    mobile-flutter-dev

echo "Flutter Development Environment started!"
echo "Access IDE at: http://localhost:8080"
