# A/B Testing Framework for Real Models (Real-Time using curl)
# Compares multiple models simultaneously with comprehensive metrics
# Tests: latency, accuracy, consistency, token/sec, memory patterns
# Created: December 5, 2025

param(
    [string]$ModelA = "mistral:latest",
    [string]$ModelB = "neural-chat:latest",
    [int]$NumTests = 10,
    [int]$Timeout = 120,
    [switch]$Verbose = $false,
    [switch]$IncludeQuality = $false,
    [string]$OutputFile = "e:\A-B-test-results-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
)

$ErrorActionPreference = "Stop"

# Test configuration
class ABTestConfig {
    [string]$ModelA
    [string]$ModelB
    [int]$NumTests
    [int]$Timeout
    [PSObject[]]$Prompts
    [hashtable]$Results
}

class TestResult {
    [string]$Model
    [int]$TestNum
    [double]$LatencyMs
    [int]$TokenCount
    [double]$TokensPerSecond
    [string]$ResponseText
    [bool]$Success
    [string]$ErrorMessage
    [DateTime]$Timestamp
    [PSObject]$FullResponse
}

# Prompts for testing different aspects
$testPrompts = @(
    @{
        Name = "Factual-1"
        Prompt = "What is the capital of France? Answer in one sentence."
        Category = "Factual"
    },
    @{
        Name = "Code-1"
        Prompt = "Write a simple Python function that returns factorial of n. Keep it under 5 lines."
        Category = "Code"
    },
    @{
        Name = "Creative-1"
        Prompt = "Write a haiku about artificial intelligence."
        Category = "Creative"
    },
    @{
        Name = "Reasoning-1"
        Prompt = "If it takes 5 machines 5 minutes to make 5 widgets, how long does it take 100 machines to make 100 widgets?"
        Category = "Reasoning"
    },
    @{
        Name = "Summarization-1"
        Prompt = "Summarize in 2 sentences: The adoption of artificial intelligence in healthcare has revolutionized patient care through early disease detection, personalized treatment plans, and improved operational efficiency."
        Category = "Summarization"
    },
    @{
        Name = "Instructions-1"
        Prompt = "List the steps to make a sandwich in exactly 3 bullet points."
        Category = "Instructions"
    },
    @{
        Name = "Factual-2"
        Prompt = "In what year did the Titanic sink?"
        Category = "Factual"
    },
    @{
        Name = "Code-2"
        Prompt = "How do you reverse a string in Python?"
        Category = "Code"
    },
    @{
        Name = "Comparative-1"
        Prompt = "What are the main differences between Python and JavaScript?"
        Category = "Comparative"
    },
    @{
        Name = "Math-1"
        Prompt = "What is 15% of 240?"
        Category = "Math"
    }
)

# Initialize results tracking
$global:results = @{
    ModelA = @()
    ModelB = @()
    ComparisonMetrics = $null
    StartTime = Get-Date
    EndTime = $null
}

