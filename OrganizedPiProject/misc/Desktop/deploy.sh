#!/bin/bash

# Sunshine Engine - Digital Ocean Deployment Script
# This script deploys the HTTP bot project to Digital Ocean

set -e

echo "🌞 Deploying Sunshine Engine to Digital Ocean..."

# Configuration
DROPLET_NAME="sunshine-engine-bot"
DROPLET_SIZE="s-1vcpu-1gb"
DROPLET_REGION="nyc3"
DROPLET_IMAGE="docker-20-04"

# Check if doctl is installed
if ! command -v doctl &> /dev/null; then
    echo "❌ doctl is not installed. Please install it first:"
    echo "   https://github.com/digitalocean/doctl/releases"
    exit 1
fi

# Check if Digital Ocean token is set
if [ -z "$DIGITAL_OCEAN_TOKEN" ]; then
    echo "❌ DIGITAL_OCEAN_TOKEN environment variable is not set"
    echo "   Please set it with: export DIGITAL_OCEAN_TOKEN=your_token_here"
    exit 1
fi

# Authenticate with Digital Ocean
echo "🔐 Authenticating with Digital Ocean..."
doctl auth init

# Create droplet
echo "🚀 Creating Digital Ocean droplet..."
DROPLET_ID=$(doctl compute droplet create $DROPLET_NAME \
    --size $DROPLET_SIZE \
    --region $DROPLET_REGION \
    --image $DROPLET_IMAGE \
    --ssh-keys $(doctl compute ssh-key list --format ID --no-header | head -1) \
    --format ID --no-header)

echo "✅ Droplet created with ID: $DROPLET_ID"

# Wait for droplet to be ready
echo "⏳ Waiting for droplet to be ready..."
doctl compute droplet wait $DROPLET_ID

# Get droplet IP
DROPLET_IP=$(doctl compute droplet get $DROPLET_ID --format PublicIPv4 --no-header)
echo "🌐 Droplet IP: $DROPLET_IP"

# Wait for SSH to be available
echo "🔌 Waiting for SSH to be available..."
until ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no root@$DROPLET_IP exit 2>/dev/null; do
    echo "   Still waiting..."
    sleep 5
done

# Deploy application
echo "📦 Deploying Sunshine Engine..."
ssh -o StrictHostKeyChecking=no root@$DROPLET_IP << 'EOF'
    # Update system
    apt-get update
    apt-get install -y git docker.io docker-compose
    
    # Start Docker
    systemctl start docker
    systemctl enable docker
    
    # Clone repository (replace with your GitHub repo)
    git clone https://github.com/yourusername/sunshine-engine.git /app
    cd /app
    
    # Build and start with Docker Compose
    docker-compose up -d --build
    
    # Set up firewall
    ufw allow 22
    ufw allow 80
    ufw allow 443
    ufw --force enable
EOF

echo "✅ Deployment complete!"
echo "🌐 Your Sunshine Engine HTTP Bot is running at: http://$DROPLET_IP"
echo "📊 API endpoints:"
echo "   - Health: http://$DROPLET_IP/health"
echo "   - API: http://$DROPLET_IP/api/"
echo "   - Generated content: http://$DROPLET_IP/generated/"

# Save deployment info
echo "💾 Saving deployment information..."
cat > deployment-info.txt << EOF
Droplet ID: $DROPLET_ID
Droplet IP: $DROPLET_IP
Deployment Date: $(date)
GitHub Repository: https://github.com/yourusername/sunshine-engine
Digital Ocean Dashboard: https://cloud.digitalocean.com/droplets/$DROPLET_ID
EOF

echo "📄 Deployment info saved to deployment-info.txt"
echo "🎉 Sunshine Engine is now live on Digital Ocean!"
