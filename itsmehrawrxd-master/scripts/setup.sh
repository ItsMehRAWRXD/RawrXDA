#!/bin/bash

# RawrZ Security Platform Setup Script
# This script sets up the development environment

set -e

echo " Setting up RawrZ Security Platform..."

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo " Docker is not installed. Please install Docker first."
    exit 1
fi

# Check if Docker Compose is installed
if ! command -v docker-compose &> /dev/null; then
    echo " Docker Compose is not installed. Please install Docker Compose first."
    exit 1
fi

# Create necessary directories
echo " Creating directories..."
mkdir -p uploads downloads temp logs data keys stubs payloads bots cve engines backups public

# Set proper permissions
echo " Setting permissions..."
chmod 755 uploads downloads temp logs data keys stubs payloads bots cve engines backups public

# Copy environment template if .env doesn't exist
if [ ! -f .env ]; then
    echo " Creating .env file from template..."
    cp .env.template .env
    echo "  Please edit .env file with your actual configuration values"
fi

# Install Node.js dependencies
echo " Installing Node.js dependencies..."
npm install

# Build Docker images
echo " Building Docker images..."
docker-compose build

# Start services
echo " Starting services..."
docker-compose up -d

# Wait for services to be ready
echo "⏳ Waiting for services to be ready..."
sleep 30

# Check service health
echo " Checking service health..."
docker-compose ps

# Run database migrations
echo "  Running database migrations..."
docker-compose exec rawrz-app npm run migrate

echo " Setup complete!"
echo ""
echo " Access the platform at:"
echo "   - Main Interface: http://localhost:3000"
echo "   - API Documentation: http://localhost:3000/api/docs"
echo "   - Health Check: http://localhost:3000/api/health"
echo ""
echo " Monitoring:"
echo "   - Prometheus: http://localhost:9090"
echo "   - Grafana: http://localhost:3001"
echo ""
echo " Useful commands:"
echo "   - View logs: docker-compose logs -f"
echo "   - Stop services: docker-compose down"
echo "   - Restart services: docker-compose restart"
echo "   - Update services: docker-compose pull && docker-compose up -d"
