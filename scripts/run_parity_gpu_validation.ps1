# run_parity_gpu_validation.ps1 — Real-model CLI/UI parity validation
#
# Runs the full inference path (no RAWRXD_PARITY_CPU) on a real GGUF model and
# compares CLI vs UI traces. Requires:
#   - A Vulkan/CUDA/HIP-capable GPU (gpu_enforcement gate must succeed)
#   - RawrXD_ServeInference.dll next to rawrxd.exe / RawrXD-Win32IDE.exe
#   - A real GGUF model file
#
# When hardware is missing the script exits with code 0 and a clear "deferred"
# message so it can be safely invoked from any CI lane without false failures.
# Use -Strict to convert deferral into a classified hard failure.

[CmdletBinding()]
param(
    [string]$BinDir   = "d:\rawrxd\build_pipeline\bin",
    [string]$OutDir   = "d:\rawrxd\build_pipeline\parity_gpu",
    [string]$Model    = "",
    [string]$Prompt   = "Say exactly: ready",
    [int]   $MaxTokens = 16,
    [int]   $MinRealTokens = 1,
    [string]$MatmulKernel = "",
    [string]$MatmulSpv = "",
    [switch]$DisableAutopatch,
    [switch]$Strict
)

$ErrorActionPreference = 'Stop'
$cliExe = Join-Path $BinDir "rawrxd.exe"
$uiExe  = Join-Path $BinDir "RawrXD-Win32IDE.exe"

$EXIT_GPU_NOT_FOUND    = 10
$EXIT_CPU_FALLBACK     = 20
$EXIT_TOKEN_MISMATCH   = 30
$EXIT_BACKEND_MISMATCH = 40
$EXIT_TRACE_INVALID    = 50

function Exit-Script([int]$code) {
    [Environment]::Exit($code)
}

function Defer([string]$why, [int]$strictExitCode = $EXIT_GPU_NOT_FOUND) {
    Write-Host "[DEFERRED] $why" -ForegroundColor Yellow
    if ($Strict) {
        Write-Host "[STRICT] Treating deferral as failure." -ForegroundColor Red
        Exit-Script $strictExitCode
    }
    Exit-Script 0
}

function Join-NativeArgs([string[]]$CliArgs) {
    ($CliArgs | ForEach-Object {
        if ($_ -eq $null) { '""'; return }
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\\"') + '"' } else { $_ }
    }) -join ' '
}

function Invoke-ProcessWithOutput {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$StdoutPath,
        [string]$StderrPath,
        [string]$Description
    )
    
    Remove-Item -Force $StdoutPath, $StderrPath -ErrorAction SilentlyContinue
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    
    Write-Host "[$Description] executing: $FilePath $($Arguments -join ' ')"
    $proc = Start-Process -FilePath $FilePath -ArgumentList (Join-NativeArgs $Arguments) `
        -RedirectStandardError $StderrPath -RedirectStandardOutput $StdoutPath `
        -NoNewWindow -PassThru -Wait
    
    $ErrorActionPreference = $prevEAP
    return @{
        ExitCode = $proc.ExitCode
        Stdout = if (Test-Path $StdoutPath) { Get-Content $StdoutPath -Raw } else { '' }
        Stderr = if (Test-Path $StderrPath) { Get-Content $StderrPath -Raw } else { '' }
    }
}

function Test-TraceValidity {
    param(
        [object]$Trace,
        [string]$Source,
        [int]$MinTokens
    )
    
    if (-not ($Trace.PSObject.Properties.Name -contains 'backend')) {
        Write-Host "[FAIL] $Source trace missing backend field" -ForegroundColor Red
        return $false
    }
    if ($Trace.backend -eq 'cpu-fallback' -or $Trace.backend -eq 'unknown') {
        Write-Host "[FAIL] $Source trace did not exercise a real backend: backend=$($Trace.backend)" -ForegroundColor Red
        return $false
    }
    if ($Trace.token_count -lt $MinTokens) {
        Write-Host "[FAIL] $Source trace emitted too few tokens: $($Trace.token_count) < $MinTokens" -ForegroundColor Red
        return $false
    }
    return $true
}

