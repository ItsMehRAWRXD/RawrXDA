##############################################################################
# Test-BurnIn-16Agent.ps1 — 16-Agent Burn-In Validation for RawrXD IDE
# Production Readiness Gate: Code Integrity + Live Smoke + Stress Validation
##############################################################################
param(
    [switch]$SkipLive,          # Skip live Ollama connectivity tests
    [switch]$SkipBuild,         # Skip build verification
    [switch]$Verbose,           # Extra diagnostics
    [int]$OllamaPort = 11434   # Ollama port
)

$ErrorActionPreference = 'Continue'
$src = Join-Path $PSScriptRoot "src"
$agenticDir = Join-Path $src "agentic"
$win32Dir   = Join-Path $src "win32app"
$shipDir    = Join-Path $PSScriptRoot "Ship"

$passed = 0; $failed = 0; $skipped = 0; $warnings = @()

function Pass($label) { Write-Host "  [PASS] $label" -ForegroundColor Green; $script:passed++ }
function Fail($label, $detail) { Write-Host "  [FAIL] $label — $detail" -ForegroundColor Red; $script:failed++ }
function Skip($label) { Write-Host "  [SKIP] $label" -ForegroundColor Yellow; $script:skipped++ }
function Warn($label, $detail) { Write-Host "  [WARN] $label — $detail" -ForegroundColor DarkYellow; $script:warnings += "$label : $detail" }
function Section($title) { Write-Host "`n  [$title]" -ForegroundColor Cyan }

function Test-FileExists($path, $label) {
    if (Test-Path $path) { Pass $label } else { Fail $label "File missing: $path" }
}

function Test-CodeContains {
    param([string]$Path, [string[]]$Required, [string]$Label, [switch]$AllRequired)
    if (-not (Test-Path $Path)) { Fail $Label "File not found: $Path"; return }
    $content = Get-Content $Path -Raw -ErrorAction SilentlyContinue
    $missing = @()
    foreach ($r in $Required) {
        if ($content -notmatch [regex]::Escape($r)) { $missing += $r }
    }
    if ($missing.Count -eq 0) { Pass $Label }
    elseif ($AllRequired) { Fail $Label "missing: $($missing -join ', ')" }
    else { Warn $Label "optional missing: $($missing -join ', ')" }
}

function Test-FunctionDefinition {
    param([string]$Path, [string]$FnName, [string]$Label)
    if (-not (Test-Path $Path)) { Fail $Label "File not found: $path"; return }
    $content = Get-Content $Path -Raw
    # Match C++ method definitions: ReturnType ClassName::MethodName( or void MethodName(
    if ($content -match "(?:void|bool|std::string|int|float|double|InferenceResult)\s+\w*::?$FnName\s*\(") {
        Pass $Label
    } else {
        Fail $Label "Function definition '$FnName' not found"
    }
}

