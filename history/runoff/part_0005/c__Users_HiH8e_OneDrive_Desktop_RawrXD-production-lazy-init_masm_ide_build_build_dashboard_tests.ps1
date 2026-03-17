#!/usr/bin/env pwsh
# Build script for Error Dashboard tests

$ErrorActionPreference = 'Stop'

Write-Host "[Build] Dashboard tests" -ForegroundColor Cyan

$masm = "C:\masm32"
if (-not (Test-Path $masm)) {
  Write-Host "ERROR: MASM32 not found at C:\\masm32" -ForegroundColor Red
  Write-Host "Install MASM32: http://www.masm32.com" -ForegroundColor Yellow
  exit 1
}

$ml = Join-Path $masm 'bin/ml.exe'
$link = Join-Path $masm 'bin/link.exe'
$inc = Join-Path $masm 'include'
$lib = Join-Path $masm 'lib'

$srcDir = Join-Path $PSScriptRoot '..\src'
$testsDir = Join-Path $PSScriptRoot '..\tests'
$outDir = Join-Path $PSScriptRoot 'obj'
$binDir = Join-Path $PSScriptRoot '..\build'

if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
if (-not (Test-Path $binDir)) { New-Item -ItemType Directory -Path $binDir | Out-Null }

# Compile modules
$compileArgs = @("/c","/coff","/Cp","/Zi","/I$inc")

Write-Host "Compiling error_dashboard.asm" -ForegroundColor Gray
& $ml $compileArgs "/Fo$outDir/error_dashboard.obj" "$srcDir/error_dashboard.asm"

Write-Host "Compiling error_dashboard_tests.asm" -ForegroundColor Gray
& $ml $compileArgs "/Fo$outDir/error_dashboard_tests.obj" "$testsDir/error_dashboard_tests.asm"

# Link
Write-Host "Linking dashboard_tests.exe" -ForegroundColor Gray
& $link "/SUBSYSTEM:WINDOWS" "/OUT:$binDir/dashboard_tests.exe" "$outDir/error_dashboard.obj" "$outDir/error_dashboard_tests.obj" "$lib/kernel32.lib" "$lib/user32.lib"

Write-Host "✓ Build completed: $binDir/dashboard_tests.exe" -ForegroundColor Green
