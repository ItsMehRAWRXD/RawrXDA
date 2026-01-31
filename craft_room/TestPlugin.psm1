#!/usr/bin/env pwsh
<#
.SYNOPSIS
    TestPlugin - Custom Extension
#>

param()

function Invoke-TestPlugin {
    Write-Host "Running TestPlugin..." -ForegroundColor Cyan
    # Add your custom logic here
}

Export-ModuleMember -Function @(
    'Invoke-TestPlugin'
)
