# PowerShell benchmark script for GQA+RoPE performance testing
# Usage: .\scripts\bench_gqa_rope.ps1

$exe = "build-msvc\bin-msvc\Release\RawrXD-QtShell.exe"

if (-not (Test-Path $exe)) {
    Write-Host "Error: Executable not found at $exe" -ForegroundColor Red
    exit 1
}

Write-Host "=== GQA + RoPE Performance Benchmark ===" -ForegroundColor Cyan
Write-Host "Model: Multi-head attention (32 heads, 4 KV heads, 128 head_dim)" -ForegroundColor Green
Write-Host "Generating 128 tokens (scalar path)..." -ForegroundColor Yellow
Write-Host ""

$elapsed = Measure-Command {
    & $exe --gen 128 2>&1 | Out-Null
}

$tokPerSec = 128 / $elapsed.TotalSeconds

Write-Host "Results:" -ForegroundColor Cyan
Write-Host "  Total time: $($elapsed.TotalSeconds.ToString('F2')) seconds"
Write-Host "  Tokens/sec: $($tokPerSec.ToString('F1')) tok/s"
Write-Host ""
Write-Host "Binary size:" -ForegroundColor Cyan
$size = (Get-Item $exe).Length
Write-Host "  $([math]::Round($size/1KB, 1)) KB ($size bytes)"
Write-Host ""
Write-Host "KV-cache memory (per layer):" -ForegroundColor Cyan
$kvBytes = 128 * 4 * 128 * 4 * 2  # maxTokens * nKVHead * headDim * sizeof(float) * 2 (K+V)
Write-Host "  $([math]::Round($kvBytes/1KB, 1)) KB (8x smaller than single-head)"
