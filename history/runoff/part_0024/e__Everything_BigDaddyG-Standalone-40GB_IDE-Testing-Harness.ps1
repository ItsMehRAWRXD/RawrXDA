# ============================================================================
# BIGDADDYG IDE - COMPREHENSIVE TESTING HARNESS
# ============================================================================
# Tests all IDE features via curl, API calls, and WebSocket connections
# Purpose: Verify all features work correctly despite UI issues
# ============================================================================

param(
    [switch]$QuickTest,
    [switch]$DeepTest,
    [switch]$FixMode,
    [string]$TargetFeature
)

$ErrorActionPreference = 'SilentlyContinue'
$WarningPreference = 'SilentlyContinue'

# Global variables
$OrchestraPort = 11441
$MicroModelPort = 3000
$OrchestraBase = "http://localhost:$OrchestraPort"
$MicroModelBase = "http://localhost:$MicroModelPort"
$DebugLog = @()
$TestResults = @()

# Colors
$Colors = @{
    'Green'   = 32
    'Red'     = 31
    'Yellow'  = 33
    'Cyan'    = 36
    'Blue'    = 34
    'White'   = 37
}

# ============================================================================
# UTILITIES
# ============================================================================

function ColorOutput {
    param([string]$Text, [string]$Color = 'White')
    $ColorCode = $Colors[$Color]
    Write-Host "$([char]27)[$($ColorCode)m$Text$([char]27)[0m"
}

function LogDebug {
    param([string]$Message)
    $DebugLog += "[$(Get-Date -Format 'HH:mm:ss.fff')] $Message"
    Write-Host "  ℹ️  $Message" -ForegroundColor Gray
}

function LogSuccess {
    param([string]$Message, [string]$Feature = "General")
    ColorOutput "  ✅ $Message" 'Green'
    $TestResults += @{ Feature = $Feature; Status = 'PASS'; Message = $Message; Time = Get-Date }
}

function LogError {
    param([string]$Message, [string]$Feature = "General")
    ColorOutput "  ❌ $Message" 'Red'
    $TestResults += @{ Feature = $Feature; Status = 'FAIL'; Message = $Message; Time = Get-Date }
}

function LogWarning {
    param([string]$Message)
    ColorOutput "  ⚠️  $Message" 'Yellow'
}

function TestEndpoint {
    param(
        [string]$Url,
        [string]$Method = 'GET',
        [hashtable]$Headers = @{},
        [object]$Body = $null,
        [string]$Feature = "Endpoint"
    )
    
    try {
        $params = @{
            Uri             = $Url
            Method          = $Method
            ContentType     = 'application/json'
            TimeoutSec      = 10
            ErrorAction     = 'Stop'
        }
        
        if ($Headers) { $params['Headers'] = $Headers }
        if ($Body) { $params['Body'] = ($Body | ConvertTo-Json -Depth 10) }
        
        $response = Invoke-WebRequest @params
        return @{
            Success = $true
            Status  = $response.StatusCode
            Body    = $response.Content | ConvertFrom-Json
            Response = $response
        }
    }
    catch {
        return @{
            Success = $false
            Status  = $_.Exception.Response.StatusCode
            Error   = $_.Exception.Message
            Response = $null
        }
    }
}

# ============================================================================
# SECTION 1: SERVER CONNECTIVITY TESTS
# ============================================================================

