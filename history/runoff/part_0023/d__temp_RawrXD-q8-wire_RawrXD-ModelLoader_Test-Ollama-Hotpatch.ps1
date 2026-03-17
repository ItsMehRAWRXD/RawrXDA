# Test Ollama Hotpatch Proxy
# Demonstrates real-time response modification

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  OLLAMA HOTPATCH PROXY - DEMONSTRATION                     ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Configuration
$ollamaUrl = "http://localhost:11434"
$proxyUrl = "http://localhost:11435"
$model = "quantumide-performance:latest"

Write-Host "📋 Test Configuration:" -ForegroundColor Yellow
Write-Host "   Ollama (upstream): $ollamaUrl" -ForegroundColor Gray
Write-Host "   Proxy (hotpatch):  $proxyUrl" -ForegroundColor Gray
Write-Host "   Model:             $model`n" -ForegroundColor Gray

# Test 1: Direct to Ollama (unpatched)
Write-Host "┌─ TEST 1: Direct Ollama Response (No Hotpatch) ────────────┐" -ForegroundColor Cyan

$directBody = @{
    model = $model
    prompt = "Explain quantum computing in one sentence."
    stream = $false
} | ConvertTo-Json

try {
    $directResponse = Invoke-WebRequest -Uri "$ollamaUrl/api/generate" -Method POST -Body $directBody -ContentType "application/json" -UseBasicParsing
    $directJson = $directResponse.Content | ConvertFrom-Json
    
    Write-Host "✓ Direct Response:" -ForegroundColor Green
    Write-Host "  $($directJson.response)" -ForegroundColor White
} catch {
    Write-Host "✗ Failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "└────────────────────────────────────────────────────────────┘`n" -ForegroundColor Cyan

# Test 2: Through Hotpatch Proxy (patched)
Write-Host "┌─ TEST 2: Hotpatch Proxy Response (With Rules) ────────────┐" -ForegroundColor Cyan

Write-Host "Hotpatch Rules Applied:" -ForegroundColor Yellow
Write-Host "  • Token: 'quantum' → 'QUANTUM'" -ForegroundColor Gray
Write-Host "  • Token: 'computing' → 'COMPUTING'" -ForegroundColor Gray
Write-Host "  • Regex: '\b(can|could)\b' → 'may'" -ForegroundColor Gray
Write-Host "  • Fact:  'quantum' → 'quantum (physics-based)'" -ForegroundColor Gray

$proxyBody = @{
    model = $model
    prompt = "Explain quantum computing in one sentence."
    stream = $false
} | ConvertTo-Json

try {
    $proxyResponse = Invoke-WebRequest -Uri "$proxyUrl/api/generate" -Method POST -Body $proxyBody -ContentType "application/json" -UseBasicParsing -TimeoutSec 30
    $proxyJson = $proxyResponse.Content | ConvertFrom-Json
    
    Write-Host "`n✓ Hotpatched Response:" -ForegroundColor Green
    Write-Host "  $($proxyJson.response)" -ForegroundColor White
} catch {
    Write-Host "✗ Failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "   (Proxy may not be running on port 11435)" -ForegroundColor Yellow
}

Write-Host "└────────────────────────────────────────────────────────────┘`n" -ForegroundColor Cyan

# Test 3: Safety Filter Demo
Write-Host "┌─ TEST 3: Safety Filter Demonstration ──────────────────────┐" -ForegroundColor Cyan

Write-Host "Safety Rules Applied:" -ForegroundColor Yellow
Write-Host "  • Block: 'password|secret|confidential'" -ForegroundColor Gray
Write-Host "  • Replace unsafe terms with [FILTERED]`n" -ForegroundColor Gray

$safetyBody = @{
    model = $model
    prompt = "What is the password for the quantum system?"
    stream = $false
} | ConvertTo-Json