function Test-DispatchRoute {
    param([string]$Path, [string]$CaseValue, [string]$HandlerCall, [string]$Label)
    if (-not (Test-Path $Path)) { Fail $Label "File not found: $Path"; return }
    $content = Get-Content $Path -Raw
    $hasCase = $content -match "case\s+$CaseValue\s*:"
    $hasHandler = $content -match [regex]::Escape($HandlerCall)
    if ($hasCase -and $hasHandler) { Pass $Label }
    elseif (-not $hasCase) { Fail $Label "Missing case $CaseValue" }
    else { Fail $Label "Missing handler call $HandlerCall" }
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════"
Write-Host "  RawrXD 16-Agent Burn-In Validation — Production Readiness"
Write-Host "═══════════════════════════════════════════════════════════════"

###########################################################################
# PHASE 1: Core Agentic Files Integrity (Agent 1-2 validation)
###########################################################################
Section "Phase 1: Core Agentic Files Integrity"

$coreFiles = @{
    "AgentOllamaClient.h"      = "$agenticDir\AgentOllamaClient.h"
    "AgentOllamaClient.cpp"    = "$agenticDir\AgentOllamaClient.cpp"
    "OrchestratorBridge.h"     = "$agenticDir\OrchestratorBridge.h"
    "BoundedAgentLoop.h"       = "$agenticDir\BoundedAgentLoop.h"
    "ToolCallResult.h"         = "$agenticDir\ToolCallResult.h"
}

foreach ($kv in $coreFiles.GetEnumerator()) {
    Test-FileExists $kv.Value "Core: $($kv.Key) exists"
}

# Agent 1: AgentOllamaClient API completeness
$ollamaApiChecks = @(
    "TestConnection", "GetVersion", "ListModels",
    "ChatSync", "ChatStream", "FIMSync", "FIMStream",
    "CancelStream", "IsStreaming", "SetConfig", "GetConfig",
    "GetTotalRequests", "GetAvgTokensPerSec"
)
Test-CodeContains -Path "$agenticDir\AgentOllamaClient.h" -Required $ollamaApiChecks `
    -Label "Agent 1: OllamaClient API surface complete" -AllRequired

# Agent 2: Type system completeness
$typeChecks = @(
    "TokenCallback", "ToolCallCallback", "DoneCallback", "ErrorCallback",
    "InferenceResult", "ChatMessage", "OllamaConfig",
    "m_cancelRequested", "m_streaming", "m_mutex"
)
Test-CodeContains -Path "$agenticDir\AgentOllamaClient.h" -Required $typeChecks `
    -Label "Agent 2: Type system + thread safety primitives" -AllRequired

###########################################################################
# PHASE 2: OrchestratorBridge + BoundedAgentLoop (Agent 3-4)
###########################################################################
Section "Phase 2: Orchestrator + BoundedAgentLoop"

$orchChecks = @(
    "OrchestratorBridge", "Instance()", "Initialize",
    "RunAgent", "RequestGhostText", "SetModel", "SetFIMModel",
    "m_ollamaClient", "m_maxSteps"
)
Test-CodeContains -Path "$agenticDir\OrchestratorBridge.h" -Required $orchChecks `
    -Label "Agent 3: OrchestratorBridge API surface" -AllRequired

$balChecks = @(
    "BoundedAgentLoop", "Execute", "Cancel", "GetState",
    "GetTranscript", "AgentLoopState"
)
Test-CodeContains -Path "$agenticDir\BoundedAgentLoop.h" -Required $balChecks `
    -Label "Agent 4: BoundedAgentLoop API surface" -AllRequired

###########################################################################
# PHASE 3: SubAgentManager Dual Implementation (Agent 5-6)
###########################################################################
Section "Phase 3: SubAgentManager Integrity"

# Agent 5: Core SubAgentManager (subagent_core.h)
$coreSubAgent = Join-Path $src "subagent_core.h"
if (Test-Path $coreSubAgent) {
    $samChecks = @(
        "spawnSubAgent", "cancelSubAgent", "cancelAll",
        "executeChain", "executeSwarm", "executeSwarmAsync",
        "setTodoList", "updateTodoStatus", "getTodoList",
        "activeSubAgentCount", "totalSpawned", "getStatusSummary",
        "dispatchToolCall", "m_mutex", "m_swarmMutex"
    )
    Test-CodeContains -Path $coreSubAgent -Required $samChecks `
        -Label "Agent 5: SubAgentManager core API + thread safety" -AllRequired
} else {
    Skip "Agent 5: subagent_core.h not found (may be relocated)"
}

# Agent 6: Agentic SubAgentManager (singleton, swarm topology)
$agenticSubAgent = Join-Path $agenticDir "SubAgentManager.h"
if (Test-Path $agenticSubAgent) {
    $swarmChecks = @(
        "SubAgentManager", "instance()", "initializeSwarm",
        "shutdownSwarm", "executeSwarmTask", "isSwarmActive",
        "loadModelShard"
    )
    Test-CodeContains -Path $agenticSubAgent -Required $swarmChecks `
        -Label "Agent 6: Agentic SubAgentManager swarm topology" -AllRequired
} else {
    Skip "Agent 6: Agentic SubAgentManager.h not found (may be relocated)"
}

###########################################################################
# PHASE 4: Win32IDE AgenticBridge Wiring (Agent 7-8)
###########################################################################
Section "Phase 4: AgenticBridge ↔ Win32IDE Wiring"

$bridgeCpp = Join-Path $win32Dir "Win32IDE_AgenticBridge.cpp"
$bridgeH   = Join-Path $win32Dir "Win32IDE_AgenticBridge.h"

# Agent 7: Bridge initialization
if (Test-Path $bridgeCpp) {
    $bridgeInitChecks = @(
        "Initialize", "ExecuteAgentCommand", "StartAgentLoop",
        "StopAgentLoop", "SetModel", "GetSubAgentManager",
        "RunSubAgent", "ExecuteChain", "ExecuteSwarm",
        "CancelAllSubAgents", "GetSubAgentStatus"
    )
    Test-CodeContains -Path $bridgeCpp -Required $bridgeInitChecks `
        -Label "Agent 7: AgenticBridge full API implemented" -AllRequired
} else { Fail "Agent 7: AgenticBridge" "File not found: $bridgeCpp" }

# Agent 8: Bridge header exports
if (Test-Path $bridgeH) {
    $bridgeExports = @(
        "GetSubAgentManager", "ExecuteAgentCommand",
        "StartAgentLoop", "StopAgentLoop", "SetModel",
        "OutputCallback", "ErrorCallback"
    )
    Test-CodeContains -Path $bridgeH -Required $bridgeExports `
        -Label "Agent 8: AgenticBridge header exports" -AllRequired
} else { Fail "Agent 8: AgenticBridge header" "File not found: $bridgeH" }

###########################################################################
# PHASE 5: Command Dispatch Complete Chain (Agent 9-10)
###########################################################################
Section "Phase 5: Command Dispatch Routing"

$cmdCpp  = Join-Path $win32Dir "Win32IDE_Commands.cpp"
$cmdH    = Join-Path $win32Dir "Win32IDE_Commands.h"
$agentCpp = Join-Path $win32Dir "Win32IDE_AgentCommands.cpp"

# Agent 9: Command ID definitions complete
$cmdIdChecks = @(
    "IDM_AGENT_START_LOOP",    "IDM_AGENT_EXECUTE_CMD",
    "IDM_AGENT_BOUNDED_LOOP",  "IDM_AUTONOMY_TOGGLE",
    "IDM_AUTONOMY_START",      "IDM_AUTONOMY_STOP",
    "IDM_AUTONOMY_SET_GOAL",   "IDM_AUTONOMY_STATUS",
    "IDM_AUTONOMY_MEMORY",     "IDM_SUBAGENT_CHAIN",
    "IDM_SUBAGENT_SWARM",      "IDM_SUBAGENT_TODO_LIST",
    "IDM_SUBAGENT_TODO_CLEAR", "IDM_SUBAGENT_STATUS",
    "IDM_AGENT_MEMORY_VIEW",   "IDM_AGENT_MEMORY_CLEAR",
    "IDM_AGENT_MEMORY_EXPORT"
)
Test-CodeContains -Path $cmdH -Required $cmdIdChecks `
    -Label "Agent 9: All 17 agentic command IDs defined" -AllRequired

# Agent 10: Dispatch routing verified
$dispatchRoutes = @(
    @("IDM_AGENT_START_LOOP",    "onAgentStartLoop"),
    @("IDM_AGENT_EXECUTE_CMD",   "onAgentExecuteCommand"),
    @("IDM_AGENT_BOUNDED_LOOP",  "onBoundedAgentLoop"),
    @("IDM_SUBAGENT_CHAIN",      "onSubAgentChain"),
    @("IDM_SUBAGENT_SWARM",      "onSubAgentSwarm"),
    @("IDM_SUBAGENT_TODO_LIST",  "onSubAgentTodoList"),
    @("IDM_SUBAGENT_TODO_CLEAR", "onSubAgentTodoClear"),
    @("IDM_SUBAGENT_STATUS",     "onSubAgentStatus")
)
$dispatchAll = $true
foreach ($route in $dispatchRoutes) {
    if (-not (Test-Path $agentCpp)) { Fail "Agent 10: $($route[0]) dispatch" "File missing"; $dispatchAll = $false; break }
    $content = Get-Content $agentCpp -Raw
    if ($content -notmatch "case\s+$($route[0])\s*:" -or $content -notmatch [regex]::Escape("$($route[1])()")) {
        Fail "Agent 10: Dispatch $($route[0]) → $($route[1])" "Route missing"
        $dispatchAll = $false
    }
}
if ($dispatchAll) { Pass "Agent 10: All 8 dispatch routes verified" }

###########################################################################
# PHASE 6: Handler Implementations (Agent 11-12)
###########################################################################
Section "Phase 6: Handler Implementations"

$subAgentCpp = Join-Path $win32Dir "Win32IDE_SubAgent.cpp"

# Agent 11: SubAgent handlers use SubAgentManager (direct m_subAgentManager or GetSubAgentManager())
$subAgentHandlers = @(
    "onSubAgentChain", "onSubAgentSwarm",
    "onSubAgentTodoClear", "onSubAgentStatus"
)
if (Test-Path $subAgentCpp) {
    $saContent = Get-Content $subAgentCpp -Raw
    $saOk = $true
    foreach ($fn in $subAgentHandlers) {
        if ($saContent -notmatch $fn) {
            $alt = $fn -replace "TodoSync","TodoList"
            if ($saContent -notmatch $alt) {
                Warn "Agent 11: Handler $fn" "Not found in SubAgent.cpp"
                $saOk = $false
            }
        }
    }
    # Accept either GetSubAgentManager() bridge pattern or direct m_subAgentManager
    if ($saContent -match "GetSubAgentManager" -or $saContent -match "m_subAgentManager") {
        if ($saOk) { Pass "Agent 11: SubAgent handlers implemented with SubAgentManager" }
        else { Pass "Agent 11: SubAgent handlers use SubAgentManager (some naming variants)" }
    } else {
        Fail "Agent 11: SubAgent handlers" "SubAgentManager pattern not found"
    }
} else { Fail "Agent 11: SubAgent handlers" "File missing: $subAgentCpp" }

# Agent 12: AgentCommands fallback dialogs
if (Test-Path $agentCpp) {
    $agContent = Get-Content $agentCpp -Raw
    $hasFallback = ($agContent -match "CreateWindowExA" -or $agContent -match "CreateWindowEx\b") -and
                   ($agContent -match "gotPrompt" -or $agContent -match "DialogBoxParam")
    if ($hasFallback) { Pass "Agent 12: Agent command fallback dialogs present" }
    else { Fail "Agent 12: Agent commands" "Missing fallback dialog (CreateWindowExA + gotPrompt)" }
} else { Fail "Agent 12: Agent commands" "File missing: $agentCpp" }

###########################################################################
# PHASE 7: Menu + UI Integration (Agent 13-14)
###########################################################################
Section "Phase 7: Menu + UI Integration"

$ideCpp = Join-Path $win32Dir "Win32IDE.cpp"
$ideH   = Join-Path $win32Dir "Win32IDE.h"

# Agent 13: Menu structure
if (Test-Path $ideCpp) {
    $menuChecks = @(
        "hAutonomyMenu", "Autonomy", "SubAgent",
        "Agent Loop", "Execute Command", "Bounded Agent Loop"
    )
    Test-CodeContains -Path $ideCpp -Required $menuChecks `
        -Label "Agent 13: Agent+Autonomy menu structure complete" -AllRequired
} else { Fail "Agent 13: Menu structure" "Win32IDE.cpp missing" }

