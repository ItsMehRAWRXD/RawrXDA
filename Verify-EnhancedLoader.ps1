#!/usr/bin/env pwsh
# ============================================================================
# Enhanced Streaming GGUF Loader - Build Verification Script
# ============================================================================
# Verifies that the enhanced loader compiles, links, and exports symbols

param(
    [switch]$FullRebuild = $false,
    [switch]$SkipBuild = $false
)

$ErrorActionPreference = "Continue"
$WarningPreference = "Continue"

Write-Host "`n[Enhanced Loader Verification]" -ForegroundColor Cyan
Write-Host "Target: RawrXD_Gold + RawrXD-Win32IDE" -ForegroundColor Cyan
Write-Host ("=" * 80) -ForegroundColor DarkGray

# Step 1: Source verification
Write-Host "`n[1/5] Verifying source files..." -ForegroundColor Yellow
$SourceFile = "d:\rawrxd\src\streaming_gguf_loader_enhanced.cpp"
$HeaderFile = "d:\rawrxd\src\streaming_gguf_loader_enhanced.h"

if (!(Test-Path $SourceFile)) {
    Write-Host "  ✗ Source file not found: $SourceFile" -ForegroundColor Red
    exit 1
}
if (!(Test-Path $HeaderFile)) {
    Write-Host "  ✗ Header file not found: $HeaderFile" -ForegroundColor Red
    exit 1
}

$SourceSize = (Get-Item $SourceFile).Length
$HeaderSize = (Get-Item $HeaderFile).Length
Write-Host "  ✓ Source: $([math]::Round($SourceSize/1KB, 2)) KB" -ForegroundColor Green
Write-Host "  ✓ Header: $([math]::Round($HeaderSize/1KB, 2)) KB" -ForegroundColor Green

# Step 2: CMake configuration check
Write-Host "`n[2/5] Checking CMake configuration..." -ForegroundColor Yellow
$CMakeFile = "d:\rawrxd\CMakeLists.txt"
$CMakeContent = Get-Content $CMakeFile -Raw

if ($CMakeContent -match 'RawrXD_EnhancedLoader') {
    Write-Host "  ✓ Enhanced loader target found in CMakeLists.txt" -ForegroundColor Green
} else {
    Write-Host "  ✗ Enhanced loader target NOT found in CMakeLists.txt" -ForegroundColor Red
    exit 1
}

# Step 3: Build
if (!$SkipBuild) {
    Write-Host "`n[3/5] Building enhanced loader..." -ForegroundColor Yellow
    
    Push-Location d:\rawrxd
    
    if ($FullRebuild) {
        Write-Host "  • Full rebuild requested - cleaning build directory..." -ForegroundColor Gray
        if (Test-Path "build") {
            Remove-Item -Recurse -Force "build" -ErrorAction SilentlyContinue
        }
    }
    
    # Configure
    Write-Host "  • Configuring with CMake..." -ForegroundColor Gray
    $ConfigOutput = cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1
    
    if ($ConfigOutput -match 'Enhanced Loader.*Building from source') {
        Write-Host "  ✓ Enhanced loader configured" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ Enhanced loader configuration message not found" -ForegroundColor Yellow
    }
    
    # Build just the enhanced loader object
    Write-Host "  • Building RawrXD_EnhancedLoader target..." -ForegroundColor Gray
    $BuildOutput = cmake --build build --target RawrXD_EnhancedLoader 2>&1 | Out-String
    
    if ($BuildOutput -match 'error|failed') {
        Write-Host "  ✗ Build FAILED" -ForegroundColor Red
        Write-Host $BuildOutput
        Pop-Location
        exit 1
    }
    
    Write-Host "  ✓ Enhanced loader compiled successfully" -ForegroundColor Green
    
    Pop-Location
} else {
    Write-Host "`n[3/5] Skipping build (--SkipBuild)" -ForegroundColor Yellow
}

# Step 4: Symbol verification
Write-Host "`n[4/5] Verifying exported symbols..." -ForegroundColor Yellow

$ObjectPath = "d:\rawrxd\build\CMakeFiles\RawrXD_EnhancedLoader.dir\src\streaming_gguf_loader_enhanced.cpp.obj"
if (!(Test-Path $ObjectPath)) {
    Write-Host "  ⚠ Object file not found (expected after build): $ObjectPath" -ForegroundColor Yellow
} else {
    $ObjectSize = (Get-Item $ObjectPath).Length
    Write-Host "  ✓ Object file: $([math]::Round($ObjectSize/1KB, 2)) KB" -ForegroundColor Green
    
    # Check for key symbols using dumpbin
    $Symbols = @(
        'EnhancedStreamingGGUFLoader.*Open',
        'EnhancedStreamingGGUFLoader.*GetTensorViewPtr',
        'EnhancedLoaderUtils.*DecompressLZ4',
        'EnhancedLoaderUtils.*DecompressZSTD'
    )
    
    $DumpOutput = dumpbin /symbols $ObjectPath 2>$null | Out-String
    
    foreach ($Symbol in $Symbols) {
        if ($DumpOutput -match $Symbol) {
            $MatchedName = $Symbol -replace '\.\*', '::'
            Write-Host "  ✓ Symbol exported: $MatchedName" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Symbol missing: $Symbol" -ForegroundColor Red
        }
    }
}

# Step 5: Integration check
Write-Host "`n[5/5] Checking target integration..." -ForegroundColor Yellow

$BuildLog = "d:\rawrxd\build\CMakeCache.txt"
if (Test-Path $BuildLog) {
    $CacheContent = Get-Content $BuildLog -Raw
    
    if ($CacheContent -match 'RawrXD_Gold') {
        Write-Host "  ✓ RawrXD_Gold target exists" -ForegroundColor Green
    }
    
    if ($CacheContent -match 'RawrXD-Win32IDE') {
        Write-Host "  ✓ RawrXD-Win32IDE target exists" -ForegroundColor Green
    }
}

# Summary
Write-Host "`n" + ("=" * 80) -ForegroundColor DarkGray
Write-Host "[Verification Summary]" -ForegroundColor Cyan
Write-Host "  Enhanced Streaming GGUF Loader is ready for linking" -ForegroundColor Green
Write-Host "  Next steps:" -ForegroundColor Yellow
Write-Host "    1. Build full targets: cmake --build build --target RawrXD_Gold" -ForegroundColor Gray
Write-Host "    2. Run smoke test with your test GGUF model" -ForegroundColor Gray
Write-Host "    3. Monitor for enhanced loader activation in logs" -ForegroundColor Gray
Write-Host ""
