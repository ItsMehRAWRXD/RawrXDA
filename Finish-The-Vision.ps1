param(
    [switch]$Fast,
    [switch]$Strict,
    [string]$BuildDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$runner = Join-Path $repoRoot "Run-14Day-ProductionFinishers.ps1"
$scorecard = Join-Path $repoRoot "reports\14day\final_scorecard.md"

if (-not (Test-Path $runner)) {
    Write-Error "Missing production finisher runner: $runner"
    exit 1
}

$args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $runner, "-AllDays")
if ($Strict -or -not $PSBoundParameters.ContainsKey("Strict")) {
    $args += "-Strict"
}
if ($Fast) {
    $args += "-Fast"
}
if ($BuildDir) {
    $args += @("-BuildDir", $BuildDir)
}

Write-Host "Finishing the vision..." -ForegroundColor Cyan
Write-Host ("powershell {0}" -f ($args -join ' ')) -ForegroundColor DarkCyan

& powershell @args
$code = $LASTEXITCODE

if ($code -ne 0) {
    Write-Error "Production finisher failed with exit code $code"
    exit $code
}

if (Test-Path $scorecard) {
    Write-Host "" 
    Write-Host "Final scorecard:" -ForegroundColor Green
    Get-Content -Path $scorecard
}

exit 0
