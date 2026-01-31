#!/bin/bash
set -e

echo " Testing All RawrZ Security Platform Engines"
echo "=============================================="

# Check if the service is running
if ! systemctl is-active --quiet rawrz.service; then
    echo " RawrZ service is not running. Please start it first."
    exit 1
fi

echo " RawrZ service is running"
echo ""

# Base URL for API calls
BASE_URL="http://localhost:3000"

echo "1. Testing Health Endpoint..."
echo "=============================="
curl -s "$BASE_URL/api/health" | jq '.' || echo "Health check failed"
echo ""

echo "2. Testing Engines List Endpoint..."
echo "==================================="
curl -s "$BASE_URL/api/engines" | jq '.' || echo "Engines list failed"
echo ""

echo "3. Testing Real Encryption Engine..."
echo "===================================="
echo "Testing encryption endpoint..."

# Create test data
TEST_DATA="Hello, RawrZ Security Platform! This is a test message for encryption."
ENCODED_DATA=$(echo -n "$TEST_DATA" | base64)

# Test encryption
echo "Encrypting test data..."
ENCRYPT_RESPONSE=$(curl -s -X POST "$BASE_URL/api/real-encryption/encrypt" \
    -H "Content-Type: application/json" \
    -d "{\"data\": \"$ENCODED_DATA\"}")

echo "Encryption response:"
echo "$ENCRYPT_RESPONSE" | jq '.' || echo "Encryption failed"

# Extract keys and encrypted data for decryption test
if echo "$ENCRYPT_RESPONSE" | jq -e '.success' > /dev/null; then
    echo ""
    echo "Testing decryption endpoint..."
    
    # Extract the encrypted data and keys
    ENCRYPTED_DATA=$(echo "$ENCRYPT_RESPONSE" | jq -r '.encrypted')
    KEYS=$(echo "$ENCRYPT_RESPONSE" | jq '.keys')
    
    # Test decryption
    DECRYPT_RESPONSE=$(curl -s -X POST "$BASE_URL/api/real-encryption/decrypt" \
        -H "Content-Type: application/json" \
        -d "{\"encrypted\": \"$ENCRYPTED_DATA\", \"keys\": $KEYS}")
    
    echo "Decryption response:"
    echo "$DECRYPT_RESPONSE" | jq '.' || echo "Decryption failed"
    
    # Verify the decrypted data matches original
    if echo "$DECRYPT_RESPONSE" | jq -e '.success' > /dev/null; then
        DECRYPTED_DATA=$(echo "$DECRYPT_RESPONSE" | jq -r '.decrypted' | base64 -d)
        if [ "$DECRYPTED_DATA" = "$TEST_DATA" ]; then
            echo " Encryption/Decryption test PASSED"
        else
            echo " Encryption/Decryption test FAILED - data mismatch"
        fi
    else
        echo " Decryption test FAILED"
    fi
else
    echo " Encryption test FAILED"
fi

echo ""
echo "4. Testing File Upload Endpoint..."
echo "=================================="
# Create a test file
echo "This is a test file for upload functionality" > test-upload.txt

echo "Testing file upload..."
UPLOAD_RESPONSE=$(curl -s -X POST "$BASE_URL/api/upload" \
    -F "file=@test-upload.txt")

echo "Upload response:"
echo "$UPLOAD_RESPONSE" | jq '.' || echo "Upload failed"

if echo "$UPLOAD_RESPONSE" | jq -e '.success' > /dev/null; then
    echo " File upload test PASSED"
else
    echo " File upload test FAILED"
fi

# Clean up test file
rm -f test-upload.txt

echo ""
echo "5. Testing All Engine Endpoints (Simulated)..."
echo "=============================================="

# List of all engines that should be available
ENGINES=(
    "real-encryption-engine"
    "advanced-crypto"
    "burner-encryption-engine"
    "dual-crypto-engine"
    "stealth-engine"
    "mutex-engine"
    "compression-engine"
    "stub-generator"
    "advanced-stub-generator"
    "polymorphic-engine"
    "anti-analysis"
    "advanced-anti-analysis"
    "advanced-fud-engine"
    "hot-patchers"
    "full-assembly"
    "memory-manager"
    "backup-system"
    "mobile-tools"
    "network-tools"
    "reverse-engineering"
    "digital-forensics"
    "malware-analysis"
    "advanced-analytics-engine"
    "red-shells"
    "private-virus-scanner"
    "ai-threat-detector"
    "jotti-scanner"
    "http-bot-generator"
    "irc-bot-generator"
    "beaconism-dll-sideloading"
    "ev-cert-encryptor"
    "multi-platform-bot-generator"
    "native-compiler"
    "performance-worker"
    "health-monitor"
    "implementation-checker"
    "file-operations"
    "openssl-management"
    "dotnet-workaround"
    "camellia-assembly"
    "api-status"
    "cve-analysis-engine"
    "http-bot-manager"
    "payload-manager"
    "plugin-architecture"
    "template-generator"
)

echo "Testing engine availability..."
PASSED=0
FAILED=0

for engine in "${ENGINES[@]}"; do
    echo -n "Testing $engine: "
    
    # Test if engine endpoint exists (most engines don't have specific endpoints yet)
    # For now, we'll just verify they're listed in the engines endpoint
    if curl -s "$BASE_URL/api/engines" | jq -e ".engines[] | select(.name == \"$engine\")" > /dev/null; then
        echo " Available"
        ((PASSED++))
    else
        echo " Not found"
        ((FAILED++))
    fi
done

echo ""
echo "6. Testing Web Interface..."
echo "==========================="
echo "Testing main web interface..."
WEB_RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL/")
if [ "$WEB_RESPONSE" = "200" ]; then
    echo " Web interface accessible (HTTP $WEB_RESPONSE)"
else
    echo " Web interface not accessible (HTTP $WEB_RESPONSE)"
fi

echo ""
echo "7. Performance Test..."
echo "====================="
echo "Testing API response times..."

# Test multiple health checks to measure performance
echo "Running 10 health checks to measure performance..."
TOTAL_TIME=0
for i in {1..10}; do
    RESPONSE_TIME=$(curl -s -o /dev/null -w "%{time_total}" "$BASE_URL/api/health")
    TOTAL_TIME=$(echo "$TOTAL_TIME + $RESPONSE_TIME" | bc -l)
    echo -n "."
done
echo ""

AVERAGE_TIME=$(echo "scale=3; $TOTAL_TIME / 10" | bc -l)
echo "Average response time: ${AVERAGE_TIME}s"

if (( $(echo "$AVERAGE_TIME < 1.0" | bc -l) )); then
    echo " Performance test PASSED (fast response)"
else
    echo " Performance test WARNING (slow response)"
fi

echo ""
echo "8. Stress Test..."
echo "================"
echo "Testing concurrent requests..."
echo "Sending 5 concurrent health checks..."

# Run 5 concurrent requests
for i in {1..5}; do
    curl -s "$BASE_URL/api/health" > /dev/null &
done
wait

echo " Stress test completed"

echo ""
echo "9. Final Engine Status Summary..."
echo "================================="
echo "Engines tested: ${#ENGINES[@]}"
echo "Engines passed: $PASSED"
echo "Engines failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo " ALL ENGINES TEST PASSED!"
else
    echo " Some engines failed testing"
fi

echo ""
echo "10. Service Status Check..."
echo "==========================="
systemctl status rawrz.service --no-pager -l

echo ""
echo " All engine tests completed!"
echo " RawrZ Security Platform is accessible at: http://$(hostname -I | awk '{print $1}'):3000"
