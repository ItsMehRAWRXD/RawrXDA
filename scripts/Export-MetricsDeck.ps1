#Requires -Version 5.1
<#
.SYNOPSIS
  Reads docs/deck JSON metrics, injects into HTML template (Chart.js charts), exports PDF via Edge headless.

.PARAMETER MetricsJson
  Path to metrics JSON (default: docs/deck/sample-metrics.json under repo root).

.PARAMETER OutDir
  Output directory for deck.html and deck.pdf (default: docs/deck/out).

.EXAMPLE
  cd D:\rawrxd
  .\scripts\Export-MetricsDeck.ps1
  .\scripts\Export-MetricsDeck.ps1 -MetricsJson .\logs\my-run.json
#>
[CmdletBinding()]
param(
    [string] $MetricsJson = "",
    [string] $OutDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
if (-not $MetricsJson) {
    $MetricsJson = Join-Path $repoRoot "docs\deck\sample-metrics.json"
}
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot "docs\deck\out"
}

if (-not (Test-Path -LiteralPath $MetricsJson)) {
    Write-Error "Metrics JSON not found: $MetricsJson"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$raw = Get-Content -LiteralPath $MetricsJson -Raw -Encoding UTF8
$null = $raw | ConvertFrom-Json

$bytes = [System.Text.Encoding]::UTF8.GetBytes($raw)
$b64 = [Convert]::ToBase64String($bytes)

$templatePath = Join-Path $repoRoot "docs\deck\metrics-deck.template.html"
if (-not (Test-Path -LiteralPath $templatePath)) {
    Write-Error "Template not found: $templatePath"
}

$tpl = Get-Content -LiteralPath $templatePath -Raw -Encoding UTF8
$html = $tpl.Replace("__METRICS_B64__", $b64)

$metaQuick = ($raw | ConvertFrom-Json).meta
$titleSafe = if ($metaQuick.title) { $metaQuick.title } else { "RawrXD Metrics Deck" }
$html = $html.Replace("__TITLE_PLACEHOLDER__", [System.Net.WebUtility]::HtmlEncode($titleSafe))

$outHtml = Join-Path $OutDir "deck.html"
$outPdf = Join-Path $OutDir "deck.pdf"
Set-Content -LiteralPath $outHtml -Value $html -Encoding UTF8

Write-Host "Wrote $outHtml"

function Find-Edge {
    $candidates = @(
        (Join-Path ${env:ProgramFiles} "Microsoft\Edge\Application\msedge.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft\Edge\Application\msedge.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }
    return $null
}

$edge = Find-Edge
if (-not $edge) {
    Write-Warning "Microsoft Edge not found — open deck.html in a browser and Print to PDF manually."
    exit 0
}

$htmlUri = ([Uri]((Get-Item -LiteralPath $outHtml).FullName)).AbsoluteUri
$argList = @(
    "--headless=new",
    "--disable-gpu",
    "--no-pdf-header-footer",
    "--print-to-pdf=$outPdf",
    $htmlUri
)
$p = Start-Process -FilePath $edge -ArgumentList $argList -Wait -PassThru -NoNewWindow
if ($p.ExitCode -ne 0) {
    Write-Warning "Edge headless exit $($p.ExitCode). Try opening $outHtml and printing to PDF."
} elseif (Test-Path -LiteralPath $outPdf) {
    Write-Host "Wrote $outPdf"
}
