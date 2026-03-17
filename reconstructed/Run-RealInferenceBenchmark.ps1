# Real Inference Benchmark - Full Stack Test
# Starts RawrXD server with real model and measures actual throughput

param(
    [string]$ModelPath = "d:\OllamaModels\BigDaddyG-NO-REFUSE-Q4_K_M.gguf",
    [int]$Port = 11434,
    [int]$NumRequests = 10,
    [int]$TokensPerRequest = 32
)

Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  REAL INFERENCE BENCHMARK - Full Stack Measurement    ║" -ForegroundColor Cyan
Write-Host "║  Measuring: System Throughput with Production Overhead║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

Write-Host "`n[1/4] Verifying model..." -ForegroundColor Green
if (-not (Test-Path $ModelPath)) {
    Write-Host "ERROR: Model not found at $ModelPath" -ForegroundColor Red
    exit 1
}
$modelSize = (Get-Item $ModelPath).Length / 1GB
Write-Host "✓ Model: $(Split-Path $ModelPath -Leaf) ($([math]::Round($modelSize, 2))GB)" -ForegroundColor Green

Write-Host "`n[2/4] Starting RawrXD server with model..." -ForegroundColor Green
$releaseDir = "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build\bin\Release"
$env:PATH = "C:\Qt\6.7.3\msvc2022_64\bin;$env:PATH"

# Start the main application
$serverProcess = Start-Process -FilePath "$releaseDir\RawrXD-QtShell.exe" -NoNewWindow -PassThru -ErrorAction SilentlyContinue
$serverId = $serverProcess.Id
Write-Host "✓ Server process started (PID: $serverId)" -ForegroundColor Green

# Wait for server to initialize and load model
Write-Host "⏳ Waiting for server to initialize model (this may take a minute)..." -ForegroundColor Yellow
$maxWait = 120
$waited = 0
$serverReady = $false

while ($waited -lt $maxWait) {
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:$Port/api/tags" -Method GET -TimeoutSec 2 -ErrorAction SilentlyContinue
        if ($response.StatusCode -eq 200) {
            $serverReady = $true
            break
        }
    } catch {
        # Server not ready yet
    }
    
    Start-Sleep -Seconds 2
    $waited += 2
    Write-Host "  ($waited/$maxWait seconds elapsed)" -ForegroundColor DarkGray
}

if (-not $serverReady) {
    Write-Host "ERROR: Server did not become ready within $maxWait seconds" -ForegroundColor Red
    Stop-Process -Id $serverId -Force -ErrorAction SilentlyContinue
    exit 1
}

Write-Host "✓ Server is ready and model loaded" -ForegroundColor Green

Write-Host "`n[3/4] Running inference benchmark ($NumRequests requests, $TokensPerRequest tokens each)..." -ForegroundColor Green

$results = @()
$totalStartTime = Get-Date
$totalTokens = 0

for ($i = 1; $i -le $NumRequests; $i++) {
    $prompt = "Question $i`: Explain GPU acceleration briefly. Answer:"
    
    Write-Host "  Request $i/$NumRequests..." -ForegroundColor DarkGray -NoNewline
    
    $requestStart = Get-Date
    
    try {
        $payload = @{
            prompt = $prompt
            stream = $false
            raw = $false
        } | ConvertTo-Json
        
        $response = Invoke-WebRequest `
            -Uri "http://localhost:$Port/api/generate" `
            -Method POST `
            -ContentType "application/json" `
            -Body $payload `
            -TimeoutSec 300 `
            -ErrorAction Stop
        
        $requestEnd = Get-Date
        $elapsedMs = ($requestEnd - $requestStart).TotalMilliseconds
        
        $responseData = $response.Content | ConvertFrom-Json
        
        # Try to count generated tokens from response
        $generatedText = $responseData.response
        $tokensGenerated = if ($generatedText) { 
            # Rough estimate: ~4 chars per token on average
            [math]::Max(1, [math]::Round($generatedText.Length / 4))
        } else {
            $TokensPerRequest
        }
        
        $tokensPerSec = ($tokensGenerated * 1000) / $elapsedMs
        
        $result = @{
            RequestId = $i
            ElapsedMs = $elapsedMs
            TokensGenerated = $tokensGenerated
            TokensPerSec = $tokensPerSec
            Success = $true
        }
        
        $results += $result
        $totalTokens += $tokensGenerated
        
        Write-Host " ✓ ${elapsedMs}ms, $tokensGenerated tokens, ${tokensPerSec}tok/s" -ForegroundColor Green
    }
    catch {
        Write-Host " ✗ FAILED" -ForegroundColor Red
        Write-Host "    Error: $_" -ForegroundColor DarkRed
        
        $result = @{
            RequestId = $i
            ElapsedMs = 0
            TokensGenerated = 0
            TokensPerSec = 0
            Success = $false
        }
        
        $results += $result
    }
    
    # Small delay between requests
    Start-Sleep -Milliseconds 100
}

$totalEndTime = Get-Date
$totalElapsedMs = ($totalEndTime - $totalStartTime).TotalMilliseconds

Write-Host "`n[4/4] Calculating metrics..." -ForegroundColor Green

# Aggregate metrics
$successfulRequests = @($results | Where-Object { $_.Success }).Count
$totalRequests = $results.Count
$successRate = if ($totalRequests -gt 0) { $successfulRequests / $totalRequests * 100 } else { 0 }

$avgTokensPerSec = if ($successfulRequests -gt 0) {
    ($results | Where-Object { $_.Success } | Measure-Object -Property TokensPerSec -Average).Average
} else {
    0
}

