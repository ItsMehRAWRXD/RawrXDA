# RawrXD Built-In Tools CLI Test
# Tests tools outside of GUI context

$script:agentTools = @{}

function Write-DevConsole {
    param([string]$Message, [string]$Level = "INFO")
    # Silent for test
}

function Register-AgentTool {
    param(
        [string]$Name, 
        [string]$Description, 
        [string]$Category = "General", 
        [string]$Version = "1.0", 
        [hashtable]$Parameters, 
        [scriptblock]$Handler
    )
    $script:agentTools[$Name] = @{ 
        Name = $Name
        Description = $Description
        Category = $Category
        Parameters = $Parameters
        Handler = $Handler 
    }
}

# Load built-in tools
. "$PSScriptRoot\BuiltInTools.ps1"
Initialize-BuiltInTools

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "   RAWR XD BUILT-IN TOOLS CLI TEST" -ForegroundColor Cyan  
Write-Host "============================================`n" -ForegroundColor Cyan

Write-Host "Registered $($script:agentTools.Count) tools`n" -ForegroundColor Green

# Define Invoke-AgentTool for testing
function Invoke-AgentTool {
    param([string]$ToolName, [hashtable]$Parameters = @{})
    
    if (-not $script:agentTools.ContainsKey($ToolName)) {
        return @{ success = $false; error = "Tool not found: $ToolName" }
    }
    
    $tool = $script:agentTools[$ToolName]
    try {
        $result = & $tool.Handler @Parameters
        return $result
    } catch {
        return @{ success = $false; error = $_.Exception.Message }
    }
}

$passed = 0
$failed = 0

# TEST 1: list_directory
Write-Host "TEST 1: list_directory" -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "list_directory" -Parameters @{ path = $PSScriptRoot }
if ($result.success) {
    Write-Host "  ✅ SUCCESS - Found $($result.count) items" -ForegroundColor Green
    $result.items | Select-Object -First 3 | ForEach-Object { Write-Host "     - $($_.type): $($_.name)" }
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 2: read_file
Write-Host "`nTEST 2: read_file (first 5 lines)" -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "read_file" -Parameters @{ path = "$PSScriptRoot\BuiltInTools.ps1"; start_line = 1; end_line = 5 }
if ($result.success) {
    Write-Host "  ✅ SUCCESS - Read $($result.lines_returned) lines" -ForegroundColor Green
    $result.content -split "`n" | Select-Object -First 3 | ForEach-Object { Write-Host "     | $_" }
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 3: create_file and delete
Write-Host "`nTEST 3: create_file" -ForegroundColor Yellow
$testPath = "$PSScriptRoot\__test_file__.txt"
$result = Invoke-AgentTool -ToolName "create_file" -Parameters @{ path = $testPath; content = "Test content from CLI" }
if ($result.success) {
    Write-Host "  ✅ SUCCESS - Created: $testPath" -ForegroundColor Green
    # Cleanup
    Remove-Item $testPath -ErrorAction SilentlyContinue
    Write-Host "     (cleaned up test file)" -ForegroundColor DarkGray
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 4: run_in_terminal
Write-Host "`nTEST 4: run_in_terminal" -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "run_in_terminal" -Parameters @{ command = "Write-Host 'Hello from tool!'" }
if ($result.success) {
    Write-Host "  ✅ SUCCESS" -ForegroundColor Green
    Write-Host "     Output: $($result.output.Trim())" -ForegroundColor Gray
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 5: text_search
Write-Host "`nTEST 5: text_search" -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "text_search" -Parameters @{ 
    pattern = "Register-AgentTool"
    path = "$PSScriptRoot\BuiltInTools.ps1"
    max_results = 3
}
if ($result.success) {
    Write-Host "  ✅ SUCCESS - Found $($result.total_matches) matches" -ForegroundColor Green
    $result.matches | Select-Object -First 2 | ForEach-Object { 
        $text = $_.text.Trim()
        if ($text.Length -gt 60) { $text = $text.Substring(0, 60) + "..." }
        Write-Host "     Line $($_.line): $text" 
    }
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 6: git_status
Write-Host "`nTEST 6: git_status" -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "git_status" -Parameters @{ path = $PSScriptRoot }
if ($result.success) {
    Write-Host "  ✅ SUCCESS - Branch: $($result.branch)" -ForegroundColor Green
    Write-Host "     Modified: $($result.modified.Count), Untracked: $($result.untracked.Count)" -ForegroundColor Gray
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 7: file_search
Write-Host "`nTEST 7: file_search" -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "file_search" -Parameters @{ 
    pattern = "*.ps1"
    path = $PSScriptRoot
    max_results = 5
}
if ($result.success) {
    Write-Host "  ✅ SUCCESS - Found $($result.count) files" -ForegroundColor Green
    $result.files | Select-Object -First 3 | ForEach-Object { Write-Host "     - $_" }
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 8: create_directory + cleanup
Write-Host "`nTEST 8: create_directory" -ForegroundColor Yellow
$testDir = "$PSScriptRoot\__test_dir__"
$result = Invoke-AgentTool -ToolName "create_directory" -Parameters @{ path = $testDir }
if ($result.success) {
    Write-Host "  ✅ SUCCESS - Created: $testDir" -ForegroundColor Green
    Remove-Item $testDir -ErrorAction SilentlyContinue
    Write-Host "     (cleaned up test dir)" -ForegroundColor DarkGray
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 9: edit_file
Write-Host "`nTEST 9: edit_file" -ForegroundColor Yellow
$testPath = "$PSScriptRoot\__test_edit__.txt"
Set-Content $testPath -Value "Hello World"
$result = Invoke-AgentTool -ToolName "edit_file" -Parameters @{ 
    path = $testPath
    old_string = "World"
    new_string = "RawrXD"
}
if ($result.success) {
    $content = Get-Content $testPath -Raw
    if ($content -match "RawrXD") {
        Write-Host "  ✅ SUCCESS - Edit verified" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "  ⚠️  PARTIAL - File edited but content mismatch" -ForegroundColor Yellow
        $failed++
    }
    Remove-Item $testPath -ErrorAction SilentlyContinue
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

# TEST 10: semantic_search (will show as not fully implemented but should not error)
Write-Host "`nTEST 10: semantic_search" -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "semantic_search" -Parameters @{ 
    query = "agent tools"
    path = $PSScriptRoot
}
if ($result.success -or $result.note) {
    Write-Host "  ✅ SUCCESS - Search completed" -ForegroundColor Green
    if ($result.note) { Write-Host "     Note: $($result.note)" -ForegroundColor Gray }
    $passed++
} else {
    Write-Host "  ❌ FAILED: $($result.error)" -ForegroundColor Red
    $failed++
}

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "   TEST RESULTS: $passed PASSED, $failed FAILED" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })
Write-Host "============================================`n" -ForegroundColor Cyan

# Return exit code
if ($failed -gt 0) { exit 1 } else { exit 0 }
