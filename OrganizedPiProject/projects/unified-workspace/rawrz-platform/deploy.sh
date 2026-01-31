#!/bin/bash

# RawrZ HTTP Encryptor - Complete DigitalOcean Deployment Script
echo "🚀 RawrZ HTTP Encryptor DigitalOcean Deployment"
echo "==============================================="

sleep 2

# Step 1: Update System
echo "📦 Updating system packages..."
sudo apt update && sudo apt upgrade -y -qq

# Step 2: Install Node.js
echo "📦 Installing Node.js..."
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
    sudo apt-get install -y nodejs
fi

# Step 3: Clone from GitHub if not already present
echo "📂 Backing up if needed..."
if [ ! -d "rawrz-http-encryptor" ]; then
    echo "📝 Repository ready for GitHub sync"
else
    cd rawrz-http-encryptor
fi

# Step 4: Install Dependencies
echo "📦 Installing dependencies..."
npm install --production

# Step 5: Setup Environment
if [ ! -f .env ]; then
    echo "⚙️ Generating secure auth token..."
    echo "AUTH_TOKEN=$(openssl rand -hex 32)" > .env
    echo "PORT=8080" >> .env
    echo "NODE_ENV=production" >> .env
fi

# Step 6: Setup Security
echo "🔒 Setting up secure user..."
sudo useradd -r -s /bin/false rawrz 2>/dev/null || true
sudo mkdir -p /var/log/rawrz 2>/dev/null || true

# Step 7: Firewall Rules (UFW)
echo "🔥 Configuring firewall..."
sudo ufw allow 22/tcp      # SSH
sudo ufw allow 8080/tcp    # RawrZ HTTP Encryptor
sudo ufw --force enable 2>/dev/null || true

# Step 8: Create systemd service
echo "⚙️ Installing service..."
sudo tee /etc/systemd/system/rawrz-encryptor.service > /dev/null <<EOF
[Unit]
Description=RawrZ HTTP Encryption Platform
After=network.target

[Service]
Type=simple
User=rawrz
Group=rawrz
WorkingDirectory=$(pwd)
ExecStart=/usr/bin/node server.js
Restart=always
RestartSec=10

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ReadWritePaths=$(pwd)

[Install]
WantedBy=multi-user.target
EOF

# Step 9: Start Services
echo "🎯 Starting services..."
sudo systemctl daemon-reload
sudo systemctl enable rawrz-encryptor
sudo systemctl restart rawrz-encryptor

# Step 10: Verify installation
echo "✅ Installation Status:"
echo "====================="
sudo systemctl status rawrz-encryptor --no-pager -l 0 --lines=10

LOCAL_IP=$(hostname -I | awk '{print $1}')
EXTERNAL_IP=$(curl -s checkip.amazonaws.com 2>/dev/null || echo "external-ip-fetch-failed")

echo ""
echo "🌐 RawrZ HTTP Encryptor Ready!"
echo "=============================="
echo "LOCAL:   http://$LOCAL_IP:8080"
echo "EXTERNAL: http://$EXTERNAL_IP:8080"
echo ""
echo "📡 Available Endpoints:"
echo "https://$EXTERNAL_IP:8080/panel      (Main Platform)"
echo "https://$EXTERNAL_IP:8080/encryptor   (Encryption Interface)"
echo "https://$EXTERNAL_IP:8080/health      (Health Check)"
echo ""
echo "🔑 Auth token is set in: .env"
echo ""

# Step 11: Optional: Add nginx reverse proxy
echo "⚠️ OPTIONAL: Install nginx proxy for SSL:"
echo "sudo apt install nginx -y"
echo "nano /etc/nginx/sites-available/rawrz-encryptor"
echo ""
echo "✅ RawrZ HTTP Encryptor deployed successfully!"
