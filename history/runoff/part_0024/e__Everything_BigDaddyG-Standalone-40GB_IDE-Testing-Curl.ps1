#!/usr/bin/env pwsh
<#
    BIGDADDYG IDE - CURL-BASED TEST SUITE
    Comprehensive API testing without PowerShell dependencies
#>

param(
    [switch]$Quick,
    [switch]$Full,
    [switch]$ModelsOnly,
    [switch]$ChatOnly,
    [switch]$WebSocketOnly,
    [switch]$StressTest,
    [string]$CustomEndpoint = $null
)

# Configuration
$OrchestraUrl = "http://localhost:11441"
$MicroUrl = "http://localhost:3000"
$Results = @()
$PassCount = 0
$FailCount = 0

# ============================================================================
# TEST UTILITIES
# ============================================================================

function Invoke-CurlTest {
    param(
        [string]$Method,
        [string]$Url,
        [object]$Body = $null,
        [string]$TestName = "API Test"
    )
    
    $curlArgs = @()
    
    # Build curl command
    if ($Method) { $curlArgs += "-X", $Method }
    $curlArgs += "-s", "-w", "`n%{http_code}"
    
    if ($Body) {
        $jsonBody = $Body | ConvertTo-Json -Compress
        $curlArgs += "-d", $jsonBody
        $curlArgs += "-H", "Content-Type: application/json"
    }
    
    $curlArgs += "-H", "Accept: application/json"
    $curlArgs += "-m", "10"  # 10 second timeout
    $curlArgs += $Url
    
    try {
        $output = curl @curlArgs 2>&1
        
        if ($LASTEXITCODE -eq 0 -and $output) {
            $lines = $output -split "`n"
            $responseBody = $lines[0..($lines.Count-2)] -join "`n"
            $httpCode = $lines[-1]
            
            if ($httpCode -match "^[2-3]\d{2}$") {
                Write-Host "✅ $TestName ($httpCode)" -ForegroundColor Green
                $PassCount++
                return @{ Success = $true; Code = $httpCode; Body = $responseBody }
            } else {
                Write-Host "❌ $TestName ($httpCode)" -ForegroundColor Red
                $FailCount++
                return @{ Success = $false; Code = $httpCode; Body = $responseBody }
            }
        } else {
            Write-Host "❌ $TestName (Connection failed)" -ForegroundColor Red
            $FailCount++
            return @{ Success = $false; Error = "Connection failed" }
        }
    }
    catch {
        Write-Host "❌ $TestName (Error: $_)" -ForegroundColor Red
        $FailCount++
        return @{ Success = $false; Error = $_.Exception.Message }
    }
}

# ============================================================================
# TEST SECTIONS
# ============================================================================

function Test-BasicConnectivity {
    Write-Host "`n=== SERVER CONNECTIVITY ===" -ForegroundColor Cyan
    
    Invoke-CurlTest -Method GET -Url "$OrchestraUrl/v1/models" -TestName "Orchestra Server"
    Invoke-CurlTest -Method GET -Url "$MicroUrl/" -TestName "Micro-Model-Server"
}

function Test-ModelListing {
    Write-Host "`n=== MODEL LISTING ===" -ForegroundColor Cyan
    
    $result = Invoke-CurlTest -Method GET -Url "$OrchestraUrl/v1/models" -TestName "Get Models"
    
    if ($result.Success -and $result.Body) {
        try {
            $models = $result.Body | ConvertFrom-Json
            Write-Host "  📊 Total models: $(@($models).Count)" -ForegroundColor Yellow
            
            if (@($models).Count -gt 0) {
                Write-Host "  Sample models:" -ForegroundColor Yellow
                @($models) | Select-Object -First 3 | ForEach-Object {
                    Write-Host "    • $($_.name)" -ForegroundColor Gray
                }
            }
        }
        catch {
            Write-Host "  ⚠️ Could not parse model response" -ForegroundColor Yellow
        }
    }
}