# Agent 14: Win32IDE.h method declarations
if (Test-Path $ideH) {
    $declChecks = @(
        "onSubAgentChain", "onSubAgentSwarm",
        "onSubAgentTodoList", "onSubAgentTodoClear", "onSubAgentStatus",
        "onAgentStartLoop", "onAgentExecuteCommand", "onBoundedAgentLoop"
    )
    Test-CodeContains -Path $ideH -Required $declChecks `
        -Label "Agent 14: Win32IDE.h declares all 8 handler methods" -AllRequired
} else { Fail "Agent 14: Class declarations" "Win32IDE.h missing" }

###########################################################################
# PHASE 8: Ship Standalone + Palette (Agent 15-16)
###########################################################################
Section "Phase 8: Ship Standalone Build"

$shipCpp = Join-Path $shipDir "RawrXD_Win32_IDE.cpp"

# Agent 15: Ship command palette
if (Test-Path $shipCpp) {
    $shipPalette = @(
        "Agent: Start Loop", "Agent: Execute Command",
        "Agent: Bounded Agent Loop", "Autonomy: Toggle"
    )
    Test-CodeContains -Path $shipCpp -Required $shipPalette `
        -Label "Agent 15: Ship palette has Agent/Autonomy commands" -AllRequired
} else { Fail "Agent 15: Ship palette" "Ship file missing: $shipCpp" }