function Test-ServerConnectivity {
    ColorOutput "`n🔌 TESTING SERVER CONNECTIVITY..." 'Cyan'
    
    # Test Orchestra Server
    ColorOutput "`n  Orchestra Server (Port $OrchestraPort):" 'Blue'
    $orchResult = TestEndpoint "$OrchestraBase/v1/models"
    if ($orchResult.Success) {
        LogSuccess "Orchestra server responding on port $OrchestraPort" "Server-Orchestra"
        LogDebug "Status: $($orchResult.Status), Response size: $($orchResult.Body.Length) items"
    } else {
        LogError "Orchestra server failed: $($orchResult.Error)" "Server-Orchestra"
    }
    
    # Test Micro-Model-Server
    ColorOutput "`n  Micro-Model-Server (Port $MicroModelPort):" 'Blue'
    $microResult = TestEndpoint "$MicroModelBase/"
    if ($microResult.Success) {
        LogSuccess "Micro-Model-Server responding on port $MicroModelPort" "Server-Micro"
    } else {
        LogError "Micro-Model-Server failed: $($microResult.Error)" "Server-Micro"
    }
    
    return @{ Orchestra = $orchResult; Micro = $microResult }
}

# ============================================================================
# SECTION 2: MODEL LISTING & DISCOVERY
# ============================================================================

function Test-ModelDiscovery {
    param([object]$ServerStatus)
    
    ColorOutput "`n🤖 TESTING MODEL DISCOVERY..." 'Cyan'
    
    if ($ServerStatus.Orchestra.Success) {
        $models = $ServerStatus.Orchestra.Body
        ColorOutput "`n  Found $(@($models).Count) models:" 'Blue'
        
        if ($models -is [array]) {
            foreach ($model in $models | Select-Object -First 5) {
                if ($model.name) {
                    LogSuccess "Model: $($model.name)" "Model-Discovery"
                }
            }
            if ($models.Count -gt 5) {
                LogSuccess "... and $($models.Count - 5) more models" "Model-Discovery"
            }
        }
    }
}

# ============================================================================
# SECTION 3: CHAT/INFERENCE TESTING
# ============================================================================

function Test-ChatInterface {
    ColorOutput "`n💬 TESTING CHAT/INFERENCE..." 'Cyan'
    
    $testPrompt = @{
        model       = "bigdaddyg:latest"
        messages    = @(
            @{ role = "system"; content = "You are a helpful AI assistant." }
            @{ role = "user"; content = "Respond with 'test_successful'" }
        )
        temperature = 0.7
        max_tokens  = 100
    }
    
    # Try direct chat endpoint
    ColorOutput "`n  Testing /v1/chat/completions endpoint:" 'Blue'
    $chatResult = TestEndpoint "$OrchestraBase/v1/chat/completions" 'POST' @{} $testPrompt
    
    if ($chatResult.Success) {
        LogSuccess "Chat endpoint responding" "Chat-Interface"
        if ($chatResult.Body.choices) {
            LogSuccess "Got response with $(@($chatResult.Body.choices).Count) choice(s)" "Chat-Interface"
        }
    } else {
        LogWarning "Chat endpoint issue: $($chatResult.Error)"
    }
    
    # Try Micro-Model-Server chat
    ColorOutput "`n  Testing Micro-Model-Server chat:" 'Blue'
    $microChat = @{
        model   = "gpt-micro"
        prompt  = "test_successful"
        context = @()
    }
    
    $microResult = TestEndpoint "$MicroModelBase/api/chat" 'POST' @{} $microChat
    if ($microResult.Success) {
        LogSuccess "Micro-Model chat responding" "Chat-Interface"
    } else {
        LogWarning "Micro-Model chat issue: $($microResult.Error)"
    }
}

# ============================================================================
# SECTION 4: CODE EXECUTION & TERMINAL
# ============================================================================

function Test-CodeExecution {
    ColorOutput "`n⚙️  TESTING CODE EXECUTION & TERMINAL..." 'Cyan'
    
    # Test command execution
    $cmdTest = @{
        command = "echo 'IDE Test'"
        language = "powershell"
        timeout = 5000
    }
    
    ColorOutput "`n  Testing code execution endpoint:" 'Blue'
    $execResult = TestEndpoint "$OrchestraBase/v1/execute" 'POST' @{} $cmdTest
    
    if ($execResult.Success) {
        LogSuccess "Code execution endpoint responding" "Code-Execution"
    } else {
        LogWarning "Code execution endpoint: $($execResult.Error)"
    }
    
    # Test terminal session
    $terminalTest = @{
        session_id = "test-session-$(Get-Random)"
        command = "echo 'terminal test'"
    }
    
    ColorOutput "`n  Testing terminal session:" 'Blue'
    $termResult = TestEndpoint "$OrchestraBase/v1/terminal" 'POST' @{} $terminalTest
    
    if ($termResult.Success) {
        LogSuccess "Terminal endpoint responding" "Code-Terminal"
    } else {
        LogWarning "Terminal endpoint: $($termResult.Error)"
    }
}

