#!/bin/bash

# Deploy llama.cpp server to DigitalOcean Droplet
# Target: Basic Regular Droplet (1 vCPU / 1 GB RAM / 25 GB SSD) @ $6/month

set -e

DROPLET_IP="${1:-}"
DROPLET_USER="root"
MODELS_DIR="/models"
API_PORT="8080"

if [ -z "${DROPLET_IP}" ]; then
    echo "Usage: $0 <droplet-ip>"
    echo "Example: $0 192.168.1.100"
    exit 1
fi

echo "=== DigitalOcean Droplet Deployment ==="
echo "Target: ${DROPLET_IP}"
echo "User: ${DROPLET_USER}"
echo ""

# Step 1: SSH into droplet and install Docker
echo "[1/5] Installing Docker on droplet..."
ssh "${DROPLET_USER}@${DROPLET_IP}" << 'EOF'
set -e
apt-get update
apt-get install -y docker.io docker-compose
systemctl start docker
systemctl enable docker
mkdir -p /models /app/cache
echo "[✓] Docker installed and configured"
EOF

# Step 2: Download GGUF models from DigitalOcean Spaces
echo "[2/5] Downloading 2GB quantum models from Spaces CDN..."
ssh "${DROPLET_USER}@${DROPLET_IP}" << 'EOF'
set -e

# Model URLs from Spaces (CDN cached, TLS)
SPACES_CDN="https://rawrxd-quantum-models.nyc3.cdn.digitaloceanspaces.com/models"

echo "[*] Downloading models to /models..."
cd /models

# Download each 2GB model
for model in quantum-2b-q4.gguf quantum-3b-q4.gguf quantum-7b-q4.gguf; do
    echo "  [↓] $model..."
    wget -q --show-progress "${SPACES_CDN}/${model}" || echo "  [!] Failed to download $model"
done

echo "[✓] Models downloaded"
ls -lh /models/
EOF

# Step 3: Deploy llama.cpp server container
echo "[3/5] Starting llama.cpp server container..."
ssh "${DROPLET_USER}@${DROPLET_IP}" << 'EOF'
set -e

# Pull latest llama.cpp server image
docker pull ghcr.io/ggerganov/llama.cpp:server

# Run server with CPU-only, 2 threads (optimal for 1 vCPU droplet)
docker run -d \
    --name llama-server \
    --restart unless-stopped \
    -p 8080:8080 \
    -v /models:/models \
    -v /app/cache:/root/.cache \
    ghcr.io/ggerganov/llama.cpp:server \
    --host 0.0.0.0 \
    --port 8080 \
    --threads 2 \
    --ctx-size 2048 \
    --batch-size 512 \
    -m /models/quantum-2b-q4.gguf

echo "[✓] Server container running"
EOF

# Step 4: Verify server health
echo "[4/5] Verifying server health..."
sleep 5

for attempt in {1..10}; do
    if ssh "${DROPLET_USER}@${DROPLET_IP}" "curl -s http://localhost:${API_PORT}/health" > /dev/null; then
        echo "[✓] Server health check passed"
        break
    else
        if [ $attempt -eq 10 ]; then
            echo "[✗] Server failed to start"
            exit 1
        fi
        echo "  [*] Waiting for server (attempt $attempt/10)..."
        sleep 3
    fi
done

# Step 5: Display access information
echo "[5/5] Deployment complete"
echo ""
echo "=== API Access Information ==="
echo "Endpoint: http://${DROPLET_IP}:${API_PORT}"
echo "Health Check: curl http://${DROPLET_IP}:${API_PORT}/health"
echo "API Docs: http://${DROPLET_IP}:${API_PORT}/docs"
echo ""
echo "Example API call:"
echo "curl -X POST http://${DROPLET_IP}:${API_PORT}/v1/chat/completions \\"
echo "  -H 'Content-Type: application/json' \\"
echo "  -d '{\"model\": \"quantum-2b-q4\", \"messages\": [{\"role\": \"user\", \"content\": \"Hello\"}]}'"
echo ""
echo "=== Monitoring ==="
echo "View logs: ssh ${DROPLET_USER}@${DROPLET_IP} docker logs -f llama-server"
echo "View metrics: ssh ${DROPLET_USER}@${DROPLET_IP} docker stats llama-server"
echo ""
echo "=== Cost ==="
echo "Droplet: \$6/month (1 vCPU / 1 GB RAM)"
echo "Spaces: \$5/month (250 GB / 1 TB egress free)"
echo "Total: \$11/month"
echo "Credit burn: \$22 for 2 months with \$200 DigitalOcean credit"
