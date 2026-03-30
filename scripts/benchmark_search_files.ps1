#requires -Version 5.1
<#
.SYNOPSIS
  Measures client and server-reported latency for POST /api/search-files (RawrXD Win32IDE local server).

.DESCRIPTION
  Request body matches Win32IDE_LocalServer: path, pattern, query, maxResults, searchContent.
  Server may return query_ms and backend (recursive-scan). Use -MaxP95Ms to fail CI when p95 exceeds a budget.

.PARAMETER SkipLaunch
  Do not start the IDE exe; assume something is already listening on BaseUrl.

.PARAMETER MaxP95Ms
  If set, exit code 2 when measured client p95 exceeds this value (warmup excluded).
#>
param(
    [string]$BaseUrl = "http://127.0.0.1:11435",
    [string]$Root = "",
    [string]$Query = "Win32IDE_LocalServer",
    [string]$Pattern = "*.cpp",
    [int]$MaxResults = 64,
    [int]$Iterations = 40,
    [int]$Warmup = 6,
    [int]$RequestTimeoutSec = 30,
    [string]$OutJson = "",
    [string]$ExePath = "",
    [switch]$SkipLaunch,
    [switch]$KillOnExit,
    [double]$MaxP95Ms = 0
)

$ErrorActionPreference = "Stop"

function Resolve-DefaultRoot {
    $here = Split-Path -Parent $PSScriptRoot
    if (Test-Path (Join-Path $here "src\win32app")) { return $here }
    return $PSScriptRoot
}

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Resolve-DefaultRoot
}
$Root = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path

function Get-Percentile {
    param([double[]]$Values, [double]$P)
    if ($null -eq $Values -or $Values.Count -eq 0) { return 0.0 }
    $s = [double[]]($Values | Sort-Object)
    $n = $s.Length
    if ($n -eq 1) { return $s[0] }
    $rank = ($P / 100.0) * ($n - 1)
    $lo = [int][math]::Floor($rank)
    $hi = [int][math]::Ceiling($rank)
    if ($lo -eq $hi) { return $s[$lo] }
    $w = $rank - $lo
    return $s[$lo] * (1.0 - $w) + $s[$hi] * $w
}

function Find-DefaultExe {
    $candidates = @(
        (Join-Path (Split-Path -Parent $PSScriptRoot) "build\bin\RawrXD-Win32IDE.exe"),
        (Join-Path (Split-Path -Parent $PSScriptRoot) "build\dist\rawrxd.exe"),
        "D:\rxdn\bin\RawrXD-Win32IDE.exe"
    ) | Where-Object { $_ -and (Test-Path $_) }
    if ($candidates.Count -ge 1) { return $candidates[0] }
    return $null
}

$launched = $null
if (-not $SkipLaunch) {
    if ([string]::IsNullOrWhiteSpace($ExePath)) {
        $ExePath = Find-DefaultExe
    }
    if (-not $ExePath -or -not (Test-Path $ExePath)) {
        Write-Error "No ExePath and no default build found. Pass -ExePath or -SkipLaunch with IDE already running."
    }
    $launched = Start-Process -FilePath $ExePath -PassThru -WindowStyle Minimized
    Start-Sleep -Seconds 5
    if ($launched.HasExited) {
        Write-Error "Launched process exited immediately (pid=$($launched.Id) code=$($launched.ExitCode)). Use headless server or -SkipLaunch."
    }
}

