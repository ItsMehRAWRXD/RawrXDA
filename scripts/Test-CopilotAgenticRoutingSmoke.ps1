# Verifies HandleCopilotSend routes local GGUF selections to native bridge (not Ollama-only AgenticChatSession).
# No external dependencies — run: pwsh -File scripts/Test-CopilotAgenticRoutingSmoke.ps1
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$cpp = Join-Path $repoRoot "src\win32app\Win32IDE.cpp"
if (-not (Test-Path -LiteralPath $cpp)) {
    Write-Error "Missing $cpp"
    exit 1
}
$txt = Get-Content -LiteralPath $cpp -Raw
if ($txt -notmatch 'wantsAgenticChat\s*&&\s*layerAvailable\s*&&\s*!selectedLocalModel') {
    Write-Error "Expected routing guard (wantsAgenticChat && layerAvailable && !selectedLocalModel) in HandleCopilotSend"
    exit 1
}
if ($txt -notmatch 'SetChatModel') {
    Write-Error "Expected SetChatModel wiring for Ollama agentic session"
    exit 1
}
Write-Host "OK: Copilot agentic routing smoke (source guards present)."
exit 0
