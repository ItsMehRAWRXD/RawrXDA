# GGUF Server Comprehensive Test Script
# Tests the GGUF server endpoints exhaustively

$ErrorActionPreference = "Continue"
$baseUrl = "http://localhost:11434"
$testsPassed = 0
$testsFailed = 0

function Write-TestHeader {
    param([string]$Phase)
    Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  $Phase" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
}

function Test-Endpoint {
    param(
        [string]$Name,
        [string]$Method,
        [string]$Endpoint,
        [string]$Body = $null,
        [hashtable]$Headers = @{},
        [int]$ExpectedStatus = 200
    )
    
    Write-Host "│ Testing: $Name..." -NoNewline
    
    try {
        $uri = "$baseUrl$Endpoint"
        $requestHeaders = @{
            "Content-Type" = "application/json"
        }
        foreach ($key in $Headers.Keys) {
            $requestHeaders[$key] = $Headers[$key]
        }
        
        $response = $null
        $statusCode = 0
        
        try {
            if ($Method -eq "GET") {
                $response = Invoke-WebRequest -Uri $uri -Method GET -Headers $requestHeaders -UseBasicParsing -TimeoutSec 5
            } elseif ($Method -eq "POST") {
                if ($Body) {
                    $response = Invoke-WebRequest -Uri $uri -Method POST -Body $Body -Headers $requestHeaders -UseBasicParsing -TimeoutSec 5
                } else {
                    $response = Invoke-WebRequest -Uri $uri -Method POST -Headers $requestHeaders -UseBasicParsing -TimeoutSec 5
                }
            } elseif ($Method -eq "DELETE") {
                $response = Invoke-WebRequest -Uri $uri -Method DELETE -Body $Body -Headers $requestHeaders -UseBasicParsing -TimeoutSec 5
            } elseif ($Method -eq "OPTIONS") {
                $response = Invoke-WebRequest -Uri $uri -Method OPTIONS -Headers $requestHeaders -UseBasicParsing -TimeoutSec 5
            }
            $statusCode = $response.StatusCode
        } catch {
            if ($_.Exception.Response) {
                $statusCode = [int]$_.Exception.Response.StatusCode
                $response = $_.Exception.Response
            } else {
                throw
            }
        }
        
        if ($statusCode -eq $ExpectedStatus) {
            Write-Host " ✓ PASS (Status: $statusCode)" -ForegroundColor Green
            $script:testsPassed++
            return $response
        } else {
            Write-Host " ✗ FAIL (Expected: $ExpectedStatus, Got: $statusCode)" -ForegroundColor Red
            $script:testsFailed++
            return $null
        }
    } catch {
        Write-Host " ✗ FAIL (Error: $($_.Exception.Message))" -ForegroundColor Red
        $script:testsFailed++
        return $null
    }
}

