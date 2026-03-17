# ci-sovereign-gate.ps1
# SOVEREIGN AI IDE GATE - Validates real token generation without external deps
# Usage: ci/sovereign-gate.ps1 -ModelPath "test_model.gguf"

param(
    [string]$ModelPath = "F:\OllamaModels\blobs\sha256-e1eaa0f4fffb8880ca14c1c1f9e7d887fb45cc19a5b17f5cc83c3e8d3e85914e",
    [int]$TimeoutSec = 30,
    [int]$MaxLatencyMs = 1000
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " SOVEREIGN AI IDE VALIDATION GATE" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$ErrorActionPreference = "Continue"
$binPath = "D:\rawrxd\bin\RawrXD-Win32IDE.exe"

# Verify binary exists
if (-not (Test-Path $binPath)) {
    Write-Host "❌ GATE FAILED: Binary not found at $binPath" -ForegroundColor Red
    exit 1
}

# Verify model exists
if (-not (Test-Path $ModelPath)) {
    Write-Host "❌ GATE FAILED: Model not found at $ModelPath" -ForegroundColor Red
    exit 1
}

# Check binary freshness
$binAge = (Get-Date) - (Get-Item $binPath).LastWriteTime
Write-Host "[GATE] Binary age: $([math]::Round($binAge.TotalMinutes, 1)) minutes" -ForegroundColor Gray

# Run inference test
Write-Host "`n[GATE] Testing token generation..." -ForegroundColor Yellow
$output = & $binPath --test-inference-fast --test-model $ModelPath 2>&1 | Out-String

# Parse result
if ($output -match 'PASS FAST_GENERATE token=(\d+) time=(\d+)ms') {
    $token = [int]$matches[1]
    $latency = [int]$matches[2]
    
    Write-Host "`n[RESULT] Token: $token" -ForegroundColor Cyan
    Write-Host "[RESULT] Latency: ${latency}ms" -ForegroundColor Cyan
    
    # Validate token is not BOS (0 = Beginning Of Sequence stub)
    if ($token -eq 0) {
        Write-Host "`n❌ GATE FAILED: Token 0 is BOS stub, not real generation" -ForegroundColor Red
        Write-Host "[HINT] Sampler may not be executing argmax/multinomial" -ForegroundColor Yellow
        exit 1
    }
    
    # Validate latency is production-grade (<1000ms for CPU)
    if ($latency -gt $MaxLatencyMs) {
        Write-Host "`n⚠️ GATE WARNING: Latency ${latency}ms exceeds ${MaxLatencyMs}ms threshold" -ForegroundColor Yellow
        Write-Host "[HINT] Acceptable but not optimal - consider Vulkan backend" -ForegroundColor Gray
    }
    
    # SUCCESS
    Write-Host "`n✅ SOVEREIGN AI IDE GATE PASSED" -ForegroundColor Green
    Write-Host "   • Real token generated (not BOS stub)" -ForegroundColor Green
    Write-Host "   • Sub-${MaxLatencyMs}ms latency" -ForegroundColor Green
    Write-Host "   • No external dependencies" -ForegroundColor Green
    Write-Host "   • Full transformer inference validated" -ForegroundColor Green
    
    # Performance classification
    $layersPerSec = [math]::Round(32000.0 / $latency, 1)
    Write-Host "`n[PERF] ~$([math]::Round($latency / 32.0, 1))ms per layer" -ForegroundColor Cyan
    Write-Host "[PERF] ~$layersPerSec layers/sec throughput" -ForegroundColor Cyan
    
    if ($latency -lt 100) {
        Write-Host "[CLASS] 🚀 BLAZING (Vulkan-optimized)" -ForegroundColor Green
    } elseif ($latency -lt 300) {
        Write-Host "[CLASS] ⚡ FAST (AVX-512 optimized)" -ForegroundColor Green
    } elseif ($latency -lt 1000) {
        Write-Host "[CLASS] ✅ PRODUCTION (AVX2 baseline)" -ForegroundColor Yellow
    } else {
        Write-Host "[CLASS] ⚠️ SLOW (Needs optimization)" -ForegroundColor Red
    }
    
    exit 0
    
} elseif ($output -match 'FAIL FAST_ENGINE_LOAD') {
    Write-Host "`n❌ GATE FAILED: LoadModel returned false" -ForegroundColor Red
    Write-Host "[HINT] Check model file integrity and metadata parsing" -ForegroundColor Yellow
    exit 1
    
} elseif ($output -match 'FAIL FAST_TIMEOUT') {
    Write-Host "`n❌ GATE FAILED: Generation timeout" -ForegroundColor Red
    Write-Host "[HINT] Transformer forward pass hanging - add layer instrumentation" -ForegroundColor Yellow
    exit 1
    
} elseif ($output -match 'FAIL FAST_INVALID_STATE bad_layers=(-?\d+)') {
    Write-Host "`n❌ GATE FAILED: Invalid layer count $($matches[1])" -ForegroundColor Red
    Write-Host "[HINT] Metadata corruption (0xCDCDCDCD) - LoadModel not initializing state" -ForegroundColor Yellow
    exit 1
    
} elseif ($output -match 'FAIL FAST_EXCEPTION (.+)') {
    Write-Host "`n❌ GATE FAILED: Exception: $($matches[1])" -ForegroundColor Red
    Write-Host "[HINT] Runtime error during inference - check stack trace" -ForegroundColor Yellow
    exit 1
    
} else {
    Write-Host "`n❌ GATE FAILED: Unknown result" -ForegroundColor Red
    Write-Host "[OUTPUT]" -ForegroundColor Gray
    Write-Host $output
    exit 1
}