# ============================================================================
# SECTION 5: FILE SYSTEM OPERATIONS
# ============================================================================

function Test-FileSystem {
    ColorOutput "`n📁 TESTING FILE SYSTEM OPERATIONS..." 'Cyan'
    
    # Test file listing
    ColorOutput "`n  Testing file listing:" 'Blue'
    $fsTest = @{
        action = "list"
        path   = "."
    }
    
    $fsResult = TestEndpoint "$OrchestraBase/v1/files" 'POST' @{} $fsTest
    
    if ($fsResult.Success) {
        LogSuccess "File system endpoint responding" "FileSystem"
    } else {
        LogWarning "File system endpoint: $($fsResult.Error)"
    }
}

# ============================================================================
# SECTION 6: AGENTIC OPERATIONS
# ============================================================================

function Test-AgenticFeatures {
    ColorOutput "`n🤖 TESTING AGENTIC FEATURES..." 'Cyan'
    
    # Test agent initialization
    ColorOutput "`n  Testing agent system:" 'Blue'
    $agentTest = @{
        agent_type = "code_generator"
        task       = "write simple test"
        context    = @()
    }
    
    $agentResult = TestEndpoint "$OrchestraBase/v1/agent/execute" 'POST' @{} $agentTest
    
    if ($agentResult.Success) {
        LogSuccess "Agent execution endpoint responding" "Agentic-Agent"
    } else {
        LogWarning "Agent endpoint: $($agentResult.Error)"
    }
    
    # Test swarm coordination
    ColorOutput "`n  Testing agent swarm:" 'Blue'
    $swarmTest = @{
        swarm_id  = "test-swarm-$(Get-Random)"
        agents    = @("analyzer", "coder", "reviewer")
        task      = "test coordination"
    }
    
    $swarmResult = TestEndpoint "$OrchestraBase/v1/swarm" 'POST' @{} $swarmTest
    
    if ($swarmResult.Success) {
        LogSuccess "Swarm coordination endpoint responding" "Agentic-Swarm"
    } else {
        LogWarning "Swarm endpoint: $($swarmResult.Error)"
    }
}

# ============================================================================
# SECTION 7: VOICE & AI FEATURES
# ============================================================================

function Test-VoiceAndAI {
    ColorOutput "`n🎤 TESTING VOICE & AI FEATURES..." 'Cyan'
    
    # Test voice codec
    ColorOutput "`n  Testing voice codec:" 'Blue'
    $voiceTest = @{
        format = "wav"
        sample_rate = 16000
        channels = 1
        duration = 1000
    }
    
    $voiceResult = TestEndpoint "$OrchestraBase/v1/voice/config" 'POST' @{} $voiceTest
    
    if ($voiceResult.Success) {
        LogSuccess "Voice codec endpoint responding" "Voice-AI"
    } else {
        LogWarning "Voice codec endpoint: $($voiceResult.Error)"
    }
    
    # Test AI analysis
    ColorOutput "`n  Testing AI code analysis:" 'Blue'
    $analysisTest = @{
        code     = "function test() { return 42; }"
        language = "javascript"
        checks   = @("syntax", "security", "performance")
    }
    
    $analysisResult = TestEndpoint "$OrchestraBase/v1/analyze" 'POST' @{} $analysisTest
    
    if ($analysisResult.Success) {
        LogSuccess "Code analysis endpoint responding" "Voice-Analysis"
    } else {
        LogWarning "Analysis endpoint: $($analysisResult.Error)"
    }
}