function Test-ChatEndpoint {
    Write-Host "`n=== CHAT/INFERENCE ===" -ForegroundColor Cyan
    
    $payload = @{
        model = "bigdaddyg:latest"
        messages = @(
            @{ role = "system"; content = "You are helpful" }
            @{ role = "user"; content = "Say 'test_ok'" }
        )
        temperature = 0.5
        max_tokens = 50
    }
    
    Invoke-CurlTest -Method POST -Url "$OrchestraUrl/v1/chat/completions" -Body $payload -TestName "Chat Completion"
}

function Test-MicroModelChat {
    Write-Host "`n=== MICRO-MODEL CHAT ===" -ForegroundColor Cyan
    
    $payload = @{
        model = "gpt-micro"
        prompt = "test"
        context = @()
    }
    
    Invoke-CurlTest -Method POST -Url "$MicroUrl/api/chat" -Body $payload -TestName "Micro Chat"
}

function Test-CodeExecution {
    Write-Host "`n=== CODE EXECUTION ===" -ForegroundColor Cyan
    
    $payload = @{
        command = "echo 'IDE Test'"
        language = "powershell"
        timeout = 5000
    }
    
    Invoke-CurlTest -Method POST -Url "$OrchestraUrl/v1/execute" -Body $payload -TestName "Execute Command"
}

function Test-FileOperations {
    Write-Host "`n=== FILE OPERATIONS ===" -ForegroundColor Cyan
    
    $payload = @{
        action = "list"
        path = "."
    }
    
    Invoke-CurlTest -Method POST -Url "$OrchestraUrl/v1/files" -Body $payload -TestName "List Files"
}

function Test-Settings {
    Write-Host "`n=== SETTINGS ===" -ForegroundColor Cyan
    
    Invoke-CurlTest -Method GET -Url "$OrchestraUrl/v1/settings" -TestName "Get Settings"
    
    $payload = @{
        key = "theme"
        value = "dark"
    }
    
    Invoke-CurlTest -Method PUT -Url "$OrchestraUrl/v1/config" -Body $payload -TestName "Update Config"
}

function Test-SystemInfo {
    Write-Host "`n=== SYSTEM INFO ===" -ForegroundColor Cyan
    
    Invoke-CurlTest -Method GET -Url "$OrchestraUrl/v1/system/info" -TestName "System Diagnostics"
    Invoke-CurlTest -Method GET -Url "$OrchestraUrl/v1/metrics" -TestName "Performance Metrics"
}

function Test-AgentSystems {
    Write-Host "`n=== AGENT SYSTEMS ===" -ForegroundColor Cyan
    
    $payload = @{
        agent_type = "code_generator"
        task = "test"
        context = @()
    }
    
    Invoke-CurlTest -Method POST -Url "$OrchestraUrl/v1/agent/execute" -Body $payload -TestName "Agent Execution"
    
    $swarmPayload = @{
        swarm_id = "test-$(Get-Random)"
        agents = @("analyzer", "coder")
        task = "test"
    }
    
    Invoke-CurlTest -Method POST -Url "$OrchestraUrl/v1/swarm" -Body $swarmPayload -TestName "Agent Swarm"
}

function Test-VoiceFeatures {
    Write-Host "`n=== VOICE FEATURES ===" -ForegroundColor Cyan
    
    $payload = @{
        format = "wav"
        sample_rate = 16000
        channels = 1
    }
    
    Invoke-CurlTest -Method POST -Url "$OrchestraUrl/v1/voice/config" -Body $payload -TestName "Voice Config"
}

function Test-CodeAnalysis {
    Write-Host "`n=== CODE ANALYSIS ===" -ForegroundColor Cyan
    
    $payload = @{
        code = "function test() { return 42; }"
        language = "javascript"
        checks = @("syntax", "security")
    }
    
    Invoke-CurlTest -Method POST -Url "$OrchestraUrl/v1/analyze" -Body $payload -TestName "Code Analysis"
}