if ([string]::IsNullOrWhiteSpace($Model)) {
    $candidates = @(
        "D:\tinyllama_fresh.gguf",
        "D:\TinyLlama-1.1B-Chat-v1.0.Q4_0.gguf"
    )
    $Model = ($candidates | Where-Object { Test-Path $_ } | Select-Object -First 1)
    if (-not $Model) {
        $Model = ""
    }
}

if (!(Test-Path $cliExe)) { Defer "CLI binary not built: $cliExe" $EXIT_TRACE_INVALID }
if (!(Test-Path $uiExe))  { Defer "UI binary not built: $uiExe" $EXIT_TRACE_INVALID }
if (!(Test-Path $Model))  { Defer "Model file not present: $Model" $EXIT_TRACE_INVALID }

# Probe for the inference plugin DLL.
$plugin = Join-Path $BinDir "RawrXD_ServeInference.dll"
if (!(Test-Path $plugin)) {
    $envDll = $env:RAWRXD_SERVE_INFERENCE_DLL
    if (-not $envDll -or -not (Test-Path $envDll)) {
        Defer "RawrXD_ServeInference.dll missing from $BinDir and RAWRXD_SERVE_INFERENCE_DLL" $EXIT_TRACE_INVALID
    }
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

# Make sure we are NOT in CPU-fallback mode for this lane.
Remove-Item Env:\RAWRXD_PARITY_CPU -ErrorAction SilentlyContinue

if ($DisableAutopatch) {
    $env:RAWRXD_DISABLE_AUTOPATCH = '1'
    Write-Host "[TUNING] Autopatch disabled (RAWRXD_DISABLE_AUTOPATCH=1)"
} else {
    Remove-Item Env:\RAWRXD_DISABLE_AUTOPATCH -ErrorAction SilentlyContinue
}

if (-not [string]::IsNullOrWhiteSpace($MatmulKernel)) {
    $env:RAWRXD_VULKAN_MATMUL_KERNEL = $MatmulKernel
    Write-Host "[TUNING] MatMul kernel key: $MatmulKernel"
} else {
    Remove-Item Env:\RAWRXD_VULKAN_MATMUL_KERNEL -ErrorAction SilentlyContinue
}

if (-not [string]::IsNullOrWhiteSpace($MatmulSpv)) {
    $env:RAWRXD_VULKAN_MATMUL_SPV = $MatmulSpv
    Write-Host "[TUNING] MatMul SPV: $MatmulSpv"
} else {
    Remove-Item Env:\RAWRXD_VULKAN_MATMUL_SPV -ErrorAction SilentlyContinue
}

Write-Host "=== GPU preflight ==="
Write-Host "[PREFLIGHT] inference DLL: $plugin"
$preflightResult = Invoke-ProcessWithOutput -FilePath $cliExe -Arguments @('list') `
    -StdoutPath (Join-Path $OutDir "preflight.stdout.txt") `
    -StderrPath (Join-Path $OutDir "preflight.stderr.txt") `
    -Description "PREFLIGHT"

$preflightText = ($preflightResult.Stdout, $preflightResult.Stderr -join "`n").Trim()
if ($preflightText -match 'No GPU backend available') {
    Defer "GPU preflight failed before inference (no Vulkan/CUDA/HIP device)." $EXIT_GPU_NOT_FOUND
}

$gpuHints = @($preflightText -split "`r?`n" | Where-Object { $_ -match 'GPU|Vulkan|CUDA|HIP' })
if ($gpuHints.Count -gt 0) {
    Write-Host "[PREFLIGHT] backend hints: $($gpuHints -join '; ')"
} else {
    Write-Host "[PREFLIGHT] no explicit GPU banner from 'rawrxd list'; continuing to real inference probe (exit=$preflightResult.ExitCode)"
}

Write-Host "=== Warmup run (discarded) ==="
$warmupResult = Invoke-ProcessWithOutput -FilePath $cliExe -Arguments @('run', $Model, '--prompt', $Prompt, '--max-tokens', "$MaxTokens") `
    -StdoutPath (Join-Path $OutDir "warmup.stdout.txt") `
    -StderrPath (Join-Path $OutDir "warmup.stderr.txt") `
    -Description "WARMUP"
if ($warmupResult.ExitCode -ne 0) {
    Write-Host "[WARMUP] non-zero exit=$($warmupResult.ExitCode) (continuing to strict validation)" -ForegroundColor Yellow
}

Write-Host "=== Attempting CLI inference (will defer if GPU absent) ==="
$cliTrace  = Join-Path $OutDir "cli_real.json"
$cliStdout = Join-Path $OutDir "cli.stdout.txt"
$cliStderr = Join-Path $OutDir "cli.stderr.txt"
$uiTrace   = Join-Path $OutDir "ui_real.json"
Remove-Item -Force $cliTrace, $uiTrace -ErrorAction SilentlyContinue

$prevEAP = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$cliProc = Start-Process -FilePath $cliExe `
    -ArgumentList (Join-NativeArgs @('run', $Model, '--prompt', $Prompt, '--max-tokens', "$MaxTokens", '--emit-json-trace', $cliTrace)) `
    -RedirectStandardError $cliStderr -RedirectStandardOutput $cliStdout `
    -NoNewWindow -PassThru -Wait
$cliExit = $cliProc.ExitCode
$ErrorActionPreference = $prevEAP

$cliErr = if (Test-Path $cliStderr) { Get-Content $cliStderr -Raw } else { '' }
if ($cliErr -match 'No GPU backend available') {
    Defer "GPU enforcement gate fired (no Vulkan/CUDA/HIP device)." $EXIT_GPU_NOT_FOUND
}
if ($cliErr -match '\[PIPELINE PARITY-CPU\]') {
    Write-Host "[FAIL] Unexpected parity CPU fallback marker in CLI run" -ForegroundColor Red
    Exit-Script $EXIT_CPU_FALLBACK
}
if ($cliExit -ne 0 -or -not (Test-Path $cliTrace)) {
    Write-Host "[FAIL] CLI inference failed (exit=$cliExit)" -ForegroundColor Red
    if ($cliErr) { Write-Host "stderr:`n$cliErr" }
    if (Test-Path $cliStdout) { Write-Host "stdout:`n$(Get-Content $cliStdout -Raw)" }
    Exit-Script $EXIT_TRACE_INVALID
}

$cliJson = Get-Content $cliTrace -Raw | ConvertFrom-Json
if (-not ($cliJson.PSObject.Properties.Name -contains 'backend')) {
    Write-Host "[FAIL] CLI trace missing backend field" -ForegroundColor Red
    Exit-Script $EXIT_TRACE_INVALID
}
if ($cliJson.backend -eq 'cpu-fallback' -or $cliJson.backend -eq 'unknown') {
    Write-Host "[FAIL] CLI trace did not exercise a real backend: backend=$($cliJson.backend)" -ForegroundColor Red
    Exit-Script $EXIT_CPU_FALLBACK
}
if ($cliJson.token_count -lt $MinRealTokens) {
    Write-Host "[FAIL] CLI trace emitted too few tokens: $($cliJson.token_count) < $MinRealTokens" -ForegroundColor Red
    Exit-Script $EXIT_TRACE_INVALID
}
Write-Host "[OK] CLI real-model trace written: $cliTrace (backend=$($cliJson.backend), device=$($cliJson.device))"

Write-Host "=== UI inference (headless driver) ==="
$env:RAWRXD_PIPELINE_TRACE = $uiTrace
$uiDriver = Join-Path $BinDir "rawrxd-parity-ui-driver.exe"
if (!(Test-Path $uiDriver)) {
    Write-Host "[DEFERRED] Headless UI driver not built: $uiDriver" -ForegroundColor Yellow
    Write-Host "[INFO] Build it with: ninja -C $BinDir/.. bin/rawrxd-parity-ui-driver.exe"
    Write-Host "[CLI HALF VERIFIED] real-model CLI trace at $cliTrace"
    if ($Strict) { Exit-Script $EXIT_TRACE_INVALID }
    Exit-Script 0
}

$uiStdout = Join-Path $OutDir "ui.stdout.txt"
$uiStderr = Join-Path $OutDir "ui.stderr.txt"
$prevEAP = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$uiProc = Start-Process -FilePath $uiDriver `
    -ArgumentList (Join-NativeArgs @('--model', $Model, '--prompt', $Prompt, '--num-predict', "$MaxTokens")) `
    -RedirectStandardError $uiStderr -RedirectStandardOutput $uiStdout `
    -NoNewWindow -PassThru -Wait
$uiExit = $uiProc.ExitCode
$ErrorActionPreference = $prevEAP

$uiErr = if (Test-Path $uiStderr) { Get-Content $uiStderr -Raw } else { '' }
if ($uiErr -match '\[PIPELINE PARITY-CPU\]') {
    Write-Host "[FAIL] Unexpected parity CPU fallback marker in UI run" -ForegroundColor Red
    Exit-Script $EXIT_CPU_FALLBACK
}

if ($uiExit -ne 0 -or -not (Test-Path $uiTrace)) {
    Write-Host "[FAIL] UI driver failed (exit=$uiExit)" -ForegroundColor Red
    if (Test-Path $uiStderr) { Write-Host "stderr:`n$(Get-Content $uiStderr -Raw)" }
    if (Test-Path $uiStdout) { Write-Host "stdout:`n$(Get-Content $uiStdout -Raw)" }
    Exit-Script $EXIT_TRACE_INVALID
}