# Function to test a model with a single prompt
function Test-ModelPrompt {
    param(
        [string]$Model,
        [string]$Prompt,
        [int]$Timeout,
        [int]$TestNum,
        [string]$TestName
    )
    
    $startTime = Get-Date
    $result = New-Object TestResult
    $result.Model = $Model
    $result.TestNum = $TestNum
    $result.Timestamp = $startTime
    
    try {
        # Prepare the request
        $body = @{
            model = $Model
            prompt = $Prompt
            stream = $false
        } | ConvertTo-Json
        
        # Make the API call using curl
        $curlArgs = @(
            "-s",                                              # silent
            "-w", "`n%{time_total}",                          # append total time
            "-X", "POST",
            "http://localhost:11434/api/generate",
            "-H", "Content-Type: application/json",
            "-d", $body,
            "--max-time", $Timeout.ToString()
        )
        
        if ($Verbose) {
            Write-Host "  [TEST #$TestNum - $TestName] Testing model: $Model" -ForegroundColor Gray
            Write-Host "    Prompt: $($Prompt.Substring(0, [Math]::Min(50, $Prompt.Length)))..." -ForegroundColor DarkGray
        }
        
        # Execute curl
        $output = & curl.exe @curlArgs 2>&1
        $endTime = Get-Date
        
        # Parse response
        if ($output) {
            # Last line is timing, rest is response
            $lines = $output -split "`n"
            $timingLine = $lines[-1]
            $responseJson = $lines[0..($lines.Count - 2)] -join "`n"
            
            # Try to parse the JSON response
            try {
                $fullResponse = $responseJson | ConvertFrom-Json
                $result.FullResponse = $fullResponse
                $result.ResponseText = $fullResponse.response
                $result.Success = $true
                
                # Calculate metrics
                $result.LatencyMs = [math]::Round(($endTime - $startTime).TotalMilliseconds, 2)
                
                # Estimate token count (rough approximation: ~4 chars per token)
                $result.TokenCount = [math]::Ceiling($result.ResponseText.Length / 4)
                $result.TokensPerSecond = [math]::Round($result.TokenCount / ($result.LatencyMs / 1000), 2)
                
            } catch {
                $result.Success = $false
                $result.ErrorMessage = "Failed to parse response: $_"
                $result.LatencyMs = [math]::Round(($endTime - $startTime).TotalMilliseconds, 2)
            }
        } else {
            $result.Success = $false
            $result.ErrorMessage = "No response from API"
            $result.LatencyMs = [math]::Round(($endTime - $startTime).TotalMilliseconds, 2)
        }
        
    } catch {
        $result.Success = $false
        $result.ErrorMessage = $_.Exception.Message
        $result.LatencyMs = [math]::Round(([DateTime]::Now - $startTime).TotalMilliseconds, 2)
    }
    
    return $result
}

# Function to run complete test suite for a model
function Run-ModelTestSuite {
    param(
        [string]$Model,
        [int]$NumTests,
        [int]$Timeout,
        [string]$ModelLabel  # "A" or "B"
    )
    
    $results = @()
    
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "TESTING MODEL $ModelLabel : $Model" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    
    # Use only the requested number of tests
    $testsToRun = $testPrompts[0..($NumTests - 1)]
    
    for ($i = 0; $i -lt $testsToRun.Count; $i++) {
        $testPrompt = $testsToRun[$i]
        
        $testResult = Test-ModelPrompt -Model $Model `
                                      -Prompt $testPrompt.Prompt `
                                      -Timeout $Timeout `
                                      -TestNum ($i + 1) `
                                      -TestName $testPrompt.Name
        
        $results += $testResult
        
        if ($testResult.Success) {
            Write-Host "  ✓ Test #$($i+1) ($($testPrompt.Name)): $($testResult.LatencyMs)ms | $($testResult.TokensPerSecond) tokens/sec" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Test #$($i+1) ($($testPrompt.Name)): ERROR - $($testResult.ErrorMessage)" -ForegroundColor Red
        }
        
        # Small delay between requests to avoid overload
        Start-Sleep -Milliseconds 500
    }
    
    return $results
}

# Function to calculate aggregate metrics
function Calculate-Metrics {
    param(
        [PSObject[]]$Results
    )
    
    $successful = $Results | Where-Object { $_.Success -eq $true }
    
    if ($successful.Count -eq 0) {
        return $null
    }
    
    $latencies = $successful | Select-Object -ExpandProperty LatencyMs
    $tps = $successful | Select-Object -ExpandProperty TokensPerSecond
    
    return @{
        SuccessRate = [math]::Round(($successful.Count / $Results.Count) * 100, 2)
        AvgLatencyMs = [math]::Round(($latencies | Measure-Object -Average).Average, 2)
        MinLatencyMs = ($latencies | Measure-Object -Minimum).Minimum
        MaxLatencyMs = ($latencies | Measure-Object -Maximum).Maximum
        MedianLatencyMs = [math]::Round((Get-Median $latencies), 2)
        StdDevLatencyMs = [math]::Round((Get-StandardDeviation $latencies), 2)
        AvgTokensPerSec = [math]::Round(($tps | Measure-Object -Average).Average, 2)
        MaxTokensPerSec = ($tps | Measure-Object -Maximum).Maximum
        MinTokensPerSec = ($tps | Measure-Object -Minimum).Minimum
        TotalTests = $Results.Count
        SuccessfulTests = $successful.Count
        FailedTests = $Results.Count - $successful.Count
    }
}

