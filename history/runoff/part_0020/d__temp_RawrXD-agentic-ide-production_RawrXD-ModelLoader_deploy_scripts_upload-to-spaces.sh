#!/bin/bash

# DigitalOcean Spaces Configuration for GGUF Model Repository
# Cost: $5/month for 250GB (included egress, CDN edge)

set -e

# Configuration
SPACES_BUCKET="rawrxd-quantum-models"
SPACES_REGION="nyc3"
SPACES_ENDPOINT="https://${SPACES_REGION}.digitaloceanspaces.com"
MODELS_DIR="${HOME}/models"
DO_API_TOKEN="${DIGITALOCEAN_TOKEN}"

# Model inventory (2GB quantum models)
declare -a MODELS=(
    "quantum-2b-q4.gguf"
    "quantum-3b-q4.gguf"
    "quantum-7b-q4.gguf"
    "quantum-13b-q4.gguf"
)

echo "=== DigitalOcean Spaces Upload Pipeline ==="
echo "Bucket: ${SPACES_BUCKET}"
echo "Region: ${SPACES_REGION}"
echo "Endpoint: ${SPACES_ENDPOINT}"
echo ""

# Install AWS CLI if not present (compatible with S3-like APIs)
if ! command -v aws &> /dev/null; then
    echo "[*] Installing AWS CLI..."
    pip install awscli-local
fi

# Configure AWS CLI for Spaces
echo "[*] Configuring AWS CLI for Spaces..."
aws configure set aws_access_key_id "${DO_SPACES_KEY}"
aws configure set aws_secret_access_key "${DO_SPACES_SECRET}"

# Create bucket if not exists
echo "[*] Creating Spaces bucket: ${SPACES_BUCKET}..."
aws s3api create-bucket \
    --bucket "${SPACES_BUCKET}" \
    --region "${SPACES_REGION}" \
    --endpoint-url "${SPACES_ENDPOINT}" \
    --create-bucket-configuration LocationConstraint="${SPACES_REGION}" 2>/dev/null || echo "  [✓] Bucket already exists"

# Upload models with progress tracking
echo "[*] Uploading 2GB quantum GGUF models..."
for model in "${MODELS[@]}"; do
    if [ -f "${MODELS_DIR}/${model}" ]; then
        echo "  [↑] Uploading ${model}..."
        aws s3 cp "${MODELS_DIR}/${model}" \
            "s3://${SPACES_BUCKET}/models/${model}" \
            --endpoint-url "${SPACES_ENDPOINT}" \
            --acl public-read \
            --expected-size $(stat -f%z "${MODELS_DIR}/${model}" 2>/dev/null || stat -c%s "${MODELS_DIR}/${model}") \
            --metadata "model-type=quantum,quantization=q4,size-gb=2" \
            --storage-class STANDARD
        echo "  [✓] ${model} uploaded"
    else
        echo "  [✗] ${model} not found at ${MODELS_DIR}/${model}"
    fi
done

# Generate CDN URLs for all uploaded models
echo ""
echo "[*] CDN URLs (auto-cached, TLS included):"
for model in "${MODELS[@]}"; do
    CDN_URL="https://${SPACES_BUCKET}.${SPACES_REGION}.cdn.digitaloceanspaces.com/models/${model}"
    echo "  ${CDN_URL}"
done

echo ""
echo "=== Upload Complete ==="
echo "Cost: $5/month (250GB included)"
echo "Egress: 1TB free, then $0.01/GB"
echo "CDN: Automatic edge caching, TLS auto-renewal"
