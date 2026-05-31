#Requires -Version 5.1
<#
.SYNOPSIS
  Canonical turnkey smoke: Copilot/agentic source parity + UI marshalling + routing + optional Lane B CLI + Win32 --agentic-smoke + optional TPS.

.DESCRIPTION
  1) Test-ChatCopilotParitySmoke.ps1 — persistence, TPS metrics, minimal-agent glue, CLI JSON symbols.
  2) Test-ChatAgenticUiMarshalling.ps1 — unless -SkipUiMarshalling.
  3) Smoke-AgenticChatParity.ps1 — /agent routing (unless -SkipAgenticChatParity).
  4) RawrEngine --copilot-smoke — headless throughput JSON (unless -SkipCopilotCli); -RequireCopilotCli fails if exe missing.
  5) Ship/Run-ChatAgenticSmoke.ps1 — RawrXD-Win32IDE --agentic-smoke (unless -SkipAgenticExe).
  6) RawrXD-TpsSmoke — when -RunTpsSmoke or RAWRXD_SMOKE_MODEL; -StrictTps fails on non-zero run.

  Live Ollama: set RAWRXD_AGENTIC_SMOKE_LIVE=1 before running so Win32IDE --agentic-smoke exercises /api/tags and /api/chat (Ollama must be running).

.PARAMETER SkipAgenticExe
  Skip Win32IDE --agentic-smoke.

.PARAMETER SkipAgenticChatParity
  Skip Smoke-AgenticChatParity.ps1.

.PARAMETER SkipUiMarshalling
  Skip Test-ChatAgenticUiMarshalling.ps1.

.PARAMETER SkipCopilotCli
  Skip RawrEngine --copilot-smoke.

.PARAMETER RequireCopilotCli
  Fail if RawrEngine.exe is not found.

.PARAMETER SkipTps
  Never run TPS.

.PARAMETER RunTpsSmoke
  Run TPS when exe + model resolve.

.PARAMETER StrictTps
  If TPS runs, non-zero exit fails the script.

.PARAMETER ModelPath
  GGUF for TpsSmoke (-ModelPath, then RAWRXD_SMOKE_MODEL, then model\llama-7b-q4_0.gguf).

.PARAMETER BuildDir
  CMake build dir for locating exes.

.PARAMETER IdeExe
  Path to RawrXD-Win32IDE.exe for --agentic-smoke.

.PARAMETER EngineExe
  Path to RawrEngine.exe for --copilot-smoke.
