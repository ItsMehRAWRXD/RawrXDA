param(
    [ValidateRange(1,14)]
    [int]$Day,
    [switch]$AllDays,
    [switch]$Strict,
    [switch]$Fast,
    [switch]$FastSmoke,
    [switch]$NoBuild,
    [switch]$NoTests,
    [string]$BuildDir = "",
    [string[]]$Day2Targets = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$orchestrator = Join-Path $repoRoot "scripts\production\Invoke-14Day-ProductionFinishers.ps1"

if (-not (Test-Path $orchestrator)) {
    Write-Error "Missing orchestrator: $orchestrator"
    exit 1
}

$invokeArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $orchestrator)

if ($AllDays) {
    $invokeArgs += "-AllDays"
} elseif ($PSBoundParameters.ContainsKey("Day")) {
    $invokeArgs += @("-Day", $Day)
} else {
    $invokeArgs += "-AllDays"
}

if ($Strict) { $invokeArgs += "-Strict" }
if ($FastSmoke) { $invokeArgs += "-FastSmoke" }

if ($Fast) {
    $invokeArgs += "-NoBuild"
    $invokeArgs += "-NoTests"
} else {
    if ($NoBuild) { $invokeArgs += "-NoBuild" }
    if ($NoTests) { $invokeArgs += "-NoTests" }
}

if ($BuildDir) {
    $invokeArgs += @("-BuildDir", $BuildDir)
}

if ($Day2Targets.Count -gt 0) {
    $invokeArgs += @("-Day2Targets")
    $invokeArgs += $Day2Targets
}

Write-Host "Launching 14-day finisher orchestrator..." -ForegroundColor Cyan
Write-Host "powershell $($invokeArgs -join ' ')" -ForegroundColor DarkCyan

& powershell @invokeArgs
exit $LASTEXITCODE
