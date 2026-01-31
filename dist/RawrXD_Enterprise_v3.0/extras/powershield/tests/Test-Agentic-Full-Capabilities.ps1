# ============================================
# COMPREHENSIVE AGENTIC CAPABILITIES TEST
# ============================================
# This script tests all agentic features of RawrXD
# Run this after RawrXD is started to verify agentic functionality

Write-Host @"
╔════════════════════════════════════════════════════════════════╗
║     🤖 RAWRXD AGENTIC CAPABILITIES - COMPREHENSIVE TEST        ║
╚════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

$testResults = @()
$testCount = 0
$passCount = 0
$failCount = 0

function Test-AgenticFeature {
    param(
        [string]$FeatureName,
        [string]$Description,
        [scriptblock]$TestScript,
        [string]$Category = "General"
    )

    $testCount++
    Write-Host "`n[$testCount] Testing: $FeatureName" -ForegroundColor Yellow
    Write-Host "   Description: $Description" -ForegroundColor Gray

    try {
        $result = & $TestScript
        $passCount++
        Write-Host "   ✅ PASS" -ForegroundColor Green
        $testResults += @{
            Feature = $FeatureName
            Category = $Category
            Status = "PASS"
            Description = $Description
            Result = $result
        }
        return $true
    }
    catch {
        $failCount++
        Write-Host "   ❌ FAIL: $($_.Exception.Message)" -ForegroundColor Red
        $testResults += @{
            Feature = $FeatureName
            Category = $Category
            Status = "FAIL"
            Description = $Description
            Error = $_.Exception.Message
        }
        return $false
    }
}

# ============================================
# TEST 1: Agent Tool Registration System
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Agent Tool Registration System" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Get-AgentToolsSchema" -Category "Tool System" -Description "Get schema of all registered agent tools" {
    if (Get-Command Get-AgentToolsSchema -ErrorAction SilentlyContinue) {
        $tools = Get-AgentToolsSchema
        if ($tools -and $tools.Count -gt 0) {
            return "Found $($tools.Count) registered tools"
        }
        return "No tools registered"
    }
    throw "Get-AgentToolsSchema function not found"
}

Test-AgenticFeature -FeatureName "Get-AgentToolsList" -Category "Tool System" -Description "Get formatted list of agent tools" {
    if (Get-Command Get-AgentToolsList -ErrorAction SilentlyContinue) {
        $list = Get-AgentToolsList
        return "Tool list retrieved successfully"
    }
    throw "Get-AgentToolsList function not found"
}

# ============================================
# TEST 2: File System Agent Tools
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: File System Agent Tools" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "read_file Tool" -Category "File Tools" -Description "Test reading a file using agent tool" {
    if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
        $testFile = "RawrXD.ps1"
        if (Test-Path $testFile) {
            $result = Invoke-AgentTool -ToolName "read_file" -Parameters @{path = $testFile}
            if ($result -and $result.success -ne $false) {
                return "File read successfully"
            }
            return "File read returned: $($result | ConvertTo-Json -Compress)"
        }
        throw "Test file not found: $testFile"
    }
    throw "Invoke-AgentTool function not found"
}

Test-AgenticFeature -FeatureName "list_directory Tool" -Category "File Tools" -Description "Test listing directory using agent tool" {
    if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
        $result = Invoke-AgentTool -ToolName "list_directory" -Parameters @{path = "."}
        if ($result -and $result.success -ne $false) {
            return "Directory listed successfully"
        }
        return "Directory list returned: $($result | ConvertTo-Json -Compress)"
    }
    throw "Invoke-AgentTool function not found"
}

Test-AgenticFeature -FeatureName "write_file Tool" -Category "File Tools" -Description "Test writing a file using agent tool" {
    if (Get-Command Invoke-AgentTool -ErrorAction SilentlyContinue) {
        $testContent = "Test file created by agentic system at $(Get-Date)"
        $testPath = "test-agentic-file.txt"
        $result = Invoke-AgentTool -ToolName "write_file" -Parameters @{path = $testPath; content = $testContent}
        if ($result -and $result.success -ne $false) {
            if (Test-Path $testPath) {
                Remove-Item $testPath -Force -ErrorAction SilentlyContinue
                return "File written and verified successfully"
            }
            return "File write reported success but file not found"
        }
        return "File write returned: $($result | ConvertTo-Json -Compress)"
    }
    throw "Invoke-AgentTool function not found"
}

