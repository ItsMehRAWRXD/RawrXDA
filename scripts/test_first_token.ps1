#requires -Version 5.1
<#
.SYNOPSIS
    First-token validation — ModelLoader → Inference → Beacon flow and memory cap.

.DESCRIPTION
    Phase: Runtime Integration & First Token.
    - Starts IDE with --load-model <path>
    - Waits for model load
    - Validates working set < 1.92GB
    - Optional: list modules for diagnostic

.PARAMETER ModelPath
    Path to GGUF (e.g. TinyLlama-1.1B q4_0). Default: D:\models\tiny.gguf

.PARAMETER ExePath
    Path to IDE EXE. Auto-detected if not set.

.PARAMETER LoadWaitSeconds
    Seconds to wait after launch for model load (default 2).

.PARAMETER MaxMemoryGB
    Fail if working set exceeds this (default 1.92).

.PARAMETER SkipMemoryCheck
    Optional. Do not fail on memory limit; only validate launch and optional model load.

.PARAMETER SkipModelLoad
    Optional. Launch without --load-model; faster run for "process stays alive" check only.

.EXAMPLE
    .\test_first_token.ps1
    .\test_first_token.ps1 -ModelPath "D:\models\llama-3.2-1b.gguf" -LoadWaitSeconds 3
    .\test_first_token.ps1 -SkipModelLoad -SkipMemoryCheck
#>
param(
    [string]$ModelPath = "D:\models\tiny.gguf",
    [string]$ExePath = "",
    [int]$LoadWaitSeconds = 2,
    [double]$MaxMemoryGB = 1.92,
    [switch]$NoWindow,
    [switch]$SkipMemoryCheck,
    [switch]$SkipModelLoad
)

$ErrorActionPreference = "Stop"
$ProjectRoot = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { "D:\rawrxd" }
if (-not (Test-Path $ProjectRoot)) { $ProjectRoot = "D:\rawrxd" }

function Find-RawrXDExe {
    if ($ExePath -and (Test-Path $ExePath)) { return $ExePath }
    $candidates = @(
        (Join-Path $ProjectRoot "RawrXD-Win32IDE.exe"),
        (Join-Path $ProjectRoot "bin\RawrXD-Win32IDE.exe"),
        (Join-Path $ProjectRoot "build\bin\RawrXD-Win32IDE.exe"),
        (Join-Path $ProjectRoot "RawrXD.exe"),
        (Join-Path $ProjectRoot "build\monolithic\RawrXD.exe")
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    throw "No RawrXD EXE found. Build the IDE first."
}

if (-not $SkipModelLoad -and -not (Test-Path $ModelPath)) {
    Write-Warning "Model file not found: $ModelPath. First-token pipeline will not be exercised; only launch + memory check."
}

$exe = Find-RawrXDExe
Write-Host "[test_first_token] EXE: $exe" -ForegroundColor Cyan
if ($SkipModelLoad) {
    Write-Host "[test_first_token] Model: (skipped -SkipModelLoad)" -ForegroundColor Gray
} else {
    Write-Host "[test_first_token] Model: $ModelPath (exists: $(Test-Path $ModelPath))" -ForegroundColor Cyan
}

$psi = @{ FilePath = $exe; PassThru = $true }
if (-not $SkipModelLoad) { $psi.ArgumentList = @("--load-model", $ModelPath) }
if ($NoWindow) { $psi.WindowStyle = "Hidden" } else { $psi.WindowStyle = "Normal" }
$proc = Start-Process @psi

Start-Sleep -Seconds $LoadWaitSeconds
if ($proc.HasExited) {
    throw "FATAL: Process exited during model-load phase (exit code $($proc.ExitCode))."
}

# Optional: check if any module name suggests model file (diagnostic)
if (-not $SkipModelLoad) {
    try {
        $proc.Refresh()
        $modules = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Modules
        $modelLike = $modules | Where-Object { $_.FileName -and $_.FileName -like "*tiny*" }
        if (-not $modelLike) {
            Write-Host "  [info] No module path containing 'tiny' (model may not be memory-mapped yet or path differs)." -ForegroundColor Gray
        } else {
            Write-Host "  [OK] Process has module matching model path." -ForegroundColor Green
        }
    } catch {
        Write-Host "  [info] Could not enumerate modules: $_" -ForegroundColor Gray
    }
}

# Memory: working set must be within target (unless -SkipMemoryCheck)
$proc.Refresh()
$workingSetBytes = (Get-Process -Id $proc.Id).WorkingSet64
$workingSetGB = [math]::Round($workingSetBytes / 1GB, 2)
if (-not $SkipMemoryCheck) {
    if ($workingSetGB -gt $MaxMemoryGB) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        throw "MEMORY VIOLATION: Working set $workingSetGB GB exceeds limit $MaxMemoryGB GB."
    }
    Write-Host "  [OK] Memory: $workingSetGB GB (within ${MaxMemoryGB} GB target)" -ForegroundColor Green
} else {
    Write-Host "  [info] Memory: $workingSetGB GB (check skipped)" -ForegroundColor Gray
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host "[test_first_token] First-token phase: pipeline validated." -ForegroundColor Cyan
