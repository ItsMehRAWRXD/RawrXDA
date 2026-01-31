#!/bin/bash
set -e

echo " Docker Conflict Resolution for RawrZ Platform"
echo "================================================="

echo "1. Checking current situation..."
echo "================================"

# Check if RawrZ is running
if systemctl is-active --quiet rawrz.service; then
    echo " RawrZ service is running"
    
    # Test accessibility
    if curl -s http://localhost:3000/api/health > /dev/null; then
        echo " RawrZ is accessible on port 3000"
        echo " NO ACTION NEEDED - Everything is working fine!"
        exit 0
    else
        echo " RawrZ service is running but not accessible"
    fi
else
    echo " RawrZ service is not running"
fi

echo ""
echo "2. Checking Docker conflicts..."
echo "==============================="

if command -v docker >/dev/null 2>&1; then
    echo "Docker is installed. Checking for conflicts..."
    
    # Check Docker containers using port 3000
    DOCKER_CONTAINERS=$(docker ps --filter "publish=3000" --format "table {{.ID}}\t{{.Names}}\t{{.Ports}}" 2>/dev/null || echo "")
    
    if [ -n "$DOCKER_CONTAINERS" ]; then
        echo "Found Docker containers using port 3000:"
        echo "$DOCKER_CONTAINERS"
        
        echo ""
        echo "3. Resolving Docker conflicts..."
        echo "================================"
        echo "Option 1: Stop Docker containers using port 3000"
        echo "Option 2: Change RawrZ to use a different port"
        echo ""
        echo "Which option would you prefer?"
        echo "1) Stop Docker containers (recommended if you don't need them)"
        echo "2) Change RawrZ to use port 3001"
        echo "3) Skip and keep current setup"
        
        read -p "Enter your choice (1-3): " choice
        
        case $choice in
            1)
                echo "Stopping Docker containers using port 3000..."
                docker ps --filter "publish=3000" --format "{{.ID}}" | xargs -r docker stop
                echo " Docker containers stopped"
                
                echo "Restarting RawrZ service..."
                systemctl restart rawrz.service
                sleep 5
                
                if systemctl is-active --quiet rawrz.service && curl -s http://localhost:3000/api/health > /dev/null; then
                    echo " RawrZ is now running and accessible on port 3000"
                else
                    echo " RawrZ still has issues"
                fi
                ;;
            2)
                echo "Changing RawrZ to use port 3001..."
                
                # Stop RawrZ service
                systemctl stop rawrz.service
                
                # Update systemd service to use port 3001
                sed -i 's/Environment=PORT=3000/Environment=PORT=3001/' /etc/systemd/system/rawrz.service
                
                # Reload and start
                systemctl daemon-reload
                systemctl start rawrz.service
                sleep 5
                
                if systemctl is-active --quiet rawrz.service && curl -s http://localhost:3001/api/health > /dev/null; then
                    echo " RawrZ is now running and accessible on port 3001"
                    echo " Access your platform at: http://$(hostname -I | awk '{print $1}'):3001"
                else
                    echo " RawrZ failed to start on port 3001"
                fi
                ;;
            3)
                echo "Skipping Docker conflict resolution"
                ;;
            *)
                echo "Invalid choice. Skipping."
                ;;
        esac
    else
        echo "No Docker containers found using port 3000"
    fi
else
    echo "Docker is not installed. No Docker conflicts to resolve."
fi

echo ""
echo "4. Final status check..."
echo "========================"
if systemctl is-active --quiet rawrz.service; then
    echo " RawrZ service is running"
    
    # Determine which port to test
    PORT=$(systemctl show rawrz.service --property=Environment | grep -o 'PORT=[0-9]*' | cut -d'=' -f2 || echo "3000")
    
    if curl -s http://localhost:$PORT/api/health > /dev/null; then
        echo " RawrZ is accessible on port $PORT"
        echo " Access your platform at: http://$(hostname -I | awk '{print $1}'):$PORT"
    else
        echo " RawrZ is not accessible on port $PORT"
    fi
else
    echo " RawrZ service is not running"
fi

echo ""
echo " Docker conflict resolution completed!"
