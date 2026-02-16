# ============================================================================
# SmokeTest-AgenticIDE.ps1 — Full agentic integration smoke test
# ============================================================================
# Verifies the agentic flow end-to-end: RawrEngine HTTP API (status, models,
# chat, agentic config) and optional Ollama routing. Use this after building
# RawrEngine and before or after running the Win32 IDE.
#
# Requirements: PowerShell 5.1+; RawrEngine running on $BaseUrl (default http://localhost:8080)
#   To start RawrEngine: run build_ide\RawrEngine.exe (or RawrEngine --port 8080)
#
# Optional: Ollama at http://localhost:11434 with at least one model for
#   POST /api/chat with "model" to route to Ollama.
#
# Usage:
#   .\SmokeTest-AgenticIDE.ps1
#   .\SmokeTest-AgenticIDE.ps1 -BaseUrl "http://localhost:9090"
#   .\SmokeTest-AgenticIDE.ps1 -SkipChat   # Only status + models + config
# ============================================================================

param(
    [string]$BaseUrl = "http://localhost:8080",
    [switch]$SkipChat,
    [int]$TimeoutSec = 30,
    [int]$WaitForPortSec = 0   # If > 0, wait up to this many seconds for server before failing GET /status
)

$ErrorActionPreference = "Stop"
$script:Passed = 0
$script:Failed = 0

function Write-Result { param([string]$Name, [bool]$Ok, [string]$Detail = "")
    $script:Passed += [int]$Ok
    if (-not $Ok) { $script:Failed += 1 }
    $tag = if ($Ok) { "PASS" } else { "FAIL" }
    $color = if ($Ok) { "Green" } else { "Red" }
    Write-Host "  [$tag] $Name" -ForegroundColor $color
    if ($Detail) { Write-Host "         $Detail" -ForegroundColor Gray }
}

$BaseUrl = $BaseUrl.TrimEnd('/')

# Optional: wait for server to be reachable (e.g. after Launch-And-Test.ps1 starts RawrEngine)
if ($WaitForPortSec -gt 0) {
    $deadline = [DateTime]::UtcNow.AddSeconds($WaitForPortSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        try {
            $null = Invoke-RestMethod -Uri "$BaseUrl/status" -Method GET -TimeoutSec 2 -ErrorAction Stop
            break
        } catch {
            Start-Sleep -Seconds 1
        }
    }
}

# ----------------------------------------------------------------------------
# 1) GET /status
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== 1) Server status ===" -ForegroundColor Cyan
try {
    $status = Invoke-RestMethod -Uri "$BaseUrl/status" -Method GET -TimeoutSec $TimeoutSec -ErrorAction Stop
    $ready = $status.ready -eq $true
    Write-Result "GET /status" $true "ready=$ready, backend=$($status.backend)"
} catch {
    Write-Result "GET /status" $false $_.Exception.Message
    Write-Host "`nStart RawrEngine first (e.g. build_ide\RawrEngine.exe or build\RawrEngine.exe)." -ForegroundColor Red
    Write-Host "In the RawrEngine window look for: [CompletionServer] Listening on port 8080..." -ForegroundColor Gray
    Write-Host "If the server never starts, build with: cmake --build build_ide --config Release --target RawrEngine" -ForegroundColor Gray
    exit 1
}

# ----------------------------------------------------------------------------
# 2) GET /v1/models (and /api/models)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== 2) Models list ===" -ForegroundColor Cyan
try {
    $models = Invoke-RestMethod -Uri "$BaseUrl/v1/models" -Method GET -TimeoutSec $TimeoutSec -ErrorAction Stop
    $count = 0
    if ($models.data) { $count = @($models.data).Count }
    Write-Result "GET /v1/models" $true "count=$count"
} catch {
    Write-Result "GET /v1/models" $false $_.Exception.Message
}
try {
    $apiModels = Invoke-RestMethod -Uri "$BaseUrl/api/models" -Method GET -TimeoutSec $TimeoutSec -ErrorAction Stop
    $count2 = 0
    if ($apiModels.data) { $count2 = @($apiModels.data).Count }
    Write-Result "GET /api/models" $true "count=$count2"
} catch {
    Write-Result "GET /api/models" $false $_.Exception.Message
}

# ----------------------------------------------------------------------------
# 3) GET /api/agentic/config
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== 3) Agentic config ===" -ForegroundColor Cyan
try {
    $cfg = Invoke-RestMethod -Uri "$BaseUrl/api/agentic/config" -Method GET -TimeoutSec $TimeoutSec -ErrorAction Stop
    $mode = $cfg.operationMode
    Write-Result "GET /api/agentic/config" $true "operationMode=$mode"
} catch {
    Write-Result "GET /api/agentic/config" $false $_.Exception.Message
}

# ----------------------------------------------------------------------------
# 4) POST /api/chat (agentic chat)
# ----------------------------------------------------------------------------
if (-not $SkipChat) {
    Write-Host ""
    Write-Host "=== 4) Agentic chat ===" -ForegroundColor Cyan
    $body = @{ message = "Reply with exactly: AGENTIC_SMOKE_OK" } | ConvertTo-Json
    try {
        $chat = Invoke-RestMethod -Uri "$BaseUrl/api/chat" -Method POST -Body $body -ContentType "application/json" -TimeoutSec $TimeoutSec -ErrorAction Stop
        $hasResponse = $chat.response -match "AGENTIC_SMOKE_OK" -or $null -ne $chat.response
        if (-not $hasResponse -and $chat.error) {
            Write-Result "POST /api/chat" $false $chat.error
        } else {
            Write-Result "POST /api/chat" $true "response received"
        }
    } catch {
        Write-Result "POST /api/chat" $false $_.Exception.Message
    }

    # Optional: POST /api/chat with model (Ollama routing)
    try {
        $bodyWithModel = @{ message = "Say OK"; model = "llama3.2" } | ConvertTo-Json
        $chatOllama = Invoke-RestMethod -Uri "$BaseUrl/api/chat" -Method POST -Body $bodyWithModel -ContentType "application/json" -TimeoutSec ([Math]::Min(60, $TimeoutSec * 2)) -ErrorAction Stop
        if ($chatOllama.error) {
            Write-Result "POST /api/chat (model=llama3.2)" $false $chatOllama.error
        } else {
            Write-Result "POST /api/chat (model=llama3.2)" $true "Ollama route OK"
        }
    } catch {
        Write-Result "POST /api/chat (model=llama3.2)" $false $_.Exception.Message
    }
}

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host "  Passed: $script:Passed" -ForegroundColor Green
if ($script:Failed -gt 0) {
    Write-Host "  Failed: $script:Failed" -ForegroundColor Red
    exit 1
}
Write-Host "  Agentic IDE integration smoke test passed." -ForegroundColor Green
exit 0
