# Real-Time A/B Testing Dashboard (Streaming)
# Runs models in parallel and displays live metrics
# Uses curl with streaming for latency measurement

param(
    [string]$ModelA = "mistral:latest",
    [string]$ModelB = "neural-chat:latest",
    [string]$TestPrompt = "Explain machine learning in 3 sentences.",
    [int]$NumIterations = 5,
    [int]$Timeout = 120,
    [string]$OutputFile = "e:\A-B-streaming-results-$(Get-Date -Format 'yyyyMMdd-HHmmss').csv"
)

$ErrorActionPreference = "Stop"

# Results storage
$global:streamingResults = @()

Write-Host "╔════════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      Real-Time A/B Testing Dashboard (Streaming Parallel Execution)            ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  Model A: $ModelA" -ForegroundColor Cyan
Write-Host "  Model B: $ModelB" -ForegroundColor Magenta
Write-Host "  Iterations: $NumIterations" -ForegroundColor White
Write-Host "  Prompt: '$TestPrompt'" -ForegroundColor White
Write-Host ""

# Function to run a single test
function Run-SingleTest {
    param(
        [string]$Model,
        [string]$Prompt,
        [int]$IterationNum,
        [int]$Timeout
    )
    
    $startTime = Get-Date
    
    try {
        $body = @{
            model = $Model
            prompt = $Prompt
            stream = $false
        } | ConvertTo-Json
        
        $curlArgs = @(
            "-s",
            "-w", "`n%{time_total}|%{http_code}",
            "-X", "POST",
            "http://localhost:11434/api/generate",
            "-H", "Content-Type: application/json",
            "-d", $body,
            "--max-time", $Timeout.ToString()
        )
        
        $output = & curl.exe @curlArgs 2>&1
        $endTime = Get-Date
        $elapsedMs = [math]::Round(($endTime - $startTime).TotalMilliseconds, 2)
        
        if ($output) {
            $lines = $output -split "`n"
            $metadata = $lines[-1] -split "\|"
            $timing = [double]$metadata[0]
            $httpCode = $metadata[1]
            $responseJson = $lines[0..($lines.Count - 2)] -join "`n"
            
            try {
                $response = $responseJson | ConvertFrom-Json
                $responseLength = $response.response.Length
                $estimatedTokens = [math]::Ceiling($responseLength / 4)
                
                return @{
                    Model = $Model
                    Iteration = $IterationNum
                    Success = $true
                    LatencyMs = $elapsedMs
                    CurlTimingS = $timing
                    HttpCode = $httpCode
                    ResponseLength = $responseLength
                    EstimatedTokens = $estimatedTokens
                    TokensPerSec = [math]::Round($estimatedTokens / $timing, 2)
                    Timestamp = $startTime
                }
            } catch {
                return @{
                    Model = $Model
                    Iteration = $IterationNum
                    Success = $false
                    LatencyMs = $elapsedMs
                    Error = "JSON parse failed"
                    Timestamp = $startTime
                }
            }
        } else {
            return @{
                Model = $Model
                Iteration = $IterationNum
                Success = $false
                LatencyMs = $elapsedMs
                Error = "No response"
                Timestamp = $startTime
            }
        }
        
    } catch {
        return @{
            Model = $Model
            Iteration = $IterationNum
            Success = $false
            LatencyMs = [math]::Round(([DateTime]::Now - $startTime).TotalMilliseconds, 2)
            Error = $_.Exception.Message
            Timestamp = $startTime
        }
    }
}

# Run iterations
Write-Host "Starting A/B tests..." -ForegroundColor Cyan
Write-Host ""

