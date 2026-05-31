#Requires -Version 5.1
<#
.SYNOPSIS
  Generates an institutional speculative benchmark report bundle from
  RawrXD-ComparativeBenchmark.json.

.EXAMPLE
  .\Run-SpeculativeInfraReport.ps1

.EXAMPLE
  .\Run-SpeculativeInfraReport.ps1 -InputJson .\build-win32\RawrXD-ComparativeBenchmark.json -OutDir .\reports\speculative_infra
#>
param(
    [string]$InputJson = "",
    [string]$OutDir = "",
    [string]$BenchDir = "",
    [switch]$NoPdf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$py = Join-Path $PSScriptRoot "speculative_infra_report.py"

if (-not $InputJson) {
    foreach ($p in @(
            (Join-Path $repoRoot "build-win32\RawrXD-ComparativeBenchmark.json"),
            (Join-Path $repoRoot "build-ninja\RawrXD-ComparativeBenchmark.json"),
            (Join-Path $repoRoot "build\RawrXD-ComparativeBenchmark.json"))) {
        if (Test-Path -LiteralPath $p) { $InputJson = $p; break }
    }
}
if (-not $InputJson) {
    $InputJson = Join-Path $repoRoot "build-win32\RawrXD-ComparativeBenchmark.json"
}
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot "reports\speculative_infra"
}

if (-not (Test-Path -LiteralPath $InputJson)) {
    throw "Input JSON not found: $InputJson"
}
if (-not (Test-Path -LiteralPath $py)) {
    throw "Missing script: $py"
}

$pyArgs = @($py, "--input", $InputJson, "--out-dir", $OutDir)
if ($NoPdf) {
    $pyArgs += "--no-pdf"
}
if ($BenchDir -and (Test-Path $BenchDir)) {
    $pyArgs += @("--bench-dir", $BenchDir)
}

& python @pyArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
