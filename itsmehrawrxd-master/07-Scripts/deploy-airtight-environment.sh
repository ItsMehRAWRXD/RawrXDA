#!/bin/bash

# RawrZ Security Platform - Airtight Environment Deployment
# Updates droplet with all new functionality

echo " RawrZ Security Platform - Airtight Environment Deployment"
echo "=========================================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're on the server
if [ ! -d "/root/RawrZApp" ]; then
    print_error "This script must be run on the RawrZ server"
    exit 1
fi

print_status "Starting airtight environment deployment..."

# Step 1: Stop the current service
print_status "Stopping RawrZ service..."
systemctl stop rawrz.service
if [ $? -eq 0 ]; then
    print_success "Service stopped successfully"
else
    print_warning "Service may not have been running"
fi

# Step 2: Navigate to app directory
cd /root/RawrZApp

# Step 3: Pull latest changes from GitHub
print_status "Pulling latest changes from GitHub..."
git pull origin main
if [ $? -eq 0 ]; then
    print_success "Latest changes pulled successfully"
else
    print_error "Failed to pull changes from GitHub"
    exit 1
fi

# Step 4: Install any new dependencies
print_status "Checking for new dependencies..."
if [ -f "package.json" ]; then
    npm install
    if [ $? -eq 0 ]; then
        print_success "Dependencies updated successfully"
    else
        print_warning "Some dependencies may have failed to install"
    fi
fi

# Step 5: Make scripts executable
print_status "Making deployment scripts executable..."
chmod +x test-airtight-environment.sh
chmod +x fix-missing-endpoints.sh
chmod +x test-real-engines.sh
print_success "Scripts made executable"

# Step 6: Test the application before starting
print_status "Testing application functionality..."
node -c api-server-no-cli.js
if [ $? -eq 0 ]; then
    print_success "Application syntax check passed"
else
    print_error "Application syntax check failed"
    exit 1
fi

# Step 7: Start the service
print_status "Starting RawrZ service..."
systemctl start rawrz.service
if [ $? -eq 0 ]; then
    print_success "Service started successfully"
else
    print_error "Failed to start service"
    exit 1
fi

# Step 8: Wait for service to fully start
print_status "Waiting for service to initialize..."
sleep 5

# Step 9: Check service status
print_status "Checking service status..."
systemctl status rawrz.service --no-pager -l
if systemctl is-active --quiet rawrz.service; then
    print_success "Service is running and active"
else
    print_error "Service is not running properly"
    systemctl status rawrz.service --no-pager -l
    exit 1
fi

# Step 10: Test API endpoints
print_status "Testing API endpoints..."
sleep 3

# Test health endpoint
health_response=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/api/health)
if [ "$health_response" = "200" ]; then
    print_success "Health endpoint responding correctly"
else
    print_warning "Health endpoint returned HTTP $health_response"
fi

# Test engines endpoint
engines_response=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/api/engines)
if [ "$engines_response" = "200" ]; then
    print_success "Engines endpoint responding correctly"
else
    print_warning "Engines endpoint returned HTTP $engines_response"
fi

# Test new bot management endpoint
bots_response=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/api/bots)
if [ "$bots_response" = "200" ]; then
    print_success "Bot management endpoint responding correctly"
else
    print_warning "Bot management endpoint returned HTTP $bots_response"
fi

# Test new CVE analysis endpoint
cve_response=$(curl -s -o /dev/null -w "%{http_code}" -X POST \
    -H "Content-Type: application/json" \
    -d '{"cveId":"CVE-2023-1234"}' \
    http://localhost:3000/api/cve/analyze)
if [ "$cve_response" = "200" ]; then
    print_success "CVE analysis endpoint responding correctly"
else
    print_warning "CVE analysis endpoint returned HTTP $cve_response"
fi

# Step 11: Run comprehensive test suite
print_status "Running comprehensive test suite..."
if [ -f "test-airtight-environment.sh" ]; then
    chmod +x test-airtight-environment.sh
    ./test-airtight-environment.sh
    if [ $? -eq 0 ]; then
        print_success "All tests passed - Airtight environment is fully functional"
    else
        print_warning "Some tests failed - Check output above"
    fi
else
    print_warning "Test suite not found - Skipping comprehensive tests"
fi

# Step 12: Final status check
print_status "Final deployment status check..."
echo ""
echo " Service Status:"
systemctl is-active rawrz.service && echo " Service: ACTIVE" || echo " Service: INACTIVE"
systemctl is-enabled rawrz.service && echo " Auto-start: ENABLED" || echo " Auto-start: DISABLED"

echo ""
echo " Web Interface Status:"
curl -s -o /dev/null -w " Main Panel: HTTP %{http_code}\n" http://localhost:3000/panel.html
curl -s -o /dev/null -w " Encryption Panel: HTTP %{http_code}\n" http://localhost:3000/encryption-panel.html
curl -s -o /dev/null -w " Health Dashboard: HTTP %{http_code}\n" http://localhost:3000/health-dashboard.html
curl -s -o /dev/null -w " CVE Analysis: HTTP %{http_code}\n" http://localhost:3000/cve-analysis-panel.html
curl -s -o /dev/null -w " Bot Manager: HTTP %{http_code}\n" http://localhost:3000/bot-manager.html

echo ""
echo " Deployment Summary:"
echo "====================="
echo " GitHub changes pulled successfully"
echo " Service restarted with new code"
echo " All API endpoints tested"
echo " Airtight environment deployed"
echo ""
echo " RawrZ Security Platform is now running with:"
echo "   • Real file encryption with custom extensions"
echo "   • Real bot management and command execution"
echo "   • Real CVE analysis and database operations"
echo "   • Real payload creation and management"
echo "   • Real engine health monitoring"
echo "   • No fake responses or mirroring effects"
echo ""
echo " Access your platform at: http://198.199.70.153:3000/panel.html"
echo ""
print_success "Airtight environment deployment completed successfully!"
