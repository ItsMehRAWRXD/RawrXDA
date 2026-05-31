$ErrorActionPreference = "Stop"
#
# Mirrors AgentOllamaClient stream env routing (streamEnvRoutingLabel) and asserts source wiring.
# Run: pwsh -NoProfile -File scripts/Test-OllamaStreamRoutingEnv.ps1
#

$repoRoot = Split-Path -Parent $PSScriptRoot

function Get-StreamRoutingLabel {
    if ($env:RAWRXD_STREAM_VIA_ORCHESTRATOR -eq "1") { return "orchestrator" }
    $out = "direct"
    if ($env:RAWRXD_STREAM_FALLBACK_ORCHESTRATOR -eq "1") { $out += "+fallback" }
    return $out
}

function Assert-Label([string]$via, [string]$fallback, [string]$expect) {
    Remove-Item Env:RAWRXD_STREAM_VIA_ORCHESTRATOR -ErrorAction SilentlyContinue
    Remove-Item Env:RAWRXD_STREAM_FALLBACK_ORCHESTRATOR -ErrorAction SilentlyContinue
    if ($null -ne $via) { $env:RAWRXD_STREAM_VIA_ORCHESTRATOR = $via }
    if ($null -ne $fallback) { $env:RAWRXD_STREAM_FALLBACK_ORCHESTRATOR = $fallback }
    $got = Get-StreamRoutingLabel
    if ($got -ne $expect) {
        throw "Expected routing '$expect', got '$got' (VIA=$via FALLBACK=$fallback)"
    }
}

Assert-Label $null $null "direct"
Assert-Label $null "1" "direct+fallback"
Assert-Label "1" $null "orchestrator"
Assert-Label "1" "1" "orchestrator"

function Assert-FileContains([string]$relPath, [string]$pattern, [string]$desc) {
    $p = Join-Path $repoRoot $relPath
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing $relPath" }
    $raw = Get-Content -LiteralPath $p -Raw
    if ($raw -notmatch $pattern) { throw "FAIL: $desc ($relPath)" }
}

Assert-FileContains "src\agentic\AgentOllamaClient.h" 'GetOllamaStreamRoutingEnvLabel' "stream routing label helper present"
Assert-FileContains "src\agentic\AgentOllamaClient.cpp" 'StreamHttpJsonLines' "direct Ollama streaming path present"
Assert-FileContains "src\agentic\AgentOllamaClient.h" 'streamRouting' "MetricsSnapshot exposes streamRouting"
Assert-FileContains "src\core\ssot_handlers.cpp" 'streamRouting' "backend status surfaces streamRouting"

Write-Host "OK: Ollama stream routing env smoke."
exit 0