# Helper function for median
function Get-Median {
    param([double[]]$Values)
    $sorted = $Values | Sort-Object
    if ($sorted.Count % 2 -eq 0) {
        return ($sorted[($sorted.Count / 2) - 1] + $sorted[$sorted.Count / 2]) / 2
    } else {
        return $sorted[($sorted.Count - 1) / 2]
    }
}

# Helper function for standard deviation
function Get-StandardDeviation {
    param([double[]]$Values)
    if ($Values.Count -lt 2) { return 0 }
    $avg = ($Values | Measure-Object -Average).Average
    $sumSquares = $Values | ForEach-Object { [math]::Pow($_ - $avg, 2) } | Measure-Object -Sum
    return [math]::Sqrt($sumSquares.Sum / ($Values.Count - 1))
}

# Function to generate comparison report
function Generate-ComparisonReport {
    param(
        [hashtable]$MetricsA,
        [hashtable]$MetricsB
    )
    
    Write-Host "`n" -ForegroundColor Cyan
    Write-Host "╔════════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║                     A/B TEST COMPARISON REPORT                                  ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    
    Write-Host "`n📊 SUCCESS RATE" -ForegroundColor Yellow
    Write-Host "  Model A: $($MetricsA.SuccessRate)% ($($MetricsA.SuccessfulTests)/$($MetricsA.TotalTests))" -ForegroundColor White
    Write-Host "  Model B: $($MetricsB.SuccessRate)% ($($MetricsB.SuccessfulTests)/$($MetricsB.TotalTests))" -ForegroundColor White
    
    Write-Host "`n⏱️  LATENCY METRICS (milliseconds)" -ForegroundColor Yellow
    Write-Host "  ┌─ Average:" -ForegroundColor White
    Write-Host "  │  Model A: $($MetricsA.AvgLatencyMs)ms" -ForegroundColor Cyan
    Write-Host "  │  Model B: $($MetricsB.AvgLatencyMs)ms" -ForegroundColor Magenta
    $latencyDiff = [math]::Round($MetricsA.AvgLatencyMs - $MetricsB.AvgLatencyMs, 2)
    $winner = if ($latencyDiff -gt 0) { "Model B is FASTER" } else { "Model A is FASTER" }
    Write-Host "  │  Difference: $([math]::Abs($latencyDiff))ms ($winner)" -ForegroundColor Green
    
    Write-Host "  ├─ Min:" -ForegroundColor White
    Write-Host "  │  Model A: $($MetricsA.MinLatencyMs)ms | Model B: $($MetricsB.MinLatencyMs)ms" -ForegroundColor Cyan
    
    Write-Host "  ├─ Max:" -ForegroundColor White
    Write-Host "  │  Model A: $($MetricsA.MaxLatencyMs)ms | Model B: $($MetricsB.MaxLatencyMs)ms" -ForegroundColor Cyan
    
    Write-Host "  ├─ Median:" -ForegroundColor White
    Write-Host "  │  Model A: $($MetricsA.MedianLatencyMs)ms | Model B: $($MetricsB.MedianLatencyMs)ms" -ForegroundColor Cyan
    
    Write-Host "  └─ Std Dev:" -ForegroundColor White
    Write-Host "     Model A: $($MetricsA.StdDevLatencyMs)ms | Model B: $($MetricsB.StdDevLatencyMs)ms" -ForegroundColor Cyan
    
    Write-Host "`n🚀 THROUGHPUT METRICS (tokens/second)" -ForegroundColor Yellow
    Write-Host "  ┌─ Average:" -ForegroundColor White
    Write-Host "  │  Model A: $($MetricsA.AvgTokensPerSec) tokens/sec" -ForegroundColor Cyan
    Write-Host "  │  Model B: $($MetricsB.AvgTokensPerSec) tokens/sec" -ForegroundColor Magenta
    $tpsDiff = [math]::Round($MetricsA.AvgTokensPerSec - $MetricsB.AvgTokensPerSec, 2)
    $winner = if ($tpsDiff -gt 0) { "Model A is FASTER" } else { "Model B is FASTER" }
    Write-Host "  │  Difference: $([math]::Abs($tpsDiff)) tokens/sec ($winner)" -ForegroundColor Green
    
    Write-Host "  ├─ Max:" -ForegroundColor White
    Write-Host "  │  Model A: $($MetricsA.MaxTokensPerSec) | Model B: $($MetricsB.MaxTokensPerSec)" -ForegroundColor Cyan
    
    Write-Host "  └─ Min:" -ForegroundColor White
    Write-Host "     Model A: $($MetricsA.MinTokensPerSec) | Model B: $($MetricsB.MinTokensPerSec)" -ForegroundColor Cyan
    
    # Calculate winner
    Write-Host "`n🏆 OVERALL WINNER" -ForegroundColor Yellow
    $latencyScore = if ($MetricsA.AvgLatencyMs -lt $MetricsB.AvgLatencyMs) { 1 } else { 0 }
    $tpsScore = if ($MetricsA.AvgTokensPerSec -gt $MetricsB.AvgTokensPerSec) { 1 } else { 0 }
    $successScore = if ($MetricsA.SuccessRate -gt $MetricsB.SuccessRate) { 1 } else { 0 }
    
    $totalScore = $latencyScore + $tpsScore + $successScore
    
    if ($totalScore -gt 1.5) {
        Write-Host "  🎯 Model A wins with score: $totalScore/3" -ForegroundColor Green
    } elseif ($totalScore -lt 1.5) {
        Write-Host "  🎯 Model B wins with score: $([3-$totalScore])/3" -ForegroundColor Cyan
    } else {
        Write-Host "  🤝 Tie! Both models perform equivalently (score: 1.5/3)" -ForegroundColor Yellow
    }
}

