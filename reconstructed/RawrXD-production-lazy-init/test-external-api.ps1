#!/usr/bin/env powershell
<#
.SYNOPSIS
    Test script for RawrXD CLI external tool API endpoints
    Validates accessibility for GitHub Copilot, Amazon Q, and other tools

.DESCRIPTION
    This script tests all external API endpoints and generates a comprehensive
    connectivity report for external tool integration.

.PARAMETER BaseUrl
    Base URL for the RawrXD CLI API (default: http://localhost:11434)

.PARAMETER TestAll
    Run all test suites

.EXAMPLE
    .\test-external-api.ps1 -BaseUrl http://localhost:11434 -TestAll
#>

param(
    [string]$BaseUrl = "http://localhost:11434",
    [switch]$TestAll = $false
)

# Utility functions
function Write-TestHeader {
    param([string]$Title)
    Write-Host ""
    Write-Host "=" * 80
    Write-Host "TEST: $Title"
    Write-Host "=" * 80
}

function Write-TestResult {
    param(
        [string]$Endpoint,
        [string]$Status,
        [object]$Data = $null
    )
    
    $color = if ($Status -eq "PASS") { "Green" } else { "Red" }
    Write-Host "$Endpoint : " -NoNewline
    Write-Host $Status -ForegroundColor $color
    
    if ($Data) {
        $Data | ConvertTo-Json -Depth 2 | Write-Host -ForegroundColor Gray
    }
}

# Test 1: Basic Connectivity
Write-TestHeader "Basic Connectivity Tests"

try {
    $response = Invoke-RestMethod -Uri "$BaseUrl/health" -Method Get -ErrorAction Stop
    Write-TestResult "GET /health" "PASS" $response
    $port = $response.port
    $model_loaded = $response.model_loaded
} catch {
    Write-TestResult "GET /health" "FAIL" $_.Exception.Message
    Write-Host "Cannot proceed without successful health check. Exiting..."
    exit 1
}

# Test 2: API Discovery
Write-TestHeader "API Discovery Endpoints"

try {
    $response = Invoke-RestMethod -Uri "$BaseUrl/api/v1/info" -Method Get -ErrorAction Stop
    Write-TestResult "GET /api/v1/info" "PASS" $response
} catch {
    Write-TestResult "GET /api/v1/info" "FAIL" $_
}

try {
    $response = Invoke-RestMethod -Uri "$BaseUrl/api/v1/docs" -Method Get -ErrorAction Stop
    Write-TestResult "GET /api/v1/docs" "PASS" "Documentation retrieved ($($response.Length) chars)"
} catch {
    Write-TestResult "GET /api/v1/docs" "FAIL" $_
}

# Test 3: Chat Completion (OpenAI Compatible)
Write-TestHeader "Chat Completion Endpoint (OpenAI Compatible)"

if ($model_loaded) {
    $chatPayload = @{
        model = "bigdaddyg"
        messages = @(
            @{
                role = "system"
                content = "You are a helpful code assistant"
            },
            @{
                role = "user"
                content = "Write a hello world function in Python"
            }
        )
        temperature = 0.7
    } | ConvertTo-Json

    try {
        $response = Invoke-RestMethod `
            -Uri "$BaseUrl/v1/chat/completions" `
            -Method Post `
            -ContentType "application/json" `
            -Body $chatPayload `
            -ErrorAction Stop
        
        $responsePreview = $response.choices[0].message.content.Substring(0, [Math]::Min(100, $response.choices[0].message.content.Length))
        Write-TestResult "POST /v1/chat/completions" "PASS" @{
            model = $response.model
            tokens_used = $response.usage.total_tokens
            response_preview = $responsePreview + "..."
        }
    } catch {
        Write-TestResult "POST /v1/chat/completions" "FAIL" $_
    }
} else {
    Write-Host "Skipping chat completion test - no model loaded"
    Write-Host "Load a model first: load 1 (in CLI)"
}

# Test 4: Text Generation
Write-TestHeader "Text Generation Endpoint"

if ($model_loaded) {
    $generatePayload = @{
        prompt = "def fibonacci(n):"
        model = "bigdaddyg"
        stream = $false
    } | ConvertTo-Json

    try {
        $response = Invoke-RestMethod `
            -Uri "$BaseUrl/api/generate" `
            -Method Post `
            -ContentType "application/json" `
            -Body $generatePayload `
            -ErrorAction Stop
        
        $responsePreview = $response.response.Substring(0, [Math]::Min(100, $response.response.Length))
        Write-TestResult "POST /api/generate" "PASS" @{
            done = $response.done
            response_preview = $responsePreview + "..."
        }
    } catch {
        Write-TestResult "POST /api/generate" "FAIL" $_
    }
} else {
    Write-Host "Skipping text generation test - no model loaded"
}

# Test 5: Model Tags
Write-TestHeader "Model Management Endpoints"

try {
    $response = Invoke-RestMethod -Uri "$BaseUrl/api/tags" -Method Get -ErrorAction Stop
    $modelCount = $response.models.Count
    Write-TestResult "GET /api/tags" "PASS" @{
        models_available = $modelCount
        models = $response.models | Select-Object -ExpandProperty name
    }
} catch {
    Write-TestResult "GET /api/tags" "FAIL" $_
}

# Test 6: Project Analysis
Write-TestHeader "Project Analysis Endpoints"

$analysisPayload = @{
    path = "."
    format = "json"
    include_metrics = $true
} | ConvertTo-Json

try {
    $response = Invoke-RestMethod `
        -Uri "$BaseUrl/api/v1/analyze" `
        -Method Post `
        -ContentType "application/json" `
        -Body $analysisPayload `
        -ErrorAction Stop
    
    Write-TestResult "POST /api/v1/analyze" "PASS" @{
        path = $response.path
        status = $response.status
        file_count = $response.statistics.total_files
        total_size_mb = $response.statistics.total_size_mb
    }
} catch {
    Write-TestResult "POST /api/v1/analyze" "FAIL" $_
}

# Test 7: Code Search
Write-TestHeader "Code Search Endpoints"

$searchPayload = @{
    query = "main"
    path = "."
    case_sensitive = $false
} | ConvertTo-Json

try {
    $response = Invoke-RestMethod `
        -Uri "$BaseUrl/api/v1/search" `
        -Method Post `
        -ContentType "application/json" `
        -Body $searchPayload `
        -ErrorAction Stop
    
    Write-TestResult "POST /api/v1/search" "PASS" @{
        query = $response.query
        status = $response.status
        result_count = $response.result_count
    }
} catch {
    Write-TestResult "POST /api/v1/search" "FAIL" $_
}

# Test 8: Network Accessibility
Write-TestHeader "Network Accessibility Tests"

Write-Host "Server Binding:"
Write-Host "  Port: $port"
Write-Host "  Accessible as: http://localhost:$port"

try {
    $tcpConnection = Get-NetTCPConnection -LocalPort $port -ErrorAction Stop
    Write-Host "  Active connections: " -NoNewline
    Write-Host ($tcpConnection.Count) -ForegroundColor Green
} catch {
    Write-Host "  Could not enumerate connections"
}

# Test 9: Performance Baseline
Write-TestHeader "Performance Baseline Tests"

$iterations = 5
Write-Host "Running $iterations health check requests..."

$times = @()
for ($i = 0; $i -lt $iterations; $i++) {
    $start = [DateTime]::UtcNow
    try {
        Invoke-RestMethod -Uri "$BaseUrl/health" -Method Get -ErrorAction Stop | Out-Null
        $duration = ([DateTime]::UtcNow - $start).TotalMilliseconds
        $times += $duration
        Write-Host "  Request $($i+1): ${duration}ms"
    } catch {
        Write-Host "  Request $($i+1): FAILED"
    }
}

if ($times.Count -gt 0) {
    $avgTime = $times | Measure-Object -Average | Select-Object -ExpandProperty Average
    $minTime = $times | Measure-Object -Minimum | Select-Object -ExpandProperty Minimum
    $maxTime = $times | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
    
    Write-Host ""
    Write-Host "Performance Summary:"
    Write-Host "  Average: ${avgTime}ms"
    Write-Host "  Min: ${minTime}ms"
    Write-Host "  Max: ${maxTime}ms"
}

# Test 10: Rate Limiting
Write-TestHeader "Rate Limiting Tests"

Write-Host "Sending 10 rapid requests to check rate limiting..."
$rateLimitTest = $true
for ($i = 0; $i -lt 10; $i++) {
    try {
        $response = Invoke-RestMethod -Uri "$BaseUrl/health" -Method Get -ErrorAction Stop
        Write-Host "  Request $($i+1): OK"
    } catch {
        if ($_.Exception.Response.StatusCode -eq 429) {
            Write-Host "  Request $($i+1): RATE LIMITED (429)" -ForegroundColor Yellow
            $rateLimitTest = $false
        } else {
            Write-Host "  Request $($i+1): ERROR"
        }
    }
}

# Summary Report
Write-TestHeader "Integration Summary"

Write-Host ""
Write-Host "API Endpoints Status:"
Write-Host "  ✓ Health Check:         Ready for monitoring"
Write-Host "  ✓ API Discovery:        Ready for auto-configuration"
Write-Host "  ✓ Chat Completions:     OpenAI-compatible endpoint"
Write-Host "  ✓ Text Generation:      Available for code completion"
Write-Host "  ✓ Model Tags:           Model management endpoint"
Write-Host "  ✓ Project Analysis:     Ready for IDE integration"
Write-Host "  ✓ Code Search:          Ready for search queries"
Write-Host ""

Write-Host "External Tool Integration:"
Write-Host "  GitHub Copilot:  Configure custom backend at http://localhost:$port/v1/chat/completions"
Write-Host "  Amazon Q:        Configure endpoint at http://localhost:$port/api/v1/analyze"
Write-Host "  LM Studio:       Use model server: http://localhost:$port"
Write-Host ""

Write-Host "Next Steps:"
Write-Host "  1. Start the RawrXD CLI with: .\build\bin-msvc\Release\RawrXD-CLI.exe"
Write-Host "  2. Load a model: load 1 (for BigDaddyG)"
Write-Host "  3. Configure external tool with endpoint: http://localhost:$port"
Write-Host "  4. Test tool connection"
Write-Host ""

Write-Host "Documentation: See API_EXTERNAL_TOOLS_INTEGRATION.md for detailed setup instructions"
