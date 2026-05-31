# ============================================================================
# RawrXD Comparative Benchmark Suite
# ============================================================================
# Objective: Measure RawrXD performance vs Ollama, vLLM, LM Studio
# Metrics: TPS, Latency (p50/p99), Memory, Concurrency Scaling
# ============================================================================

param(
    [string]$Model = "ministral-3b",
    [int]$Duration = 60,
    [int]$Concurrency = 1,
    [switch]$FullSuite,
    [switch]$Publish
)

$ErrorActionPreference = "Continue"

class BenchmarkResult {
    [string]$Tool
    [string]$Model
    [datetime]$Timestamp
    [double]$TPS
    [double]$LatencyP50Ms
    [double]$LatencyP99Ms
    [double]$PeakMemoryMB
    [double]$AvgMemoryMB
    [int]$TokensGenerated
    [int]$ErrorCount
    [hashtable]$Metadata
}

# ============================================================================
# RAWRXD BENCHMARKS
# ============================================================================

function Invoke-RawrXDBenchmark {
    param(
        [string]$Model,
        [int]$Duration,
        [int]$Concurrency
    )
    
    Write-Host "Starting RawrXD benchmark ($Concurrency concurrent)..." -ForegroundColor Cyan
    
    $exe = "d:\rawrxd\build-ninja\RawrXD-Win32IDE.exe"
    if (-not (Test-Path $exe)) {
        Write-Host "ERROR: RawrXD executable not found" -ForegroundColor Red
        return $null
    }
    
    # Create benchmark request JSON
    $request = @{
        model = $Model
        prompt = "Tell me about quantum computing. Explain in detail."
        stream = $false
        temperature = 0.7
        top_p = 0.9
    } | ConvertTo-Json
    
    # Spin up background process with monitoring
    $startTime = Get-Date
    $tokens = 0
    $latencies = @()
    $memSamples = @()
    $errors = 0
    
    # Run N concurrent requests over duration
    for ($i = 0; $i -lt $Concurrency; $i++) {
        Start-Job -ScriptBlock {
            param($exe, $request, $model, $duration)
            $elapsed = 0
            while ($elapsed -lt $duration) {
                try {
                    $sw = [System.Diagnostics.Stopwatch]::StartNew()
                    
                    # Call RawrXD inference API
                    $result = & "$exe" --headless --model "$model" --prompt $request --timeout 30000
                    
                    $sw.Stop()
                    @{
                        latency = $sw.ElapsedMilliseconds
                        tokens = ($result | Measure-Object -Line).Lines
                        error = $false
                    } | ConvertTo-Json | Write-Host
                    
                    $elapsed += 1
                } catch {
                    @{ error = $true; message = $_.Exception.Message } | ConvertTo-Json | Write-Host
                    $elapsed += 1
                }
            }
        } -ArgumentList $exe, $request, $Model, $Duration
    }
    
    # Collect results
    $jobs = Get-Job | Where-Object { $_.State -eq 'Running' }
    $results = @()
    
    foreach ($job in $jobs) {
        Wait-Job -Job $job | Out-Null
        $output = Receive-Job -Job $job
        
        $output | ForEach-Object {
            try {
                $parsed = $_ | ConvertFrom-Json -ErrorAction SilentlyContinue
                if ($parsed.latency) {
                    $latencies += $parsed.latency
                    $tokens += $parsed.tokens
                }
                if ($parsed.error) {
                    $errors++
                }
            } catch { }
        }
        
        Remove-Job -Job $job
    }
    
    # Memory sampling during test
    $proc = Get-Process | Where-Object { $_.Name -like "*rawrxd*" -or $_.Name -like "*inference*" }
    if ($proc) {
        $memSamples += $proc.WorkingSet64 / 1MB
    }
    
    $elapsed = (Get-Date) - $startTime
    $tps = $tokens / $elapsed.TotalSeconds
    
    $result = [BenchmarkResult]@{
        Tool = "RawrXD"
        Model = $Model
        Timestamp = Get-Date
        TPS = $tps
        LatencyP50Ms = ($latencies | Sort-Object)[($latencies.Count * 0.5)]
        LatencyP99Ms = ($latencies | Sort-Object)[($latencies.Count * 0.99)]
        PeakMemoryMB = ($memSamples | Measure-Object -Maximum).Maximum
        AvgMemoryMB = ($memSamples | Measure-Object -Average).Average
        TokensGenerated = $tokens
        ErrorCount = $errors
        Metadata = @{
            Concurrency = $Concurrency
            Duration = $Duration
            LatencySamples = $latencies.Count
        }
    }
    
    return $result
}

