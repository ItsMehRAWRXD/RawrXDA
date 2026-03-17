#!/bin/bash

echo "==================================="
echo "Cursor AI Copilot Extension Setup"
echo "==================================="

# Check for Node.js
if ! command -v node &> /dev/null; then
    echo "Error: Node.js is not installed. Please install Node.js first."
    exit 1
fi

echo "Node.js version: $(node --version)"
echo "npm version: $(npm --version)"

# Install dependencies
echo ""
echo "Installing dependencies..."
npm install

# Check if installation was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to install dependencies"
    exit 1
fi

echo ""
echo "==================================="
echo "Setup Complete!"
echo "==================================="
echo ""
echo "Next steps:"
echo "1. npm run esbuild       - Build the extension"
echo "2. npm run esbuild-watch - Build and watch for changes"
echo "3. F5 in VS Code         - Run the extension in development mode"
echo ""
echo "To package the extension:"
echo "  npm install -g vsce"
echo "  vsce package"
echo ""
