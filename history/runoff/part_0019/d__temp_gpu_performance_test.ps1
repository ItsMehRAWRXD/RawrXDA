#!/usr/bin/env pwsh
<#
.SYNOPSIS
GPU Performance Test for RawrXD with Vulkan Backend

This script:
1. Confirms GPU is detected (Vulkan)
2. Reports system and GPU specs
3. Measures token generation throughput on a real GGUF model
#>

$ErrorActionPreference = "Stop"

# Paths
$BUILD_DIR = "D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build"
$BIN_DIR = "$BUILD_DIR\bin\Release"
$MODEL_DIR = "D:\OllamaModels"
$MODEL_FILE = "$MODEL_DIR\BigDaddyG-NO-REFUSE-Q4_K_M.gguf"

Write-Host "`n" -ForegroundColor Cyan
Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   RawrXD GPU Performance Test - Vulkan Backend         ║" -ForegroundColor Cyan
Write-Host "║   Testing Token-Per-Second Throughput on AMD GPU       ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host "`n"

# Verify model exists
if (-not (Test-Path $MODEL_FILE)) {
    Write-Host "ERROR: Model not found at $MODEL_FILE" -ForegroundColor Red
    Write-Host "Available models:" -ForegroundColor Yellow
    Get-ChildItem $MODEL_DIR -Filter "*.gguf" | ForEach-Object { "  - $($_.Name) ($([math]::Round($_.Length/1GB, 2)) GB)" }
    exit 1
}

Write-Host "✓ Model found: $(Split-Path $MODEL_FILE -Leaf)" -ForegroundColor Green
Write-Host "  Size: $([math]::Round((Get-Item $MODEL_FILE).Length/1GB, 2)) GB" -ForegroundColor Green
Write-Host "`n"

# Step 1: Run simple GPU test to confirm Vulkan is working
Write-Host "STEP 1: Verifying GPU Detection (Vulkan Backend)" -ForegroundColor Cyan
Write-Host "─────────────────────────────────────────────────" -ForegroundColor Cyan

if (-not (Test-Path "$BIN_DIR\simple_gpu_test.exe")) {
    Write-Host "ERROR: simple_gpu_test.exe not found. Run cmake build first." -ForegroundColor Red
    exit 1
}

$gpu_test_output = & "$BIN_DIR\simple_gpu_test.exe" 2>&1
Write-Host $gpu_test_output
Write-Host "`n"

# Check if GPU was detected
if ($gpu_test_output -match "GPU Available: YES") {
    Write-Host "✓ GPU DETECTED AND INITIALIZED" -ForegroundColor Green
} else {
    Write-Host "✗ GPU NOT DETECTED - GPU backend failed to initialize" -ForegroundColor Red
    exit 1
}

# Extract GPU details
if ($gpu_test_output -match "Device Name: (.+)") {
    $gpu_name = $Matches[1]
    Write-Host "✓ GPU Device: $gpu_name" -ForegroundColor Green
}

if ($gpu_test_output -match "Total VRAM: ([\d\.]+)\s+GB") {
    $vram = $Matches[1]
    Write-Host "✓ VRAM Available: ${vram} GB" -ForegroundColor Green
}

Write-Host "`n"

# Step 2: Run production feature test
Write-Host "STEP 2: Running Production Feature Tests" -ForegroundColor Cyan
Write-Host "─────────────────────────────────────────" -ForegroundColor Cyan

if (-not (Test-Path "$BIN_DIR\production_feature_test.exe")) {
    Write-Host "WARNING: production_feature_test.exe not found. Building..." -ForegroundColor Yellow
    Push-Location $BUILD_DIR
    cmake --build . --config Release --target production_feature_test -j 8 | Out-Null
    Pop-Location
}

$perf_test_output = & "$BIN_DIR\production_feature_test.exe" 2>&1
Write-Host $perf_test_output
Write-Host "`n"

# Parse and display summary
Write-Host "PERFORMANCE SUMMARY" -ForegroundColor Cyan
Write-Host "─────────────────────" -ForegroundColor Cyan

if ($perf_test_output -match "GPU Detection: (.+)") {
    $gpu_status = $Matches[1]
    Write-Host "GPU Status: $gpu_status" -ForegroundColor $(if ($gpu_status -match "ENABLED") { "Green" } else { "Red" })
}

if ($perf_test_output -match "Avg Latency: ""([\d\.]+)"" ms") {
    $latency = [double]$Matches[1]
    Write-Host "Average Latency: ${latency}ms" -ForegroundColor Yellow
    
    # Expected GPU performance (rough estimate)
    $gpu_throughput = 1000 / $latency
    Write-Host "Estimated Throughput: ~$([math]::Round($gpu_throughput, 1)) tokens/sec" -ForegroundColor Cyan
}

if ($perf_test_output -match "Success Rate: ""([\d\.]+)"" %") {
    $success_rate = $Matches[1]
    Write-Host "Success Rate: ${success_rate}%" -ForegroundColor $(if ($success_rate -ge 95) { "Green" } else { "Yellow" })
}

Write-Host "`n"

# Step 3: Comparison with baseline
Write-Host "PERFORMANCE COMPARISON" -ForegroundColor Cyan
Write-Host "──────────────────────" -ForegroundColor Cyan

$cpu_baseline_latency = 197  # ms from previous CPU test
$cpu_baseline_throughput = 30.08  # tokens/sec

if ($perf_test_output -match "Avg Latency: ""([\d\.]+)"" ms") {
    $gpu_latency = [double]$Matches[1]
    $speedup = $cpu_baseline_latency / $gpu_latency
    Write-Host "CPU Baseline Latency: ${cpu_baseline_latency}ms ($cpu_baseline_throughput tok/s)" -ForegroundColor Gray
    Write-Host "GPU Measured Latency: ${gpu_latency}ms" -ForegroundColor Cyan
    Write-Host "Speedup Factor: $([math]::Round($speedup, 2))x" -ForegroundColor $(if ($speedup -gt 1) { "Green" } else { "Yellow" })
}

Write-Host "`n"
Write-Host "✓ GPU PERFORMANCE TEST COMPLETE" -ForegroundColor Green
Write-Host "`n"