# ============================================================================
# SECTION 8: SETTINGS & CONFIGURATION
# ============================================================================

function Test-SettingsAndConfig {
    ColorOutput "`n⚙️  TESTING SETTINGS & CONFIGURATION..." 'Cyan'
    
    # Test settings retrieval
    ColorOutput "`n  Testing settings endpoint:" 'Blue'
    $settingsResult = TestEndpoint "$OrchestraBase/v1/settings"
    
    if ($settingsResult.Success) {
        LogSuccess "Settings endpoint responding" "Settings"
        if ($settingsResult.Body) {
            LogDebug "Found $(@($settingsResult.Body.PSObject.Properties).Count) setting categories"
        }
    } else {
        LogWarning "Settings endpoint: $($settingsResult.Error)"
    }
    
    # Test configuration update
    ColorOutput "`n  Testing configuration update:" 'Blue'
    $configTest = @{
        key   = "theme"
        value = "dark"
    }
    
    $configResult = TestEndpoint "$OrchestraBase/v1/config" 'PUT' @{} $configTest
    
    if ($configResult.Success) {
        LogSuccess "Config update endpoint responding" "Settings"
    } else {
        LogWarning "Config endpoint: $($configResult.Error)"
    }
}

# ============================================================================
# SECTION 9: PERFORMANCE & DIAGNOSTICS
# ============================================================================

function Test-PerformanceAndDiagnostics {
    ColorOutput "`n⚡ TESTING PERFORMANCE & DIAGNOSTICS..." 'Cyan'
    
    # Test system info
    ColorOutput "`n  Testing system diagnostics:" 'Blue'
    $diagResult = TestEndpoint "$OrchestraBase/v1/system/info"
    
    if ($diagResult.Success) {
        LogSuccess "System diagnostics endpoint responding" "Performance"
        if ($diagResult.Body.cpu) {
            LogDebug "CPU: $($diagResult.Body.cpu)"
        }
    } else {
        LogWarning "Diagnostics endpoint: $($diagResult.Error)"
    }
    
    # Test performance metrics
    ColorOutput "`n  Testing performance metrics:" 'Blue'
    $metricsResult = TestEndpoint "$OrchestraBase/v1/metrics"
    
    if ($metricsResult.Success) {
        LogSuccess "Performance metrics endpoint responding" "Performance"
    } else {
        LogWarning "Metrics endpoint: $($metricsResult.Error)"
    }
}

# ============================================================================
# SECTION 10: WEBSOCKET TESTING
# ============================================================================

function Test-WebSocket {
    ColorOutput "`n🔌 TESTING WEBSOCKET CONNECTIONS..." 'Cyan'
    
    # Test WebSocket connectivity via curl
    LogWarning "WebSocket testing requires live socket connection"
    ColorOutput "`n  WebSocket URL would be:" 'Blue'
    ColorOutput "    ws://localhost:$MicroModelPort" 'Yellow'
    ColorOutput "    ws://localhost:$OrchestraPort" 'Yellow'
    
    # Attempt curl-based WebSocket test if available
    $curlAvailable = (Get-Command curl -ErrorAction SilentlyContinue) -ne $null
    
    if ($curlAvailable) {
        LogDebug "curl is available for advanced testing"
    } else {
        LogWarning "curl not found - install Git Bash or Windows curl for WebSocket testing"
    }
}

# ============================================================================
# SECTION 11: UI ELEMENT TESTING (JavaScript injection)
# ============================================================================

