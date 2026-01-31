#!/bin/bash
set -e

echo " Final RawrZ Service Fix - Systemd Type Issue"
echo "==============================================="

# Stop the service
systemctl stop rawrz.service || true

echo "1. Creating a proper daemon wrapper..."
cd /root/RawrZApp

# Create a daemon wrapper that systemd can properly track
cat <<'EOF' > start-rawrz-daemon.sh
#!/bin/bash
cd /root/RawrZApp

# Set environment
export PORT=3000
export NODE_ENV=production

# Create directories
mkdir -p uploads processed 2>/dev/null || true

# Start the application and keep it running
echo "Starting RawrZ Security Platform daemon..."
exec node api-server-no-cli.js
EOF

chmod +x start-rawrz-daemon.sh

echo "2. Creating the correct systemd service configuration..."
# The key is using Type=notify or Type=exec with proper process management
cat <<EOF > /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target
Wants=network.target

[Service]
Type=notify
ExecStart=/root/RawrZApp/start-rawrz-daemon.sh
WorkingDirectory=/root/RawrZApp
Restart=always
RestartSec=10
User=root
Group=root

# Environment
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

# Security
NoNewPrivileges=false
PrivateTmp=false
ProtectSystem=false
ProtectHome=false

# Notify systemd that we're ready
NotifyAccess=all

[Install]
WantedBy=multi-user.target
EOF

echo "3. Alternative: Creating a Type=exec version (fallback)..."
# If Type=notify doesn't work, we'll use Type=exec
cat <<EOF > /etc/systemd/system/rawrz-exec.service
[Unit]
Description=RawrZ Security Platform (Exec Type)
After=network.target

[Service]
Type=exec
ExecStart=/root/RawrZApp/start-rawrz-daemon.sh
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

echo "4. Reloading systemd and trying Type=notify first..."
systemctl daemon-reload
systemctl start rawrz.service

echo "5. Waiting for service to start..."
sleep 15

echo "6. Checking service status..."
systemctl status rawrz.service --no-pager

# Check if Type=notify worked
if systemctl is-active --quiet rawrz.service; then
    echo " Type=notify worked! Service is running."
else
    echo " Type=notify failed, trying Type=exec..."
    
    # Stop and switch to Type=exec
    systemctl stop rawrz.service
    cp /etc/systemd/system/rawrz-exec.service /etc/systemd/system/rawrz.service
    systemctl daemon-reload
    systemctl start rawrz.service
    
    sleep 10
    echo "Checking Type=exec status..."
    systemctl status rawrz.service --no-pager
fi

echo ""
echo "7. Final service check..."
if systemctl is-active --quiet rawrz.service; then
    echo " Service is running!"
    echo "Testing health endpoint..."
    sleep 5
    curl -s http://localhost:3000/api/health | head -c 200 || echo "Health check failed"
    
    echo ""
    echo " RawrZ Security Platform is accessible at:"
    echo "   http://$(hostname -I | awk '{print $1}'):3000"
    echo "   http://localhost:3000"
    
    echo ""
    echo "Service status:"
    systemctl is-active rawrz.service
    systemctl is-enabled rawrz.service
else
    echo " Service is still not running"
    echo "Recent logs:"
    journalctl -u rawrz.service --no-pager -n 10
fi

echo ""
echo " Final fix completed!"