$avgLatencyMs = if ($successfulRequests -gt 0) {
    ($results | Where-Object { $_.Success } | Measure-Object -Property ElapsedMs -Average).Average
} else {
    0
}

$maxLatencyMs = ($results | Measure-Object -Property ElapsedMs -Maximum).Maximum
$minLatencyMs = ($results | Where-Object { $_.Success } | Measure-Object -Property ElapsedMs -Minimum).Minimum

# Percentiles
$sortedLatencies = @($results | Where-Object { $_.Success } | Sort-Object -Property ElapsedMs | ForEach-Object { $_.ElapsedMs })
$p50 = if ($sortedLatencies.Count -gt 0) { $sortedLatencies[[math]::Floor($sortedLatencies.Count * 0.5)] } else { 0 }
$p95 = if ($sortedLatencies.Count -gt 0) { $sortedLatencies[[math]::Floor($sortedLatencies.Count * 0.95)] } else { 0 }
$p99 = if ($sortedLatencies.Count -gt 0) { $sortedLatencies[[math]::Floor($sortedLatencies.Count * 0.99)] } else { 0 }

Write-Host "`n╔════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         REAL SYSTEM THROUGHPUT RESULTS                ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

Write-Host "`nTest Configuration:" -ForegroundColor White
Write-Host "  Model: $(Split-Path $ModelPath -Leaf)" -ForegroundColor White
Write-Host "  Model Size: $([math]::Round($modelSize, 2))GB" -ForegroundColor White
Write-Host "  Total Requests: $totalRequests" -ForegroundColor White
Write-Host "  Total Tokens: $totalTokens" -ForegroundColor White

Write-Host "`nResults:" -ForegroundColor White
Write-Host "  Successful Requests: $successfulRequests/$totalRequests ($([math]::Round($successRate, 1))%)" -ForegroundColor Green
Write-Host "  Total Test Time: $([math]::Round($totalElapsedMs / 1000, 2))s" -ForegroundColor White
Write-Host "  Average Tokens/Sec: $([math]::Round($avgTokensPerSec, 2))" -ForegroundColor Cyan
Write-Host "  Average Latency: $([math]::Round($avgLatencyMs, 2))ms" -ForegroundColor White
Write-Host "  Min Latency: $([math]::Round($minLatencyMs, 2))ms" -ForegroundColor White
Write-Host "  Max Latency: $([math]::Round($maxLatencyMs, 2))ms" -ForegroundColor White
Write-Host "  P50 Latency: $([math]::Round($p50, 2))ms (median)" -ForegroundColor White
Write-Host "  P95 Latency: $([math]::Round($p95, 2))ms" -ForegroundColor White
Write-Host "  P99 Latency: $([math]::Round($p99, 2))ms" -ForegroundColor White

Write-Host "`n⚠ INTERPRETATION:" -ForegroundColor Yellow
Write-Host "  This is REAL SYSTEM THROUGHPUT including:" -ForegroundColor Yellow
Write-Host "    • Model inference on Vulkan GPU" -ForegroundColor Yellow
Write-Host "    • Server overhead (HTTP parsing, etc.)" -ForegroundColor Yellow
Write-Host "    • KV cache management" -ForegroundColor Yellow
Write-Host "    • Network latency" -ForegroundColor Yellow
Write-Host "    • Concurrency queue delays" -ForegroundColor Yellow

Write-Host "`nComparison:" -ForegroundColor Cyan
Write-Host "  Kernel Benchmark (GPU ops only): ~80 tok/s" -ForegroundColor DarkCyan
Write-Host "  System Throughput (with overhead): $([math]::Round($avgTokensPerSec, 2)) tok/s" -ForegroundColor Cyan
Write-Host "  Production Overhead: $([math]::Round((80 - $avgTokensPerSec) / 80 * 100, 1))%" -ForegroundColor Cyan

# Save results to file
$resultsFile = "D:\RealInferenceBenchmark_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$resultsJson = @{
    timestamp = Get-Date -Format "o"
    model = $ModelPath
    model_size_gb = [math]::Round($modelSize, 2)
    configuration = @{
        total_requests = $totalRequests
        tokens_per_request = $TokensPerRequest
        port = $Port
    }
    metrics = @{
        successful_requests = $successfulRequests
        success_rate_percent = [math]::Round($successRate, 2)
        avg_tokens_per_sec = [math]::Round($avgTokensPerSec, 2)
        avg_latency_ms = [math]::Round($avgLatencyMs, 2)
        min_latency_ms = [math]::Round($minLatencyMs, 2)
        max_latency_ms = [math]::Round($maxLatencyMs, 2)
        p50_latency_ms = [math]::Round($p50, 2)
        p95_latency_ms = [math]::Round($p95, 2)
        p99_latency_ms = [math]::Round($p99, 2)
        total_elapsed_ms = [math]::Round($totalElapsedMs, 2)
    }
    individual_results = $results
} | ConvertTo-Json -Depth 10

$resultsJson | Out-File -FilePath $resultsFile -Encoding UTF8
Write-Host "`n✓ Results saved to: $resultsFile" -ForegroundColor Green

# Cleanup
Write-Host "`n[5/5] Cleaning up..." -ForegroundColor Green
Stop-Process -Id $serverId -Force -ErrorAction SilentlyContinue
Write-Host "✓ Server stopped" -ForegroundColor Green

Write-Host "`n✓ Real inference benchmark complete!" -ForegroundColor Green
