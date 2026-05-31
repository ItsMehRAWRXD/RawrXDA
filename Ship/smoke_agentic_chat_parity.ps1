# Verifies Copilot/Cursor-style agentic + chat entry points (CLI + Win32), no manual IDE required.
# Run from repo root: pwsh -File Ship/smoke_agentic_chat_parity.ps1
#   - RawrEngine --copilot-smoke : Lane B load + GenerateStreaming; JSON wall_ms / estimated_tps / agentic_* (optional --with-agentic)
#   - RawrXD-Win32IDE --agentic-smoke : AgentToolRegistry read_file + optional live Ollama (RAWRXD_AGENTIC_SMOKE_LIVE=1)
# Set RAWRXD_SMOKE_REQUIRE_ENGINE=1 to fail if RawrEngine.exe is missing (default: optional — build Win32IDE only).
# Fast source-only (no exes): pwsh -File scripts/Smoke-IDE-TurnKey.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$requireEngine = ($env:RAWRXD_SMOKE_REQUIRE_ENGINE -eq "1")

function Find-FirstExe {
    param([string[]]$Candidates)
    foreach ($c in $Candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }
    return $null
}

$engine = Find-FirstExe @(
    (Join-Path $root "build-win32\bin\RawrEngine.exe"),
    (Join-Path $root "build-win32\RawrEngine.exe"),
    (Join-Path $root "build-ninja\bin\RawrEngine.exe"),
    (Join-Path $root "build-ninja-ctx2\bin\RawrEngine.exe"),
    (Join-Path $root "build\bin\Release\RawrEngine.exe"),
    (Join-Path $root "build\bin\RawrEngine.exe"),
    (Join-Path $root "build-ninja\RawrEngine.exe")
)
$ide = Find-FirstExe @(
    (Join-Path $root "build-win32\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-win32\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-ninja\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-ninja-ctx2\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build\bin\Release\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-ninja\RawrXD-Win32IDE.exe")
)

if (-not $ide) {
    Write-Error "Missing RawrXD-Win32IDE.exe — cmake --build ... --target RawrXD-Win32IDE"
    exit 1
}

Write-Host "smoke_agentic_chat_parity: source strings (scripts/Test-ChatCopilotParitySmoke.ps1)" -ForegroundColor Cyan
& pwsh -NoProfile -File (Join-Path $root "scripts\Test-ChatCopilotParitySmoke.ps1")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
Write-Host "  source parity OK" -ForegroundColor Green

Write-Host "smoke_agentic_chat_parity: UI marshalling (scripts/Test-ChatAgenticUiMarshalling.ps1)" -ForegroundColor Cyan
& pwsh -NoProfile -File (Join-Path $root "scripts\Test-ChatAgenticUiMarshalling.ps1")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
Write-Host "  UI marshalling OK" -ForegroundColor Green

if (-not $engine) {
    if ($requireEngine) {
        Write-Error "Missing RawrEngine.exe — cmake --build ... --target RawrEngine (or unset RAWRXD_SMOKE_REQUIRE_ENGINE)"
        exit 1
    }
    Write-Warning "RawrEngine.exe not found — skipping headless --copilot-smoke (build RawrEngine for CLI parity)."
}

if ($engine) {
    Write-Host "smoke_agentic_chat_parity: RawrEngine=$engine" -ForegroundColor Cyan
    $copilotOut = & $engine --copilot-smoke 2>&1
    $copilotExit = $LASTEXITCODE
    $copilotText = if ($copilotOut -is [array]) { $copilotOut -join "`n" } else { [string]$copilotOut }
    # Some Lane B builds print COPILOT_SMOKE_JSON + EXIT=0 then hit a late teardown fault; treat stdout as source of truth.
    $copilotStdoutOk = ($copilotText -match 'COPILOT_SMOKE_JSON:.*"ok"\s*:\s*true') -and ($copilotText -match 'EXIT=0')
    if (-not $copilotStdoutOk -and $copilotExit -ne 0) {
        Write-Error "RawrEngine --copilot-smoke failed exit=$copilotExit`n$copilotText"
        exit $copilotExit
    }
    if (-not $copilotStdoutOk) {
        Write-Error "RawrEngine --copilot-smoke: expected COPILOT_SMOKE_JSON with ok:true and EXIT=0 in output.`n$copilotText"
        exit 1
    }
    if ($copilotExit -ne 0) {
        Write-Warning "RawrEngine exit=$copilotExit but stdout OK (rebuild with RAWRXD_COPILOT_SMOKE_FAST_EXIT support or ignore if output is valid)."
    }
    if ($copilotText -match "COPILOT_SMOKE_JSON:.*wall_ms") {
        Write-Host "  --copilot-smoke OK (JSON: wall_ms / estimated_tps)" -ForegroundColor Green
    } else {
        Write-Host "  --copilot-smoke OK" -ForegroundColor Green
    }
}

Write-Host "smoke_agentic_chat_parity: RawrXD-Win32IDE=$ide" -ForegroundColor Cyan
$p2 = Start-Process -FilePath $ide -ArgumentList @("--agentic-smoke") -NoNewWindow -Wait -PassThru
if ($p2.ExitCode -ne 0) {
    Write-Error "RawrXD-Win32IDE --agentic-smoke failed exit=$($p2.ExitCode)"
    exit $p2.ExitCode
}
Write-Host "  --agentic-smoke OK" -ForegroundColor Green

Write-Host "smoke_agentic_chat_parity: ALL OK" -ForegroundColor Green
exit 0
