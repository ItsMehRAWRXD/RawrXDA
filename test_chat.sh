#!/usr/bin/env bash
# Simple test script to verify chat functionality works

# Check if Qt app is running
echo "========================================"
echo "RawrXD QtShell Chat Functionality Test"
echo "========================================"
echo ""

# Test 1: Check if model was successfully loaded (from logs)
echo "[Test 1] Checking if model loading succeeded..."
if [ -f /tmp/rawrxd_model_load.log ]; then
    if grep -q "Model loaded successfully" /tmp/rawrxd_model_load.log; then
        echo "✓ Model loaded successfully"
    else
        echo "✗ Model failed to load"
        exit 1
    fi
else
    echo "⚠ No log file found, manual inspection needed"
fi

# Test 2: Try to connect to GGUF server
echo ""
echo "[Test 2] Checking GGUF server..."
if timeout 2 bash -c 'echo > /dev/tcp/localhost/11434' 2>/dev/null; then
    echo "✓ Server is listening on port 11434"
    
    # Test 3: Send a test request
    echo ""
    echo "[Test 3] Testing inference request..."
    response=$(curl -s -X POST http://localhost:11434/api/generate \
        -H "Content-Type: application/json" \
        -d '{"prompt":"What is AI?","max_tokens":50}' \
        2>/dev/null)
    
    if [ ! -z "$response" ]; then
        echo "✓ Server responded to inference request"
        echo "Response: ${response:0:100}..."
    else
        echo "✗ No response from server"
    fi
else
    echo "✗ Server not listening on port 11434"
    echo "  Start RawrXD-QtShell and load a model first"
fi

echo ""
echo "========================================"
echo "Test Complete"
echo "========================================"
