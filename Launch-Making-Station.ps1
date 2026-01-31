#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Quick launcher for RawrXD Model/Agent Making Station

.DESCRIPTION
    Launches the comprehensive Model/Agent Making Station for creating,
    configuring, and deploying AI models and autonomous agents.

.EXAMPLE
    .\Launch-Making-Station.ps1
#>

$stationScript = Join-Path $PSScriptRoot "scripts\model_agent_making_station.ps1"

if (-not (Test-Path $stationScript)) {
    Write-Host "Error: Making Station not found at: $stationScript" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "🏭 Launching Model/Agent Making Station..." -ForegroundColor Magenta
Write-Host ""

& $stationScript @args
