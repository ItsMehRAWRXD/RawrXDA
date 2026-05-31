#Requires -Version 5.1
<#
.SYNOPSIS
  Wrapper: JSON metrics → matplotlib charts → PDF (see JsonToChartsPdf.py).

.EXAMPLE
  .\Run-JsonToChartsPdf.ps1 -InputJson .\sample_metrics_for_charts.json -OutputPdf .\report.pdf
#>
param(
    [Parameter(Mandatory = $true)][string]$InputJson,
    [string]$OutputPdf = ""
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $InputJson)) {
    Write-Error "Input not found: $InputJson"
}

$py = Join-Path $PSScriptRoot "JsonToChartsPdf.py"
if (-not (Test-Path -LiteralPath $py)) {
    Write-Error "Missing: $py"
}

$args = @($py, (Resolve-Path $InputJson).Path)
if ($OutputPdf -ne "") {
    $args += @("-o", $OutputPdf)
}

& python @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
