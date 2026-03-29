#requires -Version 5.1
<#
.SYNOPSIS
    Validates agent wiring, parity, and registry truth.

.DESCRIPTION
    Runs the following checks:
    1) Regenerates command registry from source.
    2) Ensures command_registry.json has zero hasRealHandler=false entries.
    3) Enforces CLI/GUI parity and zero wrapper macros via check_agent_parity.py.

.PARAMETER RepoRoot
    Repository root path. Defaults to D:\rawrxd.
#>

param(
    [string]$RepoRoot = "D:\rawrxd",
    [switch]$RegenerateRegistry
)

$ErrorActionPreference = "Stop"

function Run-Step([string]$Name, [scriptblock]$Action) {
    Write-Host "`n[STEP] $Name" -ForegroundColor Cyan
    & $Action
}

function Invoke-Python {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Args
    )

    $pythonCmd = $null
    $pythonArgs = @()
    if (Get-Command python -ErrorAction SilentlyContinue) {
        $pythonCmd = "python"
        $pythonArgs = $Args
    } elseif (Get-Command py -ErrorAction SilentlyContinue) {
        $pythonCmd = "py"
        $pythonArgs = @("-3") + $Args
    } else {
        throw "No Python launcher found (expected 'python' or 'py')."
    }
    & $pythonCmd @pythonArgs | Out-Host
    if ($null -eq $LASTEXITCODE) { return 0 }
    return [int]$LASTEXITCODE
}

Set-Location $RepoRoot

if ($RegenerateRegistry) {
    Run-Step "Regenerate command registry from source" {
        $srcRoot = Join-Path $RepoRoot "src"
        $exit = Invoke-Python -Args @(".\scripts\auto_register_commands.py", "--src-root", $srcRoot)
        if ($exit -ne 0) { throw "auto_register_commands.py failed (exit=$exit)." }
    }
}

Run-Step "Validate hasRealHandler flags" {
    $registryPath = Join-Path $RepoRoot "reports\command_registry.json"
    if (!(Test-Path $registryPath)) { throw "command_registry.json missing: $registryPath" }
    $doc = Get-Content $registryPath -Raw | ConvertFrom-Json
    $entries = @($doc.entries)
    $trueCount = @($entries | Where-Object { $_.hasRealHandler -eq $true }).Count
    $falseCount = @($entries | Where-Object { $_.hasRealHandler -eq $false }).Count
    Write-Host "entries=$($entries.Count) true=$trueCount false=$falseCount"
    if ($falseCount -ne 0) { throw "command_registry.json still has hasRealHandler=false entries." }
}

Run-Step "Validate CLI/GUI parity + zero wrapper macros" {
    $exit = Invoke-Python -Args @(".\scripts\check_agent_parity.py", "--repo-root", $RepoRoot, "--strict-anywhere")
    if ($exit -ne 0) { throw "check_agent_parity.py failed (exit=$exit)." }
}

Write-Host "`nAGENT WIRING VALIDATION PASS" -ForegroundColor Green

