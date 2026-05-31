param(
    [string]$RepoRoot = 'd:\rawrxd',
    [string]$BuildDir = 'd:\rawrxd\build',
    [string]$ModelPath = 'D:\TinyLlama-1.1B-Chat-v1.0.Q4_0.gguf',
    [string]$Prompt = 'say ok',
    [int]$MaxTokens = 8,
    [int]$BuildJobs = 1
)

$ErrorActionPreference = 'Stop'

$exePath = Join-Path $BuildDir 'bin\RawrXD-Win32IDE.exe'
$logDir = Join-Path $BuildDir 'forensics'
$logPath = Join-Path $logDir ('forensic_trace_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.txt')

Write-Host '[forensics] enforcing single build lane'
Get-CimInstance Win32_Process -Filter "name='ninja.exe'" -ErrorAction SilentlyContinue |
    ForEach-Object {
        try { Stop-Process -Id $_.ProcessId -Force -ErrorAction Stop } catch {}
    }

Write-Host '[forensics] building canonical target'
& ninja -C $BuildDir RawrXD-Win32IDE -j $BuildJobs
if ($LASTEXITCODE -ne 0) {
    throw "canonical build failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $exePath)) {
    throw "canonical executable missing at $exePath"
}

if (-not (Test-Path $ModelPath)) {
    throw "model file missing at $ModelPath"
}

New-Item -ItemType Directory -Path $logDir -Force | Out-Null

$env:RAWRXD_STREAMING_MATMUL_TRACE = '1'
$env:RAWRXD_TRANSFORMER_CALLSITE_TRACE = '1'
$env:RAWRXD_STREAMING_MATMUL_SUMMARY = '1'

Write-Host "[forensics] running headless trace -> $logPath"
Push-Location (Split-Path $exePath)
try {
    & $exePath --headless --model $ModelPath --prompt $Prompt --max-tokens $MaxTokens *> $logPath
    $runCode = $LASTEXITCODE
} finally {
    Pop-Location
}

Write-Host "[forensics] headless exit code: $runCode"
if (-not (Test-Path $logPath)) {
    throw 'trace log was not created'
}

$logLines = Get-Content -Path $logPath -ErrorAction Stop
$streamLines = $logLines | Where-Object { $_ -like '*[StreamingMatMul] PERF*' }
$callsiteLines = $logLines | Where-Object { $_ -like '*[ForwardMatMul]*' }

if ($streamLines.Count -eq 0) {
    throw 'no StreamingMatMul PERF lines found in log'
}

if ($callsiteLines.Count -eq 0) {
    throw 'no ForwardMatMul lines found in log'
}

$lastPerf = $streamLines[-1]

$gpuDispatch = 0.0
$cacheHitRate = 0.0
if ($lastPerf -match 'gpu_dispatch_ms=([0-9]+(?:\.[0-9]+)?)') {
    $gpuDispatch = [double]$Matches[1]
}
if ($lastPerf -match 'cache_hit_rate=([0-9]+(?:\.[0-9]+)?)') {
    $cacheHitRate = [double]$Matches[1]
}

$phaseTotals = @{}
foreach ($line in $callsiteLines) {
    if ($line -match 'phase=([^ ]+)') {
        $phase = $Matches[1]
        $ms = 0.0
        if ($line -match 'ms=([0-9]+(?:\.[0-9]+)?)') {
            $ms = [double]$Matches[1]
        }
        if (-not $phaseTotals.ContainsKey($phase)) {
            $phaseTotals[$phase] = 0.0
        }
        $phaseTotals[$phase] += $ms
    }
}

$dominantPhase = 'none'
$dominantMs = 0.0
foreach ($k in $phaseTotals.Keys) {
    if ($phaseTotals[$k] -gt $dominantMs) {
        $dominantMs = $phaseTotals[$k]
        $dominantPhase = $k
    }
}

Write-Host '[forensics] signal summary'
Write-Host ('  gpu_dispatch_ms: ' + $gpuDispatch.ToString('F3'))
Write-Host ('  cache_hit_rate: ' + $cacheHitRate.ToString('F2'))
Write-Host ('  dominant_phase: ' + $dominantPhase + ' (' + $dominantMs.ToString('F3') + ' ms)')
Write-Host ('  log: ' + $logPath)

if ($runCode -ne 0) {
    exit $runCode
}

exit 0
