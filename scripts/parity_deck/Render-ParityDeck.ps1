#Requires -Version 5.1
<#
.SYNOPSIS
  Renders a Stripe/Anthropic-inspired HTML deck with Chart.js from JSON, optionally prints PDF via Edge.

.DESCRIPTION
  - Input: parity deck JSON (see parity_deck.sample.json).
  - Optional merge: .rawrxd/metrics.json from IDE Help export (command 7006) for runtime counters.
  - Output: out/deck.html + out/deck.pdf (PDF requires Microsoft Edge).

.EXAMPLE
  .\Render-ParityDeck.ps1 -InputJson .\parity_deck.sample.json -OutDir .\out

.EXAMPLE
  .\Render-ParityDeck.ps1 -MergeMetricsFromRawrxd
#>
[CmdletBinding()]
param(
    [string] $InputJson = "",
    [string] $OutDir = "",
    [switch] $MergeMetricsFromRawrxd,
    [string] $MetricsJsonPath = "",
    [switch] $NoPdf
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $InputJson) { $InputJson = Join-Path $scriptRoot "parity_deck.sample.json" }
if (-not $OutDir) { $OutDir = Join-Path $scriptRoot "out" }

function Resolve-EdgeExe {
    $candidates = @(
        (Join-Path ${env:ProgramFiles} "Microsoft\Edge\Application\msedge.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft\Edge\Application\msedge.exe")
    )
    foreach ($p in $candidates) {
        if (Test-Path -LiteralPath $p) { return $p }
    }
    return $null
}

