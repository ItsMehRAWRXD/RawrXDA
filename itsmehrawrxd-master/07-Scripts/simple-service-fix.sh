#!/bin/bash
set -e

echo " Simple RawrZ Service Fix"
echo "==========================="

# Stop the service
systemctl stop rawrz.service || true

echo "1. Creating a simple wrapper that keeps the process alive..."
cd /root/RawrZApp

# Create a simple wrapper that handles errors and keeps the process running
cat <<'EOF' > start-rawrz-simple.sh
#!/bin/bash
cd /root/RawrZApp

# Set environment
export PORT=3000
export NODE_ENV=production

# Create directories
mkdir -p uploads processed 2>/dev/null || true

# Start with error handling
echo "Starting RawrZ Security Platform..."
exec node api-server-no-cli.js
EOF

chmod +x start-rawrz-simple.sh

echo "2. Creating a simple systemd service..."
cat <<EOF > /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target

[Service]
Type=simple
ExecStart=/root/RawrZApp/start-rawrz-simple.sh
WorkingDirectory=/root/RawrZApp
Restart=always
RestartSec=5
User=root
Environment=PORT=3000
Environment=NODE_ENV=production

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=rawrz

# Keep process alive
KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
EOF

echo "3. Starting service..."
systemctl daemon-reload
systemctl start rawrz.service

echo "4. Waiting and checking status..."
sleep 10
systemctl status rawrz.service --no-pager

echo ""
echo "5. Testing connectivity..."
sleep 5
curl -s http://localhost:3000/api/health || echo "Health check failed"

echo ""
echo " Simple fix applied!"
