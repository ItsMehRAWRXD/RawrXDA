#!/bin/bash

echo "========================================"
echo "  RawrZ Security Platform Desktop"
echo "  Offline Desktop Edition v2.0.0"
echo "========================================"
echo

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python3 not found. Please install Python 3.8+"
    exit 1
fi

# Check if .NET is available
if ! command -v dotnet &> /dev/null; then
    echo "ERROR: .NET not found. Please install .NET 6.0+"
    exit 1
fi

echo "Starting RawrZ Desktop Backend..."
echo

# Start Python backend in background
python3 python_backend_integration.py &
BACKEND_PID=$!

# Wait a moment for backend to start
sleep 3

echo "Backend started (PID: $BACKEND_PID). Launching Desktop App..."
echo

# Build and run .NET desktop app
cd RawrZDesktop
dotnet build --configuration Release
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build .NET desktop app"
    kill $BACKEND_PID 2>/dev/null
    exit 1
fi

echo "Starting RawrZ Desktop Application..."
echo
dotnet run --configuration Release

echo
echo "RawrZ Desktop Application closed."
kill $BACKEND_PID 2>/dev/null
echo "Backend stopped."
