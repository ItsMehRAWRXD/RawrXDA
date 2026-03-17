#!/usr/bin/env pwsh
# Quick Build - MASM GGUF Compressor

$ErrorActionPreference = "Stop"

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Building MASM GGUF Compressor" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

$projectRoot = "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader"
$sourceDir = Join-Path $projectRoot "masm-compressor"
$buildDir = Join-Path $projectRoot "build-masm-compress"

# Clean
if (Test-Path $buildDir) {
    Remove-Item -Path $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

Set-Location $buildDir

# Configure
Write-Host "[1/2] Configuring CMake..." -ForegroundColor Yellow
cmake -G "Visual Studio 17 2022" -A x64 -S $sourceDir -B .

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "[2/2] Building..." -ForegroundColor Yellow
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Build failed!" -ForegroundColor Red
    exit 1
}

$exePath = Join-Path $buildDir "Release\masm_gguf_compress.exe"

if (Test-Path $exePath) {
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host "✓ BUILD SUCCESSFUL!" -ForegroundColor Green
    Write-Host "========================================`n" -ForegroundColor Green
    
    Write-Host "Executable: $exePath" -ForegroundColor Cyan
    Write-Host "Size: $([math]::Round((Get-Item $exePath).Length / 1KB, 2)) KB`n" -ForegroundColor Cyan
    
    Write-Host "USAGE:" -ForegroundColor Yellow
    Write-Host "  Load model:" -ForegroundColor White
    Write-Host "    masm_gguf_compress.exe model.gguf`n" -ForegroundColor Cyan
    
    Write-Host "  Compress model:" -ForegroundColor White
    Write-Host "    masm_gguf_compress.exe -c input.gguf output.masm.gguf godmode" -ForegroundColor Cyan
    Write-Host "    masm_gguf_compress.exe -c input.gguf output.masm.gguf brutal`n" -ForegroundColor Cyan
    
    Write-Host "COMPRESSION MODES:" -ForegroundColor Yellow
    Write-Host "  brutal  = Fast, ~15% compression" -ForegroundColor White
    Write-Host "  godmode = Slow, ~40% compression`n" -ForegroundColor White
} else {
    Write-Host "✗ Executable not found!" -ForegroundColor Red
    exit 1
}