try {
    $safetyResponse = Invoke-WebRequest -Uri "$proxyUrl/api/generate" -Method POST -Body $safetyBody -ContentType "application/json" -UseBasicParsing -TimeoutSec 30
    $safetyJson = $safetyResponse.Content | ConvertFrom-Json
    
    Write-Host "✓ Safety-Filtered Response:" -ForegroundColor Green
    Write-Host "  $($safetyJson.response)" -ForegroundColor White
} catch {
    Write-Host "✗ Failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "└────────────────────────────────────────────────────────────┘`n" -ForegroundColor Cyan

# Test 4: Streaming with Hotpatch
Write-Host "┌─ TEST 4: Streaming Response (Hotpatched) ──────────────────┐" -ForegroundColor Cyan

$streamBody = @{
    model = $model
    prompt = "Count from 1 to 5."
    stream = $true
} | ConvertTo-Json

Write-Host "Streaming test (collecting chunks for 5 seconds)..." -ForegroundColor Yellow

try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    $connectTask = $tcpClient.ConnectAsync("localhost", 11435)
    
    if ($connectTask.Wait(2000)) {
        $stream = $tcpClient.GetStream()
        $writer = New-Object System.IO.StreamWriter($stream)
        $writer.AutoFlush = $true
        $reader = New-Object System.IO.StreamReader($stream)
        
        # Send request
        $writer.WriteLine("POST /api/generate HTTP/1.1")
        $writer.WriteLine("Host: localhost")
        $writer.WriteLine("Content-Type: application/json")
        $writer.WriteLine("Content-Length: $($streamBody.Length)")
        $writer.WriteLine("")
        $writer.Write($streamBody)
        $writer.Flush()
        
        # Read headers
        while ($true) {
            $line = $reader.ReadLine()
            if ([string]::IsNullOrWhiteSpace($line)) { break }
        }
        
        # Read stream chunks
        $chunks = 0
        $timeout = [DateTime]::Now.AddSeconds(5)
        while ([DateTime]::Now -lt $timeout -and $chunks -lt 10) {
            if ($stream.DataAvailable) {
                $line = $reader.ReadLine()
                if ($line -and $line.Trim().StartsWith("{")) {
                    $chunkJson = $line | ConvertFrom-Json
                    if ($chunkJson.response) {
                        Write-Host "  Chunk $($chunks + 1): $($chunkJson.response)" -ForegroundColor Cyan
                        $chunks++
                    }
                }
            } else {
                Start-Sleep -Milliseconds 100
            }
        }
        
        $writer.Close()
        $reader.Close()
        $stream.Close()
        $tcpClient.Close()
        
        Write-Host "`n✓ Received $chunks streamed chunks" -ForegroundColor Green
    } else {
        throw "Connection timeout"
    }
} catch {
    Write-Host "✗ Streaming failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "└────────────────────────────────────────────────────────────┘`n" -ForegroundColor Cyan

# Summary
Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  SUMMARY                                                   ║" -ForegroundColor Cyan
Write-Host "╠════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  The hotpatch proxy allows real-time modification of       ║" -ForegroundColor White
Write-Host "║  Ollama responses WITHOUT retraining the model.            ║" -ForegroundColor White
Write-Host "║                                                            ║" -ForegroundColor White
Write-Host "║  Use cases:                                                ║" -ForegroundColor White
Write-Host "║  • Correct common model mistakes                           ║" -ForegroundColor White
Write-Host "║  • Add factual context to responses                        ║" -ForegroundColor White
Write-Host "║  • Filter unsafe/unwanted content                          ║" -ForegroundColor White
Write-Host "║  • Adjust tone/formality in real-time                      ║" -ForegroundColor White
Write-Host "║  • Apply domain-specific terminology                       ║" -ForegroundColor White
Write-Host "║  • Inject disclaimers or citations                         ║" -ForegroundColor White
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "To start the proxy, compile and run:" -ForegroundColor Yellow
Write-Host "  1. Add ollama_hotpatch_proxy.cpp to CMakeLists.txt" -ForegroundColor Gray
Write-Host "  2. Build: cmake --build build --config Release" -ForegroundColor Gray
Write-Host "  3. Run with custom rules programmatically`n" -ForegroundColor Gray
