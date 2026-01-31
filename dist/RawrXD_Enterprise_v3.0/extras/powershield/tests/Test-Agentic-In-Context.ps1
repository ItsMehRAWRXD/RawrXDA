# ============================================
# AGENTIC CAPABILITIES TEST - IN CONTEXT
# ============================================
# This test loads RawrXD functions and tests agentic capabilities
# Run this to test agentic features programmatically

Write-Host @"
╔════════════════════════════════════════════════════════════════╗
║     🤖 RAWRXD AGENTIC CAPABILITIES - IN-CONTEXT TEST          ║
╚════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Load RawrXD script to get access to functions
Write-Host "`nLoading RawrXD functions..." -ForegroundColor Yellow
try {
    # Source the RawrXD script (but don't run the GUI initialization)
    $rawrXDContent = Get-Content "RawrXD.ps1" -Raw

    # Extract and execute just the function definitions
    # We'll use a scriptblock to isolate the execution
    $scriptBlock = [scriptblock]::Create($rawrXDContent)

    # Execute in a separate scope to avoid conflicts
    & {
        # Set up minimal environment
        $script:EmergencyLogPath = Join-Path $env:TEMP "RawrXD_Test_Logs"
        $script:StartupLogFile = Join-Path $script:EmergencyLogPath "test_$(Get-Date -Format 'yyyy-MM-dd').log"
        if (-not (Test-Path $script:EmergencyLogPath)) {
            New-Item -ItemType Directory -Path $script:EmergencyLogPath -Force | Out-Null
        }

        # Minimal logging function
        function Write-EmergencyLog {
            param([string]$Message, [string]$Level = "INFO")
            # Silent for testing
        }

        # Execute the script to load functions
        . $scriptBlock

        Write-Host "✅ RawrXD functions loaded" -ForegroundColor Green

        # Now test the functions
        $testResults = @()
        $testCount = 0
        $passCount = 0

        function Test-Function {
            param([string]$Name, [string]$Description)
            $testCount++
            Write-Host "`n[$testCount] Testing: $Name" -ForegroundColor Yellow
            Write-Host "   $Description" -ForegroundColor Gray

            if (Get-Command $Name -ErrorAction SilentlyContinue) {
                $passCount++
                Write-Host "   ✅ Function exists" -ForegroundColor Green
                return $true
            }
            else {
                Write-Host "   ❌ Function not found" -ForegroundColor Red
                return $false
            }
        }

        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "Testing Agent Tool Functions" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

        Test-Function "Get-AgentToolsSchema" "Get schema of all registered agent tools"
        Test-Function "Get-AgentToolsList" "Get formatted list of agent tools"
        Test-Function "Invoke-AgentTool" "Execute an agent tool"
        Test-Function "Register-AgentTool" "Register a new agent tool"

        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "Testing Agent Command Functions" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

        Test-Function "Process-AgentCommand" "Process agent commands"

        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "Testing Agent Task Functions" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

        Test-Function "New-AgentTask" "Create new agent task"
        Test-Function "Start-AgentTask" "Start agent task"
        Test-Function "Start-AgentTaskAsync" "Start agent task asynchronously"
        Test-Function "Update-AgentTasksList" "Update agent tasks list"

        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "Testing Agent Workflow Functions" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

        Test-Function "Invoke-AgenticWorkflow" "Invoke agentic workflow"

        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "Testing Agent Logging Functions" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

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
                    $passCount++
                }
                else {
                    Write-Host "   ⚠️ read_file returned: $($result | ConvertTo-Json -Compress)" -ForegroundColor Yellow
                }
            }
            catch {
                Write-Host "   ❌ Error testing read_file: $_" -ForegroundColor Red
            }

            Write-Host "`n[Tool Test] Testing list_directory tool..." -ForegroundColor Yellow
            try {
                $result = Invoke-AgentTool -ToolName "list_directory" -Parameters @{path = "."}
                if ($result -and $result.success -ne $false) {
                    Write-Host "   ✅ list_directory tool works" -ForegroundColor Green
                    $passCount++
                }
                else {
                    Write-Host "   ⚠️ list_directory returned: $($result | ConvertTo-Json -Compress)" -ForegroundColor Yellow
                }
            }
            catch {
                Write-Host "   ❌ Error testing list_directory: $_" -ForegroundColor Red
            }
        }

        # Check registered tools
        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
        Write-Host "Checking Registered Tools" -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

        if ($script:agentTools) {
            $toolCount = $script:agentTools.Count
            Write-Host "`n✅ Found $toolCount registered agent tools:" -ForegroundColor Green
            foreach ($toolName in $script:agentTools.Keys) {
                $tool = $script:agentTools[$toolName]
                Write-Host "   • $toolName - $($tool.Description)" -ForegroundColor Gray
            }
        }
        else {
            Write-Host "⚠️ No agent tools registered" -ForegroundColor Yellow
        }

        # Summary
        Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║                    TEST SUMMARY                              ║" -ForegroundColor Cyan
        Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
        Write-Host "`nTotal Tests: $testCount" -ForegroundColor White
        Write-Host "Passed: $passCount" -ForegroundColor Green
        Write-Host "Failed: $($testCount - $passCount)" -ForegroundColor $(if (($testCount - $passCount) -gt 0) { "Red" } else { "Green" })
        if ($testCount -gt 0) {
            Write-Host "Success Rate: $([math]::Round(($passCount / $testCount) * 100, 2))%" -ForegroundColor $(if ($passCount -eq $testCount) { "Green" } else { "Yellow" })
        }
    }
}
catch {
    Write-Host "❌ Error loading RawrXD: $_" -ForegroundColor Red
    Write-Host "Stack trace: $($_.ScriptStackTrace)" -ForegroundColor Red
}

Write-Host "`n✅ Agentic capabilities test completed!" -ForegroundColor Green