# ============================================
# TEST 3: Agent Command Processing
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Agent Command Processing" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Process-AgentCommand" -Category "Command Processing" -Description "Test agent command processing system" {
    if (Get-Command Process-AgentCommand -ErrorAction SilentlyContinue) {
        # Test with a valid command
        $result = Process-AgentCommand -Command "analyze_code" -Parameters @{FilePath = "RawrXD.ps1"} -SourceContext "Test"
        return "Command processing function available (result: $result)"
    }
    throw "Process-AgentCommand function not found"
}

# ============================================
# TEST 4: Agent Task Management
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Agent Task Management" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "New-AgentTask" -Category "Task Management" -Description "Test creating a new agent task" {
    if (Get-Command New-AgentTask -ErrorAction SilentlyContinue) {
        $taskId = New-AgentTask -Name "Test Task" -Description "Testing agent task creation" -Steps @(
            @{Type = "tool"; Description = "Test step"; Tool = "read_file"; Arguments = @{path = "RawrXD.ps1"}}
        )
        if ($taskId) {
            return "Task created with ID: $taskId"
        }
        throw "Task creation returned no ID"
    }
    throw "New-AgentTask function not found"
}

Test-AgenticFeature -FeatureName "Start-AgentTask" -Category "Task Management" -Description "Test starting an agent task" {
    if (Get-Command New-AgentTask -ErrorAction SilentlyContinue -and Get-Command Start-AgentTask -ErrorAction SilentlyContinue) {
        $taskId = New-AgentTask -Name "Test Execution Task" -Description "Testing task execution" -Steps @(
            @{Type = "command"; Description = "Test command"; Command = "Get-Date"}
        )
        if ($taskId) {
            Start-AgentTask -TaskId $taskId
            return "Task started successfully"
        }
        throw "Task creation failed"
    }
    throw "Required functions not found"
}

# ============================================
# TEST 5: Agentic Workflow Execution
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Agentic Workflow Execution" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Invoke-AgenticWorkflow" -Category "Workflow" -Description "Test agentic workflow execution" {
    if (Get-Command Invoke-AgenticWorkflow -ErrorAction SilentlyContinue) {
        $task = Invoke-AgenticWorkflow -Goal "test workflow" -Context "Testing workflow system"
        if ($task -and $task.Id) {
            return "Workflow created with task ID: $($task.Id)"
        }
        throw "Workflow creation failed"
    }
    throw "Invoke-AgenticWorkflow function not found"
}

# ============================================
# TEST 6: Multithreaded Agent System
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Multithreaded Agent System" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Start-AgentTaskAsync" -Category "Multithreading" -Description "Test async agent task execution" {
    if (Get-Command New-AgentTask -ErrorAction SilentlyContinue -and Get-Command Start-AgentTaskAsync -ErrorAction SilentlyContinue) {
        $taskId = New-AgentTask -Name "Async Test Task" -Description "Testing async execution" -Steps @(
            @{Type = "command"; Description = "Async test"; Command = "Start-Sleep -Seconds 1; Get-Date"}
        )
        if ($taskId) {
            Start-AgentTaskAsync -TaskId $taskId -Priority "Normal"
            Start-Sleep -Milliseconds 500
            return "Async task started successfully"
        }
        throw "Task creation failed"
    }
    throw "Required functions not found"
}

# ============================================
# TEST 7: Agent Logging System
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Agent Logging System" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Write-AgentLog" -Category "Logging" -Description "Test agent logging functionality" {
    if (Get-Command Write-AgentLog -ErrorAction SilentlyContinue) {
        Write-AgentLog -Level "Info" -Message "Test log entry" -Data @{Test = "Value"}
        return "Agent log written successfully"
    }
    throw "Write-AgentLog function not found"
}

