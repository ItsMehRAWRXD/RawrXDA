#!/usr/bin/env pwsh
# Quick test compile for qt6_foundation.asm

$ML64 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"

Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Testing qt6_foundation.asm Compilation" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Clean old obj if exists
if (Test-Path "qt6_foundation.obj") {
    Remove-Item "qt6_foundation.obj" -Force
    Write-Host "[CLEAN] Removed old qt6_foundation.obj"
}

Write-Host "[COMPILE] qt6_foundation.asm..." -NoNewline

# Compile
$output = & $ML64 /c /Cp /nologo /Zi /Fo "qt6_foundation.obj" "qt6_foundation.asm" 2>&1 | Out-String
$exitCode = $LASTEXITCODE

if ($exitCode -eq 0 -and (Test-Path "qt6_foundation.obj")) {
    $size = (Get-Item "qt6_foundation.obj").Length
    Write-Host " [OK] ($size bytes)" -ForegroundColor Green
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Green
    Write-Host "  qt6_foundation.asm COMPILES SUCCESSFULLY!" -ForegroundColor Green
    Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Green
    exit 0
} else {
    Write-Host " [FAILED]" -ForegroundColor Red
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Red
    Write-Host "  COMPILATION ERRORS:" -ForegroundColor Red
    Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Red
    Write-Host $output
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Red
    Write-Host "  qt6_foundation.asm FAILED TO COMPILE (Exit Code: $exitCode)" -ForegroundColor Red
    Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Red
    exit 1
}
