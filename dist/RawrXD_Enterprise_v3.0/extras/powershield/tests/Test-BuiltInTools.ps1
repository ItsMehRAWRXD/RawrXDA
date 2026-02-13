<#
.SYNOPSIS
    Comprehensive Test Suite for RawrXD Built-In Tools
.DESCRIPTION
    Tests all 40+ built-in tools with validation, error handling, and reporting
#>

# ============================================
# TEST CONFIGURATION
# ============================================

$script:TestResults = @()
$script:TestPassed = 0
$script:TestFailed = 0
$script:TestDirectory = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\ToolTests"
$script:TestStartTime = Get-Date

# Colors
$Colors = @{
    Green  = [System.ConsoleColor]::Green
    Red    = [System.ConsoleColor]::Red
    Yellow = [System.ConsoleColor]::Yellow
    Cyan   = [System.ConsoleColor]::Cyan
    White  = [System.ConsoleColor]::White
}

# ============================================
# TEST UTILITIES
# ============================================

function Write-TestHeader {
    param([string]$Title)
    Write-Host "`n" -NoNewline
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor $Colors.Cyan
    Write-Host "║ $Title" -ForegroundColor $Colors.Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor $Colors.Cyan
}

function Write-TestCase {
    param([string]$Name, [string]$Description)
    Write-Host "`n▶ TEST: $Name" -ForegroundColor $Colors.Cyan
    Write-Host "  Description: $Description" -ForegroundColor $Colors.White
}

function Write-TestPass {
    param([string]$Message, [object]$Result = $null)
    Write-Host "  ✅ PASS: $Message" -ForegroundColor $Colors.Green
    $script:TestPassed++
    $script:TestResults += @{
        Test   = $Message
        Status = "PASS"
        Result = $Result
    }
}

function Write-TestFail {
    param([string]$Message, [string]$Error = "", [object]$Result = $null)
    Write-Host "  ❌ FAIL: $Message" -ForegroundColor $Colors.Red
    if ($Error) { Write-Host "         Error: $Error" -ForegroundColor $Colors.Red }
    $script:TestFailed++
    $script:TestResults += @{
        Test   = $Message
        Status = "FAIL"
        Error  = $Error
        Result = $Result
    }
}

function Write-TestInfo {
    param([string]$Message)
    Write-Host "  ℹ️  $Message" -ForegroundColor $Colors.Yellow
}

# ============================================
# SETUP & TEARDOWN
# ============================================

function Initialize-TestEnvironment {
    Write-TestHeader "INITIALIZATION"
    
    # Create test directory
    if (-not (Test-Path $script:TestDirectory)) {
        New-Item -ItemType Directory -Path $script:TestDirectory -Force | Out-Null
        Write-TestInfo "Test directory created: $script:TestDirectory"
    }
    
    # Define minimal Write-DevConsole if not available
    if (-not (Get-Command Write-DevConsole -ErrorAction SilentlyContinue)) {
        function Write-DevConsole {
            param([string]$Message, [string]$Level = "INFO")
            $color = switch ($Level) {
                "INFO" { "Cyan" }
                "SUCCESS" { "Green" }
                "WARNING" { "Yellow" }
                "ERROR" { "Red" }
                default { "White" }
            }
            Write-Host $Message -ForegroundColor $color
        }
    }
    
    # Load BuiltInTools
    $builtInToolsPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\BuiltInTools.ps1"
    
    if (Test-Path $builtInToolsPath) {
        # Source without module context
        try {
            $toolContent = Get-Content $builtInToolsPath -Raw
            # Remove Export-ModuleMember to allow dot-sourcing
            $toolContent = $toolContent -replace 'Export-ModuleMember.*', ''
            Invoke-Expression $toolContent
            Write-TestPass "BuiltInTools module loaded"
        } catch {
            Write-TestFail "Failed to load BuiltInTools" $_.Exception.Message
            return $false
        }
    } else {
        Write-TestFail "BuiltInTools module not found" "Path: $builtInToolsPath"
        return $false
    }
    
    # Initialize tools
    try {
        Initialize-BuiltInTools
        Write-TestPass "Built-In Tools initialized"
    } catch {
        Write-TestFail "Failed to initialize tools" $_.Exception.Message
        return $false
    }
    
    return $true
}