# ============================================================================
# OLLAMA BENCHMARKS
# ============================================================================

function Invoke-OllamaBenchmark {
    param(
        [string]$Model,
        [int]$Duration,
        [int]$Concurrency
    )
    
    Write-Host "Starting Ollama benchmark ($Concurrency concurrent)..." -ForegroundColor Cyan
    
    # Check if Ollama is running
    $ollamaProc = Get-Process | Where-Object { $_.Name -like "*ollama*" }
    if (-not $ollamaProc) {
        Write-Host "ERROR: Ollama not running. Start with: ollama serve" -ForegroundColor Red
        return $null
    }
    
    $baseUrl = "http://localhost:11434"
    $latencies = @()
    $tokens = 0
    $errors = 0
    
    # Concurrent requests
    $jobs = @()
    for ($i = 0; $i -lt $Concurrency; $i++) {
        $job = Start-Job -ScriptBlock {
            param($baseUrl, $model, $duration)
            $elapsed = 0
            $output = @()
            
            while ($elapsed -lt $duration) {
                try {
                    $sw = [System.Diagnostics.Stopwatch]::StartNew()
                    
                    $response = Invoke-RestMethod -Uri "$baseUrl/api/generate" `
                        -Method Post `
                        -Body (@{
                            model = $model
                            prompt = "Explain quantum computing."
                            stream = $false
                        } | ConvertTo-Json) `
                        -ContentType "application/json" `
                        -TimeoutSec 30
                    
                    $sw.Stop()
                    $output += @{
                        latency = $sw.ElapsedMilliseconds
                        tokens = ($response.response -split '\s+' | Measure-Object).Count
                    }
                    $elapsed += 1
                } catch {
                    $output += @{ error = $true }
                    $elapsed += 1
                }
            }
            return $output | ConvertTo-Json
        } -ArgumentList $baseUrl, $Model, $Duration
        
        $jobs += $job
    }
    
    # Collect results
    foreach ($job in $jobs) {
        Wait-Job -Job $job | Out-Null
        $output = Receive-Job -Job $job
        
        try {
            $parsed = $output | ConvertFrom-Json -ErrorAction SilentlyContinue
            $parsed | ForEach-Object {
                if ($_.latency) {
                    $latencies += $_.latency
                    $tokens += $_.tokens
                }
                if ($_.error) {
                    $errors++
                }
            }
        } catch { }
        
        Remove-Job -Job $job
    }
    
    $elapsed = $Duration
    $tps = $tokens / $elapsed
    
    $result = [BenchmarkResult]@{
        Tool = "Ollama"
        Model = $Model
        Timestamp = Get-Date
        TPS = $tps
        LatencyP50Ms = if ($latencies.Count -gt 0) { ($latencies | Sort-Object)[($latencies.Count * 0.5)] } else { 0 }
        LatencyP99Ms = if ($latencies.Count -gt 0) { ($latencies | Sort-Object)[($latencies.Count * 0.99)] } else { 0 }
        PeakMemoryMB = 0
        AvgMemoryMB = 0
        TokensGenerated = $tokens
        ErrorCount = $errors
        Metadata = @{
            Concurrency = $Concurrency
            Duration = $Duration
            LatencySamples = $latencies.Count
        }
    }
    
    return $result
}

# ============================================================================
# VLLM BENCHMARKS
# ============================================================================

