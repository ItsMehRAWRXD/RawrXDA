param(
    [string]$Bin = "D:\rawrxd\build_pipeline\bin\RawrXD-Win32IDE.exe",
    [string]$Model = "D:\phi3mini.gguf",
    [string]$Trace = "D:\rawrxd\tmp\smoke_trace.json",
    [int]$TimeoutMs = 120000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host "=== RAWRXD AGENTIC VALIDATION START ==="

# -----------------------------
# 1. BUILD VALIDATION
# -----------------------------
Write-Host "`n[1/4] Checking binary..."

if (!(Test-Path -LiteralPath $Bin)) {
    Write-Host "FAIL: binary missing"
    exit 1
}

$binInfo = Get-Item -LiteralPath $Bin
Write-Host "OK: binary found"
Write-Host ("Size: {0} bytes" -f $binInfo.Length)
Write-Host ("Timestamp: {0}" -f $binInfo.LastWriteTime)

if (!(Test-Path -LiteralPath $Model)) {
    Write-Host "FAIL: model missing"
    exit 1
}

# -----------------------------
# 2. ENV SETUP (deterministic)
# -----------------------------
Write-Host "`n[2/4] Setting runtime environment..."

$traceDir = Split-Path -Parent $Trace
if (![string]::IsNullOrWhiteSpace($traceDir) -and !(Test-Path -LiteralPath $traceDir)) {
    New-Item -ItemType Directory -Path $traceDir -Force | Out-Null
}

$env:RAWRXD_SMOKE_CHAT = "1"
$env:RAWRXD_PIPELINE_TRACE = $Trace
$env:RAWRXD_PIPELINE_STRICT = "1"
$env:RAWRXD_PARITY_CPU = "1"
$env:RAWRXD_SMOKE_MODEL = $Model
$env:RAWRXD_SMOKE_PROMPT = "Reply with exactly: OK"

if (Test-Path -LiteralPath $Trace) {
    Remove-Item -LiteralPath $Trace -ErrorAction SilentlyContinue
}

# -----------------------------
# 3. EXECUTION
# -----------------------------
Write-Host "`n[3/4] Running smoke execution..."

$args = @(
    "--chat-ui-smoke-noninteractive",
    "--test-model", $Model,
    "--test-prompt", "Reply with exactly: OK",
    "--test-timeout-ms", "$TimeoutMs"
)

$proc = Start-Process `
    -FilePath $Bin `
    -ArgumentList $args `
    -NoNewWindow `
    -PassThru `
    -Wait

$exit = $proc.ExitCode
Write-Host "Process exit code: $exit"

# -----------------------------
# 4. VALIDATION
# -----------------------------
Write-Host "`n[4/4] Validating artifacts..."

$traceExists = Test-Path -LiteralPath $Trace
$traceNonEmpty = $false

if ($traceExists) {
    $traceInfo = Get-Item -LiteralPath $Trace
    Write-Host "TRACE: FOUND"
    Write-Host ("Size: {0}" -f $traceInfo.Length)
    Write-Host ("Time: {0}" -f $traceInfo.LastWriteTime)

    $content = Get-Content -LiteralPath $Trace -Raw
    if (![string]::IsNullOrWhiteSpace($content)) {
        Write-Host "TRACE_CONTENT: OK"
        $traceNonEmpty = $true
    } else {
        Write-Host "TRACE_CONTENT: EMPTY (FAIL)"
    }
} else {
    Write-Host "TRACE: MISSING (FAIL)"
}

# -----------------------------
# FINAL GATE
# -----------------------------
Write-Host "`n=== RESULT ==="

if ($exit -eq 0 -and $traceExists -and $traceNonEmpty) {
    Write-Host "PASS: FULL PIPELINE VALIDATED"
    exit 0
}

if ($exit -ne 0) {
    Write-Host "FAIL: runtime failed (non-zero exit)"
} elseif (!$traceExists) {
    Write-Host "FAIL: trace missing"
} else {
    Write-Host "FAIL: trace empty"
}

Write-Host "FAIL: PIPELINE BROKEN"
exit 2
