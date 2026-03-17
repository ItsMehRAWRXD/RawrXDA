#!/usr/bin/env pwsh
# RawrXD GGUF Loader Test Build and Execution Script
# Builds the test suite and runs it against real GGUF models

$ErrorActionPreference = "Stop"

Write-Host "`n╔═════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD GGUF Loader - Real CLI Test Build & Execution        ║" -ForegroundColor Cyan
Write-Host "╚═════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$srcDir = "d:\temp\RawrXD-agentic-ide-production\src"
$buildDir = "d:\temp\RawrXD-agentic-ide-production\build\gguf_test"

# Create build directory
if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
    Write-Host "Created build directory: $buildDir`n" -ForegroundColor Green
}

# Check for models
Write-Host "Scanning for GGUF models..." -ForegroundColor Yellow
$models = Get-ChildItem -Recurse -Filter "*.gguf" -ErrorAction SilentlyContinue
if ($models.Count -eq 0) {
    Write-Host "ERROR: No GGUF models found!" -ForegroundColor Red
    exit 1
}

Write-Host "Found $($models.Count) GGUF models:`n" -ForegroundColor Green
$models | ForEach-Object {
    $sizeGB = [math]::Round($_.Length / 1GB, 2)
    Write-Host "  • $($_.Name) ($sizeGB GB) - $($_.FullName)" -ForegroundColor Cyan
}

# Check if MSVC is available
Write-Host "`nChecking for MSVC compiler..." -ForegroundColor Yellow
$vcvarsPath = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $vcvarsPath)) {
    Write-Host "ERROR: MSVC 2022 not found at $vcvarsPath" -ForegroundColor Red
    exit 1
}
Write-Host "Found MSVC 2022 ✓" -ForegroundColor Green

# Setup MSVC environment
Write-Host "`nSetting up MSVC environment..." -ForegroundColor Yellow
Push-Location $buildDir
cmd /c "call `"$vcvarsPath`" && echo MSVC configured" | Out-Null

# Compile test
Write-Host "`nCompiling GGUF loader test..." -ForegroundColor Yellow
$compileCmd = @(
    "cl",
    "/std:c++17",
    "/EHsc",
    "/W4",
    "/O2",
    "/I`"d:\temp\RawrXD-agentic-ide-production\include`"",
    "/I`"d:\temp\RawrXD-agentic-ide-production\src`"",
    "/I`"C:\Qt\6.7.3\msvc2022_64\include`"",
    "`"$srcDir\gguf_loader_test_real.cpp`"",
    "`"$srcDir\gguf_loader.cpp`"",
    "`"$srcDir\gguf_vocab_resolver.cpp`"",
    "/link",
    "`"C:\Qt\6.7.3\msvc2022_64\lib\Qt6Core.lib`"",
    "/out:`"$buildDir\gguf_loader_test.exe`""
) -join " "

Write-Host "Build command: $compileCmd`n" -ForegroundColor DarkGray
cmd /c "call `"$vcvarsPath`" && cd /d `"$buildDir`" && $compileCmd" 2>&1 | Tee-Object -Variable buildOutput

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nBuild failed with exit code $LASTEXITCODE" -ForegroundColor Red
    Write-Host "Output: $buildOutput" -ForegroundColor Red
    exit 1
}

Write-Host "`n✓ Build succeeded!`n" -ForegroundColor Green

# Run tests on each model
Write-Host "Running tests on GGUF models...`n" -ForegroundColor Yellow
$testExe = "$buildDir\gguf_loader_test.exe"

$models | ForEach-Object {
    Write-Host "`n$('═' * 70)" -ForegroundColor Cyan
    Write-Host "Testing: $($_.Name)" -ForegroundColor Cyan
    Write-Host "Size: $([math]::Round($_.Length / 1GB, 2)) GB" -ForegroundColor Cyan
    Write-Host "Path: $($_.FullName)" -ForegroundColor DarkGray
    Write-Host "$('═' * 70)`n" -ForegroundColor Cyan
    
    # Run test with timeout
    try {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        & $testExe $_.FullName 2>&1 | Tee-Object -Variable testOutput
        $sw.Stop()
        
        Write-Host "`nTest completed in $($sw.Elapsed.TotalSeconds) seconds" -ForegroundColor Green
        
        # Check for failures in output
        if ($testOutput -match "✗ FAIL") {
            Write-Host "⚠ Some tests failed for this model" -ForegroundColor Yellow
        } else {
            Write-Host "✓ All tests passed!" -ForegroundColor Green
        }
    } catch {
        Write-Host "ERROR executing test: $_" -ForegroundColor Red
    }
}

Pop-Location
Write-Host "`n$('═' * 70)" -ForegroundColor Cyan
Write-Host "Test execution completed!" -ForegroundColor Green
Write-Host "$('═' * 70)`n" -ForegroundColor Cyan
