#!/bin/bash
set -e

echo " RawrZ Service Debug Script"
echo "============================="

# Stop the service first
echo "1. Stopping service..."
systemctl stop rawrz.service || true

echo ""
echo "2. Checking recent service logs..."
echo "Last 30 lines of service logs:"
journalctl -u rawrz.service --no-pager -n 30

echo ""
echo "3. Checking for any error patterns..."
echo "Looking for ERROR, FAILED, or EXCEPTION in logs:"
journalctl -u rawrz.service --no-pager | grep -i -E "(error|failed|exception|fatal)" | tail -10 || echo "No obvious errors found"

echo ""
echo "4. Testing manual execution of the startup script..."
cd /root/RawrZApp
echo "Current directory: $(pwd)"
echo "Testing startup script manually:"
timeout 10s ./start-rawrz-robust.sh || echo "Manual execution failed or timed out"

echo ""
echo "5. Checking if the startup script exists and is executable..."
ls -la start-rawrz-robust.sh || echo "Startup script not found"

echo ""
echo "6. Testing direct Node.js execution..."
echo "Testing Node.js with the application file:"
timeout 10s node api-server-no-cli.js || echo "Direct Node.js execution failed or timed out"

echo ""
echo "7. Checking Node.js and application file..."
echo "Node.js version: $(node --version)"
echo "Application file exists: $(ls -la api-server-no-cli.js)"

echo ""
echo "8. Checking for any port conflicts..."
netstat -tlnp | grep :3000 || echo "Port 3000 is free"

echo ""
echo "9. Checking system resources..."
echo "Memory usage:"
free -h
echo "Disk space:"
df -h /

echo ""
echo "10. Checking if there are any permission issues..."
echo "Current user: $(whoami)"
echo "Directory permissions:"
ls -la /root/RawrZApp/ | head -10

echo ""
echo " Debug information collected!"
echo "Please review the output above to identify the issue."