function Test-StressLoad {
    Write-Host "`n=== STRESS TEST (10 concurrent requests) ===" -ForegroundColor Cyan
    
    $jobs = @()
    
    for ($i = 1; $i -le 10; $i++) {
        $job = Start-Job -ScriptBlock {
            param($url)
            curl -s -w "%{http_code}" "$url" 2>&1
        } -ArgumentList "$OrchestraUrl/v1/models"
        $jobs += $job
    }
    
    $results = @()
    $jobs | Wait-Job | ForEach-Object {
        $output = Receive-Job -Job $_
        $code = $output[-3..-1] -join ""
        if ($code -match "^[2-3]\d{2}$") {
            $results += $true
        }
    }
    
    $passed = ($results | Where-Object { $_ }).Count
    Write-Host "  📊 $passed/10 requests succeeded" -ForegroundColor $(if ($passed -eq 10) { 'Green' } else { 'Yellow' })
    
    $jobs | Remove-Job
}

function Test-WebSocketConnectivity {
    Write-Host "`n=== WEBSOCKET CONNECTIVITY ===" -ForegroundColor Cyan
    
    Write-Host "  ℹ️  WebSocket endpoints:" -ForegroundColor Cyan
    Write-Host "    • ws://localhost:11441" -ForegroundColor Gray
    Write-Host "    • ws://localhost:3000" -ForegroundColor Gray
    
    Write-Host "  💡 To test WebSocket, use:" -ForegroundColor Yellow
    Write-Host "    • wscat -c ws://localhost:11441" -ForegroundColor White
    Write-Host "    • Or use browser DevTools console" -ForegroundColor White
}

function Show-Menu {
    Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  BIGDADDYG IDE - CURL TEST HARNESS    ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\IDE-Testing-Curl.ps1 [option]" -ForegroundColor White
    Write-Host ""
    Write-Host "  -Quick           Quick connectivity test" -ForegroundColor Yellow
    Write-Host "  -ModelsOnly      Test model listing only" -ForegroundColor Yellow
    Write-Host "  -ChatOnly        Test chat endpoints only" -ForegroundColor Yellow
    Write-Host "  -Full            Run complete test suite" -ForegroundColor Yellow
    Write-Host "  -StressTest      Load testing (10 concurrent)" -ForegroundColor Yellow
    Write-Host "  -WebSocketOnly   WebSocket connection guide" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Cyan
    Write-Host "  .\IDE-Testing-Curl.ps1 -Quick" -ForegroundColor White
    Write-Host "  .\IDE-Testing-Curl.ps1 -Full" -ForegroundColor White
    Write-Host "  .\IDE-Testing-Curl.ps1 -StressTest" -ForegroundColor White
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

if ($Help -or (-not $Quick -and -not $Full -and -not $ModelsOnly -and -not $ChatOnly -and -not $WebSocketOnly -and -not $StressTest)) {
    Show-Menu
    exit
}

Write-Host "`n🧪 IDE TESTING HARNESS - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "────────────────────────────────────────────" -ForegroundColor Cyan

if ($Quick) {
    Test-BasicConnectivity
} elseif ($ModelsOnly) {
    Test-BasicConnectivity
    Test-ModelListing
} elseif ($ChatOnly) {
    Test-ChatEndpoint
    Test-MicroModelChat
} elseif ($WebSocketOnly) {
    Test-WebSocketConnectivity
} elseif ($StressTest) {
    Test-BasicConnectivity
    Test-StressLoad
} else {
    # Full test suite
    Test-BasicConnectivity
    Test-ModelListing
    Test-ChatEndpoint
    Test-MicroModelChat
    Test-CodeExecution
    Test-FileOperations
    Test-Settings
    Test-SystemInfo
    Test-AgentSystems
    Test-VoiceFeatures
    Test-CodeAnalysis
    Test-StressLoad
    Test-WebSocketConnectivity
}

Write-Host "`n════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "✅ Passed: $PassCount | ❌ Failed: $FailCount" -ForegroundColor $(if ($FailCount -eq 0) { 'Green' } else { 'Yellow' })
Write-Host "════════════════════════════════════════════`n" -ForegroundColor Cyan
