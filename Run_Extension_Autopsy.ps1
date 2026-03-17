# EXTENSION AUTOPSY - One-shot: Harvest, fix typo, wire host, optional purge
# Run from D:\rawrxd

param(
    [switch]$Harvest,
    [switch]$FixTypo,
    [switch]$Wire,
    [switch]$UninstallBloat,
    [switch]$All
)

if ($All) {
    $Harvest = $FixTypo = $Wire = $true
}

if (-not ($Harvest -or $FixTypo -or $Wire -or $UninstallBloat)) {
    Write-Host "Usage: .\Run_Extension_Autopsy.ps1 -All" -ForegroundColor Yellow
    Write-Host "   Or: -Harvest | -FixTypo | -Wire | -UninstallBloat" -ForegroundColor Gray
    exit 0
}

if ($Harvest) {
    Write-Host "`n=== HARVEST ===" -ForegroundColor Cyan
    & "$PSScriptRoot\ExtensionHarvester.ps1"
}

if ($FixTypo) {
    Write-Host "`n=== FIX opencaht -> openChat ===" -ForegroundColor Cyan
    & "$PSScriptRoot\FixBigDaddyGTypo.ps1"
}

if ($Wire) {
    Write-Host "`n=== WIRE EXTENSIONS ===" -ForegroundColor Cyan
    & "$PSScriptRoot\extension-host\Wire_Extensions.ps1"
}

if ($UninstallBloat) {
    Write-Host "`n=== UNINSTALL BLOAT ===" -ForegroundColor Cyan
    & "$PSScriptRoot\UninstallExtensionBloat.ps1"
}

Write-Host "`nDONE." -ForegroundColor Green
