# ============================================
# AGENTIC CAPABILITIES TEST - VIA CLI
# ============================================
# This test uses RawrXD's CLI commands to test agentic features
# The CLI should have identical features to the GUI

Write-Host @"
╔════════════════════════════════════════════════════════════════╗
║     🤖 RAWRXD AGENTIC CAPABILITIES - CLI TEST                 ║
╚════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Load RawrXD script to get all functions
Write-Host "`nLoading RawrXD functions..." -ForegroundColor Yellow

# Create a script that loads RawrXD and then runs tests
$testScript = @'
# Suppress GUI initialization
$script:WindowsFormsAvailable = $false

# Source RawrXD
. ".\RawrXD.ps1"

# Now test agentic functions
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Testing Agentic Functions" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

$testResults = @()
$testCount = 0
$passCount = 0

function Test-Function {
    param([string]$Name, [string]$Description)
    $script:testCount++
    Write-Host "`n[$script:testCount] Testing: $Name" -ForegroundColor Yellow
    Write-Host "   $Description" -ForegroundColor Gray

    if (Get-Command $Name -ErrorAction SilentlyContinue) {
        $script:passCount++
        Write-Host "   ✅ Function exists" -ForegroundColor Green
        return $true
    }
    else {
        Write-Host "   ❌ Function not found" -ForegroundColor Red
        return $false
    }
}

# Test Agent Tool Functions
Write-Host "`n[Agent Tools]" -ForegroundColor Cyan
Test-Function "Get-AgentToolsSchema" "Get schema of all registered agent tools"
Test-Function "Get-AgentToolsList" "Get formatted list of agent tools"
Test-Function "Invoke-AgentTool" "Execute an agent tool"
Test-Function "Register-AgentTool" "Register a new agent tool"

# Test Agent Command Functions
Write-Host "`n[Agent Commands]" -ForegroundColor Cyan
Test-Function "Process-AgentCommand" "Process agent commands"

# Test Agent Task Functions
Write-Host "`n[Agent Tasks]" -ForegroundColor Cyan
Test-Function "New-AgentTask" "Create new agent task"
Test-Function "Start-AgentTask" "Start agent task"
Test-Function "Start-AgentTaskAsync" "Start agent task asynchronously"
Test-Function "Update-AgentTasksList" "Update agent tasks list"

# Test Agent Workflow Functions
Write-Host "`n[Agent Workflows]" -ForegroundColor Cyan
Test-Function "Invoke-AgenticWorkflow" "Invoke agentic workflow"

# Test Agent Logging Functions
Write-Host "`n[Agent Logging]" -ForegroundColor Cyan
Test-Function "Write-AgentLog" "Write agent log entry"