if (-not (Test-Path -LiteralPath $InputJson)) {
    throw "Input JSON not found: $InputJson"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$deckText = Get-Content -LiteralPath $InputJson -Raw -Encoding UTF8

$metricsBlock = "null"
if ($MergeMetricsFromRawrxd -or $MetricsJsonPath) {
    $repoRoot = (Resolve-Path (Join-Path $scriptRoot "..\..")).Path
    $mp = if ($MetricsJsonPath) {
        $MetricsJsonPath
    } elseif (Test-Path (Join-Path (Get-Location) ".rawrxd\metrics.json")) {
        (Join-Path (Get-Location) ".rawrxd\metrics.json")
    } elseif ($env:RAWRXD_REPO_ROOT -and (Test-Path -LiteralPath (Join-Path $env:RAWRXD_REPO_ROOT ".rawrxd\metrics.json"))) {
        Join-Path $env:RAWRXD_REPO_ROOT ".rawrxd\metrics.json"
    } else {
        Join-Path $repoRoot ".rawrxd\metrics.json"
    }
    if (Test-Path -LiteralPath $mp) {
        $raw = Get-Content -LiteralPath $mp -Raw -Encoding UTF8
        # Validate JSON
        $null = $raw | ConvertFrom-Json
        $metricsBlock = $raw
    }
}

$html = @"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>RawrXD Parity Deck</title>
  <link rel="preconnect" href="https://fonts.googleapis.com" />
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />
  <link href="https://fonts.googleapis.com/css2?family=DM+Sans:ital,opsz,wght@0,9..40,400;0,9..40,600;1,9..40,400&family=Instrument+Serif:ital@0;1&display=swap" rel="stylesheet" />
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.6/dist/chart.umd.min.js"></script>
  <style>
    :root {
      --bg0: #050507;
      --bg1: #0e0e14;
      --card: #12121a;
      --line: rgba(255,255,255,0.07);
      --muted: #9ca3af;
      --text: #f4f4f5;
      --accent-a: #c4b5fd;
      --accent-b: #fb923c;
      --radius: 14px;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "DM Sans", system-ui, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1e1b4b33, transparent 55%),
                  radial-gradient(900px 500px at 90% 0%, #43140733, transparent 50%),
                  linear-gradient(180deg, var(--bg0), var(--bg1));
      color: var(--text);
      min-height: 100vh;
    }
    .wrap { max-width: 1080px; margin: 0 auto; padding: 48px 28px 80px; }
    h1 {
      font-family: "Instrument Serif", Georgia, serif;
      font-weight: 400;
      font-size: clamp(2.1rem, 4vw, 2.75rem);
      letter-spacing: -0.02em;
      margin: 0 0 12px;
      background: linear-gradient(120deg, var(--accent-a), #fff 42%, var(--accent-b));
      -webkit-background-clip: text;
      background-clip: text;
      color: transparent;
    }
    .sub { color: var(--muted); font-size: 1.05rem; max-width: 52ch; line-height: 1.55; margin: 0 0 28px; }
    .grid { display: grid; gap: 18px; }
    @media (min-width: 900px) { .grid-2 { grid-template-columns: 1fr 1fr; } }
    .card {
      background: linear-gradient(145deg, rgba(255,255,255,0.04), rgba(255,255,255,0.01));
      border: 1px solid var(--line);
      border-radius: var(--radius);
      padding: 22px 22px 18px;
      box-shadow: 0 24px 80px rgba(0,0,0,0.35);
    }
    .card h2 {
      font-family: "Instrument Serif", Georgia, serif;
      font-size: 1.25rem;
      margin: 0 0 14px;
      font-weight: 400;
      letter-spacing: -0.01em;
    }
    table.pos {
      width: 100%;
      border-collapse: collapse;
      font-size: 0.92rem;
    }
    table.pos th, table.pos td {
      border-bottom: 1px solid var(--line);
      padding: 10px 8px;
      vertical-align: top;
      text-align: left;
    }
    table.pos th { color: var(--muted); font-weight: 600; font-size: 0.78rem; text-transform: uppercase; letter-spacing: 0.08em; }
    .intro { color: #d4d4d8; line-height: 1.65; font-size: 0.95rem; margin-bottom: 16px; }
    canvas { max-height: 320px; }
    pre.metrics {
      margin: 0;
      max-height: 280px;
      overflow: auto;
      font-size: 0.72rem;
      line-height: 1.45;
      color: #a1a1aa;
      background: #0a0a0f;
      border-radius: 10px;
      padding: 14px;
      border: 1px solid var(--line);
    }
    .foot {
      margin-top: 36px;
      font-size: 0.82rem;
      color: #71717a;
      line-height: 1.5;
    }
    .badge {
      display: inline-block;
      font-size: 0.68rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      padding: 4px 10px;
      border-radius: 999px;
      border: 1px solid var(--line);
      color: var(--muted);
      margin-bottom: 18px;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="badge">Positioning · Ollama / vLLM / RawrXD</div>
    <h1 id="deck-title">Loading…</h1>
    <p class="sub" id="deck-sub"></p>

    <div class="grid">
      <div class="card">
        <h2>Competitive framing</h2>
        <p class="intro" id="pos-intro"></p>
        <table class="pos" id="pos-table" aria-label="Competitor comparison"></table>
      </div>
      <div class="card" id="metrics-card" style="display:none">
        <h2>Runtime metrics snapshot</h2>
        <p class="intro">Merged from <code>.rawrxd/metrics.json</code> (IDE export).</p>
        <pre class="metrics" id="metrics-pre"></pre>
      </div>
    </div>

    <div class="grid grid-2" style="margin-top:18px">
      <div class="card"><h2 id="t1"></h2><canvas id="c-throughput"></canvas></div>
      <div class="card"><h2 id="t2"></h2><canvas id="c-ttft"></canvas></div>
    </div>

    <div class="card" style="margin-top:18px">
      <h2 id="ts-title"></h2>
      <canvas id="c-series"></canvas>
    </div>

    <p class="foot" id="deck-foot"></p>
  </div>

  <script id="deck-data" type="application/json">__DECK_JSON__</script>
  <script id="metrics-data" type="application/json">__METRICS_JSON__</script>
  <script>
    const deck = JSON.parse(document.getElementById('deck-data').textContent);
    const metricsRaw = document.getElementById('metrics-data').textContent.trim();
    const metrics = metricsRaw === 'null' ? null : JSON.parse(metricsRaw);

    document.getElementById('deck-title').textContent = deck.deck.title;
    document.getElementById('deck-sub').textContent = deck.deck.subtitle;
    document.getElementById('deck-foot').textContent = deck.deck.footnote || '';
    document.getElementById('pos-intro').textContent = deck.positioning.intro || '';

    const cols = deck.positioning.columns;
    const thead = '<tr><th>Dimension</th>' + cols.map(c => '<th>' + c + '</th>').join('') + '</tr>';
    const rows = deck.positioning.rows.map(r =>
      '<tr><td><strong>' + r.label + '</strong></td>' +
      r.values.map(v => '<td>' + v + '</td>').join('') + '</tr>'
    ).join('');
    document.getElementById('pos-table').innerHTML = thead + rows;

    Chart.defaults.color = '#a1a1aa';
    Chart.defaults.borderColor = 'rgba(255,255,255,0.08)';
    const mkBar = (spec, canvasId) => {
      const el = document.getElementById(canvasId);
      if (!el) return;
      new Chart(el, {
        type: 'bar',
        data: {
          labels: spec.labels,
          datasets: [{
            data: spec.values,
            backgroundColor: spec.colors || ['#6366f1','#a78bfa','#f472b6'],
            borderWidth: 0,
            borderRadius: 6
          }]
        },
        options: {
          plugins: { legend: { display: false } },
          scales: {
            x: { grid: { display: false } },
            y: { grid: { color: 'rgba(255,255,255,0.06)' }, beginAtZero: true }
          }
        }
      });
    };

    deck.charts.forEach(ch => {
      if (ch.id === 'throughput') {
        document.getElementById('t1').textContent = ch.title;
        mkBar(ch, 'c-throughput');
      }
      if (ch.id === 'ttft') {
        document.getElementById('t2').textContent = ch.title;
        mkBar(ch, 'c-ttft');
      }
    });

    const ts = deck.timeSeries;
    document.getElementById('ts-title').textContent = ts.title;
    const ds = ts.datasets.map(d => ({
      label: d.label,
      data: d.data,
      borderColor: d.borderColor || '#f59e0b',
      tension: 0.35,
      fill: d.fill !== false
    }));
    new Chart(document.getElementById('c-series'), {
      type: 'line',
      data: { labels: ts.labels, datasets: ds },
      options: {
        plugins: { legend: { labels: { color: '#d4d4d8' } } },
        scales: {
          x: { grid: { color: 'rgba(255,255,255,0.06)' } },
          y: { grid: { color: 'rgba(255,255,255,0.06)' }, beginAtZero: false }
        }
      }
    });

    if (metrics) {
      document.getElementById('metrics-card').style.display = 'block';
      document.getElementById('metrics-pre').textContent = JSON.stringify(metrics, null, 2);
    }
  </script>
</body>
</html>
"@

$html = $html.Replace("__DECK_JSON__", $deckText)
$html = $html.Replace("__METRICS_JSON__", $metricsBlock)

$outHtml = Join-Path $OutDir "deck.html"
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($outHtml, $html, $utf8NoBom)
Write-Host "Wrote $outHtml"

if (-not $NoPdf) {
    $edge = Resolve-EdgeExe
    if (-not $edge) {
        Write-Warning "Microsoft Edge not found; skip PDF. Open deck.html in a browser and print to PDF."
    } else {
        $outPdf = Join-Path $OutDir "deck.pdf"
        $resolved = (Resolve-Path -LiteralPath $outHtml).Path
        $uri = ([Uri]::new($resolved)).AbsoluteUri
        & $edge --headless --disable-gpu --print-to-pdf="$outPdf" $uri
        Start-Sleep -Milliseconds 500
        if (Test-Path -LiteralPath $outPdf) {
            Write-Host "Wrote $outPdf"
        } else {
            Write-Warning "PDF not found after Edge print; open $outHtml and use Print to PDF."
        }
    }
}