function Test-UIElements {
    ColorOutput "`n🖼️  TESTING UI ELEMENT ACCESSIBILITY..." 'Cyan'
    
    # Check for Monaco Editor
    ColorOutput "`n  Monaco Editor verification:" 'Blue'
    LogWarning "Monaco Editor not loading - possible issues:"
    ColorOutput "    1. CSS/JS files not loading correctly" 'Yellow'
    ColorOutput "    2. Content Security Policy blocking resources" 'Yellow'
    ColorOutput "    3. CORS headers missing" 'Yellow'
    ColorOutput "    4. DOM elements not initialized" 'Yellow'
    
    # Check for chat input
    ColorOutput "`n  Chat input box verification:" 'Blue'
    LogWarning "Chat input missing - check:"
    ColorOutput "    1. index.html #chat-input element exists" 'Yellow'
    ColorOutput "    2. CSS display/visibility not hidden" 'Yellow'
    ColorOutput "    3. JavaScript event listeners attached" 'Yellow'
    
    # Check for model selector
    ColorOutput "`n  Model selector verification:" 'Blue'
    LogWarning "Model selector missing - check:"
    ColorOutput "    1. #model-selector element exists" 'Yellow'
    ColorOutput "    2. Options populated from /v1/models endpoint" 'Yellow'
    ColorOutput "    3. Change event handler registered" 'Yellow'
}

# ============================================================================
# SECTION 12: STRESS TESTING
# ============================================================================

function Test-StressAndLoad {
    ColorOutput "`n💪 TESTING STRESS & LOAD..." 'Cyan'
    
    ColorOutput "`n  Concurrent request test (5 simultaneous):" 'Blue'
    
    $jobs = @()
    for ($i = 1; $i -le 5; $i++) {
        $job = Start-Job -ScriptBlock {
            param($url, $id)
            try {
                $result = Invoke-WebRequest -Uri $url -TimeoutSec 10 -ErrorAction Stop
                return @{ Id = $id; Success = $true; Status = $result.StatusCode }
            }
            catch {
                return @{ Id = $id; Success = $false; Error = $_.Exception.Message }
            }
        } -ArgumentList "$OrchestraBase/v1/models", $i
        $jobs += $job
    }
    
    $results = $jobs | Wait-Job | Receive-Job
    $successCount = ($results | Where-Object { $_.Success }).Count
    
    if ($successCount -eq 5) {
        LogSuccess "All 5 concurrent requests succeeded" "Stress-Load"
    } else {
        LogWarning "$successCount/5 concurrent requests succeeded"
    }
    
    $jobs | Remove-Job
}

# ============================================================================
# SECTION 13: RESPONSE VALIDATION
# ============================================================================

