# Real Kernel Sweep — Execute and Capture Results
param(
    [string]$ModelPath = "D:\phi3mini.gguf",
    [int]$MaxTokens = 8,
    [string]$CliPath = "D:\rawrxd\build_pipeline\bin\rawrxd.exe"
)

$Variants = @("baseline", "q4k_q8_1_u32", "q4_0_u32")
$results = @()

foreach ($variant in $Variants) {
    Write-Host ""
    Write-Host "=== Variant: $variant ===" -ForegroundColor Cyan
    
    if ($variant -eq "baseline") {
        Remove-Item Env:\RAWRXD_VULKAN_MATMUL_KERNEL -ErrorAction SilentlyContinue
        Remove-Item Env:\RAWRXD_VULKAN_MATMUL_SPV -ErrorAction SilentlyContinue
    } else {
        $env:RAWRXD_VULKAN_MATMUL_KERNEL = $variant
        $env:RAWRXD_VULKAN_MATMUL_SPV = $variant
    }
    
    Write-Host "  Warmup..." -ForegroundColor Gray
    $null = & $CliPath run $ModelPath --prompt "Hi" --max-tokens 4 --no-display 2>&1
    
    Write-Host "  Measuring $MaxTokens tokens..." -ForegroundColor Gray
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $output = & $CliPath run $ModelPath --prompt "The quick brown fox jumps over the lazy dog. The meaning of life is" --max-tokens $MaxTokens --no-display 2>&1
    $sw.Stop()
    
    $totalMs = $sw.ElapsedMilliseconds
    
    $phaseMatches = [regex]::Matches($output, '\[PHASE\] per_token\s+([\d.]+) ms')
    $perTokenMs = if ($phaseMatches.Count -gt 0) { 
        $sum = 0
        foreach ($m in $phaseMatches) { $sum += [double]$m.Groups[1].Value }
        [math]::Round($sum / $phaseMatches.Count, 1)
    } else { 
        [math]::Round($totalMs / $MaxTokens, 1)
    }
    
    $firstTokenMatch = [regex]::Match($output, '\[PHASE\] tokens=1 total_acc=([\d.]+) ms')
    $firstTokenMs = if ($firstTokenMatch.Success) { [double]$firstTokenMatch.Groups[1].Value } else { 0 }
    
    $tokPerSec = [math]::Round(1000 / $perTokenMs, 1)
    
    $result = [PSCustomObject]@{
        Variant = $variant
        TotalMs = $totalMs
        FirstTokenMs = [math]::Round($firstTokenMs, 1)
        PerTokenMs = $perTokenMs
        TokPerSec = $tokPerSec
        Tokens = $MaxTokens
        PhaseSamples = $phaseMatches.Count
    }
    $results += $result
    
    $color = if ($tokPerSec -ge 10) { "Green" } elseif ($tokPerSec -ge 7) { "Yellow" } else { "Red" }
    Write-Host "  Result: $perTokenMs ms/token = $tokPerSec tok/s" -ForegroundColor $color
    
    Start-Sleep -Milliseconds 500
}

Write-Host ""
Write-Host "=== SUMMARY ===" -ForegroundColor Cyan
$ranked = $results | Sort-Object TokPerSec -Descending
$ranked | Format-Table -AutoSize

$best = $ranked[0]
Write-Host "Winner: $($best.Variant) at $($best.TokPerSec) tok/s" -ForegroundColor Green

# Export JSON for reporting
$results | ConvertTo-Json -Depth 3 | Out-File "D:\rawrxd\docs\kernel_sweep_results.json" -Encoding utf8
Write-Host "Results saved to: D:\rawrxd\docs\kernel_sweep_results.json" -ForegroundColor Gray
