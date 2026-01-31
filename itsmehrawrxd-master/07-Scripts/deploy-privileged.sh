#!/bin/bash

# RawrZ Security Platform - Privileged Deployment Script
# This script deploys the application with elevated privileges

set -e

echo "========================================"
echo "RawrZ Security Platform - Privileged Deploy"
echo "========================================"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "[ERROR] This script requires root privileges"
    echo ""
    echo "Please run with sudo:"
    echo "  sudo ./deploy-privileged.sh"
    echo ""
    exit 1
fi

echo "[OK] Running with root privileges"
echo ""

# Stop any existing containers
echo "[INFO] Stopping existing containers..."
docker-compose -f docker-compose.privileged.yml down 2>/dev/null || true

# Remove old images
echo "[INFO] Cleaning up old images..."
docker image prune -f

# Build and start with privileged access
echo "[INFO] Building privileged container..."
docker-compose -f docker-compose.privileged.yml build --no-cache

echo "[INFO] Starting privileged container..."
docker-compose -f docker-compose.privileged.yml up -d

# Wait for container to be ready
echo "[INFO] Waiting for container to be ready..."
sleep 10

# Check container status
if docker-compose -f docker-compose.privileged.yml ps | grep -q "Up"; then
    echo "[OK] Container is running with privileged access"
    echo ""
    echo "RawrZ Security Platform is now running with full privileges:"
    echo "  - Main Panel: http://localhost:3000"
    echo "  - API Endpoint: http://localhost:3000/api/rawrz-engine/status"
    echo "  - Health Check: http://localhost:3000/health"
    echo ""
    echo "All engines now have full system access:"
    echo "  - Red Killer: Full registry and service control"
    echo "  - Private Virus Scanner: Complete system scanning"
    echo "  - AI Threat Detector: Full model training and saving"
    echo "  - All other engines: Maximum functionality"
    echo ""
else
    echo "[ERROR] Container failed to start"
    echo ""
    echo "Checking logs..."
    docker-compose -f docker-compose.privileged.yml logs
    exit 1
fi

echo "[INFO] Deployment completed successfully!"
echo ""
echo "To view logs: docker-compose -f docker-compose.privileged.yml logs -f"
echo "To stop: docker-compose -f docker-compose.privileged.yml down"
