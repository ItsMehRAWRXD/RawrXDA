#!/bin/bash

echo ""
echo "========================================"
echo "  RawrZ HTTP Control Center Launcher"
echo "========================================"
echo ""
echo "Starting RawrZ Control Center..."
echo ""

# Change to the script directory
cd "$(dirname "$0")"

# Start the control center
npm start
