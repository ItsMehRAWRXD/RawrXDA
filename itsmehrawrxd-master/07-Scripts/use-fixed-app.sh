#!/bin/bash
set -e

echo " Using Fixed Application Version"
echo "=================================="

# Stop the service
systemctl stop rawrz.service || true

echo "1. Backing up original application..."
cd /root/RawrZApp
cp api-server-no-cli.js api-server-no-cli.js.backup

echo "2. Using the fixed application version..."
cp api-server-fixed.js api-server-no-cli.js

echo "3. Creating simple systemd service..."
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

# Process management
KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
EOF

echo "4. Starting service with fixed application..."
systemctl daemon-reload
systemctl start rawrz.service

echo "5. Waiting and checking status..."
sleep 15
systemctl status rawrz.service --no-pager

echo ""
echo "6. Testing functionality..."
sleep 5
if systemctl is-active --quiet rawrz.service; then
    echo " Service is running!"
    echo "Testing health endpoint..."
    curl -s http://localhost:3000/api/health | head -c 200 || echo "Health check failed"
    
    echo ""
    echo " RawrZ Security Platform is accessible at:"
    echo "   http://$(hostname -I | awk '{print $1}'):3000"
    echo "   http://localhost:3000"
else
    echo " Service is still not running"
    echo "Recent logs:"
    journalctl -u rawrz.service --no-pager -n 10
    
    echo ""
    echo "Restoring original application..."
    cp api-server-no-cli.js.backup api-server-no-cli.js
fi

echo ""
echo " Fixed application test completed!"
