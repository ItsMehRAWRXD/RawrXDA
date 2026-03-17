#!/bin/bash

# Local development: Run llama.cpp server + RawrXD IDE with docker-compose
# This mirrors the DigitalOcean deployment locally

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$(dirname "${SCRIPT_DIR}")"
DOCKER_DIR="${DEPLOY_DIR}/docker"

echo "=== Local Development Setup ==="
echo "Deploy directory: ${DEPLOY_DIR}"
echo ""

# Ensure models directory exists
mkdir -p "${DEPLOY_DIR}/models"
mkdir -p "${DEPLOY_DIR}/cache"

# Check if any models exist
MODELS_COUNT=$(find "${DEPLOY_DIR}/models" -name "*.gguf" 2>/dev/null | wc -l)
if [ "${MODELS_COUNT}" -eq 0 ]; then
    echo "[!] Warning: No GGUF models found in ${DEPLOY_DIR}/models/"
    echo "[*] Expected models: quantum-2b-q4.gguf, quantum-3b-q4.gguf, etc."
    echo "[*] Download from: https://huggingface.co/search?other=gguf"
    echo ""
fi

# Build custom llama.cpp image
echo "[1/3] Building llama.cpp Docker image..."
docker build -f "${DOCKER_DIR}/Dockerfile" -t rawrxd-llama-server:latest "${DOCKER_DIR}/"

# Start services
echo "[2/3] Starting services with docker-compose..."
cd "${DEPLOY_DIR}/docker"
docker-compose up -d

# Wait for server to be ready
echo "[3/3] Waiting for server to be ready..."
for attempt in {1..15}; do
    if curl -s http://localhost:8080/health > /dev/null 2>&1; then
        echo "[✓] Server is ready"
        break
    else
        if [ $attempt -eq 15 ]; then
            echo "[✗] Server failed to start"
            docker-compose logs
            exit 1
        fi
        echo "  [*] Waiting for server... (${attempt}/15)"
        sleep 2
    fi
done

echo ""
echo "=== Local Development Environment Ready ==="
echo ""
echo "API Server: http://localhost:8080"
echo "  - Health: curl http://localhost:8080/health"
echo "  - API Docs: http://localhost:8080/docs"
echo "  - Swagger UI: http://localhost:8080/swagger"
echo ""
echo "Models mounted at: /models (inside container)"
echo "  - Host path: ${DEPLOY_DIR}/models"
echo ""
echo "Example completion request:"
echo 'curl -X POST http://localhost:8080/v1/completions \'
echo '  -H "Content-Type: application/json" \'
echo '  -d '"'"'{"prompt": "Hello", "n_predict": 32}'"'"
echo ""
echo "Container logs:"
echo "  docker-compose -f ${DEPLOY_DIR}/docker/docker-compose.yml logs -f"
echo ""
echo "Stop services:"
echo "  docker-compose -f ${DEPLOY_DIR}/docker/docker-compose.yml down"
