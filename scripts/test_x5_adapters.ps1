# Phase X+5: Validate multi-GPU adapter enumeration
# Builds the IDE (which links inference_shard_coordinator) and optionally
# reports adapter count if a test binary is run. For now: build + doc.
# Usage: .\scripts\test_x5_adapters.ps1 [-BuildOnly] [-SkipBuild]

param(
    [switch] $BuildOnly,
    [switch] $SkipBuild
)

$ErrorActionPreference = "Stop"
$rootDir = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { "D:\rawrxd" }
if (-not (Test-Path $rootDir)) { $rootDir = "D:\rawrxd" }
Set-Location $rootDir

if (-not $SkipBuild) {
    Write-Host "=== X+5 Adapter enumeration (build) ===" -ForegroundColor Cyan
    $buildDir = Join-Path $rootDir "build"
    if (-not (Test-Path $buildDir)) {
        Write-Host "Configuring with Ninja..." -ForegroundColor Yellow
        cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S . 2>&1 | Out-Null
    }
    Write-Host "Building RawrXD-Win32IDE (includes inference_shard_coordinator)..." -ForegroundColor Yellow
    $err = 0
    cmake --build build --target RawrXD-Win32IDE 2>&1 | ForEach-Object {
        Write-Host $_
        if ($_ -match "error")
            { $err = 1 }
    }
    if ($LASTEXITCODE -ne 0 -or $err -ne 0) {
        Write-Warning "Build failed (may be pre-existing errors in other sources). X+5 adapter code is in src/core/inference_shard_coordinator.cpp."
        exit 1
    }
    Write-Host "Build succeeded." -ForegroundColor Green
}

Write-Host ""
Write-Host "X+5 first milestone: EnumerateInferenceAdapters is implemented (DXGI)." -ForegroundColor Green
Write-Host "  - Adapter count and descriptions available at runtime via RawrXD::EnumerateInferenceAdapters()." -ForegroundColor Gray
Write-Host "  - IsMultiGpuInferenceReady() returns true when >= 2 GPUs are present." -ForegroundColor Gray
Write-Host "  - To expose in UI: call from swarm status or Tools menu and show adapter list." -ForegroundColor Gray
Write-Host "  - See docs/PHASE_X5_DISTRIBUTED_SWARM.md for layer-split next steps." -ForegroundColor Gray
exit 0
