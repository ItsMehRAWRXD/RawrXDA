#!/bin/bash
set -e

echo " Targeted RawrZ Service Fix"
echo "============================="

# Stop the service
echo "1. Stopping current service..."
systemctl stop rawrz.service || true

# The issue is likely that the Node.js app is starting but exiting immediately
# Let's create a simple test to see what's happening
echo ""
echo "2. Testing Node.js application directly..."
cd /root/RawrZApp

# Create a simple test script to see what's happening
cat <<'EOF' > test-app.js
console.log('Starting RawrZ test...');
console.log('Node.js version:', process.version);
console.log('Current directory:', process.cwd());
console.log('Environment PORT:', process.env.PORT);

try {
    require('./api-server-no-cli.js');
    console.log('Application loaded successfully');
} catch (error) {
    console.error('Error loading application:', error.message);
    console.error('Stack trace:', error.stack);
    process.exit(1);
}
EOF

echo "Running test script..."
timeout 15s node test-app.js || echo "Test script failed or timed out"

echo ""
echo "3. Creating a more robust systemd service configuration..."
# The issue might be with the service type or configuration
cat <<EOF > /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target
Wants=network.target

[Service]
Type=forking
ExecStart=/bin/bash -c 'cd /root/RawrZApp && nohup /usr/bin/node api-server-no-cli.js > /var/log/rawrz.log 2>&1 & echo \$! > /var/run/rawrz.pid'
ExecStop=/bin/bash -c 'kill \$(cat /var/run/rawrz.pid) 2>/dev/null || true'
PIDFile=/var/run/rawrz.pid
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

[Install]
WantedBy=multi-user.target
EOF

echo ""
echo "4. Creating log directory and setting permissions..."
mkdir -p /var/log
touch /var/log/rawrz.log
chmod 644 /var/log/rawrz.log

echo ""
echo "5. Reloading systemd and starting service..."
systemctl daemon-reload
systemctl start rawrz.service

echo ""
echo "6. Waiting for service to start..."
sleep 15

echo ""
echo "7. Checking service status..."
systemctl status rawrz.service --no-pager

echo ""
echo "8. Checking application logs..."
if [ -f "/var/log/rawrz.log" ]; then
    echo "Application log contents:"
    tail -20 /var/log/rawrz.log
else
    echo "No application log found"
fi

echo ""
echo "9. Testing service functionality..."
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
    echo "Checking recent logs:"
    journalctl -u rawrz.service --no-pager -n 10
fi

echo ""
echo " Targeted fix completed!"