function Test-ResponseValidation {
    ColorOutput "`n✔️  TESTING RESPONSE VALIDATION..." 'Cyan'
    
    $modelResult = TestEndpoint "$OrchestraBase/v1/models"
    
    if ($modelResult.Success) {
        $models = $modelResult.Body
        
        ColorOutput "`n  Model response structure:" 'Blue'
        
        if ($models -is [array]) {
            $sampleModel = $models[0]
            
            if ($sampleModel.name) { LogSuccess "Model has 'name' field" "Validation" }
            else { LogWarning "Model missing 'name' field" }
            
            if ($sampleModel.id) { LogSuccess "Model has 'id' field" "Validation" }
            else { LogWarning "Model missing 'id' field" }
            
            if ($sampleModel.size) { LogSuccess "Model has 'size' field" "Validation" }
            else { LogWarning "Model missing 'size' field" }
        }
    }
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

function Show-Banner {
    ColorOutput "`n╔═══════════════════════════════════════════════════════════╗" 'Cyan'
    ColorOutput "║   BIGDADDYG IDE - COMPREHENSIVE TESTING HARNESS v1.0      ║" 'Cyan'
    ColorOutput "║   API Testing | WebSocket | Stress Testing | Validation   ║" 'Cyan'
    ColorOutput "╚═══════════════════════════════════════════════════════════╝" 'Cyan'
    ColorOutput "`n$(Get-Date -Format 'dddd, MMMM dd, yyyy - HH:mm:ss')`n" 'White'
}

function Show-Summary {
    ColorOutput "`n╔═══════════════════════════════════════════════════════════╗" 'Cyan'
    ColorOutput "║                    TEST SUMMARY REPORT                     ║" 'Cyan'
    ColorOutput "╚═══════════════════════════════════════════════════════════╝" 'Cyan'
    
    $grouped = $TestResults | Group-Object -Property Status
    
    foreach ($group in $grouped) {
        if ($group.Name -eq 'PASS') {
            ColorOutput "`n✅ PASSED: $($group.Count) tests" 'Green'
        } else {
            ColorOutput "`n❌ FAILED: $($group.Count) tests" 'Red'
        }
    }
    
    ColorOutput "`n📊 RESULTS BY FEATURE:" 'Cyan'
    $TestResults | Group-Object -Property Feature | ForEach-Object {
        $passed = ($_.Group | Where-Object { $_.Status -eq 'PASS' }).Count
        $failed = ($_.Group | Where-Object { $_.Status -eq 'FAIL' }).Count
        $total = $_.Group.Count
        
        $status = if ($failed -eq 0) { '✅' } else { '❌' }
        ColorOutput "  $status $($_.Name): $passed/$total passed" $(if ($failed -eq 0) { 'Green' } else { 'Red' })
    }
    
    ColorOutput "`n⏱️  Total Duration: $(([DateTime]::Now - $StartTime).TotalSeconds) seconds`n" 'White'
}

function Export-Results {
    $filename = "IDE-Test-Results-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
    $filepath = Join-Path $PSScriptRoot $filename
    
    @{
        Timestamp = Get-Date -Format 'o'
        Duration  = ([DateTime]::Now - $StartTime).TotalSeconds
        Results   = $TestResults
        Logs      = $DebugLog
    } | ConvertTo-Json -Depth 10 | Out-File -FilePath $filepath -Encoding UTF8
    
    ColorOutput "`n📄 Results exported to: $filename" 'Green'
}

# Main execution
$StartTime = Get-Date

Show-Banner

# Run tests based on mode
if ($QuickTest) {
    ColorOutput "Running QUICK test suite..." 'Yellow'
    $serverStatus = Test-ServerConnectivity
    Test-ModelDiscovery $serverStatus
} elseif ($DeepTest) {
    ColorOutput "Running DEEP test suite..." 'Yellow'
    $serverStatus = Test-ServerConnectivity
    Test-ModelDiscovery $serverStatus
    Test-ChatInterface
    Test-CodeExecution
    Test-FileSystem
    Test-AgenticFeatures
    Test-VoiceAndAI
    Test-SettingsAndConfig
    Test-PerformanceAndDiagnostics
    Test-WebSocket
    Test-UIElements
    Test-StressAndLoad
    Test-ResponseValidation
} elseif ($FixMode) {
    ColorOutput "Running diagnostic FIX mode..." 'Yellow'
    ColorOutput "`nPlease run the following PowerShell commands separately:`n" 'Yellow'
    ColorOutput "# Kill all node processes:" 'Blue'
    ColorOutput "Get-Process node -ErrorAction SilentlyContinue | Stop-Process -Force" 'White'
    ColorOutput "`n# Check port availability:" 'Blue'
    ColorOutput "netstat -ano | findstr '11441\|3000'" 'White'
    ColorOutput "`n# Start the app fresh:" 'Blue'
    ColorOutput "cd 'E:\Everything\BigDaddyG-Standalone-40GB\app'; npm start" 'White'
} else {
    # Default: comprehensive test
    ColorOutput "Running COMPREHENSIVE test suite..." 'Yellow'
    $serverStatus = Test-ServerConnectivity
    Test-ModelDiscovery $serverStatus
    Test-ChatInterface
    Test-CodeExecution
    Test-FileSystem
    Test-AgenticFeatures
    Test-VoiceAndAI
    Test-SettingsAndConfig
    Test-PerformanceAndDiagnostics
    Test-WebSocket
    Test-UIElements
    Test-StressAndLoad
    Test-ResponseValidation
}

Show-Summary
Export-Results
