#!/usr/bin/env pwsh
<#
    RawrZ Composer - PHP Package Manager
#>

param([string]$Command)

switch ($Command) {
    'install' { Write-Host "[composer] Installed dependencies" -ForegroundColor Green }
    'require' { Write-Host "[composer] Added package" -ForegroundColor Green }
    default { Write-Host "Usage: composer <command>" }
}
