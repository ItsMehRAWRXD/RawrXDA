#!/bin/bash
# =====================================================================
# NASM IDE Swarm Launcher (Linux/macOS)
# Starts coordinator and all 10 agents
# =====================================================================

echo ""
echo "========================================"
echo " NASM IDE Swarm System Launcher"
echo "========================================"
echo ""

# Check Python installation
if ! command -v python3 &> /dev/null; then
    echo "[ERROR] Python 3 not found! Please install Python 3.8+"
    exit 1
fi

# Check dependencies
echo "[1/4] Checking dependencies..."
if ! python3 -c "import aiohttp" 2>/dev/null; then
    echo "[INFO] Installing aiohttp..."
    pip3 install aiohttp
fi

# Create directories
echo "[2/4] Setting up directories..."
mkdir -p models
mkdir -p logs

# Check config
echo "[3/4] Validating configuration..."
if [ ! -f "swarm_config.json" ]; then
    echo "[ERROR] swarm_config.json not found!"
    exit 1
fi

# Start coordinator
echo "[4/4] Starting swarm coordinator..."
echo ""
echo "========================================"
echo " Coordinator: http://localhost:8888"
echo " Agents: 10 total (ports 8891-8900)"
echo "========================================"
echo ""
echo "Press Ctrl+C to shutdown swarm"
echo ""

python3 coordinator.py

echo ""
echo "Swarm coordinator stopped."