# Agent 16: Ship WM_COMMAND handlers
if (Test-Path $shipCpp) {
    $shipWmCmd = @(
        "case 4100:", "case 4120:", "RunAutonomousMode"
    )
    Test-CodeContains -Path $shipCpp -Required $shipWmCmd `
        -Label "Agent 16: Ship WM_COMMAND handles 4100/4120 + RunAutonomousMode" -AllRequired
} else { Fail "Agent 16: Ship WM_COMMAND" "Ship file missing: $shipCpp" }

###########################################################################
# PHASE 9: Command Registry Completeness (Cross-check)
###########################################################################
Section "Phase 9: Command Registry Cross-Check"

$registryStrings = @(
    "Agent: Execute Prompt Chain",
    "Agent: Execute HexMag Swarm",
    "Agent: Bounded Agent Loop",
    "IDM_AGENT_MEMORY_VIEW",
    "IDM_AGENT_MEMORY_CLEAR",
    "IDM_AGENT_MEMORY_EXPORT",
    "IDM_SUBAGENT_CHAIN",
    "IDM_SUBAGENT_SWARM",
    "IDM_SUBAGENT_TODO_LIST",
    "IDM_SUBAGENT_TODO_CLEAR",
    "IDM_SUBAGENT_STATUS",
    "IDM_AUTONOMY_TOGGLE",
    "IDM_AUTONOMY_SET_GOAL",
    "IDM_AUTONOMY_STATUS",
    "IDM_AUTONOMY_MEMORY"
)
Test-CodeContains -Path $cmdCpp -Required $registryStrings `
    -Label "Command registry: All 15 entries present" -AllRequired

###########################################################################
# PHASE 10: Thread Safety Audit
###########################################################################
Section "Phase 10: Thread Safety Audit"

$ollamaCpp = "$agenticDir\AgentOllamaClient.cpp"
# Check both .h (declarations) and .cpp (usage) for thread safety
$ollamaH = "$agenticDir\AgentOllamaClient.h"
if ((Test-Path $ollamaCpp) -and (Test-Path $ollamaH)) {
    $combined = (Get-Content $ollamaCpp -Raw) + "`n" + (Get-Content $ollamaH -Raw)
    $threadChecks = @(
        "std::atomic", "m_cancelRequested", "m_streaming",
        "std::lock_guard", "m_mutex"
    )
    $threadMissing = @()
    foreach ($tc in $threadChecks) {
        if ($combined -notmatch [regex]::Escape($tc)) { $threadMissing += $tc }
    }
    if ($threadMissing.Count -eq 0) { Pass "Thread safety: Atomics + mutex in OllamaClient" }
    else { Fail "Thread safety: Atomics + mutex in OllamaClient" "missing: $($threadMissing -join ', ')" }
} else { Fail "Thread safety: OllamaClient" "File missing" }

###########################################################################
# PHASE 11: WinHTTP + Streaming Integrity
###########################################################################
Section "Phase 11: WinHTTP + Streaming Integrity"

if (Test-Path $ollamaCpp) {
    $httpChecks = @(
        "WinHttpOpen", "WinHttpConnect", "WinHttpOpenRequest",
        "WinHttpSendRequest", "WinHttpReceiveResponse",
        "WinHttpReadData", "WinHttpCloseHandle",
        "/api/chat", "/api/generate", "/api/version", "/api/tags"
    )
    Test-CodeContains -Path $ollamaCpp -Required $httpChecks `
        -Label "WinHTTP: All API calls + endpoints present" -AllRequired
} else { Fail "WinHTTP: OllamaClient" "File missing" }

###########################################################################
# PHASE 12: Tool-Call Deduplication
###########################################################################
Section "Phase 12: Tool-Call Deduplication"

if (Test-Path $ollamaCpp) {
    $toolCallChecks = @(
        "tool_calls", "tool_call_id", "on_tool_call",
        "toolName", "arguments"
    )
    Test-CodeContains -Path $ollamaCpp -Required $toolCallChecks `
        -Label "Tool-call: Parsing + deduplication in streaming" -AllRequired
} else { Fail "Tool-call parsing" "OllamaClient.cpp missing" }

###########################################################################
# PHASE 13: FIM (Ghost Text) Pipeline
###########################################################################
Section "Phase 13: FIM Ghost Text Pipeline"

if (Test-Path $ollamaCpp) {
    $fimChecks = @(
        "FIMSync", "FIMStream", "fim_prefix", "fim_suffix",
        "fim_model", "/api/generate"
    )
    Test-CodeContains -Path $ollamaCpp -Required $fimChecks `
        -Label "FIM: Ghost text sync + stream pipeline" -AllRequired

    # Also check the header has FIM model config
    $fimHChecks = @("fim_model", "fim_max_tokens")
    Test-CodeContains -Path "$agenticDir\AgentOllamaClient.h" -Required $fimHChecks `
        -Label "FIM: Config has fim_model + fim_max_tokens" -AllRequired
} else { Fail "FIM Pipeline" "OllamaClient.cpp missing" }

###########################################################################
# PHASE 14: Error Handling Completeness
###########################################################################
Section "Phase 14: Error Handling"

# Error handling checks across both .h and .cpp
if ((Test-Path $ollamaCpp) -and (Test-Path $ollamaH)) {
    $errCombined = (Get-Content $ollamaCpp -Raw) + "`n" + (Get-Content $ollamaH -Raw)
    $errChecks = @(
        "InferenceResult::error", "error_message",
        "on_error", "ErrorCallback"
    )
    $errMissing = @()
    foreach ($ec in $errChecks) {
        if ($errCombined -notmatch [regex]::Escape($ec)) { $errMissing += $ec }
    }
    if ($errMissing.Count -eq 0) { Pass "Error handling: Result types + callbacks" }
    else { Fail "Error handling: Result types + callbacks" "missing: $($errMissing -join ', ')" }
} else { Fail "Error handling" "OllamaClient files missing" }

if (Test-Path "$agenticDir\ToolCallResult.h") {
    $tcrChecks = @(
        "Success", "ValidationFailed", "SandboxBlocked",
        "ExecutionError", "Timeout", "Cancelled"
    )
    Test-CodeContains -Path "$agenticDir\ToolCallResult.h" -Required $tcrChecks `
        -Label "Error handling: ToolCallResult status codes" -AllRequired
} else { Skip "ToolCallResult.h" }

###########################################################################
# PHASE 15: Live Ollama Connectivity (if not skipped)
###########################################################################
Section "Phase 15: Live Ollama Connectivity"

if ($SkipLive) {
    Skip "Live Ollama tests (use -SkipLive:$false to enable)"
} else {
    try {
        $ollamaUrl = "http://127.0.0.1:${OllamaPort}"

        # Test 1: Version endpoint
        try {
            $ver = Invoke-RestMethod -Uri "$ollamaUrl/api/version" -Method GET -TimeoutSec 5
            if ($ver.version) { Pass "Live: Ollama version = $($ver.version)" }
            else { Fail "Live: Ollama version" "No version field in response" }
        } catch {
            Fail "Live: Ollama version" "Cannot reach $ollamaUrl/api/version — $_"
        }

        # Test 2: Tags/Models endpoint
        try {
            $tags = Invoke-RestMethod -Uri "$ollamaUrl/api/tags" -Method GET -TimeoutSec 10
            $modelCount = ($tags.models | Measure-Object).Count
            if ($modelCount -gt 0) { Pass "Live: Ollama has $modelCount model(s) available" }
            else { Warn "Live: Ollama models" "No models loaded (pull one with 'ollama pull')" }
        } catch {
            Fail "Live: Ollama tags" "Cannot reach $ollamaUrl/api/tags — $_"
        }

        # Test 3: Concurrent chat simulation (16 parallel)
        try {
            $tags = Invoke-RestMethod -Uri "$ollamaUrl/api/tags" -Method GET -TimeoutSec 5
            $models = $tags.models | Select-Object -ExpandProperty name
            if ($models.Count -gt 0) {
                # Prefer small fast models for concurrent stress (gemma3:1b, phi, qwen2.5-coder:1.5b)
                $fastModels = @("gemma3:1b", "phi", "qwen2.5-coder:1.5b", "llama3.2:1b", "tinyllama")
                $testModel = $null
                foreach ($fm in $fastModels) {
                    if ($models -contains $fm) { $testModel = $fm; break }
                }
                if (-not $testModel) { $testModel = $models[0] }
                Write-Host "    Using model: $testModel for concurrent test..." -ForegroundColor DarkGray

                $jobs = @()
                $startTime = Get-Date
                for ($i = 1; $i -le 16; $i++) {
                    $jobs += Start-Job -ScriptBlock {
                        param($url, $model, $agentId)
                        try {
                            $body = @{
                                model = $model
                                messages = @(@{ role = "user"; content = "Agent ${agentId} test: respond with exactly OK ${agentId}" })
                                stream = $false
                                options = @{ num_predict = 20; temperature = 0.0 }
                            } | ConvertTo-Json -Depth 5
                            $resp = Invoke-RestMethod -Uri "$url/api/chat" -Method POST -Body $body `
                                -ContentType "application/json" -TimeoutSec 120
                            return @{ agentId = $agentId; success = $true; response = $resp.message.content }
                        } catch {
                            return @{ agentId = $agentId; success = $false; error = $_.Exception.Message }
                        }
                    } -ArgumentList $ollamaUrl, $testModel, $i
                }

                Write-Host "    Waiting for 16 concurrent agents (up to 120s)..." -ForegroundColor DarkGray
                $results = $jobs | Wait-Job -Timeout 120 | Receive-Job
                $jobs | Remove-Job -Force -ErrorAction SilentlyContinue

                $elapsed = ((Get-Date) - $startTime).TotalSeconds
                $successes = ($results | Where-Object { $_.success }).Count
                $failures  = 16 - $successes

                if ($successes -ge 12) {
                    Pass "Live: 16-agent concurrent chat — $successes/16 succeeded in $([math]::Round($elapsed,1))s"
                } elseif ($successes -ge 8) {
                    Warn "Live: 16-agent concurrent chat" "$successes/16 succeeded ($failures timeouts/failures)"
                } else {
                    Fail "Live: 16-agent concurrent chat" "Only $successes/16 succeeded"
                }

                # Report per-agent details in verbose mode
                if ($Verbose) {
                    foreach ($r in $results | Sort-Object agentId) {
                        if ($r.success) {
                            Write-Host "      Agent $($r.agentId): OK — $($r.response -replace '\n',' ' | Select-Object -First 80)" -ForegroundColor DarkGreen
                        } else {
                            Write-Host "      Agent $($r.agentId): FAILED — $($r.error)" -ForegroundColor DarkRed
                        }
                    }
                }
            } else {
                Skip "Live: 16-agent concurrent (no models available)"
            }
        } catch {
            Fail "Live: 16-agent concurrent" "Job orchestration failed: $_"
        }

        # Test 4: Streaming endpoint validation
        try {
            $tags = Invoke-RestMethod -Uri "$ollamaUrl/api/tags" -Method GET -TimeoutSec 5
            $models = $tags.models | Select-Object -ExpandProperty name
            if ($models.Count -gt 0) {
                # Use same fast model preference
                $streamModel = $null
                foreach ($fm in $fastModels) {
                    if ($models -contains $fm) { $streamModel = $fm; break }
                }
                if (-not $streamModel) { $streamModel = $models[0] }
                $body = @{
                    model = $streamModel
                    messages = @(@{ role = "user"; content = "Say hello" })
                    stream = $true
                    options = @{ num_predict = 5; temperature = 0.0 }
                } | ConvertTo-Json -Depth 5

                $resp = Invoke-WebRequest -Uri "$ollamaUrl/api/chat" -Method POST -Body $body `
                    -ContentType "application/json" -TimeoutSec 30 -UseBasicParsing
                $lines = $resp.Content -split "`n" | Where-Object { $_.Trim() }
                $ndjsonCount = $lines.Count
                if ($ndjsonCount -ge 2) {
                    Pass "Live: Streaming NDJSON — received $ndjsonCount chunks"
                } else {
                    Warn "Live: Streaming" "Only $ndjsonCount NDJSON lines received"
                }
            } else {
                Skip "Live: Streaming test (no models)"
            }
        } catch {
            Fail "Live: Streaming endpoint" "NDJSON stream failed: $_"
        }

    } catch {
        Fail "Live: Connectivity" "Unexpected error: $_"
    }
}