try {
    $healthUrl = ($BaseUrl.TrimEnd("/")) + "/health"
    try {
        Invoke-RestMethod -Method Get -Uri $healthUrl -TimeoutSec 3 | Out-Null
    } catch {
        Write-Warning "Health check failed: $($_.Exception.Message). Continuing with search requests."
    }

    $url = ($BaseUrl.TrimEnd("/")) + "/api/search-files"
    $bodyObj = @{
        path          = $Root
        pattern       = $Pattern
        query         = $Query
        maxResults    = $MaxResults
        searchContent = $true
    }
    $jsonBody = $bodyObj | ConvertTo-Json -Compress

    $clientMs = [System.Collections.Generic.List[double]]::new()
    $serverMs = [System.Collections.Generic.List[double]]::new()
    $failures = 0
    $backendSample = $null

    $totalRuns = $Warmup + $Iterations
    for ($i = 0; $i -lt $totalRuns; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            $resp = Invoke-WebRequest -Method Post -Uri $url -Body $jsonBody -ContentType "application/json; charset=utf-8" -TimeoutSec $RequestTimeoutSec -UseBasicParsing
            $sw.Stop()
            if ($resp.StatusCode -ne 200) { $failures++; continue }
            $parsed = $resp.Content | ConvertFrom-Json
            if ($parsed.backend) { $backendSample = [string]$parsed.backend }
            if ($i -ge $Warmup) {
                $clientMs.Add([double]$sw.Elapsed.TotalMilliseconds)
                if ($null -ne $parsed.query_ms) {
                    $serverMs.Add([double]$parsed.query_ms)
                }
            }
        } catch {
            $sw.Stop()
            $failures++
            Write-Warning "Request failed: $($_.Exception.Message)"
        }
    }

    $arr = $clientMs.ToArray()
    if ($arr.Length -eq 0) {
        Write-Host "No successful measured iterations (after warmup). Failures: $failures / $totalRuns" -ForegroundColor Red
        exit 1
    }
    $summary = [ordered]@{
        baseUrl       = $BaseUrl
        root          = $Root
        query         = $Query
        pattern       = $Pattern
        maxResults    = $MaxResults
        warmup        = $Warmup
        iterations    = $Iterations
        failures      = $failures
        client_avg_ms = if ($arr.Length) { ($arr | Measure-Object -Average).Average } else { 0 }
        client_p50_ms = Get-Percentile -Values $arr -P 50
        client_p95_ms = Get-Percentile -Values $arr -P 95
        server_avg_ms = if ($serverMs.Count) { ($serverMs | Measure-Object -Average).Average } else { $null }
        server_p50_ms = if ($serverMs.Count) { Get-Percentile -Values ($serverMs.ToArray()) -P 50 } else { $null }
        server_p95_ms = if ($serverMs.Count) { Get-Percentile -Values ($serverMs.ToArray()) -P 95 } else { $null }
        backend_sample = $backendSample
    }

    Write-Host ("Client latency: avg={0:N3} ms p50={1:N3} ms p95={2:N3} ms (n={3})" -f $summary.client_avg_ms, $summary.client_p50_ms, $summary.client_p95_ms, $arr.Length)
    if ($serverMs.Count -gt 0) {
        Write-Host ("Server query_ms: avg={0:N3} p50={1:N3} p95={2:N3}" -f $summary.server_avg_ms, $summary.server_p50_ms, $summary.server_p95_ms)
    } else {
        Write-Host "Server query_ms: (not present in response)"
    }
    if ($failures -gt 0) {
        Write-Warning "Failures: $failures / $totalRuns"
    }

    if ($OutJson) {
        ($summary | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath $OutJson -Encoding UTF8
        Write-Host "Wrote $OutJson"
    }

    if ($MaxP95Ms -gt 0 -and $summary.client_p95_ms -gt $MaxP95Ms) {
        Write-Host ("FAIL: client p95 {0:N3} ms exceeds gate {1} ms" -f $summary.client_p95_ms, $MaxP95Ms) -ForegroundColor Red
        exit 2
    }
    if ($MaxServerP95Ms -gt 0 -and $summary.server_p95_ms -and $summary.server_p95_ms -gt $MaxServerP95Ms) {
        Write-Host ("FAIL: server query_ms p95 {0:N3} ms exceeds gate {1} ms" -f $summary.server_p95_ms, $MaxServerP95Ms) -ForegroundColor Red
        exit 3
    }
    exit 0
} finally {
    if ($KillOnExit -and $null -ne $launched -and -not $launched.HasExited) {
        Stop-Process -Id $launched.Id -Force -ErrorAction SilentlyContinue
    }
}
