[CmdletBinding()]
param(
    [ValidateSet("debug", "release")]
    [string]$Config = "debug",
    [switch]$Rebuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildWrapper = Join-Path $RepoRoot "Build-AgenticIDE.ps1"
$BuildScript = Join-Path $RepoRoot "src\asm\monolithic\Build-Monolithic.ps1"
$OutputExe = Join-Path $RepoRoot "build\monolithic\bin\RawrXD_Monolithic.exe"

if (-not (Test-Path $BuildWrapper)) {
    throw "Build wrapper not found: $BuildWrapper"
}

$needsBuild = $Rebuild -or -not (Test-Path $OutputExe)

if (-not $needsBuild) {
    $latestSource = Get-ChildItem -Path (Join-Path $RepoRoot "src\asm\monolithic") -Filter "*.asm" -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1

    $latestScript = Get-Item $BuildScript
    $outputItem = Get-Item $OutputExe

    if (($null -ne $latestSource -and $latestSource.LastWriteTimeUtc -gt $outputItem.LastWriteTimeUtc) -or
        $latestScript.LastWriteTimeUtc -gt $outputItem.LastWriteTimeUtc) {
        $needsBuild = $true
    }
}

if ($needsBuild) {
    & $BuildWrapper -Config $Config
}

if (-not (Test-Path $OutputExe)) {
    throw "Launch target not found: $OutputExe"
}

Start-Process -FilePath $OutputExe | Out-Null
Write-Host "Launched $OutputExe" -ForegroundColor Green
