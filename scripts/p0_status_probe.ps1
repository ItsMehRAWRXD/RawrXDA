param(
    [string]$ApiHost = "http://127.0.0.1:11435",
    [string]$OutDir = "."
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$endpoints = @(
    "/api/status",
    "/api/hotpatch/status",
    "/api/governor/status",
    "/api/quantum/status",
    "/api/router/status",
    "/api/backend/status",
    "/api/hybrid/status",
    "/api/swarm/status",
    "/api/debug/status"
)

$results = @{}

foreach ($ep in $endpoints) {
    $url = "$ApiHost$ep"
    Write-Host "Querying $url" -ForegroundColor Cyan
    try {
        $resp = Invoke-RestMethod -Method Get -Uri $url -TimeoutSec 5
        $results[$ep] = $resp
    } catch {
        $results[$ep] = @{ error = $_.Exception.Message }
    }
}

$outPath = Join-Path $OutDir "p0_status_probe.json"
$results | ConvertTo-Json -Depth 6 | Set-Content -Path $outPath
Write-Host "Saved: $outPath" -ForegroundColor Green