# Test actual tool execution
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Testing Tool Execution" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
    Write-Host "`n[Tool Test] Testing read_file tool..." -ForegroundColor Yellow
    try {
        $result = Invoke-AgentTool -ToolName "read_file" -Parameters @{path = "RawrXD.ps1"}
        if ($result -and $result.success -ne $false) {
            Write-Host "   ✅ read_file tool works" -ForegroundColor Green
            $script:passCount++
        }
        else {
            Write-Host "   ⚠️ read_file returned: $($result | ConvertTo-Json -Compress)" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "   ❌ Error: $_" -ForegroundColor Red
    }

    Write-Host "`n[Tool Test] Testing list_directory tool..." -ForegroundColor Yellow
    try {
        $result = Invoke-AgentTool -ToolName "list_directory" -Parameters @{path = "."}
        if ($result -and $result.success -ne $false) {
            Write-Host "   ✅ list_directory tool works" -ForegroundColor Green
            $script:passCount++
        }
        else {
            Write-Host "   ⚠️ list_directory returned: $($result | ConvertTo-Json -Compress)" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "   ❌ Error: $_" -ForegroundColor Red
    }

    Write-Host "`n[Tool Test] Testing write_file tool..." -ForegroundColor Yellow
    try {
        $testContent = "Test file created by agentic system at $(Get-Date)"
        $testPath = "test-agentic-cli-file.txt"
        $result = Invoke-AgentTool -ToolName "write_file" -Parameters @{path = $testPath; content = $testContent}
        if ($result -and $result.success -ne $false) {
            if (Test-Path $testPath) {
                Remove-Item $testPath -Force -ErrorAction SilentlyContinue
                Write-Host "   ✅ write_file tool works" -ForegroundColor Green
                $script:passCount++
            }
            else {
                Write-Host "   ⚠️ write_file reported success but file not found" -ForegroundColor Yellow
            }
        }
        else {
            Write-Host "   ⚠️ write_file returned: $($result | ConvertTo-Json -Compress)" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "   ❌ Error: $_" -ForegroundColor Red
    }
}

# Check registered tools
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Registered Agent Tools" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

if ($script:agentTools) {
    $toolCount = $script:agentTools.Count
    Write-Host "`n✅ Found $toolCount registered agent tools:" -ForegroundColor Green
    $categories = @{}
    foreach ($toolName in $script:agentTools.Keys) {
        $tool = $script:agentTools[$toolName]
        $category = if ($tool.Category) { $tool.Category } else { "General" }
        if (-not $categories.ContainsKey($category)) {
            $categories[$category] = @()
        }
        $categories[$category] += $toolName
    }
    foreach ($category in $categories.Keys | Sort-Object) {
        Write-Host "`n  [$category]" -ForegroundColor Cyan
        foreach ($toolName in $categories[$category]) {
            $tool = $script:agentTools[$toolName]
            Write-Host "    • $toolName - $($tool.Description)" -ForegroundColor Gray
        }
    }
}
else {
    Write-Host "⚠️ No agent tools registered" -ForegroundColor Yellow
}

# Test agent task creation
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Testing Agent Task Creation" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

if (Get-Command New-AgentTask -ErrorAction SilentlyContinue) {
    Write-Host "`n[Task Test] Creating test agent task..." -ForegroundColor Yellow
    try {
        $taskId = New-AgentTask -Name "CLI Test Task" -Description "Testing agent task creation from CLI" -Steps @(
            @{Type = "tool"; Description = "Test read file"; Tool = "read_file"; Arguments = @{path = "RawrXD.ps1"}}
        )
        if ($taskId) {
            Write-Host "   ✅ Task created with ID: $taskId" -ForegroundColor Green
            $script:passCount++
        }
        else {
            Write-Host "   ❌ Task creation returned no ID" -ForegroundColor Red
        }
    }
    catch {
        Write-Host "   ❌ Error: $_" -ForegroundColor Red
    }
}

# Summary
Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    TEST SUMMARY                              ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

Write-Host "`nTotal Tests: $script:testCount" -ForegroundColor White
Write-Host "Passed: $script:passCount" -ForegroundColor Green
Write-Host "Failed: $($script:testCount - $script:passCount)" -ForegroundColor $(if (($script:testCount - $script:passCount) -gt 0) { "Red" } else { "Green" })
if ($script:testCount -gt 0) {
    $successRate = [math]::Round(($script:passCount / $script:testCount) * 100, 2)
    Write-Host "Success Rate: $successRate%" -ForegroundColor $(if ($script:passCount -eq $script:testCount) { "Green" } else { "Yellow" })
}

Write-Host "`n✅ CLI-based agentic capabilities test completed!" -ForegroundColor Green
'@

# Execute the test script
$testScript | Out-File -FilePath "temp-agentic-test.ps1" -Encoding UTF8
try {
    powershell -ExecutionPolicy Bypass -File "temp-agentic-test.ps1" 2>&1
}
finally {
    if (Test-Path "temp-agentic-test.ps1") {
        Remove-Item "temp-agentic-test.ps1" -Force -ErrorAction SilentlyContinue
    }
}

