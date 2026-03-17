#!/usr/bin/env pwsh
<#
    RawrZ Runtime Manager
    Unified interface for all portable runtimes
#>

param(
    [ValidateSet('python','node','js','javascript','ruby','php','perl','lua')]
    [string]$Runtime,
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args
)

$runtimeMap = @{
    'python' = 'Python\python.ps1'
    'node' = 'JavaScript\node.ps1'
    'js' = 'JavaScript\node.ps1'
    'javascript' = 'JavaScript\node.ps1'
    'ruby' = 'Ruby\ruby.ps1'
    'php' = 'PHP\php.ps1'
    'perl' = 'Perl\perl.ps1'
    'lua' = 'Lua\lua.ps1'
}

if (-not $Runtime) {
    Write-Host "RawrZ Unified Runtime Manager" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Available runtimes:" -ForegroundColor Yellow
    Write-Host "  python      - Python 3.11+"
    Write-Host "  node/js     - Node.js compatible"
    Write-Host "  ruby        - Ruby 3.2+"
    Write-Host "  php         - PHP 8.2+"
    Write-Host "  perl        - Perl 5.36+"
    Write-Host "  lua         - Lua 5.4"
    Write-Host ""
    Write-Host "Usage: rawrz <runtime> <script> [args...]" -ForegroundColor Gray
    exit 0
}

$runtimeScript = Join-Path $PSScriptRoot $runtimeMap[$Runtime.ToLower()]

if (-not (Test-Path $runtimeScript)) {
    Write-Host "Error: Runtime '$Runtime' not found" -ForegroundColor Red
    exit 1
}

& $runtimeScript @Args
