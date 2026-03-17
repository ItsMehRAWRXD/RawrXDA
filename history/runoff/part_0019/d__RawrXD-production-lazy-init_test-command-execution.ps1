#!/usr/bin/env powershell
<#
.SYNOPSIS
    Test script for RawrXD CLI remote command execution
    Validates that external tools can execute commands through the REST API

.DESCRIPTION
    Tests all command execution endpoints and verifies responses

.PARAMETER BaseUrl
    Base URL for the RawrXD CLI API (default: http://localhost:11434)

.EXAMPLE
    .\test-command-execution.ps1 -BaseUrl http://localhost:11434
#>

param(
    [string]$BaseUrl = "http://localhost:11434"
)

# Test configuration
$testsPassed = 0
$testsFailed = 0

function Write-TestHeader {
    param([string]$Title)
    Write-Host ""
    Write-Host "=" * 80
    Write-Host "TEST: $Title"
    Write-Host "=" * 80
}

function Test-Endpoint {
    param(
        [string]$Name,
        [string]$Method,
        [string]$Endpoint,
        [object]$Body = $null,
        [int]$ExpectedStatus = 200
    )
    
    try {
        $params = @{
            Uri = "$BaseUrl$Endpoint"
            Method = $Method
            ContentType = "application/json"
            ErrorAction = "Stop"
        }
        
        if ($Body) {
            $params.Body = $Body | ConvertTo-Json
        }
        
        $response = Invoke-RestMethod @params
        
        Write-Host "✓ $Name" -ForegroundColor Green
        Write-Host "  Response: $(($response | ConvertTo-Json -Compress).Substring(0, 100))..."
        $script:testsPassed++
        return $response
    }
    catch {
        Write-Host "✗ $Name" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)"
        $script:testsFailed++
        return $null
    }
}

# ============================================================================
# TEST SUITE 1: Basic Health & Info
# ============================================================================
Write-TestHeader "Health & API Information"

Test-Endpoint "Health Check" "GET" "/health"
Test-Endpoint "API Info" "GET" "/api/v1/info"
Test-Endpoint "API Documentation" "GET" "/api/v1/docs"

# ============================================================================
# TEST SUITE 2: Command Execution - System Commands
# ============================================================================
Write-TestHeader "System Command Execution"

Test-Endpoint "Help Command" "POST" "/api/v1/execute" @{ command = "help" }
Test-Endpoint "Status Command" "POST" "/api/v1/execute" @{ command = "status" }
Test-Endpoint "Models Command" "POST" "/api/v1/execute" @{ command = "models" }
Test-Endpoint "Version Command" "POST" "/api/v1/execute" @{ command = "version" }

# ============================================================================
# TEST SUITE 3: Command Execution - Model Commands
# ============================================================================
Write-TestHeader "Model Loading Commands"

Test-Endpoint "Load Model (Numeric)" "POST" "/api/v1/execute" @{ command = "load 1" }
Test-Endpoint "Load Model (Named)" "POST" "/api/v1/execute" @{ command = "load bigdaddyg" }

# ============================================================================
# TEST SUITE 4: Command Execution - Analysis Commands
# ============================================================================
Write-TestHeader "Project Analysis Commands"

Test-Endpoint "Analyze Project" "POST" "/api/v1/execute" @{ command = "analyze" }
Test-Endpoint "Search Code" "POST" "/api/v1/execute" @{ command = "search class" }

# ============================================================================
# TEST SUITE 5: Command Execution - Chat/Generate Commands
# ============================================================================
Write-TestHeader "AI Chat & Generation Commands"

Test-Endpoint "Chat with AI" "POST" "/api/v1/execute" @{ command = "chat hello world" }
Test-Endpoint "Generate Text" "POST" "/api/v1/execute" @{ command = "generate def example" }

# ============================================================================
# TEST SUITE 6: Command Status Tracking
# ============================================================================
Write-TestHeader "Command Status & Tracking"

Test-Endpoint "Get Command Status" "GET" "/api/v1/status"
Test-Endpoint "Get Specific Command Status" "GET" "/api/v1/status?id=cmd_test_123"

# ============================================================================
# TEST SUITE 7: External Tool Endpoints
# ============================================================================
Write-TestHeader "External Tool Endpoints"

Test-Endpoint "Project Analysis Endpoint" "POST" "/api/v1/analyze" @{ path = "."; format = "json" }
Test-Endpoint "Code Search Endpoint" "POST" "/api/v1/search" @{ query = "function"; path = "." }

# ============================================================================
# TEST SUITE 8: Integration Test - Full Workflow
# ============================================================================
Write-TestHeader "Full Integration Workflow"

Write-Host "Scenario: External tool loads model and executes commands"

# Step 1: Check health
Write-Host "Step 1: Checking health..."
$health = Test-Endpoint "Health Check" "GET" "/health"
if ($health.status -eq "ok") {
    Write-Host "  ✓ Server is running on port $($health.port)"
}

# Step 2: Load model
Write-Host "Step 2: Loading model..."
$load_result = Test-Endpoint "Load Model" "POST" "/api/v1/execute" @{ command = "load 1" }
if ($load_result.status -eq "success") {
    Write-Host "  ✓ Model loaded successfully"
}

# Step 3: Get server info
Write-Host "Step 3: Getting server info..."
$info = Test-Endpoint "Server Info" "POST" "/api/v1/execute" @{ command = "status" }
if ($info.status -eq "success") {
    Write-Host "  ✓ Server info retrieved"
}