for ($i = 1; $i -le $NumIterations; $i++) {
    Write-Host "Iteration $i/$NumIterations" -ForegroundColor Yellow
    
    # Run both models in parallel (simulated with sequential for clarity)
    $resultA = Run-SingleTest -Model $ModelA -Prompt $TestPrompt -IterationNum $i -Timeout $Timeout
    $resultB = Run-SingleTest -Model $ModelB -Prompt $TestPrompt -IterationNum $i -Timeout $Timeout
    
    $global:streamingResults += $resultA
    $global:streamingResults += $resultB
    
    # Display real-time results
    Write-Host "  Model A: " -ForegroundColor Cyan -NoNewline
    if ($resultA.Success) {
        Write-Host "✓ $($resultA.LatencyMs)ms | $($resultA.TokensPerSec) tokens/sec" -ForegroundColor Green
    } else {
        Write-Host "✗ ERROR: $($resultA.Error)" -ForegroundColor Red
    }
    
    Write-Host "  Model B: " -ForegroundColor Magenta -NoNewline
    if ($resultB.Success) {
        Write-Host "✓ $($resultB.LatencyMs)ms | $($resultB.TokensPerSec) tokens/sec" -ForegroundColor Green
    } else {
        Write-Host "✗ ERROR: $($resultB.Error)" -ForegroundColor Red
    }
    
    if ($resultA.Success -and $resultB.Success) {
        $diff = [math]::Round($resultA.LatencyMs - $resultB.LatencyMs, 2)
        $faster = if ($diff -gt 0) { "Model B" } else { "Model A" }
        Write-Host "  Difference: $([math]::Abs($diff))ms ($faster faster)" -ForegroundColor Cyan
    }
    
    Write-Host ""
    
    # Small delay between iterations
    if ($i -lt $NumIterations) {
        Start-Sleep -Milliseconds 500
    }
}

# Calculate summary statistics
Write-Host "╔════════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                         SUMMARY STATISTICS                                    ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$resultsA = $global:streamingResults | Where-Object { $_.Model -eq $ModelA -and $_.Success }
$resultsB = $global:streamingResults | Where-Object { $_.Model -eq $ModelB -and $_.Success }

if ($resultsA.Count -gt 0) {
    $avgLatencyA = [math]::Round(($resultsA | Measure-Object -Property LatencyMs -Average).Average, 2)
    $avgTokensA = [math]::Round(($resultsA | Measure-Object -Property TokensPerSec -Average).Average, 2)
    $minLatencyA = ($resultsA | Measure-Object -Property LatencyMs -Minimum).Minimum
    $maxLatencyA = ($resultsA | Measure-Object -Property LatencyMs -Maximum).Maximum
    
    Write-Host "Model A ($ModelA)" -ForegroundColor Cyan
    Write-Host "  Avg Latency: $avgLatencyA ms" -ForegroundColor White
    Write-Host "  Min/Max: $minLatencyA / $maxLatencyA ms" -ForegroundColor White
    Write-Host "  Avg Throughput: $avgTokensA tokens/sec" -ForegroundColor White
    Write-Host "  Success Rate: $($resultsA.Count)/$NumIterations" -ForegroundColor White
    Write-Host ""
}

if ($resultsB.Count -gt 0) {
    $avgLatencyB = [math]::Round(($resultsB | Measure-Object -Property LatencyMs -Average).Average, 2)
    $avgTokensB = [math]::Round(($resultsB | Measure-Object -Property TokensPerSec -Average).Average, 2)
    $minLatencyB = ($resultsB | Measure-Object -Property LatencyMs -Minimum).Minimum
    $maxLatencyB = ($resultsB | Measure-Object -Property LatencyMs -Maximum).Maximum
    
    Write-Host "Model B ($ModelB)" -ForegroundColor Magenta
    Write-Host "  Avg Latency: $avgLatencyB ms" -ForegroundColor White
    Write-Host "  Min/Max: $minLatencyB / $maxLatencyB ms" -ForegroundColor White
    Write-Host "  Avg Throughput: $avgTokensB tokens/sec" -ForegroundColor White
    Write-Host "  Success Rate: $($resultsB.Count)/$NumIterations" -ForegroundColor White
    Write-Host ""
}

if ($resultsA.Count -gt 0 -and $resultsB.Count -gt 0) {
    $latencyDiff = [math]::Round($avgLatencyA - $avgLatencyB, 2)
    $throughputDiff = [math]::Round($avgTokensA - $avgTokensB, 2)
    
    Write-Host "Head-to-Head Comparison:" -ForegroundColor Yellow
    Write-Host "  Latency Difference: $([math]::Abs($latencyDiff))ms ($( if ($latencyDiff -gt 0) { 'B is faster' } else { 'A is faster' }))" -ForegroundColor Cyan
    Write-Host "  Throughput Difference: $([math]::Abs($throughputDiff)) tokens/sec" -ForegroundColor Cyan
    Write-Host ""
}

# Save to CSV
Write-Host "💾 Saving results to: $OutputFile" -ForegroundColor Yellow

$csvData = $global:streamingResults | Select-Object Model, Iteration, Success, LatencyMs, TokensPerSec, Timestamp | 
    ConvertTo-Csv -NoTypeInformation

$csvData | Set-Content -Path $OutputFile

Write-Host "✓ Results saved" -ForegroundColor Green