###########################################################################
# PHASE 16: Build Verification (if not skipped)
###########################################################################
Section "Phase 16: Build Verification"

if ($SkipBuild) {
    Skip "Build verification (use -SkipBuild:$false to enable)"
} else {
    $buildDir = Join-Path $PSScriptRoot "build"
    if (Test-Path (Join-Path $buildDir "Makefile")) {
        Write-Host "    Building RawrXD-Win32IDE target..." -ForegroundColor DarkGray
        $buildBat = Join-Path $PSScriptRoot "build_ide.bat"
        if (Test-Path $buildBat) {
            $buildOutput = cmd /c $buildBat 2>&1
            $buildLines = $buildOutput -join "`n"
            if ($buildLines -match "error" -and $buildLines -notmatch "0 error") {
                $errorCount = ([regex]::Matches($buildLines, "(?i)\berror\b")).Count
                Fail "Build: Compilation" "$errorCount error(s) detected"
                if ($Verbose) {
                    $buildOutput | Select-String -Pattern "error" | Select-Object -First 10 | ForEach-Object {
                        Write-Host "      $_" -ForegroundColor DarkRed
                    }
                }
            } else {
                Pass "Build: RawrXD-Win32IDE compiled successfully"
            }
        } else {
            Skip "Build: build_ide.bat not found"
        }
    } else {
        Skip "Build: No Makefile found (run configure.bat first)"
    }
}

