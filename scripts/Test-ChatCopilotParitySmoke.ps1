# Validates source-level Copilot parity: routing, tool persistence, TPS metrics, minimal-agent completion encoding.
# Run: pwsh -File scripts/Test-ChatCopilotParitySmoke.ps1
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent

function Assert-FileContains([string]$relPath, [string]$pattern, [string]$desc) {
    $p = Join-Path $repoRoot $relPath
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing $relPath" }
    $raw = Get-Content -LiteralPath $p -Raw
    if ($raw -notmatch $pattern) { throw "FAIL: $desc ($relPath)" }
}

Assert-FileContains "src\win32app\Win32IDE_ChatMessageRenderer.cpp" 'kToolLabel' "tool role uses distinct label color (Copilot-style)"
Assert-FileContains "src\win32app\Win32IDE_ChatMessageRenderer.cpp" 'DisplayRoleForChatUi' "role headers normalized (User/Copilot/Tool/System)"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'recordCopilotThroughputAtComplete' "chat TPS throughput helper"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'chat\.copilot\.estimated_tps' "TPS gauge name"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'chat\.minimal_agentic\.estimated_tps' "minimal-agentic TPS gauge"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'chat\.minimal_agentic\.generation_ms' "minimal-agentic latency histogram"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'MAKEWPARAM' "worker encodes tool count in PostMessage"
Assert-FileContains "src\win32app\Win32IDE_CopilotNativeGlue.cpp" 'applyMinimalAgenticCompletion' "glue finishes via applyMinimalAgenticCompletion"
Assert-FileContains "src\win32app\Win32IDE_VSCodeUI.cpp" 'recordToolTurnInChatHistory\("minimal_agentic"' "aggregated tool row when tool_steps empty but calls counted"
Assert-FileContains "src\win32app\Win32IDE_VSCodeUI.cpp" 'appendModelLoadReadyCopilotTurns' "model load persists system turn + assistant welcome"
Assert-FileContains "src\win32app\Win32IDE_VSCodeUI.cpp" 'rehydrateConversationSessionFromChatHistory' "minimal-agent transcript_delta resyncs ConversationSession after system/tool"
Assert-FileContains "src\agent\project_scoped_chat.cpp" 'MessageRole::Tool' "disk format supports tool role"
Assert-FileContains "src\win32app\Win32IDE_ChatHistoryPersistence.cpp" 'recordToolTurnInChatHistory' "persistence wiring"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'postCopilotRecordToolTurnSafe' "agentic tool turns marshaled to UI thread"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'postAgenticAssistantFinalSafe' "agentic assistant completion marshaled to UI thread"
Assert-FileContains "src\win32app\Win32IDE.cpp" 'void Win32IDE::openWorkspaceFolder' "File>Open Folder → applyWorkspaceFolderForChatHistory + agentic guardrails"
Assert-FileContains "src\win32app\Win32IDE_VSCodeUI.cpp" 'tpsCopilot = METRICS.getGauge' "status bar reads Copilot TPS gauge for UX parity"
Assert-FileContains "src\win32app\Win32IDE.h" 'WM_STATUSBAR_REFRESH_COPILOT' "posted refresh after async chat throughput sampling"
Assert-FileContains "src\agentic\AgentToolHandlers.cpp" 'same tree as File Explorer' "agent prompt ties tools to IDE explorer"
Assert-FileContains "src\agentic\agent_controller_minimal.h" 'transcript_delta' "minimal agent exposes transcript slice for replay"
Assert-FileContains "src\win32app\agentic_headless_laneb_link_stubs.cpp" 'SubAgentManager::dispatchToolCall' "RawrEngine links headless SubAgent + profiler stubs"
Assert-FileContains "src\core\feature_handlers_model_load.cpp" 'handleFileLoadModel' "IDE/CLI GGUF load handler lives in linkable TU (autonomy + Lane B parity)"
Assert-FileContains "src\core\feature_handlers.cpp" 'feature_handlers_model_load.cpp' "feature_handlers delegates model load to shared TU"
Assert-FileContains "src\win32app\agentic_bridge_headless.cpp" 'HeadlessLaneBToolSurface' "Lane B headless build stubs HTTP tool surface when full ToolRegistry is not linked"
Assert-FileContains "src\win32app\agentic_bridge_headless.cpp" 'RAWRXD_HEADLESS_ONLY' "headless bridge gates full ToolRegistry vs Lane B stub"
Assert-FileContains "src\agentic\agent_controller_minimal.cpp" 'BuildCompactToolCatalogForPrompt' "registry tool catalog wired into minimal agent prompts"
Assert-FileContains "src\agentic\agent_controller_minimal.cpp" '\.HasTool' "tool registration uses AgentToolHandlers dispatch table"
Assert-FileContains "src\agentic\agentic_controller_wiring.cpp" 'workspace_root' "processAgenticRequest syncs workspace + sandbox"
Assert-FileContains "src\agentic\ToolRegistry.cpp" 'static AgentToolRegistry instance' "AgentToolRegistry::Instance uses Meyers singleton (stable mutex / --agentic-smoke)"
Assert-FileContains "src\win32app\Win32IDE_ChatPanel.cpp" 'rehydrateConversationSessionFromChatHistory' "persisted chat replays into ConversationSession (Copilot-style context)"
Assert-FileContains "src\win32app\Win32IDE_ChatPanel.cpp" 'AddToolResult' "tool turns rehydrate into session for follow-up prompts"
Assert-FileContains "src\agentic\AgentToolHandlers.cpp" 'm_dispatchTable\["read_file"\]' "explorer-scope read_file registered for agentic IDE"
Assert-FileContains "src\agentic\AgentToolHandlers.cpp" 'm_dispatchTable\["list_dir"\]' "explorer-scope list_dir registered for agentic IDE"
Assert-FileContains "src\main_headless_core.cpp" 'wall_ms' "CLI copilot-smoke JSON includes wall clock ms"
Assert-FileContains "src\main_headless_core.cpp" 'estimated_tps' "CLI copilot-smoke JSON includes rough throughput (chars/sec)"
Assert-FileContains "src\main_headless_core.cpp" 'agentic_response_type' "CLI copilot-smoke JSON includes AgenticBridge ExecuteAgentCommand branch"
Assert-FileContains "src\main_headless_core.cpp" 'ExecuteAgentCommand' "CLI copilot-smoke invokes headless agentic command path"
Assert-FileContains "src\main_headless_core.cpp" 'RAWRXD_COPILOT_SMOKE_FAST_EXIT' "optional fast exit after successful copilot-smoke (smoketest teardown)"
Assert-FileContains "src\main.cpp" 'agentic:, or @agent' "CLI /agent usage documents same prefixes as Win32 Copilot (parity string)"

$mac = Join-Path $repoRoot "src\agentic\agent_controller_minimal.cpp"
$macRaw = Get-Content -LiteralPath $mac -Raw
if ($macRaw -match 'std::\s*cout|std::\s*cerr') {
    throw "FAIL: agent_controller_minimal.cpp must log via RawrXD::Logging::Logger, not std::cout/std::cerr"
}

Write-Host "OK: Chat Copilot parity smoke (persistence + TPS metrics + minimal-agent encoding + CLI throughput JSON)."
exit 0
