#!/bin/bash
set -e

echo " Bulletproof RawrZ Service Fix"
echo "================================="

# Stop the service
systemctl stop rawrz.service || true

echo "1. Creating the simplest possible systemd service..."
cd /root/RawrZApp

# The simplest possible approach - just run node directly
cat <<EOF > /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/node /root/RawrZApp/api-server-no-cli.js
WorkingDirectory=/root/RawrZApp
Restart=always
RestartSec=10
User=root

# Environment
Environment=PORT=3000
Environment=NODE_ENV=production

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=rawrz

# Simple process management
KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
EOF

echo "2. Starting service..."
systemctl daemon-reload
systemctl start rawrz.service

echo "3. Waiting and checking..."
sleep 15

echo "Service status:"
systemctl status rawrz.service --no-pager

echo ""
echo "Testing connectivity..."
sleep 5
curl -s http://localhost:3000/api/health || echo "Health check failed"

echo ""
echo " Bulletproof fix applied!"
