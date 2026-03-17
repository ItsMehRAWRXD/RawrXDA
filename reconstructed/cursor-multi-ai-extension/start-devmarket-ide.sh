#!/bin/bash

# Start DevMarket IDE with Backend
echo "Starting DevMarket IDE Backend Server..."

# Kill any existing servers on the ports
lsof -ti:3001 | xargs kill -9 2>/dev/null
lsof -ti:8080 | xargs kill -9 2>/dev/null

# Start backend server in background
cd "$(dirname "$0")/DevMarketIDE"
node backend-server.js &
BACKEND_PID=$!

# Wait for backend to start
sleep 2

# Start frontend server in background
cd ..
node local-dev-server.js &
FRONTEND_PID=$!

echo "Backend Server PID: $BACKEND_PID"
echo "Frontend Server PID: $FRONTEND_PID"

echo ""
echo "DevMarket IDE is now running:"
echo "  Frontend: http://localhost:8080/DevMarketIDE/"
echo "  Backend API: http://localhost:3001"
echo ""
echo "Press Ctrl+C to stop both servers"

# Function to cleanup on exit
cleanup() {
    echo ""
    echo "Stopping servers..."
    kill $BACKEND_PID 2>/dev/null
    kill $FRONTEND_PID 2>/dev/null
    exit 0
}

# Set trap to cleanup on script exit
trap cleanup INT TERM

# Wait for both processes
wait