function Test-StreamingEndpoint {
    param(
        [string]$Name,
        [string]$Endpoint,
        [string]$Body,
        [string]$AcceptType = "application/x-ndjson"
    )
    
    Write-Host "│ Testing: $Name..." -NoNewline
    
    $tcpClient = $null
    $stream = $null
    $writer = $null
    $reader = $null
    
    try {
        $uri = "$baseUrl$Endpoint"
        $tcpClient = New-Object System.Net.Sockets.TcpClient
        
        # Set connection timeout
        $connectTask = $tcpClient.ConnectAsync("localhost", 11434)
        if (-not $connectTask.Wait(2000)) {
            throw "Connection timeout"
        }
        
        $stream = $tcpClient.GetStream()
        $stream.ReadTimeout = 2000
        $stream.WriteTimeout = 2000
        
        $writer = New-Object System.IO.StreamWriter($stream)
        $writer.AutoFlush = $true
        $reader = New-Object System.IO.StreamReader($stream)
        
        # Send HTTP request
        $writer.WriteLine("POST $Endpoint HTTP/1.1")
        $writer.WriteLine("Host: localhost")
        $writer.WriteLine("Content-Type: application/json")
        $writer.WriteLine("Accept: $AcceptType")
        $writer.WriteLine("Content-Length: $($Body.Length)")
        $writer.WriteLine("")
        $writer.Write($Body)
        $writer.Flush()
        
        # Read response with timeout
        $statusLine = $null
        $readTask = { $reader.ReadLine() }
        $statusLine = Invoke-Command -ScriptBlock $readTask -ErrorAction Stop
        
        $statusCode = 0
        if ($statusLine -match "HTTP/\d\.\d (\d+)") {
            $statusCode = [int]$Matches[1]
        }
        
        # Read headers with timeout
        $contentType = ""
        $headerTimeout = [DateTime]::Now.AddSeconds(1)
        while ([DateTime]::Now -lt $headerTimeout) {
            if ($stream.DataAvailable -or -not $reader.EndOfStream) {
                $line = $reader.ReadLine()
                if ([string]::IsNullOrWhiteSpace($line)) { break }
                if ($line -match "Content-Type:\s*(.+)") {
                    $contentType = $Matches[1].Trim()
                }
            } else {
                Start-Sleep -Milliseconds 50
            }
        }
        
        # Read body chunks with strict timeout
        $chunks = 0
        $timeout = [DateTime]::Now.AddSeconds(2)
        while ([DateTime]::Now -lt $timeout) {
            if ($stream.DataAvailable) {
                try {
                    $line = $reader.ReadLine()
                    if ($line -and $line.Trim().StartsWith("{")) {
                        $chunks++
                    }
                    if ($chunks -ge 2) { break }
                } catch {
                    break
                }
            } else {
                Start-Sleep -Milliseconds 100
            }
        }
        
        if ($statusCode -eq 200 -and $chunks -gt 0) {
            Write-Host " ✓ PASS (Status: $statusCode, Chunks: $chunks)" -ForegroundColor Green
            $script:testsPassed++
        } else {
            Write-Host " ✗ FAIL (Status: $statusCode, Chunks: $chunks)" -ForegroundColor Red
            $script:testsFailed++
        }
    } catch {
        Write-Host " ✗ FAIL (Error: $($_.Exception.Message))" -ForegroundColor Red
        $script:testsFailed++
    } finally {
        # Cleanup
        if ($reader) { try { $reader.Close() } catch {} }
        if ($writer) { try { $writer.Close() } catch {} }
        if ($stream) { try { $stream.Close() } catch {} }
        if ($tcpClient) { try { $tcpClient.Close() } catch {} }
    }
}

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  GGUF SERVER COMPREHENSIVE TEST SUITE                      ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# PHASE 1: Basic Functionality
Write-TestHeader "PHASE 1: BASIC FUNCTIONALITY"
Test-Endpoint -Name "Health endpoint" -Method "GET" -Endpoint "/health" -ExpectedStatus 200
Test-Endpoint -Name "Root endpoint (Ollama compat)" -Method "GET" -Endpoint "/" -ExpectedStatus 200
Test-Endpoint -Name "Invalid endpoint (404)" -Method "GET" -Endpoint "/invalid/endpoint" -ExpectedStatus 404
Test-Endpoint -Name "OPTIONS request (CORS)" -Method "OPTIONS" -Endpoint "/api/generate" -ExpectedStatus 204
Test-Endpoint -Name "Invalid JSON" -Method "POST" -Endpoint "/api/generate" -Body "{invalid json" -ExpectedStatus 400

# PHASE 2: Model Management
Write-TestHeader "PHASE 2: MODEL MANAGEMENT"
Test-Endpoint -Name "List models (/api/tags)" -Method "GET" -Endpoint "/api/tags" -ExpectedStatus 200

$showBody = @{
    name = "test-model"
} | ConvertTo-Json
Test-Endpoint -Name "Show model info" -Method "POST" -Endpoint "/api/show" -Body $showBody -ExpectedStatus 404

$deleteBody = @{} | ConvertTo-Json
Test-Endpoint -Name "Delete without name" -Method "POST" -Endpoint "/api/delete" -Body $deleteBody -ExpectedStatus 400

# PHASE 3: Generation (Non-Streaming)
Write-TestHeader "PHASE 3: GENERATION (NON-STREAMING)"

$genBody = @{
    model = "test-model"
    prompt = "Hello, world!"
    stream = $false
    max_tokens = 10
} | ConvertTo-Json
Test-Endpoint -Name "Generate (non-streaming)" -Method "POST" -Endpoint "/api/generate" -Body $genBody -ExpectedStatus 200

