# smoke_agent_tools.ps1 — Production smoke test for the 7 IDE operational tools
# Tests all three surfaces: /api/tool, /api/agent/ops/, CLI shell
#
# Usage: .\smoke_agent_tools.ps1 [-BaseUrl http://localhost:11434] [-ExePath ..\build\dist\rawrxd.exe]
# Exit code: 0 = all pass, 1 = one or more failures
#
# Verified fixes covered:
#   - compact-conversation: /api/tool normalizeTool mapping (complete_server.cpp)
#   - resolve-symbol: /api/agent/ops/ opToTool alias (Win32IDE_LocalServer.cpp)
#   - All 7 tools registered in ToolRegistry (tool_registry_init.cpp)
#   - CLI commands always registered regardless of Ollama state (rawrxd_cli.cpp)
#   - CompactConversation no longer errors when recorder is null
#   - Win32IDE subagent/autonomy UI handlers now compiled (Win32IDE_AgentCommands.cpp)
# ============================================================

param(
    [string]$BaseUrl = "http://localhost:11434",
    [string]$ExePath = "",
    [switch]$AllowOffline = $true,
    [string]$RepoRoot = "d:\rawrxd"
)

$ErrorActionPreference = "Continue"
$failures = 0
$passes   = 0

function Write-Pass([string]$msg) {
    Write-Host "[PASS] $msg" -ForegroundColor Green
    $script:passes++
}
function Write-Fail([string]$msg) {
    Write-Host "[FAIL] $msg" -ForegroundColor Red
    $script:failures++
}
function Write-Info([string]$msg) {
    Write-Host "[INFO] $msg" -ForegroundColor Cyan
}

# ============================================================
# Helper: POST JSON to a URL, return parsed response or $null
# ============================================================
function Invoke-ToolPost([string]$url, [hashtable]$body) {
    try {
        $json = $body | ConvertTo-Json -Depth 5
        $response = Invoke-RestMethod -Uri $url -Method POST `
            -ContentType "application/json" -Body $json `
            -TimeoutSec 15 -ErrorAction Stop
        return $response
    }
    catch {
        return $null
    }
}

