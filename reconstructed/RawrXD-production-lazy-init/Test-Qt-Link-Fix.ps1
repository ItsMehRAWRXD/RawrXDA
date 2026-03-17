# Test Qt Link Error Fix
# This script tests the CMakeLists.txt fix for duplicate MOC symbols

$ErrorActionPreference = "Stop"

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  Qt Link Error Fix - Test Script" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

$ProjectRoot = "d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader"
$BuildDir = "$ProjectRoot\build"
$TestLog = "D:\qt_fix_test_results.txt"

Write-Host "`n[1/4] Verifying CMakeLists.txt fix..." -ForegroundColor Yellow

# Check that manual MOC files are commented out
$cmakelists = Get-Content "$ProjectRoot\CMakeLists.txt" -Raw
if ($cmakelists -match "src/qtapp/moc_includes\.cpp" -and $cmakelists -notmatch "#.*src/qtapp/moc_includes\.cpp") {
    Write-Error "ERROR: moc_includes.cpp is still active in CMakeLists.txt!"
}
if ($cmakelists -match "zero_day_agentic_engine_moc_trigger\.cpp" -and $cmakelists -notmatch "#.*zero_day_agentic_engine_moc_trigger\.cpp") {
    Write-Error "ERROR: zero_day_agentic_engine_moc_trigger.cpp is still active in CMakeLists.txt!"
}
Write-Host "  ✓ Manual MOC files are properly commented out" -ForegroundColor Green

# Verify AUTOMOC is enabled
if ($cmakelists -match "AUTOMOC ON") {
    Write-Host "  ✓ AUTOMOC is enabled for RawrXD-AgenticIDE" -ForegroundColor Green
} else {
    Write-Warning "  ! AUTOMOC might not be enabled - check line 2064"
}

Write-Host "`n[2/4] Cleaning build directory..." -ForegroundColor Yellow
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
    Write-Host "  ✓ Build directory cleaned" -ForegroundColor Green
} else {
    Write-Host "  ✓ No existing build directory" -ForegroundColor Green
}

# Clean old MOC files
$oldMocFiles = Get-ChildItem -Path $ProjectRoot -Recurse -Filter "moc_*.cpp" -ErrorAction SilentlyContinue
if ($oldMocFiles) {
    Write-Host "  Removing $($oldMocFiles.Count) old MOC files..." -ForegroundColor Gray
    $oldMocFiles | Remove-Item -Force
}

Write-Host "`n[3/4] Running CMake configuration..." -ForegroundColor Yellow
Set-Location $ProjectRoot
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
Set-Location $BuildDir

$cmakeOutput = cmake -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="C:\Qt\6.7.3\msvc2022_64" `
    -DQt6_DIR="C:\Qt\6.7.3\msvc2022_64\lib\cmake\Qt6" `
    -DCMAKE_BUILD_TYPE=Release `
    .. 2>&1

$cmakeOutput | Out-File $TestLog

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed - see $TestLog"
}
Write-Host "  ✓ CMake configuration successful" -ForegroundColor Green

Write-Host "`n[4/4] Building RawrXD-AgenticIDE..." -ForegroundColor Yellow
Write-Host "  This may take several minutes..." -ForegroundColor Gray

$buildStart = Get-Date
$buildOutput = cmake --build . --config Release --target RawrXD-AgenticIDE -j 8 2>&1
$buildEnd = Get-Date
$buildDuration = ($buildEnd - $buildStart).TotalMinutes

$buildOutput | Out-File $TestLog -Append

# Check for link errors
$linkErrors = $buildOutput | Select-String -Pattern "error LNK|duplicate symbol|unresolved external"

if ($linkErrors) {
    Write-Host "`n❌ BUILD FAILED - Link errors detected:" -ForegroundColor Red
    $linkErrors | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host "`nFull log: $TestLog" -ForegroundColor Yellow
    exit 1
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "`n❌ BUILD FAILED" -ForegroundColor Red
    Write-Host "  Exit code: $LASTEXITCODE" -ForegroundColor Red
    Write-Host "  Full log: $TestLog" -ForegroundColor Yellow
    
    # Show last 30 lines of output
    Write-Host "`nLast 30 lines of build output:" -ForegroundColor Yellow
    $buildOutput | Select-Object -Last 30 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    exit 1
}

Write-Host "`n✅ BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "  Build time: $([math]::Round($buildDuration, 1)) minutes" -ForegroundColor Green

# Verify executable
$exePath = "$BuildDir\bin\Release\RawrXD-AgenticIDE.exe"
if (Test-Path $exePath) {
    $exeInfo = Get-Item $exePath
    Write-Host "  ✓ Executable created: $([math]::Round($exeInfo.Length/1MB, 2)) MB" -ForegroundColor Green
} else {
    Write-Warning "  ! Executable not found at expected location"
}

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  🎉 Qt Link Error Fix VERIFIED!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor White
Write-Host "1. Run full deployment script: Build-And-Deploy-Production.ps1" -ForegroundColor Gray
Write-Host "2. Verify DirectX DLLs are copied automatically" -ForegroundColor Gray
Write-Host "3. Confirm ZIP generation completes end-to-end" -ForegroundColor Gray
Write-Host ""
Write-Host "Full test log: $TestLog" -ForegroundColor White
