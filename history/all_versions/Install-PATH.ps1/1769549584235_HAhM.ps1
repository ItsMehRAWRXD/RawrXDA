#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Adds RawrXD to the system PATH

.DESCRIPTION
    Installs RawrXD CLI to user or system PATH for easy access from any location.
#>

param(
    [switch]$System  # Install to system PATH (requires admin)
)

$RawrXDPath = "C:\RawrXD"

Write-Host "`n========================================================" -ForegroundColor Cyan
Write-Host "  RawrXD Toolchain - PATH Installation" -ForegroundColor Green
Write-Host "========================================================`n" -ForegroundColor Cyan

# Check if already in PATH
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -like "*$RawrXDPath*") {
    Write-Host "✓ RawrXD is already in your PATH" -ForegroundColor Green
    Write-Host "`nCurrent PATH includes: $RawrXDPath" -ForegroundColor Gray
    exit 0
}

# Install to PATH
try {
    if ($System) {
        # System PATH (requires admin)
        $scope = "Machine"
        Write-Host "[Admin] Installing to SYSTEM PATH..." -ForegroundColor Yellow
    } else {
        # User PATH
        $scope = "User"
        Write-Host "[User] Installing to USER PATH..." -ForegroundColor Yellow
    }
    
    $currentPath = [Environment]::GetEnvironmentVariable("Path", $scope)
    $newPath = $currentPath + ";$RawrXDPath"
    
    [Environment]::SetEnvironmentVariable("Path", $newPath, $scope)
    
    Write-Host "✓ SUCCESS: Added C:\RawrXD to PATH" -ForegroundColor Green
    Write-Host "`nYou can now run from anywhere:" -ForegroundColor Cyan
    Write-Host "  RawrXD-CLI.ps1 info" -ForegroundColor White
    Write-Host "  RawrXD-CLI.bat generate-pe myapp.exe" -ForegroundColor White
    Write-Host "`nNOTE: Close and reopen your terminal for changes to take effect" -ForegroundColor Yellow
    
} catch {
    Write-Error "Failed to update PATH: $_"
    Write-Host "`nTry running as Administrator for system-wide installation:" -ForegroundColor Yellow
    Write-Host "  Right-click PowerShell -> Run as Administrator" -ForegroundColor Gray
    Write-Host "  .\Install-PATH.ps1 -System" -ForegroundColor Gray
    exit 1
}
