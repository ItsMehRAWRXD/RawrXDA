#!/bin/bash

# Modern RFI Scanner Setup Script
# This script helps set up the environment for the modernized RFI scanner

echo "=========================================="
echo "Modern RFI Scanner - Setup Script"
echo "=========================================="

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 is not installed. Please install Python 3.8 or higher."
    exit 1
fi

echo "✅ Python 3 found: $(python3 --version)"

# Check if pip is available
if ! command -v pip3 &> /dev/null; then
    echo "❌ pip3 is not available. Please install pip3."
    exit 1
fi

echo "✅ pip3 found: $(pip3 --version)"

# Create virtual environment if it doesn't exist
if [ ! -d "venv" ]; then
    echo "📦 Creating virtual environment..."
    python3 -m venv venv
    if [ $? -eq 0 ]; then
        echo "✅ Virtual environment created successfully"
    else
        echo "❌ Failed to create virtual environment"
        echo "   You may need to install python3-venv package"
        echo "   Try: sudo apt install python3-venv"
        exit 1
    fi
else
    echo "✅ Virtual environment already exists"
fi

# Activate virtual environment
echo "🔧 Activating virtual environment..."
source venv/bin/activate

# Install dependencies
echo "📥 Installing dependencies..."
pip install -r requirements.txt

if [ $? -eq 0 ]; then
    echo "✅ Dependencies installed successfully"
else
    echo "❌ Failed to install dependencies"
    exit 1
fi

# Run basic tests
echo "🧪 Running basic functionality tests..."
python3 simple_test.py

if [ $? -eq 0 ]; then
    echo "✅ All tests passed!"
else
    echo "❌ Some tests failed"
    exit 1
fi

echo ""
echo "=========================================="
echo "🎉 Setup completed successfully!"
echo "=========================================="
echo ""
echo "To use the scanner:"
echo "1. Activate the virtual environment: source venv/bin/activate"
echo "2. Run the scanner: python3 modern_rfi_scanner.py --help"
echo ""
echo "Example usage:"
echo "  python3 modern_rfi_scanner.py --url 'http://example.com/page.php?id=' --type rfi"
echo "  python3 modern_rfi_scanner.py --targets sample_targets.txt --type lfi"
echo ""
echo "⚠️  IMPORTANT: Only use on systems you own or have permission to test!"
echo "=========================================="