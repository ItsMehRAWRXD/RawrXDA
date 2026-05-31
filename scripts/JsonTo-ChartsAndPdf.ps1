#Requires -Version 5.1
<#
.SYNOPSIS
  Reads IDE/metrics JSON, emits a self-contained HTML page with Chart.js charts, optionally prints PDF via Edge headless.

.DESCRIPTION
  Default input: logs/command_usage_runtime.json (usage[] with category, attempts, ok, error).
  Outputs:
    - docs/deck/out/metrics_charts.html  (open in browser or print to PDF)
    - docs/deck/out/metrics_charts.pdf   (if Edge/Chromium found)

.EXAMPLE
  .\scripts\JsonTo-ChartsAndPdf.ps1
  .\scripts\JsonTo-ChartsAndPdf.ps1 -InputJson .\reports\BENCHMARK_CONFIG.json -SkipPdf
#>
param(
    [string] $InputJson = "",
    [string] $OutDir = "",
    [switch] $SkipPdf
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

if (-not $InputJson) {
    $InputJson = Join-Path $repoRoot "logs\command_usage_runtime.json"
}
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot "docs\deck\out"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not (Test-Path -LiteralPath $InputJson)) {
    Write-Warning "Input JSON not found: $InputJson — writing demo data stub."
    $demo = @{
        telemetryEnabled = $true
        usage            = @(
            @{ id = 1; category = "File"; attempts = 12; ok = 11; error = 1 }
            @{ id = 2; category = "View"; attempts = 8; ok = 8; error = 0 }
            @{ id = 3; category = "Terminal"; attempts = 24; ok = 22; error = 2 }
            @{ id = 4; category = "Agent"; attempts = 15; ok = 14; error = 1 }
        )
    } | ConvertTo-Json -Depth 6
    $raw = $demo
} else {
    $raw = Get-Content -LiteralPath $InputJson -Raw -Encoding UTF8
}

$obj = $raw | ConvertFrom-Json
$rows = @()
if ($obj.usage) {
    $rows = @($obj.usage)
} elseif ($obj.PSObject.Properties.Name -contains "series") {
    # alternate schema
    $rows = @($obj.series)
}

# Aggregate by category
$agg = @{}
foreach ($r in $rows) {
    $cat = [string]$r.category
    if (-not $cat) { $cat = "Other" }
    if (-not $agg.ContainsKey($cat)) {
        $agg[$cat] = @{ attempts = 0; ok = 0; error = 0 }
    }
    $agg[$cat].attempts += [int]($r.attempts)
    $agg[$cat].ok += [int]($r.ok)
    $agg[$cat].error += [int]($r.error)
}

$labels = @($agg.Keys | Sort-Object)
$attemptsData = @($labels | ForEach-Object { $agg[$_].attempts })
$okData = @($labels | ForEach-Object { $agg[$_].ok })
$errData = @($labels | ForEach-Object { $agg[$_].error })

$labelsJson = ($labels | ForEach-Object { '"' + ($_ -replace '\\', '\\\\' -replace '"', '\"') + '"' }) -join ","
$attemptsJson = ($attemptsData -join ",")
$okJson = ($okData -join ",")
$errJson = ($errData -join ",")

$htmlPath = Join-Path $OutDir "metrics_charts.html"
$srcName = Split-Path $InputJson -Leaf