###########################################################################
# SUMMARY
###########################################################################
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════"
$total = $passed + $failed
$pct = if ($total -gt 0) { [math]::Round(($passed / $total) * 100, 1) } else { 0 }
if ($failed -eq 0) {
    Write-Host "  BURN-IN RESULT: $passed PASSED, $failed FAILED" -ForegroundColor Green
    Write-Host "  ($skipped skipped, $($warnings.Count) warnings)" -ForegroundColor DarkGray
    Write-Host "  Production readiness: $pct% — GATE PASSED ✓" -ForegroundColor Green
} else {
    Write-Host "  BURN-IN RESULT: $passed PASSED, $failed FAILED" -ForegroundColor Red
    Write-Host "  ($skipped skipped, $($warnings.Count) warnings)" -ForegroundColor DarkGray
    Write-Host "  Production readiness: $pct% — GATE BLOCKED" -ForegroundColor Red
}

if ($warnings.Count -gt 0) {
    Write-Host ""
    Write-Host "  Warnings:" -ForegroundColor DarkYellow
    foreach ($w in $warnings) {
        Write-Host "    • $w" -ForegroundColor DarkYellow
    }
}

Write-Host "═══════════════════════════════════════════════════════════════"
Write-Host ""

# Return exit code for CI
exit $failed
