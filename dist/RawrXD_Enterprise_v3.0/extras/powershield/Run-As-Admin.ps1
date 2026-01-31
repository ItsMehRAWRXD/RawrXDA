<#!
.SYNOPSIS
    Launch RawrXD with Administrator privileges
.DESCRIPTION
    Helper script to elevate RawrXD.ps1 to admin rights if needed.
    Useful for Event Log features and system-level operations.
!#>

param()

$rawrxdPath = Join-Path $PSScriptRoot "RawrXD.ps1"

if (-not (Test-Path $rawrxdPath -PathType Leaf)) {
  Write-Host "ERROR: RawrXD.ps1 not found at: $rawrxdPath" -ForegroundColor Red
  Read-Host "Press Enter to exit"
  exit 1
}

# Check if already admin
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if ($isAdmin) {
  Write-Host "Already running as Administrator. Launching RawrXD..." -ForegroundColor Green
  # Add cache-busting timestamp to force script reload
  $timestamp = Get-Date -Format "yyyyMMddHHmmss"
  & pwsh -NoProfile -NoLogo -File $rawrxdPath -CacheBust $timestamp
}
else {
  Write-Host "Requesting Administrator privileges..." -ForegroundColor Yellow
  try {
    # Add cache-busting timestamp to force script reload
    $timestamp = Get-Date -Format "yyyyMMddHHmmss"
    Start-Process pwsh -ArgumentList "-NoProfile -NoLogo -File `"$rawrxdPath`" -CacheBust $timestamp" -Verb RunAs
    Write-Host "✅ RawrXD launched with elevated privileges." -ForegroundColor Green
  }
  catch {
    Write-Host "❌ Failed to elevate: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "`nYou can run RawrXD without admin (some features like Event Log will be disabled)." -ForegroundColor Yellow
    $choice = Read-Host "Run without admin anyway? (y/n)"
    if ($choice -eq 'y' -or $choice -eq 'Y') {
      # Add cache-busting timestamp to force script reload
      $timestamp = Get-Date -Format "yyyyMMddHHmmss"
      & pwsh -NoProfile -NoLogo -File $rawrxdPath -CacheBust $timestamp
    }
  }
}