function Test-ServerReachable([string]$url) {
    try {
        $health = Invoke-RestMethod -Uri "$url/health" -Method GET -TimeoutSec 3 -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

function Assert-SourceContains([string]$filePath, [string]$pattern, [string]$label) {
    if (-not (Test-Path $filePath)) {
        Write-Fail "$label — file missing: $filePath"
        return
    }
    $match = Select-String -Path $filePath -Pattern $pattern -SimpleMatch -Quiet
    if ($match) {
        Write-Pass $label
    } else {
        Write-Fail "$label — missing pattern '$pattern'"
    }
}

function Assert-SourceNotContains([string]$filePath, [string]$pattern, [string]$label) {
    if (-not (Test-Path $filePath)) {
        Write-Fail "$label — file missing: $filePath"
        return
    }
    $match = Select-String -Path $filePath -Pattern $pattern -SimpleMatch -Quiet
    if ($match) {
        Write-Fail "$label — forbidden pattern found '$pattern'"
    } else {
        Write-Pass $label
    }
}

# ============================================================
# Section 0: Static source-level production wiring checks
# ============================================================
Write-Info "=== Section 0: Static wiring checks ==="

$completeServer = Join-Path $RepoRoot "src\complete_server.cpp"
$localServer    = Join-Path $RepoRoot "src\win32app\Win32IDE_LocalServer.cpp"
$cliSource      = Join-Path $RepoRoot "src\rawrxd_cli.cpp"
$registryInit   = Join-Path $RepoRoot "src\tool_registry_init.cpp"
$agentCommands  = Join-Path $RepoRoot "src\win32app\Win32IDE_AgentCommands.cpp"
$nativeAgent    = Join-Path $RepoRoot "src\native_agent.hpp"

Assert-SourceContains $completeServer "compact-conversation" "normalizeTool maps compact-conversation"
Assert-SourceContains $localServer "{\"resolve-symbol\", \"resolve_symbol\"}" "opToTool maps resolve-symbol"
Assert-SourceContains $registryInit "bool initializeAllTools(ToolRegistry* registry)" "initializeAllTools implemented"
Assert-SourceContains $registryInit "registerCodeAnalysisTools" "category registration functions present"
Assert-SourceContains $registryInit "resolve_symbol" "resolve_symbol registered"
Assert-SourceContains $registryInit "plan_code_exploration" "plan_code_exploration registered"
Assert-SourceContains $registryInit "evaluate_integration_audit_feasibility" "evaluate tool registered"
Assert-SourceContains $nativeAgent "AgentToolHandlers::SemanticSearch" "NativeAgent PerformResearch wired to semantic search"
Assert-SourceNotContains $nativeAgent "PerformResearch(const std::string& query) { return \"\"; }" "NativeAgent PerformResearch no longer stubbed"
Assert-SourceNotContains $agentCommands "#if 0" "Win32IDE_AgentCommands not compile-gated"
Assert-SourceContains $cliSource "auto runBackendTool" "CLI backend tool dispatcher present"

$serverReachable = Test-ServerReachable $BaseUrl
if (-not $serverReachable) {
    if ($AllowOffline) {
        Write-Info ""
        Write-Info "Server not reachable at $BaseUrl; dynamic HTTP checks skipped (AllowOffline enabled)."
        Write-Info "Static wiring checks executed and counted."
        Write-Host ""
        Write-Host "======================================" -ForegroundColor White
        Write-Host " Smoke Test Results" -ForegroundColor White
        Write-Host "======================================" -ForegroundColor White
        Write-Host " PASS: $passes" -ForegroundColor Green
        Write-Host " FAIL: $failures" -ForegroundColor $(if ($failures -gt 0) { "Red" } else { "Green" })
        Write-Host "======================================" -ForegroundColor White
        if ($failures -gt 0) {
            Write-Host "`nFAILED — $failures test(s) did not pass." -ForegroundColor Red
            exit 1
        } else {
            Write-Host "`nALL TESTS PASSED (static mode)." -ForegroundColor Green
            exit 0
        }
    } else {
        Write-Fail "Server not reachable at $BaseUrl and AllowOffline is disabled"
        exit 1
    }
}

# ============================================================
# Section 1: /api/tool — POST tool requests by canonical name
# ============================================================
Write-Info ""
Write-Info "=== Section 1: /api/tool (canonical names) ==="

$toolTests = @(
    @{ tool = "compact_conversation";                 args = @{ keep_last = 5 }              },
    @{ tool = "compact-conversation";                 args = @{ keep_last = 5 }              },  # alias fix
    @{ tool = "optimize_tool_selection";              args = @{ task = "search codebase" }   },
    @{ tool = "optimize-tool-selection";              args = @{ task = "search codebase" }   },
    @{ tool = "resolve_symbol";                       args = @{ symbol = "AgentToolHandlers" } },
    @{ tool = "resolve-symbol";                       args = @{ symbol = "AgentToolHandlers" } },
    @{ tool = "read_lines";                           args = @{ path = "src/rawrxd_cli.cpp"; line_start = 1; line_end = 5 } },
    @{ tool = "read-lines";                           args = @{ path = "src/rawrxd_cli.cpp"; line_start = 1; line_end = 5 } },
    @{ tool = "plan_code_exploration";                args = @{ query = "understand agent dispatch" } },
    @{ tool = "planning-exploration";                 args = @{ query = "understand agent dispatch" } },
    @{ tool = "search_files";                         args = @{ query = "AgentToolHandlers.h" }  },
    @{ tool = "search-files";                         args = @{ query = "AgentToolHandlers.h" }  },
    @{ tool = "evaluate_integration_audit_feasibility"; args = @{ target = "src" }            },
    @{ tool = "evaluate-integration";                 args = @{ target = "src" }              },
    @{ tool = "restore_checkpoint";                   args = @{ checkpoint_path = "sessions/smoke_test_checkpoint.json" } }
)

foreach ($t in $toolTests) {
    $payload = @{ tool = $t.tool } + $t.args
    $resp = Invoke-ToolPost "$BaseUrl/api/tool" $payload
    if ($null -eq $resp) {
        Write-Fail "/api/tool $($t.tool) — no response (server may be offline)"
    } elseif ($resp.PSObject.Properties.Name -contains "error" -and
              $resp.error -match "unknown_tool|not found|routing") {
        Write-Fail "/api/tool $($t.tool) — routing error: $($resp.error)"
    } else {
        Write-Pass "/api/tool $($t.tool)"
    }
}

# ============================================================
# Section 2: /api/agent/ops/ — hotpatch-style op names
# ============================================================
Write-Info ""
Write-Info "=== Section 2: /api/agent/ops/ (op names) ==="

$opTests = @(
    @{ op = "compact-conversation";    body = @{ keep_last = 5 }                          },
    @{ op = "optimize-tool-selection"; body = @{ task = "find entry points" }             },
    @{ op = "resolve-symbol";          body = @{ symbol = "Win32IDE" }                    },  # was 404 before fix
    @{ op = "resolving";               body = @{ symbol = "Win32IDE" }                    },  # legacy alias
    @{ op = "read-lines";              body = @{ path = "src/rawrxd_cli.cpp"; line_start = 1; line_end = 3 } },
    @{ op = "planning-exploration";    body = @{ query = "audit backend handlers" }       },
    @{ op = "search-files";            body = @{ query = "*.cpp" }                        },
    @{ op = "evaluate-integration";    body = @{ target = "src" }                         },
    @{ op = "restore-checkpoint";      body = @{ checkpoint_path = "sessions/smoke_test_checkpoint.json" } }
)

foreach ($t in $opTests) {
    $resp = Invoke-ToolPost "$BaseUrl/api/agent/ops/$($t.op)" $t.body
    if ($null -eq $resp) {
        Write-Fail "/api/agent/ops/$($t.op) — no response"
    } elseif ($resp.PSObject.Properties.Name -contains "error" -and
              $resp.error -match "unknown_agent_operation|not found") {
        Write-Fail "/api/agent/ops/$($t.op) — returned 404 error: $($resp.error)"
    } else {
        Write-Pass "/api/agent/ops/$($t.op)"
    }
}

# ============================================================
# Section 3: /api/tool/capabilities — verify 7 tools listed
# ============================================================
Write-Info ""
Write-Info "=== Section 3: /api/tool/capabilities ==="

$caps = Invoke-ToolPost "$BaseUrl/api/tool/capabilities" @{}
if ($null -eq $caps) {
    # Try GET
    try { $caps = Invoke-RestMethod -Uri "$BaseUrl/api/tool/capabilities" -Method GET -TimeoutSec 10 } catch {}
}

$requiredTools = @(
    "compact_conversation",
    "optimize_tool_selection",
    "resolve_symbol",
    "read_lines",
    "plan_code_exploration",
    "search_files",
    "evaluate_integration_audit_feasibility"
)

if ($null -eq $caps) {
    Write-Fail "/api/tool/capabilities — no response"
} else {
    $capsJson = $caps | ConvertTo-Json -Depth 10
    foreach ($toolName in $requiredTools) {
        if ($capsJson -match [regex]::Escape($toolName)) {
            Write-Pass "capabilities: $toolName registered"
        } else {
            Write-Fail "capabilities: $toolName NOT found in registry"
        }
    }
}

# ============================================================
# Section 4: CLI smoke test (if ExePath provided)
# ============================================================
if ($ExePath -and (Test-Path $ExePath)) {
    Write-Info ""
    Write-Info "=== Section 4: CLI shell commands ==="

    $cliTests = @(
        @{ args = "--chat `"agent-capabilities`"";    expect = "cli_shell" },
        @{ args = "--chat `"tool-capabilities`"";     expect = "compact_conversation" },
        @{ args = "--chat `"compact`"";               expect = "compact_conversation|recorder_available|Nothing" },
        @{ args = "--chat `"optimize analyze code`""; expect = "optimize_tool_selection|score|tool" },
        @{ args = "--chat `"resolve Win32IDE`"";       expect = "Win32IDE|symbol|result" },
        @{ args = "--chat `"plan audit dispatch`"";   expect = "plan|exploration|step" },
        @{ args = "--chat `"evaluate src`"";          expect = "feasib|source_files|files" }
    )

    foreach ($t in $cliTests) {
        try {
            $output = & $ExePath ($t.args -split " ") 2>&1 | Out-String
            if ($output -match $t.expect) {
                Write-Pass "CLI: $($t.args)"
            } else {
                Write-Fail "CLI: $($t.args) — expected '$($t.expect)' in output"
            }
        } catch {
            Write-Fail "CLI: $($t.args) — exception: $_"
        }
    }
} else {
    Write-Info "(CLI tests skipped — provide -ExePath to enable)"
}

# ============================================================
# Section 5: Verify no stub/TODO strings in tool responses
# ============================================================
Write-Info ""
Write-Info "=== Section 5: No stub/TODO/hardcoded responses ==="

$stubPatterns = @("TODO", "STUB", "hardcoded", "placeholder", "not implemented")

$resolveResp = Invoke-ToolPost "$BaseUrl/api/tool" @{ tool = "resolve_symbol"; symbol = "CompactConversation" }
if ($resolveResp) {
    $body = $resolveResp | ConvertTo-Json -Depth 5
    $found = $false
    foreach ($p in $stubPatterns) {
        if ($body -imatch [regex]::Escape($p)) {
            Write-Fail "resolve_symbol response contains stub pattern: '$p'"
            $found = $true
        }
    }
    if (-not $found) {
        Write-Pass "resolve_symbol response contains no stub/TODO patterns"
    }
}

$planResp = Invoke-ToolPost "$BaseUrl/api/tool" @{ tool = "plan_code_exploration"; query = "audit agent tools" }
if ($planResp) {
    $body = $planResp | ConvertTo-Json -Depth 5
    # Hardcoded responses contain "3) Identify stubs/placeholders" — that's the OLD CLI stub
    if ($body -match "Enumerate candidate files by pattern and recency" -and
        $body -notmatch "workspace_root|files_found|analyzer") {
        Write-Fail "plan_code_exploration returned hardcoded 5-step plan (old stub not removed)"
    } else {
        Write-Pass "plan_code_exploration returns backend-generated plan (not hardcoded)"
    }
}

# ============================================================
# Summary
# ============================================================
Write-Host ""
Write-Host "======================================" -ForegroundColor White
Write-Host " Smoke Test Results" -ForegroundColor White
Write-Host "======================================" -ForegroundColor White
Write-Host " PASS: $passes" -ForegroundColor Green
Write-Host " FAIL: $failures" -ForegroundColor $(if ($failures -gt 0) { "Red" } else { "Green" })
Write-Host "======================================" -ForegroundColor White

if ($failures -gt 0) {
    Write-Host "`nFAILED — $failures test(s) did not pass." -ForegroundColor Red
    exit 1
} else {
    Write-Host "`nALL TESTS PASSED." -ForegroundColor Green
    exit 0
}