function Invoke-VLLMBenchmark {
    param(
        [string]$Model,
        [int]$Duration,
        [int]$Concurrency
    )
    
    Write-Host "Starting vLLM benchmark ($Concurrency concurrent)..." -ForegroundColor Cyan
    
    $baseUrl = "http://localhost:8000"
    $latencies = @()
    $tokens = 0
    $errors = 0
    
    # Check connectivity
    try {
        $health = Invoke-RestMethod -Uri "$baseUrl/health" -TimeoutSec 3
    } catch {
        Write-Host "ERROR: vLLM not running. Start with: python -m vllm.entrypoints.openai.api_server" -ForegroundColor Red
        return $null
    }
    
    # Concurrent requests
    $jobs = @()
    for ($i = 0; $i -lt $Concurrency; $i++) {
        $job = Start-Job -ScriptBlock {
            param($baseUrl, $model, $duration)
            $output = @()
            $elapsed = 0
            
            while ($elapsed -lt $duration) {
                try {
                    $sw = [System.Diagnostics.Stopwatch]::StartNew()
                    
                    $response = Invoke-RestMethod -Uri "$baseUrl/v1/completions" `
                        -Method Post `
                        -Body (@{
                            model = $model
                            prompt = "Explain quantum computing."
                            max_tokens = 256
                            temperature = 0.7
                        } | ConvertTo-Json) `
                        -ContentType "application/json" `
                        -TimeoutSec 30
                    
                    $sw.Stop()
                    $tokenCount = $response.usage.completion_tokens
                    
                    $output += @{
                        latency = $sw.ElapsedMilliseconds
                        tokens = $tokenCount
                    }
                    $elapsed += 1
                } catch {
                    $output += @{ error = $true }
                    $elapsed += 1
                }
            }
            return $output | ConvertTo-Json
        } -ArgumentList $baseUrl, $Model, $Duration
        
        $jobs += $job
    }
    
    # Collect results
    foreach ($job in $jobs) {
        Wait-Job -Job $job | Out-Null
        $output = Receive-Job -Job $job
        
        try {
            $parsed = $output | ConvertFrom-Json -ErrorAction SilentlyContinue
            $parsed | ForEach-Object {
                if ($_.latency) {
                    $latencies += $_.latency
                    $tokens += $_.tokens
                }
                if ($_.error) {
                    $errors++
                }
            }
        } catch { }
        
        Remove-Job -Job $job
    }
    
    $elapsed = $Duration
    $tps = $tokens / $elapsed
    
    $result = [BenchmarkResult]@{
        Tool = "vLLM"
        Model = $Model
        Timestamp = Get-Date
        TPS = $tps
        LatencyP50Ms = if ($latencies.Count -gt 0) { ($latencies | Sort-Object)[($latencies.Count * 0.5)] } else { 0 }
        LatencyP99Ms = if ($latencies.Count -gt 0) { ($latencies | Sort-Object)[($latencies.Count * 0.99)] } else { 0 }
        PeakMemoryMB = 0
        AvgMemoryMB = 0
        TokensGenerated = $tokens
        ErrorCount = $errors
        Metadata = @{
            Concurrency = $Concurrency
            Duration = $Duration
            LatencySamples = $latencies.Count
        }
    }
    
    return $result
}

# ============================================================================
# REPORTING
# ============================================================================

