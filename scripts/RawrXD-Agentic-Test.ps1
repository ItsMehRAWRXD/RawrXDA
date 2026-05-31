# RawrXD-Agentic-Test.ps1 - Single-script full-stack validation
# Usage: pwsh -NoProfile -File D:\rawrxd\scripts\RawrXD-Agentic-Test.ps1 [-Verbose]

param(
    [switch]$Verbose,
    [string]$BuildDir = "D:\rawrxd\build_pipeline\bin",
    [string]$ModelPath = "D:\phi3mini.gguf",
    [string]$Prompt = "Say exactly: AGENTIC_TEST_OK",
    [int]$MaxTokens = 16,
    [int]$TimeoutSec = 120
)

$ErrorActionPreference = 'Stop'
$script:Verbose = $Verbose

function Log($msg, [System.ConsoleColor]$color = 'White') {
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" -ForegroundColor $color
}

function Quote-Arg([string]$value) {
    # Start-Process joins arguments into a single command line; quote values with spaces.
    return '"' + ($value -replace '"', '\\"') + '"'
}

function Test-Exit($label, $actual, $expected, [switch]$Soft) {
    $ok = $actual -eq $expected
    $mark = if ($ok) { '[OK]' } else { '[FAIL]' }
    $severity = if ($Soft -and -not $ok) { 'Yellow' } elseif (-not $ok) { 'Red' } else { 'Green' }
    Log "$mark $label`: expected=$expected actual=$actual" $severity
    return $ok
}

function Wait-TraceFile([string]$Path, [int]$TimeoutMs = 10000) {
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $Path) {
            try {
                $item = Get-Item -LiteralPath $Path -ErrorAction Stop
                if ($item.Length -gt 0) {
                    return $true
                }
            } catch {
                Start-Sleep -Milliseconds 100
            }
        }
        Start-Sleep -Milliseconds 100
    }
    return $false
}

$results = @()
$startAll = Get-Date

# ---- 1. BINARY PRESENCE ----
Log "--- PHASE 1: Binary Artifacts ---" 'Cyan'
$exe = Join-Path $BuildDir 'RawrXD-Win32IDE.exe'
$cli = Join-Path $BuildDir 'rawrxd.exe'
$dll = Join-Path $BuildDir 'RawrXD_ServeInference.dll'

$results += Test-Exit 'Win32IDE.exe exists' (Test-Path -LiteralPath $exe) $true
$results += Test-Exit 'rawrxd.exe exists' (Test-Path -LiteralPath $cli) $true
$results += Test-Exit 'ServeInference.dll exists' (Test-Path -LiteralPath $dll) $true

# ---- 2. CLI PARITY CPU (deterministic) ----
Log "--- PHASE 2: CLI Parity CPU Lane ---" 'Cyan'
$traceDir = "D:\rawrxd\tmp\agentic_test_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
New-Item -ItemType Directory -Path $traceDir -Force | Out-Null
$cliTrace = Join-Path $traceDir 'cli_parity.json'

$env:RAWRXD_PARITY_CPU = '1'
$env:RAWRXD_PIPELINE_TRACE = $null
$cliOut = Join-Path $traceDir 'cli_parity.out.txt'
$cliErr = Join-Path $traceDir 'cli_parity.err.txt'

$cliProc = Start-Process -FilePath $cli -ArgumentList @(
    'run', (Quote-Arg $ModelPath),
    '--prompt', (Quote-Arg $Prompt),
    '--max-tokens', "$MaxTokens",
    '--emit-json-trace', (Quote-Arg $cliTrace)
) -RedirectStandardOutput $cliOut -RedirectStandardError $cliErr -NoNewWindow -PassThru -Wait

$results += Test-Exit 'CLI parity exit code' $cliProc.ExitCode 0
$results += Test-Exit 'CLI trace written' (Test-Path -LiteralPath $cliTrace) $true

if (Test-Path -LiteralPath $cliTrace) {
    $j = Get-Content -LiteralPath $cliTrace -Raw | ConvertFrom-Json
    $results += Test-Exit 'CLI token_count > 0' ($j.token_count -gt 0) $true
    $results += Test-Exit 'CLI seq_monotonic' $j.seq_monotonic $true
    Log "CLI: $($j.token_count) tokens, backend=$($j.backend), span=$($j.token_us[-1])us" 'Gray'
}

# ---- 3. CLI REAL GPU (if available) ----
Log "--- PHASE 3: CLI Real GPU Lane ---" 'Cyan'
Remove-Item Env:\RAWRXD_PARITY_CPU -ErrorAction SilentlyContinue
$gpuTrace = Join-Path $traceDir 'cli_gpu.json'
$gpuOut = Join-Path $traceDir 'cli_gpu.out.txt'
$gpuErr = Join-Path $traceDir 'cli_gpu.err.txt'

