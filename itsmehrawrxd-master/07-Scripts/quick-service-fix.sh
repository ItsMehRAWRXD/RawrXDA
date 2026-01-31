#!/bin/bash
set -e

echo " Quick RawrZ Service Fix"
echo "=========================="

# Stop service
systemctl stop rawrz.service || true

# The main issue is likely that the service is exiting immediately
# Let's create a wrapper script that keeps the process alive
echo "Creating service wrapper script..."
cat <<'EOF' > /root/RawrZApp/start-rawrz.sh
#!/bin/bash
cd /root/RawrZApp
exec /usr/bin/node api-server-no-cli.js
EOF

chmod +x /root/RawrZApp/start-rawrz.sh

# Update systemd service to use the wrapper
echo "Updating systemd service configuration..."
cat <<EOF > /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target

[Service]
Type=exec
ExecStart=/root/RawrZApp/start-rawrz.sh
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

# Process management
KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=30

# Security settings
NoNewPrivileges=false
PrivateTmp=false
ProtectSystem=false
ProtectHome=false

[Install]
WantedBy=multi-user.target
EOF

# Reload and start
systemctl daemon-reload
systemctl start rawrz.service

# Wait and check
sleep 10
echo "Service status:"
systemctl status rawrz.service --no-pager

echo ""
echo "Testing connectivity..."
curl -s http://localhost:3000/api/health || echo "Health check failed"

echo ""
echo " Quick fix applied!"
