#!/bin/bash
set -e

echo " Fixing RawrZ Systemd Service Type Issue"
echo "==========================================="

# Stop the current service
echo "Stopping current service..."
systemctl stop rawrz.service || true

# Update the systemd service file with correct service type
echo "Updating systemd service configuration..."
cat <<EOF > /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target

[Service]
Type=notify
ExecStart=/usr/bin/node api-server-no-cli.js
WorkingDirectory=/root/RawrZApp
Restart=always
RestartSec=5
User=root
Environment=PORT=3000
Environment=NODE_ENV=production

# Keep the service running
StandardOutput=journal
StandardError=journal
SyslogIdentifier=rawrz

# Security settings (relaxed for Node.js)
NoNewPrivileges=false
PrivateTmp=false
ProtectSystem=false
ProtectHome=false

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd and start the service
echo "Reloading systemd configuration..."
systemctl daemon-reload

echo "Starting RawrZ service..."
systemctl start rawrz.service

# Wait a moment and check status
sleep 5
echo "Checking service status..."
systemctl status rawrz.service --no-pager

echo ""
echo "Checking if service is actually running..."
sleep 2
systemctl is-active rawrz.service

echo ""
echo "Testing web interface accessibility..."
curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/api/health || echo "Connection failed"

echo ""
echo " Systemd service type fixed!"
echo " RawrZ Security Platform should now be accessible at: http://$(hostname -I | awk '{print $1}'):3000"
