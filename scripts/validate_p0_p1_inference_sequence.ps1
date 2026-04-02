# P0/P1 inference validation: headless golden trace + optional IDE log audit.
#
# Usage:
#   pwsh -File D:\rawrxd\scripts\validate_p0_p1_inference_sequence.ps1 -ModelPath "D:\models\BigDaddyG-Q2_K-PRUNED-16GB.gguf"
#
# Environment (optional):
#   RAWRXD_TEST_GGUF   default model path if -ModelPath omitted
#   RAWRXD_BUILD_DIR   override build root (default: D:\rawrxd\build-ninja)
#
# Manual (GUI pulse — cannot be scripted here):
#   1. Run bin\RawrXD-Win32IDE.exe
#   2. Load the same GGUF; open Output tab; send "hello"
#   3. Confirm [STEP] Layer … lines stream in the Output panel
#   4. Close IDE; re-run this script with -IdeLogOnly or full run to scan ide.log

[CmdletBinding()]
param(
    [string]$ModelPath = $env:RAWRXD_TEST_GGUF,
    [string]$BuildDir = $(if ($env:RAWRXD_BUILD_DIR) { $env:RAWRXD_BUILD_DIR } else { "D:\rawrxd\build-ninja" }),
    [switch]$SkipHeadless,
    [switch]$IdeLogOnly,
    [switch]$FailOnIdeLogIssues
)

$ErrorActionPreference = "Stop"

function Resolve-EngineExe {
    param([string]$Root)
    $candidates = @(
        (Join-Path $Root "bin\RawrEngine.exe"),
        (Join-Path $Root "RawrEngine.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return (Resolve-Path -LiteralPath $c).Path }
    }
    return $null
}

function Test-IdeLogForIssues {
    param([string]$LogPath, [switch]$FailHard)

    # Avoid matching benign loader recovery lines (e.g. MapViewOfFileEx ... failed + legacy retry ok).
    $badPatterns = @(
        "locked window too small",
        "[Forward] FATAL",
        "StreamingPin failed",
        "infer: stage=done status=fail",
        '\bOOM\b'
    )

    if (-not (Test-Path -LiteralPath $LogPath)) {
        Write-Host "[IDE log] Not found: $LogPath (skip or run IDE once)"
        if ($FailHard) { return $false }
        return $true
    }

    Write-Host "[IDE log] Scanning: $LogPath"
    $anyBad = $false
    foreach ($p in $badPatterns) {
        $hits = Select-String -LiteralPath $LogPath -Pattern $p -ErrorAction SilentlyContinue
        if ($hits) {
            $anyBad = $true
            Write-Host "  --- BAD match: $p ---"
            $hits | Select-Object -Last 6 | ForEach-Object { Write-Host "    $($_.Line)" }
        }
    }
    if (-not $anyBad) { Write-Host "  No bad patterns matched (VMM / FATAL / OOM scan)." }

    if ($anyBad -and $FailHard) { return $false }
    return $true
}

$engine = Resolve-EngineExe $BuildDir
if (-not $engine) {
    Write-Error "RawrEngine.exe not found under: $BuildDir"
}

if ($IdeLogOnly) {
    $ideLog = Join-Path $env:APPDATA "RawrXD\ide.log"
    $ok = Test-IdeLogForIssues -LogPath $ideLog -FailHard:$FailOnIdeLogIssues
    if (-not $ok) { exit 2 }
    Write-Host "[IDE log] Done."
    exit 0
}

if ([string]::IsNullOrWhiteSpace($ModelPath)) {
    Write-Error "Provide -ModelPath or set RAWRXD_TEST_GGUF for headless runs (or use -SkipHeadless -IdeLogOnly)."
}

if (-not (Test-Path -LiteralPath $ModelPath)) {
    Write-Error "Model not found: $ModelPath"
}

Write-Host "=== P0 headless: --infer max-tokens 1 ==="
Write-Host "Engine: $engine"
& $engine --infer $ModelPath --prompt "hello" --max-tokens 1
if ($LASTEXITCODE -ne 0) {
    Write-Error "P0 headless failed (exit $LASTEXITCODE)"
}

if (-not $SkipHeadless) {
    Write-Host ""
    Write-Host "=== P1 headless: --infer max-tokens 8 ==="
    & $engine --infer $ModelPath --prompt "a" --max-tokens 8
    if ($LASTEXITCODE -ne 0) {
        Write-Error "P1 headless failed (exit $LASTEXITCODE)"
    }
}

Write-Host ""
Write-Host "=== IDE log audit (%APPDATA%\RawrXD\ide.log) ==="
$ideLog = Join-Path $env:APPDATA "RawrXD\ide.log"
$logOk = Test-IdeLogForIssues -LogPath $ideLog -FailHard:$FailOnIdeLogIssues
if (-not $logOk) { exit 2 }

Write-Host ""
Write-Host "=== Manual GUI checklist (P0 pulse) ==="
Write-Host "  Launch: $(Join-Path $BuildDir 'bin\RawrXD-Win32IDE.exe')"
Write-Host "  Load:   $ModelPath"
Write-Host "  Output tab -> send hello -> expect [STEP] Layer … lines"
Write-Host "  Close IDE; optional: pwsh -File $PSScriptRoot\validate_win32ide_inference_log.ps1"
Write-Host ""
Write-Host "P0/P1 automated leg complete (headless OK)."