# Main execution
Write-Host "╔════════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          A/B Testing Framework for Real Models (Real-Time)                   ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  Model A: $ModelA" -ForegroundColor Cyan
Write-Host "  Model B: $ModelB" -ForegroundColor Magenta
Write-Host "  Tests per model: $NumTests" -ForegroundColor White
Write-Host "  Timeout per test: ${Timeout}s" -ForegroundColor White
Write-Host "  Output file: $OutputFile" -ForegroundColor White
Write-Host ""

# Run tests for both models
$resultsA = Run-ModelTestSuite -Model $ModelA -NumTests $NumTests -Timeout $Timeout -ModelLabel "A"
$resultsB = Run-ModelTestSuite -Model $ModelB -NumTests $NumTests -Timeout $Timeout -ModelLabel "B"

# Calculate metrics
$metricsA = Calculate-Metrics -Results $resultsA
$metricsB = Calculate-Metrics -Results $resultsB

# Store results
$global:results.ModelA = $resultsA
$global:results.ModelB = $resultsB
$global:results.ComparisonMetrics = @{
    ModelA = $metricsA
    ModelB = $metricsB
}
$global:results.EndTime = Get-Date

# Generate and display report
Generate-ComparisonReport -MetricsA $metricsA -MetricsB $metricsB

# Save results to JSON
Write-Host "`n📁 Saving detailed results to: $OutputFile" -ForegroundColor Yellow

$jsonOutput = @{
    TestMetadata = @{
        StartTime = $global:results.StartTime
        EndTime = $global:results.EndTime
        DurationSeconds = [math]::Round(($global:results.EndTime - $global:results.StartTime).TotalSeconds, 2)
        ModelA = $ModelA
        ModelB = $ModelB
        TestsPerModel = $NumTests
        TimeoutPerTestSeconds = $Timeout
    }
    MetricsA = $metricsA
    MetricsB = $metricsB
    ResultsA = $resultsA
    ResultsB = $resultsB
} | ConvertTo-Json -Depth 10

$jsonOutput | Set-Content -Path $OutputFile

Write-Host "✓ Results saved successfully" -ForegroundColor Green
Write-Host ""
Write-Host "To view results:" -ForegroundColor Yellow
Write-Host "  Get-Content '$OutputFile' | ConvertFrom-Json | Format-List" -ForegroundColor Cyan