# Step 4: Analyze project
Write-Host "Step 4: Analyzing project..."
$analysis = Test-Endpoint "Project Analysis" "POST" "/api/v1/execute" @{ command = "analyze" }
if ($analysis.status -eq "success") {
    Write-Host "  ✓ Project analysis complete"
}

# Step 5: Chat with AI
Write-Host "Step 5: Testing chat functionality..."
$chat = Test-Endpoint "AI Chat" "POST" "/api/v1/execute" @{ command = "chat test message" }
if ($chat.status -eq "success") {
    Write-Host "  ✓ Chat functionality working"
}

# ============================================================================
# TEST SUITE 9: Response Format Validation
# ============================================================================
Write-TestHeader "Response Format Validation"

Write-Host "Validating command response structure..."

$response = Test-Endpoint "Command with Response" "POST" "/api/v1/execute" @{ command = "help" }
if ($response) {
    $hasCommand = $response | Select-Object -ExpandProperty command -ErrorAction SilentlyContinue
    $hasStatus = $response | Select-Object -ExpandProperty status -ErrorAction SilentlyContinue
    $hasResult = $response | Select-Object -ExpandProperty result -ErrorAction SilentlyContinue
    $hasTimestamp = $response | Select-Object -ExpandProperty timestamp -ErrorAction SilentlyContinue
    
    if ($hasCommand -and $hasStatus -and $hasResult) {
        Write-Host "✓ Response format is valid" -ForegroundColor Green
        Write-Host "  - Command field: present"
        Write-Host "  - Status field: present"
        Write-Host "  - Result field: present"
        Write-Host "  - Timestamp field: $(if ($hasTimestamp) { 'present' } else { 'absent' })"
        $script:testsPassed++
    }
    else {
        Write-Host "✗ Response format is invalid" -ForegroundColor Red
        Write-Host "  Missing required fields"
        $script:testsFailed++
    }
}

# ============================================================================
# TEST SUITE 10: Error Handling
# ============================================================================
Write-TestHeader "Error Handling"

Write-Host "Testing error responses..."

$error_response = Test-Endpoint "Unknown Command" "POST" "/api/v1/execute" @{ command = "unknowncommand12345" }
if ($error_response) {
    Write-Host "  ✓ Unknown command handled gracefully"
}

# ============================================================================
# PERFORMANCE TESTS
# ============================================================================
Write-TestHeader "Performance Benchmarks"

Write-Host "Running performance tests..."

$iterations = 5
$times = @()

for ($i = 0; $i -lt $iterations; $i++) {
    $start = [DateTime]::Now
    try {
        $response = Invoke-RestMethod `
            -Uri "$BaseUrl/api/v1/execute" `
            -Method Post `
            -Body (@{ command = "help" } | ConvertTo-Json) `
            -ContentType "application/json" `
            -ErrorAction Stop
        $duration = ([DateTime]::Now - $start).TotalMilliseconds
        $times += $duration
        Write-Host "  Request $($i+1): ${duration}ms"
    }
    catch {
        Write-Host "  Request $($i+1): FAILED"
    }
}

if ($times.Count -gt 0) {
    $avg = $times | Measure-Object -Average | Select-Object -ExpandProperty Average
    $min = $times | Measure-Object -Minimum | Select-Object -ExpandProperty Minimum
    $max = $times | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
    
    Write-Host ""
    Write-Host "Performance Summary:"
    Write-Host "  Average Response Time: ${avg}ms"
    Write-Host "  Min Response Time: ${min}ms"
    Write-Host "  Max Response Time: ${max}ms"
    Write-Host "  Throughput: $([Math]::Round(1000 / $avg, 2)) requests/sec"
}

# ============================================================================
# TEST SUMMARY
# ============================================================================
Write-TestHeader "Test Summary"

$totalTests = $script:testsPassed + $script:testsFailed
$passPercentage = if ($totalTests -gt 0) { [Math]::Round(($script:testsPassed / $totalTests) * 100, 1) } else { 0 }

Write-Host "Total Tests: $totalTests"
Write-Host "Passed: $($script:testsPassed)" -ForegroundColor Green
Write-Host "Failed: $($script:testsFailed)" -ForegroundColor $(if ($script:testsFailed -gt 0) { "Red" } else { "Green" })
Write-Host "Pass Rate: $passPercentage%"

Write-Host ""
Write-Host "INTEGRATION STATUS:"
if ($script:testsFailed -eq 0) {
    Write-Host "✓ All tests passed! RawrXD CLI is fully accessible to external tools." -ForegroundColor Green
    Write-Host ""
    Write-Host "You can now:"
    Write-Host "  - Use GitHub Copilot with RawrXD as backend"
    Write-Host "  - Configure Amazon Q to use this API"
    Write-Host "  - Build VS Code extensions"
    Write-Host "  - Integrate with other development tools"
}
else {
    Write-Host "✗ Some tests failed. Check the errors above." -ForegroundColor Red
    Write-Host ""
    Write-Host "Common issues:"
    Write-Host "  - CLI not running: Start with: RawrXD-CLI.exe"
    Write-Host "  - Wrong port: Check actual port and update -BaseUrl"
    Write-Host "  - Firewall: Ensure port 11434 is accessible"
}

Write-Host ""
Write-Host "For detailed documentation, see: REMOTE_COMMAND_EXECUTION.md"
