# Phase 2C GPU Performance Tuning — Kernel A/B Sweep Harness
# Purpose: Benchmark all combinations of kernel families and quantizations
# to identify optimal pairing for production deployment
#
# Usage:
#   ./run_kernel_ab_sweep.ps1 -Model "tinyllama_fresh.gguf" -OutputDir "D:\bench_phase2c"
#   ./run_kernel_ab_sweep.ps1 -ForceTg128 -Runs 5  # Just tg128 variant, 5 iterations

param(
    [string] $Model = "F:\models\Qwen2.5-Coder-32B-Instruct-Q4_K_M.gguf",
    [string] $OutputDir = "D:\bench_phase2c",
    [int] $Runs = 3,
    [switch] $ForceTg64,
    [switch] $ForceTg128,
    [switch] $ForceFused,
    [switch] $ForceFallback,
    [switch] $QuickTest
)

#region Configuration
$RawrXDExe = "D:\rawrxd\build_pipeline\bin\rawrxd.exe"
$TestPrompt = "Say exactly: ready"
$TokenTarget = 16

# Kernel variants to test
$KernelVariants = @(
    @{ Name = "tg64_fused";     TileSize = "64";  Fused = $true;  },
    @{ Name = "tg128_fused";    TileSize = "128"; Fused = $true;  },
    @{ Name = "tg64_fallback";  TileSize = "64";  Fused = $false; },
    @{ Name = "tg128_fallback"; TileSize = "128"; Fused = $false; }
)

# Quantizations to test (if model supports)
$Quantizations = @("Q2_K", "Q4_K", "Q5_K", "Q8_1")

if ($QuickTest) {
    $Runs = 1
    $KernelVariants = @($KernelVariants[0])  # Just tg64_fused
}

#endregion

#region Utilities
function Get-TimestampMark {
    return (Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff")
}

function Write-BenchLog {
    param([string] $Message)
    Write-Host "[$((Get-TimestampMark))] $Message" -ForegroundColor Cyan
}

function Invoke-BenchmarkRun {
    param(
        [hashtable] $Config,
        [string] $Model,
        [int] $RunNumber
    )
    
    $env:GGML_VK_TILE_SIZE = $Config.TileSize
    $env:GGML_VK_FORCE_KERNEL = if ($Config.Fused) { "fused" } else { "fallback" }
    
    Write-BenchLog "Starting run $RunNumber : kernel=$($Config.Name), tile=$($Config.TileSize), fused=$($Config.Fused)"
    
    $StopWatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        $Output = & $RawrXDExe `
            --model "$Model" `
            --test-max-tokens $TokenTarget `
            --prompt "$TestPrompt" `
            2>&1
        
        $StopWatch.Stop()
        
        $RawOutput = $Output | Out-String
        
        # Parse trace JSON if emitted
        $JsonMatch = [System.Text.RegularExpressions.Regex]::Match($RawOutput, '\{.*"token_count".*?\}', [System.Text.RegularExpressions.RegexOptions]::Singleline)
        $TraceJson = if ($JsonMatch.Success) { $JsonMatch.Value } else { $null }
        
        return @{
            Success         = $true
            KernelName      = $Config.Name
            TileSize        = $Config.TileSize
            Fused           = $Config.Fused
            RunNumber       = $RunNumber
            ElapsedMs       = $StopWatch.ElapsedMilliseconds
            TokenCount      = $TokenTarget
            TokPerSec       = [math]::Round($TokenTarget * 1000.0 / $StopWatch.ElapsedMilliseconds, 2)
            RawOutput       = $RawOutput
            TraceJson       = $TraceJson
            Timestamp       = (Get-TimestampMark)
        }
    }
    catch {
        Write-BenchLog "ERROR in run $RunNumber : $($_.Exception.Message)"
        return @{
            Success    = $false
            KernelName = $Config.Name
            RunNumber  = $RunNumber
            Error      = $_.Exception.Message
            Timestamp  = (Get-TimestampMark)
        }
    }
}

#endregion

#region Main Execution
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

Write-BenchLog "=== Phase 2C GPU Performance Tuning — Kernel A/B Sweep ==="
Write-BenchLog "Model: $Model"
Write-BenchLog "Token target: $TokenTarget"
Write-BenchLog "Runs per variant: $Runs"
Write-BenchLog "Output directory: $OutputDir"
Write-BenchLog ""

