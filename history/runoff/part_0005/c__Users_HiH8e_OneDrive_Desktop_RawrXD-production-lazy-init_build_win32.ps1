#!/usr/bin/env pwsh
# Win32 IDE Quick Build Script
# Version: 1.0.0
# Date: 2025-12-18

$ErrorActionPreference = "Stop"

Write-Host "====================================" -ForegroundColor Cyan
Write-Host " RawrXD Win32 IDE - Quick Build" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""

# Check for CMake
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] CMake not found in PATH" -ForegroundColor Red
    Write-Host "Please install CMake from: https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

# Check for MSBuild
if (!(Get-Command msbuild -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] MSBuild not found in PATH" -ForegroundColor Red
    Write-Host "Please install Visual Studio 2022 or Build Tools" -ForegroundColor Yellow
    exit 1
}

$BuildDir = "build-win32-only"
$Config = "Release"

try {
    Write-Host "[1/4] Configuring CMake..." -ForegroundColor Yellow
    cmake -S win32_only -B $BuildDir -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    Write-Host "[OK] CMake configured successfully" -ForegroundColor Green
    Write-Host ""

    Write-Host "[2/4] Building Release configuration..." -ForegroundColor Yellow
    cmake --build $BuildDir --config $Config --target AgenticIDEWin
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Write-Host "[OK] Build completed successfully" -ForegroundColor Green
    Write-Host ""

    Write-Host "[3/4] Verifying binary..." -ForegroundColor Yellow
    $BinaryPath = Join-Path $BuildDir "bin\$Config\AgenticIDEWin.exe"
    if (!(Test-Path $BinaryPath)) {
        throw "Binary not found at expected location: $BinaryPath"
    }
    Write-Host "[OK] Binary found: $BinaryPath" -ForegroundColor Green
    Write-Host ""

    Write-Host "[4/4] Checking file size..." -ForegroundColor Yellow
    $FileSize = (Get-Item $BinaryPath).Length
    $FileSizeMB = [math]::Round($FileSize / 1MB, 2)
    Write-Host "Binary size: $FileSizeMB MB" -ForegroundColor Cyan
    Write-Host ""

    Write-Host "====================================" -ForegroundColor Cyan
    Write-Host " BUILD COMPLETE" -ForegroundColor Green
    Write-Host "====================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Binary location: $BinaryPath" -ForegroundColor White
    Write-Host ""
    Write-Host "To run the IDE:" -ForegroundColor Yellow
    Write-Host "  cd $BuildDir\bin\$Config" -ForegroundColor White
    Write-Host "  .\AgenticIDEWin.exe" -ForegroundColor White
    Write-Host ""
    Write-Host "Or run directly:" -ForegroundColor Yellow
    Write-Host "  & `"$BinaryPath`"" -ForegroundColor White
    Write-Host ""

    exit 0
}
catch {
    Write-Host ""
    Write-Host "[ERROR] $_" -ForegroundColor Red
    Write-Host ""
    exit 1
}