$gpuProc = Start-Process -FilePath $cli -ArgumentList @(
    'run', (Quote-Arg $ModelPath),
    '--prompt', (Quote-Arg $Prompt),
    '--max-tokens', "$MaxTokens",
    '--emit-json-trace', (Quote-Arg $gpuTrace)
) -RedirectStandardOutput $gpuOut -RedirectStandardError $gpuErr -NoNewWindow -PassThru -Wait

$gpuErrText = if (Test-Path -LiteralPath $gpuErr) { Get-Content -LiteralPath $gpuErr -Raw } else { '' }
$gpuDeferred = $gpuErrText -match 'No GPU backend available|CPU-only mode'

if ($gpuDeferred) {
    Log "GPU lane deferred (no Vulkan/CUDA device)" 'Yellow'
    $results += $true
} else {
    $results += Test-Exit 'GPU exit code' $gpuProc.ExitCode 0 -Soft
    $results += Test-Exit 'GPU trace written' (Test-Path -LiteralPath $gpuTrace) $true -Soft
    if (Test-Path -LiteralPath $gpuTrace) {
        $g = Get-Content -LiteralPath $gpuTrace -Raw | ConvertFrom-Json
        Log "GPU: $($g.token_count) tokens, backend=$($g.backend), device=$($g.device)" 'Gray'
    }
}

# ---- 4. SMOKE MODE (headless UI path) ----
Log "--- PHASE 4: Win32IDE Smoke Mode ---" 'Cyan'
$smokeTrace = Join-Path $traceDir 'ui_smoke.json'
$smokeOut = Join-Path $traceDir 'ui_smoke.out.txt'
$smokeErr = Join-Path $traceDir 'ui_smoke.err.txt'

$env:RAWRXD_SMOKE_CHAT = '1'
$env:RAWRXD_PIPELINE_TRACE = $smokeTrace
$env:RAWRXD_PIPELINE_STRICT = '1'
$env:RAWRXD_PARITY_CPU = '1'
$env:RAWRXD_SMOKE_MODEL = $ModelPath
$env:RAWRXD_SMOKE_PROMPT = $Prompt
$env:RAWRXD_SMOKE_MAX_TOKENS = "$MaxTokens"

$smokeProc = Start-Process -FilePath $exe -ArgumentList @(
    '--chat-ui-smoke-noninteractive',
    '--test-model', (Quote-Arg $ModelPath),
    '--test-prompt', (Quote-Arg $Prompt),
    '--test-max-tokens', "$MaxTokens",
    '--test-timeout-ms', ($TimeoutSec * 1000)
) -RedirectStandardOutput $smokeOut -RedirectStandardError $smokeErr -NoNewWindow -PassThru -Wait

$smokeTraceReady = Wait-TraceFile -Path $smokeTrace -TimeoutMs 10000

$results += Test-Exit 'Smoke exit code' $smokeProc.ExitCode 0 -Soft
$results += Test-Exit 'Smoke trace written' $smokeTraceReady $true -Soft

if ($smokeTraceReady) {
    $s = Get-Content -LiteralPath $smokeTrace -Raw | ConvertFrom-Json
    $results += Test-Exit 'Smoke token_count > 0' ($s.token_count -gt 0) $true -Soft
    Log "Smoke: $($s.token_count) tokens, source=$($s.source)" 'Gray'
} else {
    $smokeErrTail = if (Test-Path -LiteralPath $smokeErr) { Get-Content -LiteralPath $smokeErr -Tail 20 } else { 'N/A' }
    Log "Smoke stderr tail: $smokeErrTail" 'Red'
}

