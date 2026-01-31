#!/bin/bash

# Ollama Service Setup Script
echo " Setting up Ollama API Service..."

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

# Create logs directory
mkdir -p logs

# Set up environment variables
echo " Setting up environment variables..."
if [ ! -f .env ]; then
    cat > .env << EOF
# Ollama Service Configuration
OLLAMA_URL=http://host.docker.internal:11434
YOUR_API_KEY=your-secret-key-here
DEFAULT_MODEL=llama3
DEBUG=false
PORT=5000
EOF
    echo " Created .env file with default settings"
    echo "  Please update the YOUR_API_KEY in .env file for security"
else
    echo " .env file already exists"
fi

# Build and start the service
echo " Building and starting the service..."
docker-compose up --build -d

# Wait for service to be ready
echo "⏳ Waiting for service to be ready..."
sleep 10

# Check if service is running
if curl -f http://localhost:5000/api/health > /dev/null 2>&1; then
    echo " Service is running successfully!"
    echo " API available at: http://localhost:5000"
    echo " Health check: http://localhost:5000/api/health"
    echo " Models list: http://localhost:5000/api/models"
else
    echo " Service failed to start. Check logs with: docker-compose logs"
    exit 1
fi

echo ""
echo " Setup complete! Your Ollama API service is ready."
echo ""
echo "Next steps:"
echo "1. Update the API key in .env file"
echo "2. Configure your browser extension with: http://localhost:5000"
echo "3. Make sure Ollama is running on your system"
echo ""
echo "Useful commands:"
echo "  View logs: docker-compose logs -f"
echo "  Stop service: docker-compose down"
echo "  Restart service: docker-compose restart"
echo "  Update service: docker-compose up --build -d"
