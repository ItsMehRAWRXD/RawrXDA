#Requires -Version 5.1
<#
.SYNOPSIS
  Smoke check: Copilot-style agentic chat routing is present in Win32IDE + optional CLI build.

.NOTES
  - IDE: HandleCopilotSend routes /agent, /agentic, agentic:, @agent to AgenticChatSession when isAgenticLayerAvailable().
  - CLI: main.cpp /agent <prompt> loop is the headless analogue (tool dispatch via subAgentMgr).
#>
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ide = Join-Path $root "src\win32app\Win32IDE.cpp"
if (-not (Test-Path -LiteralPath $ide)) {
    throw "Expected Win32IDE.cpp at $ide"
}
$route = Get-Content -LiteralPath $ide -Raw
$routeChecks = @(
    @{ Name = "wantsAgenticChat"; Pattern = "wantsAgenticChat" }
    @{ Name = "agentRouteUserMessage"; Pattern = "agentRouteUserMessage" }
    @{ Name = "SetAgenticMode(true)"; Pattern = "SetAgenticMode\(true\)" }
    @{ Name = "StripAgenticPrefixForRouteParity"; Pattern = "StripAgenticPrefixForRouteParity" }
)
$failedRoute = @()
foreach ($c in $routeChecks) {
    if ($route -notmatch $c.Pattern) { $failedRoute += $c.Name }
}
if ($failedRoute.Count -gt 0) {
    throw "Smoke-AgenticChatParity: missing in Win32IDE.cpp: $($failedRoute -join ', ')"
}

$win32app = Join-Path $root "src\win32app"
$bundle = (Get-ChildItem -LiteralPath $win32app -Filter "*.cpp" -File | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
$toolChecks = @(
    @{ Name = "persistChatToolTurnToDisk"; Pattern = "persistChatToolTurnToDisk" }
    @{ Name = "conversationAddToolResult"; Pattern = "conversationAddToolResult" }
)
$failedTool = @()
foreach ($c in $toolChecks) {
    if ($bundle -notmatch $c.Pattern) { $failedTool += $c.Name }
}
if ($failedTool.Count -gt 0) {
    throw "Smoke-AgenticChatParity: missing under src/win32app: $($failedTool -join ', ')"
}
Write-Host "Smoke-AgenticChatParity: OK (Win32IDE route + win32app tool persistence APIs present)."
$exeCandidates = @(
    (Join-Path $root "build-win32\bin\Release\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-win32\bin\Debug\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-win32\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-win32\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-ninja\bin\Release\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-ninja\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-ninja-ctx2\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build\bin\Release\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build\bin\RawrXD-Win32IDE.exe"),
    (Join-Path $root "build-ninja\RawrXD-Win32IDE.exe")
)
$exeFound = $null
foreach ($c in $exeCandidates) {
    if (Test-Path -LiteralPath $c) { $exeFound = $c; break }
}
if ($exeFound) {
    Write-Host "Found $exeFound (run IDE manually to verify /agent in AI chat)."
} else {
    Write-Host "Optional: build RawrXD-Win32IDE (e.g. build-win32\bin or build-ninja\bin)."
}
exit 0
