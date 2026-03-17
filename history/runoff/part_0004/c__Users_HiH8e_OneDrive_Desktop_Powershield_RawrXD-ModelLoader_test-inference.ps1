# Test both scalar and AVX2 builds with same 10-token prompt
$prompt = "The capital of France is"
$model_path = "D:\OllamaModels\BigDaddyG-Q2_K-CHEETAH.gguf"

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "  INFERENCE BENCHMARK: SCALAR vs AVX2" -ForegroundColor White
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

# Test scalar build
Write-Host "🔹 Scalar build:" -ForegroundColor Yellow
$scalar_start = Get-Date
$scalar_output = & .\build-scalar\bin\Release\RawrXD-Server.exe --model $model_path --prompt $prompt --max-tokens 50 2>&1
$scalar_time = ((Get-Date) - $scalar_start).TotalMilliseconds

if ($scalar_output -match "(\d+\.?\d*)\s*tok/s") {
    $scalar_tokps = [float]$matches[1]
    Write-Host "  Time: $([math]::Round($scalar_time, 0)) ms" -ForegroundColor Gray
    Write-Host "  Throughput: $([math]::Round($scalar_tokps, 1)) tok/s`n" -ForegroundColor Gray
} else {
    Write-Host "  ERROR: Could not parse output`n" -ForegroundColor Red
    $scalar_tokps = 0
}

# Test AVX2 build
Write-Host "🔹 AVX2 build:" -ForegroundColor Yellow
$avx2_start = Get-Date
$avx2_output = & .\build-avx2\bin\Release\RawrXD-Server.exe --model $model_path --prompt $prompt --max-tokens 50 2>&1
$avx2_time = ((Get-Date) - $avx2_start).TotalMilliseconds

if ($avx2_output -match "(\d+\.?\d*)\s*tok/s") {
    $avx2_tokps = [float]$matches[1]
    Write-Host "  Time: $([math]::Round($avx2_time, 0)) ms" -ForegroundColor Gray
    Write-Host "  Throughput: $([math]::Round($avx2_tokps, 1)) tok/s`n" -ForegroundColor Gray
} else {
    Write-Host "  ERROR: Could not parse output`n" -ForegroundColor Red
    $avx2_tokps = 0
}

# Calculate speedup
if ($scalar_tokps -gt 0 -and $avx2_tokps -gt 0) {
    $speedup = $avx2_tokps / $scalar_tokps
    
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "📊 RESULT:" -ForegroundColor White
    Write-Host "  Speedup: $([math]::Round($speedup, 2))×" -ForegroundColor Magenta
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan
    
    if ($speedup -ge 1.8) {
        Write-Host "✅ TARGET ACHIEVED: $([math]::Round($speedup, 2))× ≥ 1.8×" -ForegroundColor Green
        Write-Host "🎯 Phase 1 COMPLETE - Ready for Q4_0!" -ForegroundColor Green
    } else {
        Write-Host "⚠️  Below target: $([math]::Round($speedup, 2))× < 1.8×" -ForegroundColor Yellow
    }
}
