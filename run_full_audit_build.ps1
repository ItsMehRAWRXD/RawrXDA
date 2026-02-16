# Full Parity Audit Build Script
# Captures ALL compilation and link errors for remediation
# Run: .\run_full_audit_build.ps1

$ErrorActionPreference = "Continue"
$BuildDir = "build_audit"
$OutputFile = "build_audit_errors.txt"
$LogFile = "build_audit_full.log"

Write-Host "=== RawrXD Full Parity Audit Build ===" -ForegroundColor Cyan
Write-Host "Target: 100.1% Cursor/VS Code/Copilot/Amazon Q parity" -ForegroundColor Cyan
Write-Host ""

# Clean build directory
if (Test-Path $BuildDir) {
    Write-Host "Removing existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

# Configure
Write-Host "Configuring CMake..." -ForegroundColor Green
Push-Location $BuildDir
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release 2>&1 | Tee-Object -FilePath "..\$LogFile" -Append

# Build with keep-going to capture ALL errors (-k 100 = continue past 100 failures)
Write-Host "Building (ninja -k 100 to capture all errors)..." -ForegroundColor Green
$start = Get-Date
ninja -k 100 2>&1 | Tee-Object -FilePath "..\$OutputFile" -Append
$duration = (Get-Date) - $start
Pop-Location

# Summary
Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Cyan
Write-Host "Duration: $($duration.ToString('hh\:mm\:ss'))"
Write-Host "Errors captured in: $OutputFile"
Write-Host "Full log: $LogFile"
Write-Host ""

# Count errors
$content = Get-Content $OutputFile -Raw -ErrorAction SilentlyContinue
$errorCount = ([regex]::Matches($content, "error C\d+|fatal error|FAILED:")).Count
$asmErrors = ([regex]::Matches($content, "\.asm.*error|A\d+")).Count
Write-Host "Approximate error count: $errorCount (C++/link) + $asmErrors (ASM)"
Write-Host ""
Write-Host "Next: Review FULL_PARITY_AUDIT_2026-02-15.md and fix errors systematically"
