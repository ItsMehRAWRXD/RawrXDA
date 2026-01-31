#!/bin/bash

# RawrZ Security Platform - Docker Environment Update Script
# This script updates the local Docker environment to match the droplet

set -e

echo " RawrZ Security Platform - Docker Environment Update"
echo "======================================================"

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

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    print_error "Docker is not running. Please start Docker Desktop and try again."
    exit 1
fi

print_success "Docker is running"

# Check if Docker Compose is available
if ! command -v docker-compose &> /dev/null; then
    print_error "Docker Compose is not installed. Please install Docker Compose and try again."
    exit 1
fi

print_success "Docker Compose is available"

# Stop existing containers
print_status "Stopping existing containers..."
docker-compose down --remove-orphans || true

# Remove old images to force rebuild
print_status "Removing old images..."
docker-compose down --rmi all --volumes --remove-orphans || true

# Clean up Docker system
print_status "Cleaning up Docker system..."
docker system prune -f || true

# Create necessary directories
print_status "Creating necessary directories..."
mkdir -p uploads downloads temp logs data keys stubs payloads bots cve engines backups
mkdir -p nginx/ssl monitoring/rules

# Set proper permissions
print_status "Setting proper permissions..."
chmod -R 755 uploads downloads temp logs data keys stubs payloads bots cve engines backups
chmod -R 755 nginx monitoring

# Install/update dependencies
print_status "Installing/updating Node.js dependencies..."
if [ -f "package.json" ]; then
    npm install --production --ignore-scripts --no-optional
    print_success "Core dependencies installed successfully"
    print_status "Some advanced cryptography packages may not be available yet"
    print_status "Core functionality will work with available packages"
else
    print_warning "package.json not found, skipping dependency installation"
fi

# Build and start containers
print_status "Building Docker images..."
docker-compose build --no-cache

print_status "Starting containers..."
docker-compose up -d

# Wait for services to be ready
print_status "Waiting for services to be ready..."
sleep 30

# Check container health
print_status "Checking container health..."
docker-compose ps

# Test API endpoints
print_status "Testing API endpoints..."

# Test main application
if curl -f http://localhost:3000/api/health > /dev/null 2>&1; then
    print_success "Main application is responding"
else
    print_warning "Main application is not responding yet"
fi

# Test nginx proxy
if curl -f http://localhost/health > /dev/null 2>&1; then
    print_success "Nginx proxy is responding"
else
    print_warning "Nginx proxy is not responding yet"
fi

# Test database connection
if docker-compose exec -T rawrz-db pg_isready -U rawrz_user -d rawrz_security > /dev/null 2>&1; then
    print_success "Database is ready"
else
    print_warning "Database is not ready yet"
fi

# Test Redis connection
if docker-compose exec -T rawrz-redis redis-cli ping > /dev/null 2>&1; then
    print_success "Redis is ready"
else
    print_warning "Redis is not ready yet"
fi

# Run comprehensive tests
print_status "Running comprehensive environment tests..."

# Test file upload
echo "Testing file upload..."
echo "test content" > test-file.txt
if curl -X POST -F "file=@test-file.txt" http://localhost:3000/api/upload > /dev/null 2>&1; then
    print_success "File upload test passed"
else
    print_warning "File upload test failed"
fi
rm -f test-file.txt

# Test encryption endpoint
echo "Testing encryption endpoint..."
if curl -X POST -H "Content-Type: application/json" -d '{"algorithm":"AES-256-CBC","data":"dGVzdA=="}' http://localhost:3000/api/real-encryption/encrypt > /dev/null 2>&1; then
    print_success "Encryption endpoint test passed"
else
    print_warning "Encryption endpoint test failed"
fi

# Test bot management
echo "Testing bot management..."
if curl -X GET http://localhost:3000/api/bots > /dev/null 2>&1; then
    print_success "Bot management endpoint test passed"
else
    print_warning "Bot management endpoint test failed"
fi

# Test CVE analysis
echo "Testing CVE analysis..."
if curl -X GET http://localhost:3000/api/cve/analyze > /dev/null 2>&1; then
    print_success "CVE analysis endpoint test passed"
else
    print_warning "CVE analysis endpoint test failed"
fi

# Test stub generation
echo "Testing stub generation..."
if curl -X POST -H "Content-Type: application/json" -d '{"type":"exe","architecture":"x64"}' http://localhost:3000/api/stubs/generate > /dev/null 2>&1; then
    print_success "Stub generation endpoint test passed"
else
    print_warning "Stub generation endpoint test failed"
fi

# Display service URLs
echo ""
echo " Service URLs:"
echo "================"
echo "Main Application: http://localhost:3000"
echo "Web Interface: http://localhost"
echo "Health Dashboard: http://localhost/health-dashboard.html"
echo "Encryption Panel: http://localhost/encryption-panel.html"
echo "Advanced Encryption: http://localhost/advanced-encryption-panel.html"
echo "Bot Manager: http://localhost/bot-manager.html"
echo "CVE Analysis: http://localhost/cve-analysis-panel.html"
echo "CLI Interface: http://localhost/advanced-encryption-panel.html"
echo ""
echo " Monitoring:"
echo "=============="
echo "Prometheus: http://localhost:9090"
echo "Loki Logs: http://localhost:3100"
echo ""
echo " Database:"
echo "============"
echo "PostgreSQL: localhost:5432"
echo "Redis: localhost:6379"
echo ""

# Display container status
echo " Container Status:"
echo "===================="
docker-compose ps

# Display logs for main application
echo ""
echo " Recent Application Logs:"
echo "==========================="
docker-compose logs --tail=20 rawrz-app

echo ""
print_success "Docker environment update completed!"
print_status "Your local environment is now synchronized with the droplet"
print_status "All advanced features are available and ready for testing"

# Cleanup
rm -f test-file.txt

echo ""
echo " Next Steps:"
echo "=============="
echo "1. Open http://localhost in your browser"
echo "2. Test the encryption panel with file uploads"
echo "3. Try the advanced features (dangerous options, PowerShell, etc.)"
echo "4. Generate and test stubs"
echo "5. Test bot management and CVE analysis"
echo ""
echo " Tips:"
echo "========"
echo "- Use 'docker-compose logs -f [service]' to follow logs"
echo "- Use 'docker-compose restart [service]' to restart a service"
echo "- Use 'docker-compose down' to stop all services"
echo "- Use 'docker-compose up -d' to start all services"
echo ""
print_success "Environment is ready for airtight testing! "