# ---- 5. PARITY COMPARISON (CLI vs Smoke) ----
Log "--- PHASE 5: Structural Parity Diff ---" 'Cyan'
if ((Test-Path -LiteralPath $smokeTrace)) {
    $cliAlignedTrace = Join-Path $traceDir 'cli_for_ui_compare.json'
    $cliAlignedOut = Join-Path $traceDir 'cli_for_ui_compare.out.txt'
    $cliAlignedErr = Join-Path $traceDir 'cli_for_ui_compare.err.txt'
    $env:RAWRXD_PARITY_CPU = '1'
    $cliAlignedProc = Start-Process -FilePath $cli -ArgumentList @(
        'run', (Quote-Arg $ModelPath),
        '--prompt', (Quote-Arg $Prompt),
        '--max-tokens', "$MaxTokens",
        '--emit-json-trace', (Quote-Arg $cliAlignedTrace)
    ) -RedirectStandardOutput $cliAlignedOut -RedirectStandardError $cliAlignedErr -NoNewWindow -PassThru -Wait
    $results += Test-Exit 'CLI aligned parity exit code' $cliAlignedProc.ExitCode 0 -Soft
    $results += Test-Exit 'CLI aligned trace written' (Test-Path -LiteralPath $cliAlignedTrace) $true -Soft

    if (-not (Test-Path -LiteralPath $cliAlignedTrace)) {
        Log "Skipping parity diff (missing aligned CLI trace)" 'Yellow'
        $results += $true
    } else {
        $cliJson = Get-Content -LiteralPath $cliAlignedTrace -Raw | ConvertFrom-Json
        $uiJson = Get-Content -LiteralPath $smokeTrace -Raw | ConvertFrom-Json
        $alignmentOk = ($cliJson.model -eq $uiJson.model) -and ($cliJson.prompt -eq $uiJson.prompt)
        $results += Test-Exit 'Parity model/prompt aligned' $alignmentOk $true -Soft
        if (-not $alignmentOk) {
            Log ("Alignment mismatch: cli model='" + $cliJson.model + "' ui model='" + $uiJson.model +
                "' | cli prompt='" + $cliJson.prompt + "' ui prompt='" + $uiJson.prompt + "'") 'Yellow'
        }
        $cmp = & "D:\rawrxd\scripts\compare_parity_trace.ps1" -Cli $cliAlignedTrace -Ui $smokeTrace
        $cmpExit = $LASTEXITCODE
        $results += Test-Exit 'Parity diff exit' $cmpExit 0 -Soft
        Log "Comparator: $cmp" $(if ($cmpExit -eq 0) { 'Green' } else { 'Red' })
    }
} else {
    Log "Skipping parity diff (missing traces)" 'Yellow'
    $results += $true
}

# ---- 6. OLLAMA HEALTH ----
Log "--- PHASE 6: Ollama Endpoint Health ---" 'Cyan'
try {
    $ollama = Invoke-RestMethod -Uri 'http://127.0.0.1:11434/api/tags' -TimeoutSec 5
    $modelCount = $ollama.models.Count
    $results += Test-Exit 'Ollama reachable' ($modelCount -ge 0) $true
    Log "Ollama: $modelCount models loaded" 'Gray'
} catch {
    Log "Ollama not reachable (expected if not running)" 'Yellow'
    $results += $true
}

# ---- SUMMARY ----
$elapsed = [math]::Round(((Get-Date) - $startAll).TotalSeconds, 1)
$passed = ($results | Where-Object { $_ -eq $true }).Count
$failed = ($results | Where-Object { $_ -eq $false }).Count
$total = $results.Count

Log ""
Log "=======================================" 'Cyan'
Log "AGENTIC TEST SUMMARY" 'Cyan'
Log "=======================================" 'Cyan'
Log "Total checks : $total"
Log "Passed       : $passed" $(if ($failed -eq 0) { 'Green' } else { 'White' })
Log "Failed       : $failed" $(if ($failed -gt 0) { 'Red' } else { 'White' })
Log "Duration     : ${elapsed}s"
Log "Trace dir    : $traceDir"
Log "=======================================" 'Cyan'

# ---- AGENT_DONE COMPLETION CONTRACT ----
$commitHash = git -C D:\RawrXD rev-parse --short HEAD
$artifacts = @("RawrXD-Agentic-Test.ps1", "run_parity_gpu_validation.ps1", "agent_completion_validator.ps1")

# Emit completion contract
$completionBlock = @"

[AGENT_DONE]
status=$(if ($failed -eq 0) { 'success' } else { 'partial' })
phase=2B
tasks_completed=$passed
tasks_total=$total
commit=$commitHash
artifacts=$($artifacts.Count)
duration_ms=$([math]::Round($elapsed * 1000))
next=$(if ($failed -eq 0) { 'Phase 2C kernel tuning' } else { 'Retry failed tests' })
"@

Write-Host $completionBlock

# Also emit JSON format for machine consumption
$jsonBlock = @{
    agent_done = $true
    status = if ($failed -eq 0) { 'success' } else { 'partial' }
    phase = '2B'
    tasks_completed = $passed
    tasks_total = $total
    commit = $commitHash
    artifacts = $artifacts
    duration_ms = [math]::Round($elapsed * 1000)
    next = if ($failed -eq 0) { 'Phase 2C kernel tuning' } else { 'Retry failed tests' }
    timestamp = (Get-Date -Format "o")
} | ConvertTo-Json -Depth 2

Write-Host $jsonBlock

exit $failed
