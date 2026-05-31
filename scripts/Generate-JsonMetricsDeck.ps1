<#
.SYNOPSIS
  Install Python deps (once) and generate Stripe/Anthropic-style PDF deck from JSON (charts + competitor framing).

.EXAMPLE
  .\scripts\Generate-JsonMetricsDeck.ps1
  .\scripts\Generate-JsonMetricsDeck.ps1 -Inputs @('logs\command_usage_runtime.json','tools\json_metrics_deck\examples\competitor_positioning.json') -OutPdf 'reports\metrics_deck.pdf'
#>
param(
    [string[]]$Inputs = @(
        (Join-Path $PSScriptRoot '..\logs\command_usage_runtime.json'),
        (Join-Path $PSScriptRoot '..\tools\json_metrics_deck\examples\competitor_positioning.json')
    ),
    [string]$OutPdf = (Join-Path $PSScriptRoot '..\reports\metrics_deck.pdf'),
    [string]$OutHtml = '',
    [string]$PythonExe = ''
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$toolDir = Join-Path $root 'tools\json_metrics_deck'
$venv = Join-Path $toolDir '.venv'
$py = Join-Path $venv 'Scripts\python.exe'
if (-not $PythonExe) {
    foreach ($c in @(
            (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'),
            (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\python.exe'))) {
        if (Test-Path $c) { $PythonExe = $c; break }
    }
}
if (-not $PythonExe) { $PythonExe = 'python' }

if (-not (Test-Path $py)) {
    & $PythonExe -m venv $venv
    & $py -m pip install -q -r (Join-Path $toolDir 'requirements.txt')
}

$outAbs = if ([System.IO.Path]::IsPathRooted($OutPdf)) { $OutPdf } else { Join-Path $root $OutPdf }
$existing = [System.Collections.Generic.List[string]]::new()
foreach ($i in $Inputs) {
    $p = if ([System.IO.Path]::IsPathRooted($i)) { $i } else { Join-Path $root $i }
    if (Test-Path $p) { $existing.Add($p) }
    else { Write-Warning "Skip missing: $p" }
}
if ($existing.Count -eq 0) {
    throw "No input JSON files found. Pass -Inputs with valid paths."
}

$argList = @((Join-Path $toolDir 'generate_pdf_deck.py'), '--out', $outAbs, '--inputs') + $existing
if ($OutHtml) {
    $htmlAbs = if ([System.IO.Path]::IsPathRooted($OutHtml)) { $OutHtml } else { Join-Path $root $OutHtml }
    $argList += @('--html', $htmlAbs)
}

& $py @argList
