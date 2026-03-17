#!/usr/bin/env pwsh
# Deploy RawrXD Win32 IDE to a staging directory
# Usage:
#   pwsh scripts/deploy_staging.ps1 -PackageZip <zipPath> -StagingRoot <path>

param(
    [Parameter(Mandatory=$true)][string]$PackageZip,
    [Parameter(Mandatory=$true)][string]$StagingRoot
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $PackageZip)) {
    Write-Host "[ERROR] Package zip not found: $PackageZip" -ForegroundColor Red
    exit 1
}

$staging = Resolve-Path $StagingRoot -ErrorAction SilentlyContinue
if (-not $staging) {
    New-Item -ItemType Directory -Force -Path $StagingRoot | Out-Null
    $staging = Resolve-Path $StagingRoot
}

Write-Host "Deploying package to staging: $staging" -ForegroundColor Cyan

# Clear existing contents
Get-ChildItem -Path $staging -Force | Remove-Item -Recurse -Force

Expand-Archive -Path $PackageZip -DestinationPath $staging -Force

Write-Host "[OK] Package extracted" -ForegroundColor Green

$candidatePaths = @(
    (Join-Path $staging "RawrXD-Win32-Deploy/bin/AgenticIDEWin.exe"),
    (Join-Path $staging "bin/AgenticIDEWin.exe")
)

$binaryFound = $false
foreach ($candidate in $candidatePaths) {
    if (Test-Path $candidate) {
        Write-Host "Binary located: $candidate" -ForegroundColor Green
        $binaryFound = $true
        break
    }
}

if (-not $binaryFound) {
    Write-Host "[WARNING] Binary not found in expected locations" -ForegroundColor Yellow
    $candidatePaths | ForEach-Object { Write-Host "  - $_" }
}

Write-Host "Staging deployment complete." -ForegroundColor Green
exit 0
