#!/bin/bash

# RawrZ Security Platform API Test Script
# This script tests all the external API endpoints

BASE_URL="http://localhost:3000"

echo "=== RawrZ Security Platform API Tests ==="
echo "Base URL: $BASE_URL"
echo ""

# Test 1: Health Check
echo "1. Testing Health Check..."
curl -s "$BASE_URL/health" | jq .
echo ""

# Test 2: Simple Test
echo "2. Testing Simple Test Endpoint..."
curl -s "$BASE_URL/api/simple-test" | jq .
echo ""

# Test 3: HTTP Bot Test
echo "3. Testing HTTP Bot Test Endpoint..."
curl -s "$BASE_URL/api/http-bot-test" | jq .
echo ""

# Test 4: Engine Status
echo "4. Testing Engine Status..."
curl -s "$BASE_URL/api/rawrz-engine/status" | jq .
echo ""

# Test 5: HTTP Bot Manager
echo "5. Testing HTTP Bot Manager..."
curl -s -X POST -H "Content-Type: application/json" -d @test-engine-request.json "$BASE_URL/api/test-engine" | jq .
echo ""

# Test 6: CVE Analysis Engine
echo "6. Testing CVE Analysis Engine..."
curl -s -X POST -H "Content-Type: application/json" -d @test-cve-request.json "$BASE_URL/api/test-engine" | jq .
echo ""

# Test 7: Payload Manager
echo "7. Testing Payload Manager..."
curl -s -X POST -H "Content-Type: application/json" -d @test-payload-request.json "$BASE_URL/api/test-engine" | jq .
echo ""

# Test 8: Plugin Architecture
echo "8. Testing Plugin Architecture..."
curl -s -X POST -H "Content-Type: application/json" -d @test-plugin-request.json "$BASE_URL/api/test-engine" | jq .
echo ""

echo "=== All Tests Completed ==="
