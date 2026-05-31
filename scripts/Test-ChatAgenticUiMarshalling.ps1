#Requires -Version 5.1
<#
.SYNOPSIS
  Offline guard: agentic Copilot completion and tool turns are marshalled on the UI thread
  (WM_COPILOT_AGENTIC_ASSISTANT_FINAL, WM_COPILOT_RECORD_TOOL_TURN) with workspace guardrail sync.
#>
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ide = Join-Path $root "src\win32app\Win32IDE.cpp"
$core = Join-Path $root "src\win32app\Win32IDE_Core.cpp"
$vsui = Join-Path $root "src\win32app\Win32IDE_VSCodeUI.cpp"
$h = Join-Path $root "src\win32app\Win32IDE.h"
$agentOllama = Join-Path $root "src\win32app\Win32IDE_AgentOllamaClient.cpp"
$persist = Join-Path $root "src\win32app\Win32IDE_ChatHistoryPersistence.cpp"
foreach ($p in @($ide, $core, $vsui, $h, $agentOllama, $persist)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing $p" }
}
$s = Get-Content -LiteralPath $ide -Raw
if ($s -notmatch "postAgenticAssistantFinalSafe") { throw "Win32IDE.cpp: expected postAgenticAssistantFinalSafe" }
if ($s -notmatch "applyAgenticAssistantFinalOnUiThread") { throw "Win32IDE.cpp: expected applyAgenticAssistantFinalOnUiThread" }
$c = Get-Content -LiteralPath $core -Raw
if ($c -notmatch "WM_COPILOT_AGENTIC_ASSISTANT_FINAL") { throw "Win32IDE_Core.cpp: expected WM_COPILOT_AGENTIC_ASSISTANT_FINAL handler" }
if ($c -notmatch "WM_COPILOT_RECORD_TOOL_TURN") { throw "Win32IDE_Core.cpp: expected WM_COPILOT_RECORD_TOOL_TURN (tool turn UI-thread message)" }
$v = Get-Content -LiteralPath $vsui -Raw
if ($v -notmatch "applyMinimalAgenticCompletion") { throw "Win32IDE_VSCodeUI.cpp: expected applyMinimalAgenticCompletion" }
$hh = Get-Content -LiteralPath $h -Raw
if ($hh -notmatch "Win32IDEAgenticCopilotFinalEnvelope") { throw "Win32IDE.h: expected Win32IDEAgenticCopilotFinalEnvelope" }
if ($hh -notmatch "syncAgenticToolGuardrailsFromWorkspace") { throw "Win32IDE.h: expected syncAgenticToolGuardrailsFromWorkspace" }
$ao = Get-Content -LiteralPath $agentOllama -Raw
if ($ao -notmatch "syncAgenticToolGuardrailsFromWorkspace") { throw "Win32IDE_AgentOllamaClient.cpp: expected syncAgenticToolGuardrailsFromWorkspace" }
if ($ao -notmatch "SetGuardrails") { throw "Win32IDE_AgentOllamaClient.cpp: expected SetGuardrails (tool allowlist parity)" }
$pp = Get-Content -LiteralPath $persist -Raw
if ($pp -notmatch "applyWorkspaceFolderForChatHistory") { throw "Win32IDE_ChatHistoryPersistence.cpp: expected applyWorkspaceFolderForChatHistory" }
if ($pp -notmatch "syncAgenticToolGuardrailsFromWorkspace") { throw "Win32IDE_ChatHistoryPersistence.cpp: expected sync after workspace folder apply" }
Write-Host "Test-ChatAgenticUiMarshalling: OK" -ForegroundColor Green
exit 0
