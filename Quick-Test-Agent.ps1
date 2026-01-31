# Quick Autonomous Diagnostic Test
# Builds and runs a single autonomous diagnostic test

$ErrorActionPreference = "Stop"

Write-Host "`n=== RawrXD Autonomous Agent Quick Test ===" -ForegroundColor Cyan

$BuildDir = "d:\lazy init ide\build"
$IDEExe = "$BuildDir\bin\Release\RawrXD-Win32IDE.exe"
$LauncherExe = "$BuildDir\bin\Release\RawrXD-DiagnosticLauncher.exe"
$TestFile = "d:\lazy init ide\src\win32app\Win32IDE.cpp"

# Build
Write-Host "`nBuilding..." -ForegroundColor Yellow
Push-Location $BuildDir
cmake --build . --config Release --target RawrXD-DiagnosticLauncher | Out-Null
Pop-Location

if (-not (Test-Path $LauncherExe)) {
    Write-Host "✗ Build failed" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Build complete" -ForegroundColor Green

# Clean logs
@("C:\RawrXD_Agent_Beacons.log", "C:\RawrXD_Diagnostic_Report.txt") | 
    Where-Object { Test-Path $_ } | 
    Remove-Item -Force

# Run test
Write-Host "`nRunning diagnostic test..." -ForegroundColor Yellow
$process = Start-Process -FilePath $LauncherExe -ArgumentList "`"$IDEExe`" `"$TestFile`"" -Wait -PassThru -NoNewWindow

# Results
Write-Host "`nResults:" -ForegroundColor Cyan
if ($process.ExitCode -eq 0) {
    Write-Host "✓ TEST PASSED" -ForegroundColor Green
} else {
    Write-Host "✗ TEST FAILED (exit code: $($process.ExitCode))" -ForegroundColor Red
}

if (Test-Path "C:\RawrXD_Diagnostic_Report.txt") {
    Write-Host "`nReport:" -ForegroundColor Gray
    Get-Content "C:\RawrXD_Diagnostic_Report.txt" | Select-Object -First 20
}

exit $process.ExitCode