$html = @"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>RawrXD — Metrics Charts</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
  <style>
    :root {
      --bg: #0c0c0e;
      --card: #16161a;
      --text: #f4f1eb;
      --muted: #8a8580;
      --accent: #c4a574;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0; font-family: "IBM Plex Sans", system-ui, sans-serif;
      background: var(--bg); color: var(--text);
      min-height: 100vh; padding: 2.5rem 1.5rem 4rem;
    }
    h1 { font-weight: 500; font-size: 1.35rem; letter-spacing: -0.02em; margin: 0 0 0.35rem; }
    p.meta { color: var(--muted); font-size: 0.85rem; margin: 0 0 2rem; }
    .grid { display: grid; gap: 1.5rem; max-width: 960px; margin: 0 auto; }
    .card {
      background: var(--card); border-radius: 12px; padding: 1.25rem 1.25rem 1.5rem;
      border: 1px solid rgba(244,241,235,0.06);
    }
    .card h2 { font-size: 0.75rem; text-transform: uppercase; letter-spacing: 0.12em; color: var(--muted); margin: 0 0 1rem; font-weight: 600; }
    canvas { max-height: 320px !important; }
    @media print {
      body { background: #fff; color: #111; }
      .card { border-color: #ddd; break-inside: avoid; }
    }
  </style>
  <link href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;500;600&display=swap" rel="stylesheet"/>
</head>
<body>
  <div class="grid">
    <div>
      <h1>Command &amp; handler usage</h1>
      <p class="meta">Source: $srcName — generated $(Get-Date -Format "yyyy-MM-dd HH:mm")</p>
    </div>
    <div class="card">
      <h2>Attempts by category</h2>
      <canvas id="c1"></canvas>
    </div>
    <div class="card">
      <h2>Success vs errors (stacked)</h2>
      <canvas id="c2"></canvas>
    </div>
  </div>
  <script>
    const labels = [$labelsJson];
    const attempts = [$attemptsJson];
    const ok = [$okJson];
    const err = [$errJson];
    Chart.defaults.color = '#c9c4bc';
    Chart.defaults.borderColor = 'rgba(244,241,235,0.08)';
    new Chart(document.getElementById('c1'), {
      type: 'bar',
      data: {
        labels,
        datasets: [{
          label: 'Attempts',
          data: attempts,
          backgroundColor: 'rgba(196,165,116,0.75)',
          borderRadius: 6
        }]
      },
      options: {
        responsive: true,
        plugins: { legend: { display: false } },
        scales: {
          x: { ticks: { maxRotation: 45, minRotation: 0 } },
          y: { beginAtZero: true }
        }
      }
    });
    new Chart(document.getElementById('c2'), {
      type: 'bar',
      data: {
        labels,
        datasets: [
          { label: 'OK', data: ok, backgroundColor: 'rgba(107,140,206,0.85)', borderRadius: { topLeft: 4, topRight: 4 } },
          { label: 'Error', data: err, backgroundColor: 'rgba(217,119,87,0.85)', borderRadius: { topLeft: 4, topRight: 4 } }
        ]
      },
      options: {
        responsive: true,
        scales: {
          x: { stacked: true, ticks: { maxRotation: 45 } },
          y: { stacked: true, beginAtZero: true }
        }
      }
    });
  </script>
</body>
</html>
"@

Set-Content -LiteralPath $htmlPath -Value $html -Encoding UTF8
Write-Host "Wrote $htmlPath"

if ($SkipPdf) { return }

function Find-EdgeOrChrome {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
        "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
        "${env:LocalAppData}\Google\Chrome\Application\chrome.exe",
        "$env:ProgramFiles\Google\Chrome\Application\chrome.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path -LiteralPath $p) { return $p }
    }
    return $null
}

$browser = Find-EdgeOrChrome
if (-not $browser) {
    Write-Warning "No Edge/Chrome found — open metrics_charts.html in a browser and use Print → Save as PDF."
    return
}

$pdfPath = Join-Path $OutDir "metrics_charts.pdf"
$uri = ([Uri](Resolve-Path $htmlPath)).AbsoluteUri
$argList = @(
    "--headless", "--disable-gpu", "--no-pdf-header-footer",
    "--print-to-pdf=$pdfPath",
    $uri
)
try {
    & $browser @argList
    if (Test-Path -LiteralPath $pdfPath) {
        Write-Host "Wrote $pdfPath"
    } else {
        Write-Warning "PDF not created; print $htmlPath manually."
    }
} catch {
    Write-Warning $_.Exception.Message
}
