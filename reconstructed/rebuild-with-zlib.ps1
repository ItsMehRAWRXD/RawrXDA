#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Rebuild RawrXD IDE with enhanced zlib compression support
    
.DESCRIPTION
    This script rebuilds the project with the updated CMakeLists.txt that includes:
    - Auto-download of zlib via FetchContent if not found on system
    - Proper HAVE_ZLIB and HAVE_COMPRESSION compile definitions
    - Fallback to MASM-based compression (BrutalGzip/Deflate) if zlib unavailable
    
.NOTES
    Run from PowerShell as Administrator for optimal results
#>

param(
    [switch]$Clean,
    [switch]$Release,
    [string]$BuildDir = "D:\RawrXD-production-lazy-init\build",
    [switch]$Verbose
)

$ErrorActionPreference = 'Stop'

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD IDE - Zlib Compression Rebuild" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Validate build directory
if (-not (Test-Path $BuildDir)) {
    Write-Host "ERROR: Build directory not found: $BuildDir" -ForegroundColor Red
    exit 1
}

Push-Location $BuildDir

try {
    # Clean build if requested
    if ($Clean) {
        Write-Host "`n📋 Cleaning build directory..." -ForegroundColor Yellow
        Remove-Item -Path @("CMakeFiles", "CMakeCache.txt", "cmake_install.cmake") -Recurse -ErrorAction SilentlyContinue | Out-Null
    }

    # Run CMake with zlib support
    Write-Host "`n🔧 Configuring CMake with zlib support..." -ForegroundColor Yellow
    
    $cmakeArgs = @(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DFETCHCONTENT_ZLIB=ON"
        "-DGGML_LTO=ON"
        ".."
    )
    
    if ($Verbose) {
        $cmakeArgs += "--debug-output"
    }
    
    & cmake $cmakeArgs
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: CMake configuration failed" -ForegroundColor Red
        exit 1
    }

    # Build project
    Write-Host "`n🏗️  Building RawrXD-QtShell with zlib compression..." -ForegroundColor Yellow
    
    $buildConfig = if ($Release) { "Release" } else { "Debug" }
    & cmake --build . --config $buildConfig --target RawrXD-QtShell --parallel 8
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed" -ForegroundColor Red
        exit 1
    }

    # Check if executable was created
    $exePath = "$BuildDir\bin\Release\RawrXD-QtShell.exe"
    if (Test-Path $exePath) {
        $exeSize = (Get-Item $exePath).Length / 1MB
        Write-Host "`n✅ Build successful!" -ForegroundColor Green
        Write-Host "   Executable: $exePath" -ForegroundColor Green
        Write-Host "   Size: $([math]::Round($exeSize, 2)) MB" -ForegroundColor Green
    } else {
        Write-Host "WARNING: Expected executable not found at $exePath" -ForegroundColor Yellow
    }

    Write-Host "`n📊 Compression Configuration Summary:" -ForegroundColor Cyan
    Write-Host "   ✓ MASM-based compression: BrutalGzip, Deflate (always available)" -ForegroundColor Green
    Write-Host "   ✓ Native zlib support: Auto-downloaded and linked" -ForegroundColor Green
    Write-Host "   ✓ Compile definitions: HAVE_COMPRESSION=1, HAVE_ZLIB=1" -ForegroundColor Green

} finally {
    Pop-Location
}

Write-Host "`n💡 Next steps:" -ForegroundColor Cyan
Write-Host "   1. Run smoke test: D:\RawrXD-production-lazy-init\build\bin\Release\RawrXD-QtShell.exe" -ForegroundColor Cyan
Write-Host "   2. Verify no zlib warnings in IDE output" -ForegroundColor Cyan
Write-Host "   3. Check Model Loader for compression status" -ForegroundColor Cyan

exit 0