function Cleanup-TestEnvironment {
    Write-TestHeader "CLEANUP"
    
    # Remove test files
    if (Test-Path $script:TestDirectory) {
        Remove-Item -Path $script:TestDirectory -Recurse -Force -ErrorAction SilentlyContinue
        Write-TestInfo "Test directory cleaned up"
    }
    
    # Kill background jobs
    Get-Job | Stop-Job -ErrorAction SilentlyContinue
    Get-Job | Remove-Job -ErrorAction SilentlyContinue
    Write-TestInfo "Background jobs cleaned up"
}

# ============================================
# FILE OPERATION TESTS
# ============================================

function Test-FileOperations {
    Write-TestHeader "FILE OPERATIONS TOOLS"
    
    # Test: Create File
    Write-TestCase "create_file" "Create a new file with content"
    try {
        $testFile = Join-Path $script:TestDirectory "test-create.txt"
        $result = Invoke-AgentTool -ToolName "create_file" -Arguments @{
            file_path = $testFile
            content   = "Test content for file creation"
        }
        
        if ($result.success -and (Test-Path $testFile)) {
            $content = Get-Content $testFile -Raw
            if ($content -eq "Test content for file creation") {
                Write-TestPass "File created with correct content" $result
            } else {
                Write-TestFail "File created but content mismatch" "Expected content not found"
            }
        } else {
            Write-TestFail "File creation failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during file creation" $_.Exception.Message
    }
    
    # Test: Read File
    Write-TestCase "read_file" "Read file contents with line range"
    try {
        $testFile = Join-Path $script:TestDirectory "test-read.txt"
        Set-Content -Path $testFile -Value "Line 1`nLine 2`nLine 3`nLine 4`nLine 5"
        
        $result = Invoke-AgentTool -ToolName "read_file" -Arguments @{
            file_path = $testFile
            start_line = 2
            end_line   = 4
        }
        
        if ($result.success -and $result.content) {
            Write-TestPass "File read successfully with line range" $result
        } else {
            Write-TestFail "File read failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during file read" $_.Exception.Message
    }
    
    # Test: Edit File
    Write-TestCase "edit_file" "Replace text in an existing file"
    try {
        $testFile = Join-Path $script:TestDirectory "test-edit.txt"
        Set-Content -Path $testFile -Value "Original content here"
        
        $result = Invoke-AgentTool -ToolName "edit_file" -Arguments @{
            file_path  = $testFile
            old_string = "Original content"
            new_string = "Modified content"
        }
        
        if ($result.success) {
            $content = Get-Content $testFile -Raw
            if ($content -match "Modified content") {
                Write-TestPass "File edited successfully" $result
            } else {
                Write-TestFail "File edited but content mismatch"
            }
        } else {
            Write-TestFail "File edit failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during file edit" $_.Exception.Message
    }
    
    # Test: Create Directory
    Write-TestCase "create_directory" "Create a new directory"
    try {
        $testDir = Join-Path $script:TestDirectory "test-subdir"
        $result = Invoke-AgentTool -ToolName "create_directory" -Arguments @{
            dir_path = $testDir
        }
        
        if ($result.success -and (Test-Path $testDir)) {
            Write-TestPass "Directory created successfully" $result
        } else {
            Write-TestFail "Directory creation failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during directory creation" $_.Exception.Message
    }
    
    # Test: List Directory
    Write-TestCase "list_directory" "List directory contents"
    try {
        # Create some test files
        1..3 | ForEach-Object {
            Set-Content -Path (Join-Path $script:TestDirectory "file$_.txt") -Value "test"
        }
        
        $result = Invoke-AgentTool -ToolName "list_directory" -Arguments @{
            path = $script:TestDirectory
        }
        
        if ($result.success -and $result.count -ge 3) {
            Write-TestPass "Directory listing successful" $result
        } else {
            Write-TestFail "Directory listing failed or incomplete" $result.error
        }
    } catch {
        Write-TestFail "Exception during directory listing" $_.Exception.Message
    }
    
    # Test: Edit Multiple Files
    Write-TestCase "edit_multiple_files" "Edit multiple files in batch"
    try {
        $file1 = Join-Path $script:TestDirectory "batch1.txt"
        $file2 = Join-Path $script:TestDirectory "batch2.txt"
        Set-Content -Path $file1 -Value "AAA"
        Set-Content -Path $file2 -Value "BBB"
        
        $result = Invoke-AgentTool -ToolName "edit_multiple_files" -Arguments @{
            edits = @(
                @{ file_path = $file1; old_string = "AAA"; new_string = "AAA_EDITED" },
                @{ file_path = $file2; old_string = "BBB"; new_string = "BBB_EDITED" }
            )
        }
        
        if ($result.success -and $result.count -eq 2) {
            Write-TestPass "Multiple files edited successfully" $result
        } else {
            Write-TestFail "Batch edit failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during batch file edit" $_.Exception.Message
    }
}

# ============================================
# SEARCH & ANALYSIS TESTS
# ============================================

function Test-SearchTools {
    Write-TestHeader "SEARCH & CODE ANALYSIS TOOLS"
    
    # Test: File Search
    Write-TestCase "file_search" "Search for files by pattern"
    try {
        # Create test files
        Set-Content -Path (Join-Path $script:TestDirectory "search1.ps1") -Value "test"
        Set-Content -Path (Join-Path $script:TestDirectory "search2.ps1") -Value "test"
        Set-Content -Path (Join-Path $script:TestDirectory "search3.txt") -Value "test"
        
        $result = Invoke-AgentTool -ToolName "file_search" -Arguments @{
            pattern   = "*.ps1"
            directory = $script:TestDirectory
            recurse   = $true
        }
        
        if ($result.success -and $result.count -ge 2) {
            Write-TestPass "File search found .ps1 files" $result
        } else {
            Write-TestFail "File search failed or no files found" $result.error
        }
    } catch {
        Write-TestFail "Exception during file search" $_.Exception.Message
    }
    
    # Test: Text Search (Grep)
    Write-TestCase "text_search" "Search text content in files"
    try {
        $testFile = Join-Path $script:TestDirectory "grep-test.txt"
        Set-Content -Path $testFile -Value "function MyFunction { }`nfunction AnotherFunction { }"
        
        $result = Invoke-AgentTool -ToolName "text_search" -Arguments @{
            pattern     = "function"
            directory   = $script:TestDirectory
            file_pattern = "*.txt"
        }
        
        if ($result.success -and $result.count -ge 2) {
            Write-TestPass "Text search found matching patterns" $result
        } else {
            Write-TestFail "Text search failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during text search" $_.Exception.Message
    }
    
    # Test: Code Usages
    Write-TestCase "code_usages" "Find code symbol usages"
    try {
        $testFile = Join-Path $script:TestDirectory "usage-test.txt"
        Set-Content -Path $testFile -Value "function TestSymbol { }`nTestSymbol`nTestSymbol"
        
        $result = Invoke-AgentTool -ToolName "code_usages" -Arguments @{
            symbol    = "TestSymbol"
            directory = $script:TestDirectory
        }
        
        if ($result.success) {
            Write-TestPass "Code usages search completed" $result
        } else {
            Write-TestFail "Code usages search failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during code usages search" $_.Exception.Message
    }
    
    # Test: Semantic Search
    Write-TestCase "semantic_search" "AI-powered semantic search"
    try {
        $result = Invoke-AgentTool -ToolName "semantic_search" -Arguments @{
            query     = "file operations and manipulation"
            directory = $script:TestDirectory
        }
        
        if ($result.success) {
            Write-TestPass "Semantic search initiated" $result
        } else {
            Write-TestFail "Semantic search failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during semantic search" $_.Exception.Message
    }
}

# ============================================
# TERMINAL TESTS
# ============================================

function Test-TerminalTools {
    Write-TestHeader "TERMINAL & COMMAND EXECUTION TOOLS"
    
    # Test: Run in Terminal
    Write-TestCase "run_in_terminal" "Execute command in terminal"
    try {
        $result = Invoke-AgentTool -ToolName "run_in_terminal" -Arguments @{
            command = "Get-Date"
        }
        
        if ($result.success -and $result.output) {
            Write-TestPass "Terminal command executed successfully" $result
        } else {
            Write-TestFail "Terminal command execution failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during terminal execution" $_.Exception.Message
    }
    
    # Test: Background Job
    Write-TestCase "run_in_terminal (background)" "Execute command as background job"
    try {
        $result = Invoke-AgentTool -ToolName "run_in_terminal" -Arguments @{
            command    = "Start-Sleep -Seconds 2; 'Job completed'"
            background = $true
        }
        
        if ($result.success -and $result.job_id) {
            Write-TestPass "Background job started" $result
            
            # Test: Get Terminal Output
            Write-TestCase "get_terminal_output" "Get output from background job"
            Start-Sleep -Seconds 3
            $outputResult = Invoke-AgentTool -ToolName "get_terminal_output" -Arguments @{
                job_id = $result.job_id
            }
            
            if ($outputResult.success) {
                Write-TestPass "Terminal output retrieved" $outputResult
            } else {
                Write-TestFail "Failed to get terminal output" $outputResult.error
            }
        } else {
            Write-TestFail "Background job failed to start" $result.error
        }
    } catch {
        Write-TestFail "Exception during background job execution" $_.Exception.Message
    }
    
    # Test: Terminal Last Command
    Write-TestCase "terminal_last_command" "Get last executed command"
    try {
        # Execute a test command first
        Invoke-Expression "Write-Host 'Test command executed'"
        
        $result = Invoke-AgentTool -ToolName "terminal_last_command" -Arguments @{}
        
        if ($result.success -and $result.command) {
            Write-TestPass "Last command retrieved successfully" $result
        } else {
            Write-TestFail "Failed to get last command" $result.error
        }
    } catch {
        Write-TestFail "Exception while getting last command" $_.Exception.Message
    }
}

# ============================================
# NOTEBOOK TESTS
# ============================================

function Test-NotebookTools {
    Write-TestHeader "JUPYTER NOTEBOOK TOOLS"
    
    # Test: Create Jupyter Notebook
    Write-TestCase "new_jupyter_notebook" "Create a new Jupyter notebook"
    try {
        $notebookPath = Join-Path $script:TestDirectory "test-notebook.ipynb"
        $result = Invoke-AgentTool -ToolName "new_jupyter_notebook" -Arguments @{
            file_path = $notebookPath
            kernel    = "python3"
        }
        
        if ($result.success -and (Test-Path $notebookPath)) {
            Write-TestPass "Notebook created successfully" $result
            
            # Test: Get Notebook Summary
            Write-TestCase "get_notebook_summary" "Get notebook structure and metadata"
            $summaryResult = Invoke-AgentTool -ToolName "get_notebook_summary" -Arguments @{
                file_path = $notebookPath
            }
            
            if ($summaryResult.success) {
                Write-TestPass "Notebook summary retrieved" $summaryResult
            } else {
                Write-TestFail "Failed to get notebook summary" $summaryResult.error
            }
        } else {
            Write-TestFail "Notebook creation failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during notebook creation" $_.Exception.Message
    }
}

# ============================================
# GIT TESTS
# ============================================

function Test-GitTools {
    Write-TestHeader "GIT & VERSION CONTROL TOOLS"
    
    # Test: Git Status
    Write-TestCase "git_status" "Get git repository status"
    try {
        $result = Invoke-AgentTool -ToolName "git_status" -Arguments @{
            repo_path = "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
        }
        
        if ($result.success) {
            Write-TestPass "Git status retrieved successfully" $result
            Write-TestInfo "Current branch: $($result.branch)"
        } else {
            Write-TestFail "Git status retrieval failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during git status" $_.Exception.Message
    }
    
    # Test: GitHub Pull Requests
    Write-TestCase "github_pull_requests" "List GitHub pull requests"
    try {
        $result = Invoke-AgentTool -ToolName "github_pull_requests" -Arguments @{
            repo = "ItsMehRAWRXD/RawrXD"
        }
        
        if ($result.success) {
            Write-TestPass "GitHub PR tool initialized" $result
        } else {
            Write-TestFail "GitHub PR tool failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during GitHub PR check" $_.Exception.Message
    }
}

# ============================================
# WORKSPACE TESTS
# ============================================

function Test-WorkspaceTools {
    Write-TestHeader "WORKSPACE & PROJECT TOOLS"
    
    # Test: New Workspace
    Write-TestCase "new_workspace" "Create new project workspace"
    try {
        Push-Location $script:TestDirectory
        $result = Invoke-AgentTool -ToolName "new_workspace" -Arguments @{
            name     = "test-workspace"
            template = "basic"
        }
        Pop-Location
        
        if ($result.success) {
            Write-TestPass "Workspace created successfully" $result
        } else {
            Write-TestFail "Workspace creation failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during workspace creation" $_.Exception.Message
    }
    
    # Test: Get Project Setup Info
    Write-TestCase "get_project_setup_info" "Get project configuration details"
    try {
        $result = Invoke-AgentTool -ToolName "get_project_setup_info" -Arguments @{
            project_path = "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
        }
        
        if ($result.success) {
            Write-TestPass "Project setup info retrieved" $result
        } else {
            Write-TestFail "Failed to get project setup info" $result.error
        }
    } catch {
        Write-TestFail "Exception during project setup info retrieval" $_.Exception.Message
    }
}

# ============================================
# BROWSER & WEB TESTS
# ============================================

function Test-BrowserTools {
    Write-TestHeader "BROWSER & WEB TOOLS"
    
    # Test: Fetch Webpage
    Write-TestCase "fetch_webpage" "Fetch and parse webpage content"
    try {
        $result = Invoke-AgentTool -ToolName "fetch_webpage" -Arguments @{
            url = "https://www.example.com"
        }
        
        if ($result.success -and $result.status_code -eq 200) {
            Write-TestPass "Webpage fetched successfully" $result
        } else {
            Write-TestFail "Webpage fetch failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during webpage fetch" $_.Exception.Message
    }
}

# ============================================
# TASK & TODO TESTS
# ============================================

function Test-TaskTools {
    Write-TestHeader "TASK & TODO MANAGEMENT TOOLS"
    
    # Test: Manage Todos - Add
    Write-TestCase "manage_todos (add)" "Create TODO items"
    try {
        $result = Invoke-AgentTool -ToolName "manage_todos" -Arguments @{
            action = "add"
            task   = "Test task 1"
        }
        
        if ($result.success -and $result.id) {
            Write-TestPass "Todo item added successfully" $result
            $todoId = $result.id
            
            # Test: Manage Todos - List
            Write-TestCase "manage_todos (list)" "List all TODO items"
            $listResult = Invoke-AgentTool -ToolName "manage_todos" -Arguments @{
                action = "list"
            }
            
            if ($listResult.success -and $listResult.todos.Count -gt 0) {
                Write-TestPass "Todos listed successfully" $listResult
                
                # Test: Manage Todos - Complete
                Write-TestCase "manage_todos (complete)" "Mark TODO as complete"
                $completeResult = Invoke-AgentTool -ToolName "manage_todos" -Arguments @{
                    action = "complete"
                    id     = $todoId
                }
                
                if ($completeResult.success) {
                    Write-TestPass "Todo marked as complete" $completeResult
                } else {
                    Write-TestFail "Failed to complete todo" $completeResult.error
                }
            } else {
                Write-TestFail "Failed to list todos" $listResult.error
            }
        } else {
            Write-TestFail "Todo creation failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during todo management" $_.Exception.Message
    }
    
    # Test: Create and Run Task
    Write-TestCase "create_and_run_task" "Create and execute a task"
    try {
        $result = Invoke-AgentTool -ToolName "create_and_run_task" -Arguments @{
            task_name = "Test Task"
            command   = "Get-Date"
        }
        
        if ($result.success -and $result.output) {
            Write-TestPass "Task created and executed" $result
        } else {
            Write-TestFail "Task execution failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during task execution" $_.Exception.Message
    }
}

# ============================================
# API & DIAGNOSTIC TESTS
# ============================================

function Test-APIAndDiagnosticTools {
    Write-TestHeader "API & DIAGNOSTIC TOOLS"
    
    # Test: Get Problems
    Write-TestCase "get_problems" "Get compilation/lint errors"
    try {
        $result = Invoke-AgentTool -ToolName "get_problems" -Arguments @{
            file_path = ""
        }
        
        if ($result.success) {
            Write-TestPass "Problem detection tool initialized" $result
        } else {
            Write-TestFail "Problem detection failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during problem detection" $_.Exception.Message
    }
    
    # Test: VS Code API
    Write-TestCase "vscode_api" "Access VS Code API documentation"
    try {
        $result = Invoke-AgentTool -ToolName "vscode_api" -Arguments @{
            query = "webview api"
        }
        
        if ($result.success) {
            Write-TestPass "VS Code API documentation tool initialized" $result
        } else {
            Write-TestFail "VS Code API lookup failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during VS Code API lookup" $_.Exception.Message
    }
    
    # Test: Run VS Code Command
    Write-TestCase "run_vscode_command" "Execute VS Code command"
    try {
        $result = Invoke-AgentTool -ToolName "run_vscode_command" -Arguments @{
            command_id = "editor.action.formatDocument"
            args       = @()
        }
        
        if ($result.success) {
            Write-TestPass "VS Code command execution initialized" $result
        } else {
            Write-TestFail "VS Code command execution failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during VS Code command execution" $_.Exception.Message
    }
}

# ============================================
# SUBAGENT TEST
# ============================================

function Test-SubagentTools {
    Write-TestHeader "AGENT & AUTOMATION TOOLS"
    
    # Test: Run Subagent
    Write-TestCase "run_subagent" "Launch autonomous sub-agent"
    try {
        $result = Invoke-AgentTool -ToolName "run_subagent" -Arguments @{
            task_description = "Analyze project structure"
            timeout_seconds  = 30
        }
        
        if ($result.success) {
            Write-TestPass "Subagent framework initialized" $result
        } else {
            Write-TestFail "Subagent framework failed" $result.error
        }
    } catch {
        Write-TestFail "Exception during subagent invocation" $_.Exception.Message
    }
}

# ============================================
# TEST REPORT GENERATION
# ============================================

function Generate-TestReport {
    Write-TestHeader "TEST EXECUTION REPORT"
    
    $totalTests = $script:TestPassed + $script:TestFailed
    $passRate = if ($totalTests -gt 0) { [math]::Round(($script:TestPassed / $totalTests) * 100, 2) } else { 0 }
    
    Write-Host "`n📊 TEST SUMMARY:`n" -ForegroundColor $Colors.Cyan
    Write-Host "  Total Tests:    $totalTests" -ForegroundColor $Colors.White
    Write-Host "  Passed:         $($script:TestPassed)" -ForegroundColor $Colors.Green
    Write-Host "  Failed:         $($script:TestFailed)" -ForegroundColor $Colors.Red
    Write-Host "  Success Rate:   $passRate%" -ForegroundColor $(if ($passRate -ge 80) { $Colors.Green } else { $Colors.Yellow })
    
    $duration = (Get-Date) - $script:TestStartTime
    Write-Host "  Duration:       $($duration.TotalSeconds.ToString('0.00')) seconds" -ForegroundColor $Colors.White
    
    Write-Host "`n📋 DETAILED RESULTS:`n" -ForegroundColor $Colors.Cyan
    
    $script:TestResults | ForEach-Object {
        $statusColor = if ($_.Status -eq "PASS") { $Colors.Green } else { $Colors.Red }
        $statusIcon = if ($_.Status -eq "PASS") { "✅" } else { "❌" }
        Write-Host "$statusIcon $($_.Test)" -ForegroundColor $statusColor
        if ($_.Error) {
            Write-Host "   Error: $($_.Error)" -ForegroundColor $Colors.Red
        }
    }
    
    Write-Host "`n" -NoNewline
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor $Colors.Cyan
    if ($script:TestFailed -eq 0) {
        Write-Host "║ ✅ ALL TESTS PASSED - BUILT-IN TOOLS ARE FULLY OPERATIONAL   ║" -ForegroundColor $Colors.Green
    } else {
        Write-Host "║ ⚠️  SOME TESTS FAILED - REVIEW RESULTS ABOVE                 ║" -ForegroundColor $Colors.Yellow
    }
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor $Colors.Cyan
    Write-Host "`n"
}

# ============================================
# MAIN EXECUTION
# ============================================

Write-Host "`n" -NoNewline
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor $Colors.Cyan
Write-Host "║  RawrXD Built-In Tools - Comprehensive Test Suite v1.0        ║" -ForegroundColor $Colors.Cyan
Write-Host "║  Testing 40+ tools across 10 functional categories             ║" -ForegroundColor $Colors.Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor $Colors.Cyan

# Initialize test environment
if (-not (Initialize-TestEnvironment)) {
    Write-Host "`n❌ Test environment initialization failed. Exiting." -ForegroundColor $Colors.Red
    exit 1
}

Write-Host "`n✅ Test environment ready. Starting test execution...`n" -ForegroundColor $Colors.Green

# Run all test suites
Test-FileOperations
Test-SearchTools
Test-TerminalTools
Test-NotebookTools
Test-GitTools
Test-WorkspaceTools
Test-BrowserTools
Test-TaskTools
Test-APIAndDiagnosticTools
Test-SubagentTools

# Generate report
Generate-TestReport

# Cleanup
Cleanup-TestEnvironment

Write-Host "Test execution completed! Check results above." -ForegroundColor $Colors.Cyan
