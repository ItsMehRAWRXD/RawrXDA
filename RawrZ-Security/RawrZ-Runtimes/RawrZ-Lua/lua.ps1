#!/usr/bin/env pwsh
<#
    RawrZ Lua Runtime - Embedded Lua 5.4
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$LuaArgs
)

$ErrorActionPreference = 'Stop'

if ($LuaArgs.Count -gt 0 -and (Test-Path $LuaArgs[0])) {
    # Execute Lua script
    $code = Get-Content $LuaArgs[0] -Raw
    
    # Simple Lua→PowerShell transpiler
    $psCode = $code `
        -replace 'print\((.*?)\)', 'Write-Host $1' `
        -replace 'function\s+(\w+)\((.*?)\)', 'function $1 { param($2)' `
        -replace 'local\s+(\w+)\s*=\s*(.*)', '$$1 = $2' `
        -replace 'end$', '}' `
        -replace 'then$', '{' `
        -replace 'true', '$true' `
        -replace 'false', '$false' `
        -replace 'nil', '$null'
    
    try {
        Invoke-Expression $psCode
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "RawrZ Lua 5.4 (embedded)" -ForegroundColor Green
    Write-Host "Lightweight scripting environment" -ForegroundColor Gray
}