$uiJson = Get-Content $uiTrace -Raw | ConvertFrom-Json
if (-not ($uiJson.PSObject.Properties.Name -contains 'backend')) {
    Write-Host "[FAIL] UI trace missing backend field" -ForegroundColor Red
    Exit-Script $EXIT_TRACE_INVALID
}
if ($uiJson.backend -eq 'cpu-fallback' -or $uiJson.backend -eq 'unknown') {
    Write-Host "[FAIL] UI trace did not exercise a real backend: backend=$($uiJson.backend)" -ForegroundColor Red
    Exit-Script $EXIT_CPU_FALLBACK
}
if ($uiJson.token_count -lt $MinRealTokens) {
    Write-Host "[FAIL] UI trace emitted too few tokens: $($uiJson.token_count) < $MinRealTokens" -ForegroundColor Red
    Exit-Script $EXIT_TRACE_INVALID
}
Write-Host "[OK] UI real-model trace written: $uiTrace (backend=$($uiJson.backend), device=$($uiJson.device))"

if ($Strict -and (($cliJson.backend -eq 'cpu-fallback') -or ($uiJson.backend -eq 'cpu-fallback'))) {
    Write-Host "[FAIL] CPU fallback detected in strict GPU run: cli=$($cliJson.backend) ui=$($uiJson.backend)" -ForegroundColor Red
    Exit-Script $EXIT_CPU_FALLBACK
}

