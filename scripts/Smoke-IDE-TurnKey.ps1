#Requires -Version 5.1
<#
.SYNOPSIS
  Fast IDE parity smoke (source checks + optional TPS). Delegates to Run-TurnkeyIdeSmoke.ps1 with
  -SkipAgenticExe -SkipCopilotCli so no built exes are required.

.DESCRIPTION
  Full binary + CLI + Win32 smoke: scripts/Run-TurnkeyIdeSmoke.ps1 (omit -SkipAgenticExe / -SkipCopilotCli).
  One-shot Ship bundle (symbols + RawrEngine + Win32 when built): Ship/smoke_agentic_chat_parity.ps1

.PARAMETER SkipAgenticParityScript
  Maps to -SkipAgenticChatParity on the canonical runner.

.PARAMETER SkipUiMarshalling
  Maps to -SkipUiMarshalling (faster; skips Test-ChatAgenticUiMarshalling.ps1).

.PARAMETER RunTpsSmoke
  Force TPS step when RawrXD-TpsSmoke.exe and model path resolve (in addition to env-based trigger).

.PARAMETER StrictTps
  When TPS runs, non-zero exit fails the script.

.PARAMETER LaneBHeadless
  When RawrEngine.exe exists (build-win32, build-ninja, or build), run --copilot-smoke (Lane B JSON + optional fast exit).

.EXAMPLE
  .\scripts\Smoke-IDE-TurnKey.ps1

.EXAMPLE
  .\scripts\Smoke-IDE-TurnKey.ps1 -LaneBHeadless

.EXAMPLE
  $env:RAWRXD_SMOKE_MODEL = 'D:\models\TinyLlama.Q4_K_M.gguf'
  .\scripts\Smoke-IDE-TurnKey.ps1 -RunTpsSmoke -StrictTps
#>
param(
    [switch]$SkipAgenticParityScript,
    [switch]$SkipUiMarshalling,
    [switch]$RunTpsSmoke,
    [switch]$StrictTps,
    [switch]$LaneBHeadless
)

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$repoRoot = Split-Path -Parent $here
$runner = Join-Path $here "Run-TurnkeyIdeSmoke.ps1"

$skipCopilotCli = $true
if ($LaneBHeadless) {
    $re = $null
    foreach ($c in @(
            (Join-Path $repoRoot "build-win32\bin\RawrEngine.exe"),
            (Join-Path $repoRoot "build-ninja\bin\RawrEngine.exe"),
            (Join-Path $repoRoot "build-win32\RawrEngine.exe"),
            (Join-Path $repoRoot "build-ninja\RawrEngine.exe"),
            (Join-Path $repoRoot "build\bin\RawrEngine.exe"))) {
        if (Test-Path -LiteralPath $c) { $re = $c; break }
    }
    if ($re) {
        $skipCopilotCli = $false
    }
    else {
        Write-Warning "Smoke-IDE-TurnKey: -LaneBHeadless but RawrEngine.exe not found; skipping Lane B CLI."
    }
}

$params = @{
    SkipAgenticExe        = $true
    SkipCopilotCli        = $skipCopilotCli
    SkipAgenticChatParity = $SkipAgenticParityScript.IsPresent
    SkipUiMarshalling     = $SkipUiMarshalling.IsPresent
    StrictTps             = $StrictTps.IsPresent
}
if ($RunTpsSmoke.IsPresent) {
    $params.RunTpsSmoke = $true
}
elseif ($env:RAWRXD_SMOKE_MODEL -and $env:RAWRXD_SMOKE_MODEL.Trim().Length -gt 0) {
    $params.RunTpsSmoke = $true
}

& $runner @params
exit $LASTEXITCODE