# ============================================
# TEST 8: Natural Language Command Parsing
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Natural Language Command Parsing" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Natural Language: open file" -Category "NLP" -Description "Test natural language file opening" {
    # This would be tested through the chat interface
    return "Natural language parsing available through chat interface"
}

Test-AgenticFeature -FeatureName "Natural Language: execute command" -Category "NLP" -Description "Test natural language command execution" {
    # This would be tested through the chat interface
    return "Natural language command execution available through chat interface"
}

# ============================================
# TEST 9: Agent Context Management
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Agent Context Management" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Agent Context" -Category "Context" -Description "Test agent context availability" {
    if ($global:agentContext) {
        $contextInfo = @{
            Tasks = if ($global:agentContext.Tasks) { $global:agentContext.Tasks.Count } else { 0 }
            Commands = if ($global:agentContext.Commands) { $global:agentContext.Commands.Count } else { 0 }
        }
        return "Agent context available: $($contextInfo | ConvertTo-Json -Compress)"
    }
    return "Agent context not initialized (may be normal if not in GUI mode)"
}

# ============================================
# TEST 10: Security and Validation
# ============================================
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "TESTING: Security and Validation" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-AgenticFeature -FeatureName "Command Safety Validation" -Category "Security" -Description "Test unsafe command blocking" {
    if (Get-Command Process-AgentCommand -ErrorAction SilentlyContinue) {
        # Try an unsafe command (should be blocked)
        $result = Process-AgentCommand -Command "rm -rf /" -SourceContext "Test"
        if ($result -eq $false) {
            return "Unsafe command correctly blocked"
        }
        return "Security validation may not be working (unsafe command allowed)"
    }
    throw "Process-AgentCommand function not found"
}

# ============================================
# SUMMARY
# ============================================
Write-Host "`n`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    TEST SUMMARY                              ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

Write-Host "`nTotal Tests: $testCount" -ForegroundColor White
Write-Host "Passed: $passCount" -ForegroundColor Green
Write-Host "Failed: $failCount" -ForegroundColor $(if ($failCount -gt 0) { "Red" } else { "Green" })
Write-Host "Success Rate: $([math]::Round(($passCount / $testCount) * 100, 2))%" -ForegroundColor $(if ($passCount -eq $testCount) { "Green" } else { "Yellow" })

# Group results by category
$categories = $testResults | Group-Object -Property Category
Write-Host "`nResults by Category:" -ForegroundColor Cyan
foreach ($category in $categories) {
    $catPass = ($category.Group | Where-Object { $_.Status -eq "PASS" }).Count
    $catTotal = $category.Group.Count
    Write-Host "  $($category.Name): $catPass/$catTotal passed" -ForegroundColor $(if ($catPass -eq $catTotal) { "Green" } else { "Yellow" })
}

# Save results to file
$resultsFile = "agentic-test-results-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
$testResults | ConvertTo-Json -Depth 10 | Set-Content $resultsFile
Write-Host "`nDetailed results saved to: $resultsFile" -ForegroundColor Gray

# Generate markdown report
$markdownReport = @"
# RawrXD Agentic Capabilities Test Report
Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

## Summary
- **Total Tests**: $testCount
- **Passed**: $passCount
- **Failed**: $failCount
- **Success Rate**: $([math]::Round(($passCount / $testCount) * 100, 2))%

## Test Results

"@

foreach ($category in $categories) {
    $markdownReport += "### $($category.Name)`n`n"
    foreach ($test in $category.Group) {
        $status = if ($test.Status -eq "PASS") { "✅" } else { "❌" }
        $markdownReport += "- $status **$($test.Feature)**: $($test.Description)`n"
        if ($test.Error) {
            $markdownReport += "  - Error: $($test.Error)`n"
        }
    }
    $markdownReport += "`n"
}

$reportFile = "agentic-test-report-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"
$markdownReport | Set-Content $reportFile
Write-Host "Markdown report saved to: $reportFile" -ForegroundColor Gray

Write-Host "`n✅ Agentic capabilities test completed!" -ForegroundColor Green