$AllResults = @()
$FilteredVariants = $KernelVariants

if ($ForceTg64) { $FilteredVariants = $FilteredVariants | Where-Object { $_.TileSize -eq "64" } }
if ($ForceTg128) { $FilteredVariants = $FilteredVariants | Where-Object { $_.TileSize -eq "128" } }
if ($ForceFused) { $FilteredVariants = $FilteredVariants | Where-Object { $_.Fused -eq $true } }
if ($ForceFallback) { $FilteredVariants = $FilteredVariants | Where-Object { $_.Fused -eq $false } }

Write-BenchLog "Testing $($FilteredVariants.Count) kernel variants, $Runs runs each = $($FilteredVariants.Count * $Runs) total runs"
Write-BenchLog ""

$StartTime = (Get-Date)

foreach ($Variant in $FilteredVariants) {
    Write-BenchLog "--- KERNEL VARIANT: $($Variant.Name) ---"
    
    $VariantRuns = @()
    for ($i = 1; $i -le $Runs; $i++) {
        $Result = Invoke-BenchmarkRun -Config $Variant -Model $Model -RunNumber $i
        
        if ($Result.Success) {
            Write-BenchLog "  Run $i : $($Result.TokPerSec) tok/s (${$($Result.ElapsedMs)}ms)"
        } else {
            Write-BenchLog "  Run $i : FAILED — $($Result.Error)"
        }
        
        $VariantRuns += $Result
        $AllResults += $Result
    }
    
    # Calculate statistics for this variant
    $SuccessRuns = $VariantRuns | Where-Object { $_.Success }
    if ($SuccessRuns.Count -gt 0) {
        $MeanTokS = ($SuccessRuns | Measure-Object -Property TokPerSec -Average).Average
        $StdDev = if ($SuccessRuns.Count -gt 1) {
            [math]::Sqrt(($SuccessRuns | Measure-Object -Property TokPerSec -StandardDeviation).StandardDeviation)
        } else { 0 }
        
        Write-BenchLog "  Summary: $MeanTokS ± $StdDev tok/s ($($SuccessRuns.Count)/$Runs successful)"
    } else {
        Write-BenchLog "  Summary: ALL RUNS FAILED"
    }
    
    Write-BenchLog ""
}

$EndTime = (Get-Date)
$TotalDuration = ($EndTime - $StartTime).TotalSeconds

Write-BenchLog "=== SUMMARY ==="
Write-BenchLog "Total duration: $([math]::Round($TotalDuration, 1)) seconds"
Write-BenchLog "Total runs: $($AllResults.Count)"
Write-BenchLog "Successful: $($AllResults | Where-Object { $_.Success } | Measure-Object | Select-Object -ExpandProperty Count)"

# Generate summary CSV
$CsvOutput = $AllResults | Where-Object { $_.Success } | ForEach-Object {
    "$($_.KernelName),$($_.TileSize),$($_.Fused),$($_.RunNumber),$($_.ElapsedMs),$($_.TokPerSec)"
}

$CsvPath = Join-Path $OutputDir "phase2c_kernel_ab_sweep_$(Get-Date -Format 'yyyyMMdd_HHmmss').csv"
"kernel_name,tile_size,fused,run_number,elapsed_ms,tok_per_sec" | Out-File -FilePath $CsvPath -Encoding UTF8
$CsvOutput | Out-File -FilePath $CsvPath -Encoding UTF8 -Append

Write-BenchLog "Results saved to: $CsvPath"

# Generate JSON summary
$JsonSummary = @{
    run_date         = (Get-Date -Format "o")
    phase            = "2C"
    objective        = "GPU kernel A/B sweep for performance baseline"
    model            = $Model
    token_target     = $TokenTarget
    prompt           = $TestPrompt
    total_runs       = $AllResults.Count
    successful_runs  = ($AllResults | Where-Object { $_.Success } | Measure-Object).Count
    total_duration_s = [math]::Round($TotalDuration, 2)
    results          = ($AllResults | Where-Object { $_.Success })
}

$JsonPath = Join-Path $OutputDir "phase2c_kernel_ab_sweep_summary_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$JsonSummary | ConvertTo-Json -Depth 10 | Out-File -FilePath $JsonPath -Encoding UTF8

Write-BenchLog "Summary JSON saved to: $JsonPath"
Write-BenchLog ""
Write-BenchLog "✅ Phase 2C kernel A/B sweep complete!"

#endregion
