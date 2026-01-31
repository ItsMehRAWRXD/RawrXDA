#!/bin/bash
set -e

echo " Docker Conflict Check for RawrZ Platform"
echo "==========================================="

echo "1. Checking current port usage..."
echo "=================================="
echo "Processes using port 3000:"
lsof -i :3000 || echo "No processes found on port 3000"

echo ""
echo "2. Checking Docker containers..."
echo "================================"
if command -v docker >/dev/null 2>&1; then
    echo "Docker is installed. Checking running containers:"
    docker ps || echo "No Docker containers running"
    
    echo ""
    echo "Checking Docker processes:"
    ps aux | grep docker || echo "No Docker processes found"
else
    echo "Docker is not installed on this system"
fi

echo ""
echo "3. Checking if RawrZ service is running..."
echo "=========================================="
if systemctl is-active --quiet rawrz.service; then
    echo " RawrZ service is running"
    
    # Test if the service is actually accessible
    echo "Testing RawrZ service accessibility..."
    if curl -s http://localhost:3000/api/health > /dev/null; then
        echo " RawrZ service is accessible on port 3000"
    else
        echo " RawrZ service is not accessible on port 3000"
    fi
else
    echo " RawrZ service is not running"
fi

echo ""
echo "4. Checking for port conflicts..."
echo "================================="
echo "Checking if multiple services are trying to use port 3000..."

# Check systemd services that might use port 3000
echo "Systemd services that might use port 3000:"
systemctl list-units --type=service | grep -E "(3000|port)" || echo "No services found with port 3000"

echo ""
echo "5. Testing alternative ports..."
echo "==============================="
echo "Testing if we can use alternative ports for RawrZ..."

# Test ports 3001-3005
for port in 3001 3002 3003 3004 3005; do
    if ! lsof -i :$port >/dev/null 2>&1; then
        echo " Port $port is available"
    else
        echo " Port $port is in use"
    fi
done

echo ""
echo "6. Docker cleanup recommendations..."
echo "==================================="
if command -v docker >/dev/null 2>&1; then
    echo "If you need to free up port 3000, you can:"
    echo "1. Stop Docker containers using port 3000:"
    echo "   docker ps --filter 'publish=3000'"
    echo "   docker stop <container_id>"
    echo ""
    echo "2. Or change RawrZ to use a different port:"
    echo "   Edit /etc/systemd/system/rawrz.service"
    echo "   Change Environment=PORT=3000 to Environment=PORT=3001"
    echo "   systemctl daemon-reload && systemctl restart rawrz.service"
else
    echo "Docker is not installed, no cleanup needed"
fi

echo ""
echo "7. Current status summary..."
echo "==========================="
if systemctl is-active --quiet rawrz.service && curl -s http://localhost:3000/api/health > /dev/null; then
    echo " RawrZ is running and accessible - NO ACTION NEEDED"
    echo " Access your platform at: http://$(hostname -I | awk '{print $1}'):3000"
else
    echo " RawrZ needs attention"
    echo "Recommendations:"
    echo "1. Check if Docker is conflicting with port 3000"
    echo "2. Consider using a different port for RawrZ"
    echo "3. Or stop Docker containers using port 3000"
fi

echo ""
echo " Docker conflict check completed!"