function Format-BenchmarkReport {
    param([BenchmarkResult[]]$Results)
    
    $header = @"
╔════════════════════════════════════════════════════════════════════════════╗
║                   COMPARATIVE BENCHMARK RESULTS                           ║
║                         $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")                          ║
╚════════════════════════════════════════════════════════════════════════════╝

"@
    
    Write-Host $header
    
    # TPS Comparison
    Write-Host "THROUGHPUT (Tokens Per Second)" -ForegroundColor Green
    Write-Host "─" * 70
    $Results | Sort-Object TPS -Descending | ForEach-Object {
        $bar = "█" * ([int]($_.TPS / 100))
        Write-Host "$($_.Tool.PadRight(15)) | TPS: $([math]::Round($_.TPS, 2).ToString().PadLeft(8)) | $bar"
    }
    
    Write-Host ""
    
    # Latency Comparison
    Write-Host "LATENCY (Milliseconds)" -ForegroundColor Yellow
    Write-Host "─" * 70
    Write-Host "Tool".PadRight(15) + "| P50 (ms)".PadRight(15) + "| P99 (ms)".PadRight(15) + "| Error Rate"
    Write-Host "─" * 70
    
    $Results | ForEach-Object {
        $errorRate = [math]::Round(($_.ErrorCount / ($_.Metadata.LatencySamples + $_.ErrorCount)) * 100, 2)
        Write-Host "$($_.Tool.PadRight(15))| $([math]::Round($_.LatencyP50Ms, 2).ToString().PadRight(14)) | $([math]::Round($_.LatencyP99Ms, 2).ToString().PadRight(14)) | $errorRate%"
    }
    
    Write-Host ""
    
    # Memory
    Write-Host "MEMORY USAGE (MB)" -ForegroundColor Magenta
    Write-Host "─" * 70
    Write-Host "Tool".PadRight(15) + "| Peak".PadRight(15) + "| Average"
    Write-Host "─" * 70
    
    $Results | ForEach-Object {
        Write-Host "$($_.Tool.PadRight(15))| $([math]::Round($_.PeakMemoryMB, 2).ToString().PadRight(14)) | $([math]::Round($_.AvgMemoryMB, 2))"
    }
    
    Write-Host ""
    Write-Host "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
}

function Export-BenchmarkJSON {
    param(
        [BenchmarkResult[]]$Results,
        [string]$Filepath = "d:\rawrxd\benchmark_results.json"
    )
    
    $export = @{
        timestamp = Get-Date -Format "o"
        results = $Results | ForEach-Object {
            @{
                tool = $_.Tool
                model = $_.Model
                tps = [math]::Round($_.TPS, 2)
                latency_p50_ms = [math]::Round($_.LatencyP50Ms, 2)
                latency_p99_ms = [math]::Round($_.LatencyP99Ms, 2)
                peak_memory_mb = [math]::Round($_.PeakMemoryMB, 2)
                avg_memory_mb = [math]::Round($_.AvgMemoryMB, 2)
                tokens_generated = $_.TokensGenerated
                error_count = $_.ErrorCount
                concurrency = $_.Metadata.Concurrency
                duration = $_.Metadata.Duration
            }
        }
    }
    
    $export | ConvertTo-Json -Depth 10 | Set-Content -Path $Filepath
    Write-Host "Results exported to: $Filepath" -ForegroundColor Green
}

# ============================================================================
# MAIN
# ============================================================================

Write-Host "RawrXD Comparative Benchmark Suite" -ForegroundColor Cyan
Write-Host "=" * 70

$results = @()

# Run sequence
if ($FullSuite) {
    # Test multiple concurrency levels
    1, 4, 8 | ForEach-Object {
        Write-Host "Concurrency: $_" -ForegroundColor Magenta
        
        $results += Invoke-RawrXDBenchmark -Model $Model -Duration $Duration -Concurrency $_
        Start-Sleep -Seconds 2
        
        $results += Invoke-OllamaBenchmark -Model $Model -Duration $Duration -Concurrency $_
        Start-Sleep -Seconds 2
        
        $results += Invoke-VLLMBenchmark -Model $Model -Duration $Duration -Concurrency $_
        Start-Sleep -Seconds 2
    }
} else {
    # Single run
    $results += Invoke-RawrXDBenchmark -Model $Model -Duration $Duration -Concurrency $Concurrency
    Start-Sleep -Seconds 2
    
    $results += Invoke-OllamaBenchmark -Model $Model -Duration $Duration -Concurrency $Concurrency
    Start-Sleep -Seconds 2
    
    $results += Invoke-VLLMBenchmark -Model $Model -Duration $Duration -Concurrency $Concurrency
}

# Report
Format-BenchmarkReport -Results $results
Export-BenchmarkJSON -Results $results

if ($Publish) {
    Write-Host "Publishing results..." -ForegroundColor Cyan
    # TODO: Upload to benchmark database / public dashboard
}