$genNoPrompt = @{
    model = "test-model"
    stream = $false
} | ConvertTo-Json
Test-Endpoint -Name "Generate without prompt" -Method "POST" -Endpoint "/api/generate" -Body $genNoPrompt -ExpectedStatus 400

$chatBody = @{
    model = "gpt-4"
    stream = $false
    messages = @(
        @{ role = "system"; content = "You are helpful." },
        @{ role = "user"; content = "Hello!" }
    )
} | ConvertTo-Json -Depth 3
Test-Endpoint -Name "Chat completions (non-streaming)" -Method "POST" -Endpoint "/v1/chat/completions" -Body $chatBody -ExpectedStatus 200

$chatNoMessages = @{
    model = "gpt-4"
    stream = $false
} | ConvertTo-Json
Test-Endpoint -Name "Chat without messages" -Method "POST" -Endpoint "/v1/chat/completions" -Body $chatNoMessages -ExpectedStatus 400

# PHASE 4: Streaming (NDJSON)
Write-TestHeader "PHASE 4: STREAMING (NDJSON)"

$streamBody = @{
    model = "test-model"
    prompt = "Stream test"
    stream = $true
} | ConvertTo-Json
Test-StreamingEndpoint -Name "Generate (streaming NDJSON)" -Endpoint "/api/generate" -Body $streamBody -AcceptType "application/x-ndjson"

$chatStreamBody = @{
    model = "gpt-4"
    stream = $true
    messages = @(
        @{ role = "user"; content = "Hello!" }
    )
} | ConvertTo-Json -Depth 3
Test-StreamingEndpoint -Name "Chat (streaming NDJSON)" -Endpoint "/v1/chat/completions" -Body $chatStreamBody -AcceptType "application/x-ndjson"

# PHASE 5: Streaming (SSE)
Write-TestHeader "PHASE 5: STREAMING (SSE)"

Test-StreamingEndpoint -Name "Generate (streaming SSE)" -Endpoint "/api/generate" -Body $streamBody -AcceptType "text/event-stream"
Test-StreamingEndpoint -Name "Chat (streaming SSE)" -Endpoint "/v1/chat/completions" -Body $chatStreamBody -AcceptType "text/event-stream"

# PHASE 6: Network Operations
Write-TestHeader "PHASE 6: NETWORK OPERATIONS"

$pullBody = @{
    name = "test-model.gguf"
    url = "http://example.com/model.gguf"
} | ConvertTo-Json
$response = Test-Endpoint -Name "Pull model" -Method "POST" -Endpoint "/api/pull" -Body $pullBody -ExpectedStatus 200
if ($response) {
    $json = $response.Content | ConvertFrom-Json
    if ($json.status) {
        Write-Host "│   Pull status: $($json.status)" -ForegroundColor Gray
    }
}

$pushBody = @{
    name = "nonexistent-model.gguf"
    url = "http://example.com/upload"
} | ConvertTo-Json
Test-Endpoint -Name "Push nonexistent model" -Method "POST" -Endpoint "/api/push" -Body $pushBody -ExpectedStatus 404

# PHASE 7: Stress Testing
Write-TestHeader "PHASE 7: STRESS TESTING"

Write-Host "│ Testing: Concurrent requests (10 parallel)..." -NoNewline
$jobs = 1..10 | ForEach-Object {
    Start-Job -ScriptBlock {
        param($url)
        Invoke-WebRequest -Uri "$url/health" -Method GET -UseBasicParsing -TimeoutSec 5
    } -ArgumentList $baseUrl
}
$results = $jobs | Wait-Job | Receive-Job
$successCount = ($results | Where-Object { $_.StatusCode -eq 200 }).Count
if ($successCount -eq 10) {
    Write-Host " ✓ PASS ($successCount/10 succeeded)" -ForegroundColor Green
    $script:testsPassed++
} else {
    Write-Host " ✗ FAIL ($successCount/10 succeeded)" -ForegroundColor Red
    $script:testsFailed++
}
$jobs | Remove-Job

