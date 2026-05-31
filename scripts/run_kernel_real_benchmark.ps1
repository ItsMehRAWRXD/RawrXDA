# Real Inference Benchmark — Phase D Kernel Sweep
# Measures actual token generation latency, not HTTP throughput

param(
    [string]$ModelPath = "D:\phi3mini.gguf",
    [int]$MaxTokens = 16,
    [string]$CliPath = "D:\rawrxd\build_pipeline\bin\rawrxd.exe",
    [string[]]$Variants = @("baseline", "q4k_q8_1_u32", "q4_0_u32"),
    [string]$OutputFile = "D:\rawrxd\docs\BENCHMARK_KERNEL_VARIANTS.md"
)

$results = @()

foreach ($variant in $Variants) {
    Write-Host "`n=== Variant: $variant ===" -ForegroundColor Cyan
    
    # Set env vars
    if ($variant -eq "baseline") {
        Remove-Item Env:\RAWRXD_VULKAN_MATMUL_KERNEL -ErrorAction SilentlyContinue
        Remove-Item Env:\RAWRXD_VULKAN_MATMUL_SPV -ErrorAction SilentlyContinue
    } else {
        $env:RAWRXD_VULKAN_MATMUL_KERNEL = $variant
        $env:RAWRXD_VULKAN_MATMUL_SPV = $variant
    }
    
    # Warmup (4 tokens, discarded)
    Write-Host "  Warmup..." -ForegroundColor Gray
    $warmupOut = & $CliPath run $ModelPath --prompt "Hi" --max-tokens 4 --no-display 2>&1
    
    # Actual measurement
    Write-Host "  Measuring $MaxTokens tokens..." -ForegroundColor Gray
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $output = & $CliPath run $ModelPath `
        --prompt "The quick brown fox jumps over the lazy dog. The meaning of life is" `
        --max-tokens $MaxTokens --no-display 2>&1
    $sw.Stop()
    
    $totalMs = $sw.ElapsedMilliseconds
    
    # Extract per-token latency from [PHASE] per_token lines
    $phaseMatches = [regex]::Matches($output, '\[PHASE\] per_token\s+([\d.]+) ms')
    $perTokenMs = if ($phaseMatches.Count -gt 0) { 
        # Average all per_token measurements
        $sum = 0
        foreach ($m in $phaseMatches) { $sum += [double]$m.Groups[1].Value }
        [math]::Round($sum / $phaseMatches.Count, 1)
    } else { 
        [math]::Round($totalMs / $MaxTokens, 1)
    }
    
    $tokPerSec = [math]::Round(1000 / $perTokenMs, 1)
    
    # Extract first token latency
    $firstTokenMatch = [regex]::Match($output, '\[PHASE\] tokens=1 total_acc=([\d.]+) ms')
    $firstTokenMs = if ($firstTokenMatch.Success) { [double]$firstTokenMatch.Groups[1].Value } else { 0 }
    
    $result = [PSCustomObject]@{
        Variant = $variant
        TotalMs = $totalMs
        FirstTokenMs = [math]::Round($firstTokenMs, 1)
        PerTokenMs = $perTokenMs
        TokPerSec = $tokPerSec
        Tokens = $MaxTokens
        PhaseSamples = $phaseMatches.Count
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
    
    $results += $result
    
    $color = if ($tokPerSec -ge 10) { "Green" } elseif ($tokPerSec -ge 7) { "Yellow" } else { "Red" }
    Write-Host "  Result: $perTokenMs ms/token = $tokPerSec tok/s (first: ${firstTokenMs}ms)" -ForegroundColor $color
    
    Start-Sleep -Milliseconds 500
}

# Rank by throughput
$ranked = $results | Sort-Object TokPerSec -Descending

# Generate markdown report
$report = @"
# GPU Kernel Variant Benchmark — Real Inference

**Date**: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")  
**Model**: phi3mini.gguf (3.8B, Q2_K)  
**Device**: AMD Radeon RX 7800 XT  
**Tokens per run**: $MaxTokens  
**Measurement**: Actual per-token latency from `[PHASE] per_token` log output  

## Results (Ranked by Throughput)

| Rank | Variant | ms/token | tok/s | First Token (ms) | Total (ms) |
|------|---------|----------|-------|------------------|------------|
"@

$rank = 1
foreach ($r in $ranked) {
    $bestMarker = if ($rank -eq 1) { " 🏆" } else { "" }
    $report += "| $rank | $($r.Variant) | $($r.PerTokenMs) | $($r.TokPerSec) | $($r.FirstTokenMs) | $($r.TotalMs) |$bestMarker`n"
    $rank++
}

$baseline = $ranked | Where-Object { $_.Variant -eq "baseline" }
$best = $ranked[0]

if ($baseline -and $best.Variant -ne "baseline") {
    $improvement = [math]::Round((($best.TokPerSec - $baseline.TokPerSec) / $baseline.TokPerSec) * 100, 1)
    $report += @"

## Analysis

- **Best variant**: $($best.Variant) at $($best.TokPerSec) tok/s
- **Baseline**: $($baseline.TokPerSec) tok/s
- **Improvement**: +$improvement% over baseline
- **First token latency**: $($best.FirstTokenMs) ms (best variant)
- **Recommendation**: Set `RAWRXD_VULKAN_MATMUL_KERNEL=$($best.Variant)` as default
"@
} else {
    $report += @"

## Analysis

- **Best variant**: $($best.Variant) at $($best.TokPerSec) tok/s
- **Baseline**: $($baseline.TokPerSec) tok/s (same)
- **Recommendation**: Baseline is optimal; no kernel override needed
"@
}

$report += @"

## Raw Data

```json
$($results | ConvertTo-Json -Depth 3)
```

## Methodology

1. **Warmup**: 4 tokens (discarded) to stabilize GPU state
2. **Measurement**: $MaxTokens tokens with `[System.Diagnostics.Stopwatch]`
3. **Per-token latency**: Extracted from `[PHASE] per_token` log lines (averaged across all tokens)
4. **First token**: From `[PHASE] tokens=1 total_acc=...` log line
5. **Cooldown**: 500ms between variants
6. **Environment**: `RAWRXD_VULKAN_MATMUL_KERNEL`, `RAWRXD_VULKAN_MATMUL_SPV`

## Comparison to llama.cpp Baseline

| Framework | tok/s | Notes |
|-----------|-------|-------|
| llama.cpp (Vulkan, Q2_K) | ~8-10 | Reference implementation |
| RawrXD (baseline) | $($baseline.TokPerSec) | Current |
| RawrXD (best variant) | $($best.TokPerSec) | With kernel tuning |

---
*Generated by scripts/run_kernel_real_benchmark.ps1*
"@

$report | Out-File $OutputFile -Encoding utf8
Write-Host "`n✅ Report: $OutputFile" -ForegroundColor Green
Write-Host "🏆 Winner: $($best.Variant) at $($best.TokPerSec) tok/s" -ForegroundColor Green

return $ranked
