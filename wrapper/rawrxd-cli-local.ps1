#!/usr/bin/env pwsh

$RootDir = Split-Path -Parent $PSScriptRoot
$CliPath = Join-Path $RootDir "Ship/rawrxd_cli_local.py"

if (-not (Test-Path $CliPath)) {
    Write-Error "CLI not found at $CliPath"
    exit 1
}

if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 $CliPath @args
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    & python $CliPath @args
} else {
    Write-Error "Python 3 is required but was not found."
    exit 1
}

exit $LASTEXITCODE
