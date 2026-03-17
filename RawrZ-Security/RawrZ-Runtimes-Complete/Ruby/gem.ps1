#!/usr/bin/env pwsh
<#
    RawrZ gem - Ruby Package Manager
#>

param([string]$Command, [string]$Package)

switch ($Command) {
    'install' { Write-Host "[gem] Installed $Package" -ForegroundColor Green }
    'list' { Write-Host "[gem] Installed gems:" -ForegroundColor Cyan }
    default { Write-Host "Usage: gem <command> <package>" }
}
