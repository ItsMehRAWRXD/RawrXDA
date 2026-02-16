#!/usr/bin/env pwsh
<#
.SYNOPSIS
RawrXD Local Universal Launcher (Windows)

.DESCRIPTION
Starts backend + web interface on localhost without hosted infrastructure.
#>

$RootDir = Split-Path -Parent $PSScriptRoot
$Launcher = Join-Path $RootDir "scripts/launch_local_universal.py"

if (-not (Test-Path $Launcher)) {
    Write-Error "Launcher not found at $Launcher"
    exit 1
}

$Python = $null
if (Get-Command py -ErrorAction SilentlyContinue) {
    $Python = @("py", "-3")
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $Python = @("python")
} else {
    Write-Error "Python 3 is required but was not found."
    exit 1
}

if ($Python.Length -gt 1) {
    & $Python[0] $Python[1] $Launcher @args
} else {
    & $Python[0] $Launcher @args
}
exit $LASTEXITCODE