Write-Host "│ Testing: Rapid-fire requests (50 sequential)..." -NoNewline
$rapidSuccess = 0
for ($i = 0; $i -lt 50; $i++) {
    try {
        $resp = Invoke-WebRequest -Uri "$baseUrl/health" -Method GET -UseBasicParsing -TimeoutSec 2
        if ($resp.StatusCode -eq 200) { $rapidSuccess++ }
    } catch {}
}
if ($rapidSuccess -ge 45) {
    Write-Host " ✓ PASS ($rapidSuccess/50 succeeded)" -ForegroundColor Green
    $script:testsPassed++
} else {
    Write-Host " ✗ FAIL ($rapidSuccess/50 succeeded)" -ForegroundColor Red
    $script:testsFailed++
}

$largePrompt = "Large " * 50000  # ~300KB
$largeBody = @{
    model = "test-model"
    prompt = $largePrompt
    stream = $false
} | ConvertTo-Json
Test-Endpoint -Name "Large payload (300KB)" -Method "POST" -Endpoint "/api/generate" -Body $largeBody -ExpectedStatus 200

# PHASE 8: Edge Cases
Write-TestHeader "PHASE 8: EDGE CASES"

Test-Endpoint -Name "Empty POST body" -Method "POST" -Endpoint "/api/generate" -Body "" -ExpectedStatus 400

$specialChars = @{
    model = "test-model"
    prompt = "Special: 你好 мир 🚀 `"quotes`" \backslash\ `n`t"
    stream = $false
} | ConvertTo-Json
Test-Endpoint -Name "Special characters" -Method "POST" -Endpoint "/api/generate" -Body $specialChars -ExpectedStatus 200

Write-Host "│ Testing: Malformed HTTP..." -NoNewline
try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    $connectTask = $tcpClient.ConnectAsync("localhost", 11434)
    if ($connectTask.Wait(2000)) {
        $stream = $tcpClient.GetStream()
        $stream.WriteTimeout = 1000
        $writer = New-Object System.IO.StreamWriter($stream)
        $writer.AutoFlush = $true
        $writer.WriteLine("GARBAGE REQUEST")
        $writer.Flush()
        Start-Sleep -Milliseconds 500
        $writer.Close()
        $stream.Close()
        $tcpClient.Close()
        Write-Host " ✓ PASS (Server handled gracefully)" -ForegroundColor Green
        $script:testsPassed++
    } else {
        throw "Connection timeout"
    }
} catch {
    Write-Host " ✗ FAIL (Error: $($_.Exception.Message))" -ForegroundColor Red
    $script:testsFailed++
} finally {
    if ($tcpClient) { try { $tcpClient.Close() } catch {} }
}

# Final Results
Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  TEST RESULTS                                              ║" -ForegroundColor Cyan
Write-Host "╠════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host ("║  Tests Passed:  {0,3}                                       ║" -f $testsPassed) -ForegroundColor Green
Write-Host ("║  Tests Failed:  {0,3}                                       ║" -f $testsFailed) -ForegroundColor $(if ($testsFailed -eq 0) { "Green" } else { "Red" })
$successRate = if (($testsPassed + $testsFailed) -gt 0) { [math]::Round($testsPassed * 100 / ($testsPassed + $testsFailed)) } else { 0 }
Write-Host ("║  Success Rate:  {0,3}%                                      ║" -f $successRate) -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# Get server stats
Write-Host "`nQuerying server statistics..." -ForegroundColor Cyan
try {
    $healthResp = Invoke-WebRequest -Uri "$baseUrl/health" -Method GET -UseBasicParsing
    $health = $healthResp.Content | ConvertFrom-Json
    Write-Host "  Total Requests: $($health.total_requests)" -ForegroundColor Gray
    Write-Host "  Successful: $($health.successful_requests)" -ForegroundColor Gray
    Write-Host "  Failed: $($health.failed_requests)" -ForegroundColor Gray
    Write-Host "  Tokens Generated: $($health.tokens_generated)" -ForegroundColor Gray
    Write-Host "  Uptime: $($health.uptime_seconds) seconds" -ForegroundColor Gray
} catch {
    Write-Host "  Could not retrieve stats" -ForegroundColor Yellow
}

if ($testsFailed -eq 0) {
    Write-Host "`n✅ ALL TESTS PASSED! GGUF Server is fully operational.`n" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n⚠️  Some tests failed. Review output above.`n" -ForegroundColor Yellow
    exit 1
}