#>
param(
    [switch]$SkipAgenticExe,
    [switch]$SkipAgenticChatParity,
    [switch]$SkipUiMarshalling,
    [switch]$SkipCopilotCli,
    [switch]$RequireCopilotCli,
    [switch]$SkipTps,
    [switch]$RunTpsSmoke,
    [switch]$StrictTps,
    [string]$ModelPath = "",
    [string]$BuildDir = "",
    [string]$IdeExe = "",
    [string]$EngineExe = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$logDir = Join-Path $repoRoot "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$summaryPath = Join-Path $logDir "turnkey_ide_smoke_last.json"

$steps = [ordered]@{}
$failed = $false

function Invoke-Step([string]$name, [scriptblock]$block) {
    try {
        & $block
        $script:steps[$name] = "ok"
        Write-Host "[turnkey] PASS: $name" -ForegroundColor Green
    }
    catch {
        $script:steps[$name] = "fail: $($_.Exception.Message)"
        $script:failed = $true
        Write-Host "[turnkey] FAIL: $name — $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Find-FirstExistingPath {
    param([string[]]$Candidates)
    foreach ($c in $Candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }
    return $null
}

Invoke-Step "chat_copilot_parity_symbols" {
    & pwsh -NoProfile -File (Join-Path $repoRoot "scripts\Test-ChatCopilotParitySmoke.ps1")
    if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
}

if (-not $SkipUiMarshalling) {
    Invoke-Step "chat_agentic_ui_marshalling" {
        $marsh = Join-Path $repoRoot "scripts\Test-ChatAgenticUiMarshalling.ps1"
        if (-not (Test-Path -LiteralPath $marsh)) { throw "missing Test-ChatAgenticUiMarshalling.ps1" }
        & pwsh -NoProfile -File $marsh
        if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
    }
}
else {
    $steps["chat_agentic_ui_marshalling"] = "skipped: -SkipUiMarshalling"
}

if (-not $SkipAgenticChatParity) {
    $acp = Join-Path $repoRoot "scripts\Smoke-AgenticChatParity.ps1"
    if (Test-Path -LiteralPath $acp) {
        Invoke-Step "agentic_chat_route_symbols" {
            & pwsh -NoProfile -File $acp
            if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
        }
    }
    else {
        $steps["agentic_chat_route_symbols"] = "skipped: Smoke-AgenticChatParity.ps1 missing"
        Write-Host "[turnkey] SKIP: Smoke-AgenticChatParity.ps1" -ForegroundColor Yellow
    }
}
else {
    $steps["agentic_chat_route_symbols"] = "skipped: -SkipAgenticChatParity"
}

if (-not $SkipCopilotCli) {
    $engine = $EngineExe
    if (-not $engine) {
        $engineCandidates = @()
        if ($BuildDir) {
            $engineCandidates += @(
                (Join-Path $BuildDir "bin\Release\RawrEngine.exe"),
                (Join-Path $BuildDir "bin\Debug\RawrEngine.exe"),
                (Join-Path $BuildDir "bin\RawrEngine.exe"),
                (Join-Path $BuildDir "RawrEngine.exe")
            )
        }
        $engineCandidates += @(
            (Join-Path $repoRoot "build-win32\bin\RawrEngine.exe"),
            (Join-Path $repoRoot "build-ninja\bin\RawrEngine.exe"),
            (Join-Path $repoRoot "build-win32\RawrEngine.exe"),
            (Join-Path $repoRoot "build\bin\Release\RawrEngine.exe"),
            (Join-Path $repoRoot "build\bin\RawrEngine.exe"),
            (Join-Path $repoRoot "build-ninja\RawrEngine.exe")
        )
        $engine = Find-FirstExistingPath $engineCandidates
    }
    if ($engine) {
        Invoke-Step "headless_copilot_smoke" {
            $prevFast = $env:RAWRXD_COPILOT_SMOKE_FAST_EXIT
            $env:RAWRXD_COPILOT_SMOKE_FAST_EXIT = "1"
            $out = & $engine --copilot-smoke 2>&1
            $ex = $LASTEXITCODE
            if ($null -ne $prevFast) { $env:RAWRXD_COPILOT_SMOKE_FAST_EXIT = $prevFast } else { Remove-Item Env:RAWRXD_COPILOT_SMOKE_FAST_EXIT -ErrorAction SilentlyContinue }
            $txt = if ($out -is [array]) { $out -join "`n" } else { [string]$out }
            $ok = ($txt -match 'COPILOT_SMOKE_JSON:.*"ok"\s*:\s*true') -and ($txt -match 'EXIT=0')
            if (-not $ok -and $ex -ne 0) { throw "RawrEngine exit $ex`n$txt" }
            if (-not $ok) { throw "missing COPILOT_SMOKE_JSON ok:true and EXIT=0 in stdout`n$txt" }
            if ($ex -ne 0) {
                Write-Host "[turnkey] WARN: RawrEngine exit=$ex but stdout indicates copilot success (set RAWRXD_COPILOT_SMOKE_FAST_EXIT=1 — scripts do this automatically)." -ForegroundColor Yellow
            }
        }
    }
    elseif ($RequireCopilotCli) {
        Invoke-Step "headless_copilot_smoke" { throw "RawrEngine.exe not found (pass -EngineExe or build RawrEngine)" }
    }
    else {
        $steps["headless_copilot_smoke"] = "skipped: RawrEngine.exe not found"
        Write-Host "[turnkey] SKIP: RawrEngine --copilot-smoke (build RawrEngine or -RequireCopilotCli to hard-fail)" -ForegroundColor Yellow
    }
}
else {
    $steps["headless_copilot_smoke"] = "skipped: -SkipCopilotCli"
}

if (-not $SkipAgenticExe) {
    $agenticScript = Join-Path $repoRoot "Ship\Run-ChatAgenticSmoke.ps1"
    if (Test-Path -LiteralPath $agenticScript) {
        Invoke-Step "win32_agentic_registry_smoke" {
            $invokeArgs = @("-NoProfile", "-File", $agenticScript)
            if ($IdeExe) { $invokeArgs += @("-ExePath", $IdeExe) }
            & pwsh @invokeArgs
            if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
        }
    }
    else {
        $steps["win32_agentic_registry_smoke"] = "skipped: Ship/Run-ChatAgenticSmoke.ps1 missing"
        Write-Host "[turnkey] SKIP: Run-ChatAgenticSmoke.ps1" -ForegroundColor Yellow
    }
}
else {
    $steps["win32_agentic_registry_smoke"] = "skipped: -SkipAgenticExe"
}

$wantTps = (-not $SkipTps) -and ($RunTpsSmoke -or ($env:RAWRXD_SMOKE_MODEL -and $env:RAWRXD_SMOKE_MODEL.Trim().Length -gt 0))

if ($wantTps) {
    if (-not $BuildDir) {
        foreach ($c in @(
                (Join-Path $repoRoot "build-win32"),
                (Join-Path $repoRoot "build-ninja"),
                (Join-Path $repoRoot "build-ninja-ctx2"),
                (Join-Path $repoRoot "build_smoke_auto"),
                (Join-Path $repoRoot "build"))) {
            if (Test-Path (Join-Path $c "CMakeCache.txt")) { $BuildDir = $c; break }
        }
    }
    if (-not $ModelPath) {
        if ($env:RAWRXD_SMOKE_MODEL -and (Test-Path -LiteralPath $env:RAWRXD_SMOKE_MODEL)) {
            $ModelPath = $env:RAWRXD_SMOKE_MODEL
        }
        else {
            $ModelPath = Join-Path $repoRoot "model\llama-7b-q4_0.gguf"
        }
    }
    $tpsExe = $null
    $tpsCandidates = @()
    if ($BuildDir) {
        $tpsCandidates += @(
            (Join-Path $BuildDir "bin\Release\RawrXD-TpsSmoke.exe"),
            (Join-Path $BuildDir "bin\RawrXD-TpsSmoke.exe"),
            (Join-Path $BuildDir "RawrXD-TpsSmoke.exe")
        )
    }
    $tpsCandidates += @(
        (Join-Path $repoRoot "build-win32\bin\RawrXD-TpsSmoke.exe"),
        (Join-Path $repoRoot "build-ninja\bin\RawrXD-TpsSmoke.exe"),
        (Join-Path $repoRoot "build-ninja-ctx2\bin\RawrXD-TpsSmoke.exe"),
        (Join-Path $repoRoot "build\bin\Release\RawrXD-TpsSmoke.exe"),
        (Join-Path $repoRoot "bin\RawrXD-TpsSmoke.exe")
    )
    foreach ($p in $tpsCandidates) {
        if ($p -and (Test-Path -LiteralPath $p)) { $tpsExe = $p; break }
    }

    if ($tpsExe -and (Test-Path -LiteralPath $ModelPath)) {
        Invoke-Step "tps_smoke" {
            $env:RAWRXD_TPS_MACHINE_JSON = "1"
            & $tpsExe $ModelPath 8
            $code = $LASTEXITCODE
            if ($code -ne 0 -and $StrictTps) { throw "RawrXD-TpsSmoke exit $code" }
            if ($code -ne 0 -and -not $StrictTps) {
                Write-Host "[turnkey] WARN: TPS exit $code" -ForegroundColor Yellow
            }
        }
    }
    else {
        $msg = "RawrXD-TpsSmoke.exe or model not found (BuildDir=$BuildDir model=$ModelPath)"
        $steps["tps_smoke"] = "skipped: $msg"
        Write-Host "[turnkey] SKIP: $msg" -ForegroundColor Yellow
    }
}
else {
    $steps["tps_smoke"] = "skipped: -RunTpsSmoke or RAWRXD_SMOKE_MODEL; or -SkipTps"
}

$obj = [ordered]@{
    ok         = (-not $failed)
    timestamp  = (Get-Date).ToString("o")
    repoRoot   = $repoRoot
    steps      = $steps
    envHints   = @{
        liveAgenticOllama = "RAWRXD_AGENTIC_SMOKE_LIVE=1"
        tpsModel          = "RAWRXD_SMOKE_MODEL=<path.gguf>"
        tpsRef            = "RAWRXD_TPS_REF"
        laneBCli          = "cmake --build <RAWRXD_BUILD_DIR|build-win32|build-ninja> --target RawrEngine ; <build-dir>\bin\RawrEngine.exe --copilot-smoke"
        ideModelAutonomy  = "ctest -C Debug -R test_ide_model_autonomy --output-on-failure (from configured build dir)"
        fullChatParity    = "pwsh -File Ship/smoke_agentic_chat_parity.ps1"
        fastSourceOnly    = "pwsh -File scripts/Smoke-IDE-TurnKey.ps1"
    }
}
$obj | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Host "[turnkey] Wrote $summaryPath" -ForegroundColor Cyan

if ($failed) { exit 1 }
exit 0
