#!/bin/bash
set -e

echo " RawrZ Service Diagnostic and Fix Script"
echo "=========================================="

# Stop the service first
echo "1. Stopping current service..."
systemctl stop rawrz.service || true

# Check service logs
echo ""
echo "2. Checking service logs..."
echo "Recent logs:"
journalctl -u rawrz.service --no-pager -n 20

echo ""
echo "3. Checking if Node.js is available..."
which node || echo "Node.js not found in PATH"
node --version || echo "Node.js version check failed"

echo ""
echo "4. Checking if the application file exists..."
ls -la /root/RawrZApp/api-server-no-cli.js || echo "Application file not found"

echo ""
echo "5. Testing manual startup..."
cd /root/RawrZApp
echo "Current directory: $(pwd)"
echo "Files in directory:"
ls -la

echo ""
echo "6. Testing Node.js execution manually..."
timeout 10s node api-server-no-cli.js || echo "Manual execution failed or timed out"

echo ""
echo "7. Checking for missing dependencies..."
if [ -f "package.json" ]; then
    echo "Package.json found, checking dependencies..."
    npm list --depth=0 2>/dev/null || echo "Dependency check failed"
else
    echo "No package.json found"
fi

echo ""
echo "8. Creating improved systemd service configuration..."
cat <<EOF > /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target
Wants=network.target

[Service]
Type=simple
ExecStart=/usr/bin/node api-server-no-cli.js
WorkingDirectory=/root/RawrZApp
Restart=always
RestartSec=10
User=root
Group=root

# Environment variables
Environment=PORT=3000
Environment=NODE_ENV=production
Environment=NODE_OPTIONS="--max-old-space-size=4096"

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=rawrz

# Process management
KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=30
TimeoutStartSec=60

# Security settings (relaxed for development)
NoNewPrivileges=false
PrivateTmp=false
ProtectSystem=false
ProtectHome=false

# Keep the service running
RemainAfterExit=no

[Install]
WantedBy=multi-user.target
EOF

echo ""
echo "9. Reloading systemd and starting service..."
systemctl daemon-reload
systemctl enable rawrz.service
systemctl start rawrz.service

echo ""
echo "10. Waiting for service to stabilize..."
sleep 15

echo ""
echo "11. Checking service status..."
systemctl status rawrz.service --no-pager

echo ""
echo "12. Checking if service is active..."
if systemctl is-active --quiet rawrz.service; then
    echo " Service is active!"
else
    echo " Service is not active"
    echo "Recent logs:"
    journalctl -u rawrz.service --no-pager -n 10
fi

echo ""
echo "13. Testing web interface..."
sleep 5
curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" http://localhost:3000/api/health || echo "Health check failed"

echo ""
echo "14. Final service check..."
systemctl is-active rawrz.service
systemctl is-enabled rawrz.service

echo ""
echo " Diagnostic and fix script completed!"
echo " RawrZ Security Platform should be accessible at: http://$(hostname -I | awk '{print $1}'):3000"