if (($cliJson.backend -ne $uiJson.backend) -or ($cliJson.device -ne $uiJson.device)) {
    Write-Host "[FAIL] backend/device mismatch: cli=[$($cliJson.backend) / $($cliJson.device)] ui=[$($uiJson.backend) / $($uiJson.device)]" -ForegroundColor Red
    Exit-Script $EXIT_BACKEND_MISMATCH
}

Write-Host "=== Structural diff ==="
$cmp = Join-Path $PSScriptRoot "compare_parity_trace.ps1"
& $cmp -Cli $cliTrace -Ui $uiTrace
$cmpExit = $LASTEXITCODE
if ($cmpExit -ne 0) {
    Write-Host "[FAIL] token parity mismatch (comparator exit=$cmpExit)" -ForegroundColor Red
    Exit-Script $EXIT_TOKEN_MISMATCH
}

Write-Host ""
Write-Host "=== GPU PARITY SUMMARY ==="
Write-Host ("backend: " + $cliJson.backend)
Write-Host ("device:  " + $cliJson.device)
Write-Host ("tokens:  " + $cliJson.token_count)
Write-Host ("commit:  " + $cliJson.build_commit)
Write-Host ("config:  " + $cliJson.build_config)
Write-Host ("run_id:  " + $cliJson.run_id)
Write-Host "parity:  OK"

Exit-Script 0
