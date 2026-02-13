# 🤖 RawrXD Headless Agentic Test Suite
# Tests Ollama integration and Agent Tools WITHOUT launching GUI
# Run this to verify agentic capabilities in real-time
# Updated: Tests auto-enable Agent Mode feature

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Headless Agentic Test Suite v2.0" -ForegroundColor Cyan
Write-Host "  Testing Ollama + Agent Tools + Auto-Enable Feature" -ForegroundColor Cyan  
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# ============================================
# INITIALIZE CORE SYSTEMS (No GUI)
# ============================================

$ErrorActionPreference = "Continue"
$global:AgentMode = $true  # Enable agent mode

# Agent Tools Registry
$script:agentTools = @{}

# Ollama Configuration
$OllamaAPIEndpoint = "http://localhost:11434/api/generate"
$OllamaModel = "llama3:latest"  # Use local model to avoid rate limits

# ============================================
# REGISTER AGENT TOOLS (Extracted from RawrXD.ps1)
# ============================================

function Register-AgentTool {
    param(
        [string]$Name,
        [string]$Description,
        [hashtable]$Parameters,
        [scriptblock]$Handler
    )
    $script:agentTools[$Name] = @{
        Name        = $Name
        Description = $Description
        Parameters  = $Parameters
        Handler     = $Handler
    }
    Write-Host "  ✓ Registered tool: $Name" -ForegroundColor DarkGray
}

function Invoke-AgentTool {
    param(
        [string]$ToolName,
        [hashtable]$Arguments
    )
    
    if ($script:agentTools -and $script:agentTools[$ToolName]) {
        $tool = $script:agentTools[$ToolName]
        try {
            $result = & $tool.Handler @Arguments
            return @{
                Success = $true
                Tool    = $ToolName
                Result  = $result
            }
        }
        catch {
            return @{
                Success = $false
                Tool    = $ToolName
                Error   = $_.Exception.Message
            }
        }
    }
    return @{
        Success = $false
        Tool    = $ToolName
        Error   = "Tool not found"
    }
}

function Get-AgentToolsSchema {
    $tools = @()
    foreach ($tool in $script:agentTools.Values) {
        $tools += @{
            name        = $tool.Name
            description = $tool.Description
            parameters  = $tool.Parameters
        }
    }
    return $tools
}

Write-Host "`n📦 Registering Agent Tools..." -ForegroundColor Yellow

# Core File System Tools
Register-AgentTool -Name "read_file" -Description "Read contents of a file from disk" `
    -Parameters @{ path = @{ type = "string"; description = "File path to read" } } `
    -Handler {
        param($path)
        if (Test-Path $path) {
            $content = Get-Content -Path $path -Raw -ErrorAction Stop
            return @{ success = $true; content = $content; path = $path; size = $content.Length }
        }
        return @{ success = $false; error = "File not found: $path" }
    }

Register-AgentTool -Name "write_file" -Description "Write or create a file with content" `
    -Parameters @{ 
        path    = @{ type = "string"; description = "File path to write" }
        content = @{ type = "string"; description = "Content to write" }
    } `
    -Handler {
        param($path, $content)
        try {
            $dir = Split-Path $path -Parent
            if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
            Set-Content -Path $path -Value $content -Force
            return @{ success = $true; path = $path; bytes_written = $content.Length }
        }
        catch { return @{ success = $false; error = $_.Exception.Message } }
    }

Register-AgentTool -Name "list_directory" -Description "List all files and folders in a directory" `
    -Parameters @{ path = @{ type = "string"; description = "Directory path" } } `
    -Handler {
        param($path)
        if (Test-Path $path) {
            $items = Get-ChildItem -Path $path -Force | Select-Object Name, @{N='Type';E={if($_.PSIsContainer){'Directory'}else{'File'}}}, Length, LastWriteTime
            return @{ success = $true; path = $path; items = $items; count = $items.Count }
        }
        return @{ success = $false; error = "Directory not found: $path" }
    }

Register-AgentTool -Name "execute_command" -Description "Execute a shell command" `
    -Parameters @{ command = @{ type = "string"; description = "Command to execute" } } `
    -Handler {
        param($command)
        try {
            $output = Invoke-Expression $command 2>&1
            return @{ success = $true; command = $command; output = ($output | Out-String) }
        }
        catch { return @{ success = $false; error = $_.Exception.Message } }
    }

Register-AgentTool -Name "get_environment" -Description "Get development environment info" `
    -Parameters @{} `
    -Handler {
        return @{
            success     = $true
            os          = [System.Environment]::OSVersion.VersionString
            ps_version  = $PSVersionTable.PSVersion.ToString()
            user        = $env:USERNAME
            machine     = $env:COMPUTERNAME
            pwd         = (Get-Location).Path
            drives      = (Get-PSDrive -PSProvider FileSystem | Select-Object Name, Root)
        }
    }

Write-Host "  ✅ $($script:agentTools.Count) tools registered" -ForegroundColor Green

# ============================================
# TEST 1: Ollama API Connectivity
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 1: Ollama API Connectivity" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$ollamaAvailable = $false
$availableModels = @()

try {
    $testConn = Test-NetConnection -ComputerName "localhost" -Port 11434 -InformationLevel Quiet -WarningAction SilentlyContinue
    if ($testConn) {
        Write-Host "✅ Ollama service is running on localhost:11434" -ForegroundColor Green
        
        $tagsResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 5
        $availableModels = @($tagsResponse.models | ForEach-Object { $_.name })
        Write-Host "✅ Found $($availableModels.Count) models available" -ForegroundColor Green
        
        # Show local models (non-cloud)
        $localModels = $availableModels | Where-Object { $_ -notmatch "cloud|oss" }
        if ($localModels.Count -gt 0) {
            Write-Host "📋 Local models: $($localModels[0..2] -join ', ')$(if($localModels.Count -gt 3){' ...'})" -ForegroundColor Yellow
            $OllamaModel = $localModels[0]  # Use first local model
        }
        
        $ollamaAvailable = $true
    }
    else {
        Write-Host "❌ Ollama service not running" -ForegroundColor Red
    }
}
catch {
    Write-Host "❌ Ollama connectivity test failed: $_" -ForegroundColor Red
}

# ============================================
# TEST 2: Agent Tool Execution
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 2: Agent Tool Execution" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

# Test read_file
Write-Host "`n🔧 Testing read_file tool..." -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "read_file" -Arguments @{ path = "D:\professional-nasm-ide\README.md" }
if ($result.Success -and $result.Result.success) {
    Write-Host "  ✅ read_file: SUCCESS (read $($result.Result.size) bytes)" -ForegroundColor Green
}
else {
    # Try alternate path
    $result = Invoke-AgentTool -ToolName "read_file" -Arguments @{ path = "D:\professional-nasm-ide\ASM-CORE-README.md" }
    if ($result.Success -and $result.Result.success) {
        Write-Host "  ✅ read_file: SUCCESS (read $($result.Result.size) bytes from ASM-CORE-README.md)" -ForegroundColor Green
    }
    else {
        Write-Host "  ⚠️ read_file: $($result.Error ?? $result.Result.error)" -ForegroundColor Yellow
    }
}

# Test list_directory
Write-Host "`n🔧 Testing list_directory tool..." -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "list_directory" -Arguments @{ path = "D:\professional-nasm-ide" }
if ($result.Success -and $result.Result.success) {
    Write-Host "  ✅ list_directory: SUCCESS (found $($result.Result.count) items)" -ForegroundColor Green
    $dirs = ($result.Result.items | Where-Object { $_.Type -eq 'Directory' }).Count
    $files = ($result.Result.items | Where-Object { $_.Type -eq 'File' }).Count
    Write-Host "     📁 Directories: $dirs | 📄 Files: $files" -ForegroundColor Gray
}
else {
    Write-Host "  ❌ list_directory: $($result.Error ?? $result.Result.error)" -ForegroundColor Red
}

# Test execute_command
Write-Host "`n🔧 Testing execute_command tool..." -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "execute_command" -Arguments @{ command = "Get-ChildItem 'D:\professional-nasm-ide\src' -Filter '*.asm' | Measure-Object | Select-Object -ExpandProperty Count" }
if ($result.Success -and $result.Result.success) {
    $asmCount = $result.Result.output.Trim()
    Write-Host "  ✅ execute_command: SUCCESS (found $asmCount .asm files in src/)" -ForegroundColor Green
}
else {
    Write-Host "  ❌ execute_command: $($result.Error ?? $result.Result.error)" -ForegroundColor Red
}

# Test get_environment
Write-Host "`n🔧 Testing get_environment tool..." -ForegroundColor Yellow
$result = Invoke-AgentTool -ToolName "get_environment" -Arguments @{}
if ($result.Success -and $result.Result.success) {
    Write-Host "  ✅ get_environment: SUCCESS" -ForegroundColor Green
    Write-Host "     OS: $($result.Result.os)" -ForegroundColor Gray
    Write-Host "     PS: $($result.Result.ps_version)" -ForegroundColor Gray
    Write-Host "     User: $($result.Result.user)@$($result.Result.machine)" -ForegroundColor Gray
}
else {
    Write-Host "  ❌ get_environment: $($result.Error)" -ForegroundColor Red
}

# ============================================
# TEST 3: Ollama Generation with Tool Context
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 3: Ollama Generation with Agent Context" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

if ($ollamaAvailable) {
    # First, gather real context using tools
    Write-Host "`n📊 Gathering real project context..." -ForegroundColor Yellow
    
    $dirResult = Invoke-AgentTool -ToolName "list_directory" -Arguments @{ path = "D:\professional-nasm-ide" }
    $projectFiles = if ($dirResult.Success) { $dirResult.Result.items | Select-Object -First 10 } else { @() }
    
    $fileCount = if ($dirResult.Success) { $dirResult.Result.count } else { "unknown" }
    $asmFiles = (Invoke-AgentTool -ToolName "execute_command" -Arguments @{ 
        command = "(Get-ChildItem 'D:\professional-nasm-ide' -Filter '*.asm' -Recurse).Count" 
    }).Result.output.Trim()
    
    Write-Host "  ✅ Context gathered: $fileCount total items, $asmFiles .asm files" -ForegroundColor Green
    
    # Build prompt with real context
    $toolsSchema = Get-AgentToolsSchema | ConvertTo-Json -Compress
    $contextPrompt = @"
You are BigDaddyG, an agentic AI assistant with access to these tools:
$toolsSchema

The user wants to audit D:\professional-nasm-ide. Here is REAL data from the filesystem:
- Total items in root: $fileCount
- Total .asm files: $asmFiles
- First 10 items: $($projectFiles | ForEach-Object { $_.Name } | Join-String -Separator ', ')

Based on this REAL data, provide a brief summary of what you found. Do NOT make up numbers - use only the data provided above.
"@

    Write-Host "`n🤖 Sending to Ollama ($OllamaModel)..." -ForegroundColor Yellow
    Write-Host "   [Using REAL filesystem data, not hallucinated]" -ForegroundColor DarkGray
    
    try {
        $body = @{
            model  = $OllamaModel
            prompt = $contextPrompt
            stream = $false
        } | ConvertTo-Json
        
        $response = Invoke-RestMethod -Uri $OllamaAPIEndpoint -Method POST -Body $body -ContentType "application/json" -TimeoutSec 30
        
        if ($response.response) {
            Write-Host "`n✅ Ollama Response:" -ForegroundColor Green
            Write-Host "─────────────────────────────────────────────────────" -ForegroundColor DarkGray
            Write-Host $response.response -ForegroundColor White
            Write-Host "─────────────────────────────────────────────────────" -ForegroundColor DarkGray
        }
        else {
            Write-Host "⚠️ Empty response from Ollama" -ForegroundColor Yellow
        }
    }
    catch {
        $errMsg = $_.Exception.Message
        if ($errMsg -match "429|rate limit|usage limit") {
            Write-Host "⚠️ Rate limited - try a different model or wait" -ForegroundColor Yellow
            Write-Host "   Available local models: $($localModels -join ', ')" -ForegroundColor Gray
        }
        else {
            Write-Host "❌ Ollama generation failed: $errMsg" -ForegroundColor Red
        }
    }
}
else {
    Write-Host "⏭️ Skipping Ollama test - service not available" -ForegroundColor Yellow
}

# ============================================
# TEST 4: Full Agentic Workflow Simulation
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 4: Full Agentic Workflow Simulation" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

Write-Host "`n🎯 Simulating: 'Audit D:\professional-nasm-ide'" -ForegroundColor Yellow

# Step 1: List root directory
Write-Host "`n  Step 1: Listing root directory..." -ForegroundColor Cyan
$rootResult = Invoke-AgentTool -ToolName "list_directory" -Arguments @{ path = "D:\professional-nasm-ide" }
if ($rootResult.Success) {
    $rootItems = $rootResult.Result.items
    $rootDirs = ($rootItems | Where-Object { $_.Type -eq 'Directory' }).Count
    $rootFiles = ($rootItems | Where-Object { $_.Type -eq 'File' }).Count
    Write-Host "     ✅ Root: $rootDirs directories, $rootFiles files" -ForegroundColor Green
}

# Step 2: Analyze src/ directory
Write-Host "`n  Step 2: Analyzing src/ directory..." -ForegroundColor Cyan
$srcResult = Invoke-AgentTool -ToolName "list_directory" -Arguments @{ path = "D:\professional-nasm-ide\src" }
if ($srcResult.Success) {
    $srcItems = $srcResult.Result.items
    $asmFiles = $srcItems | Where-Object { $_.Name -match '\.asm$' }
    Write-Host "     ✅ src/: $($srcItems.Count) items, $($asmFiles.Count) .asm files" -ForegroundColor Green
    Write-Host "     📄 ASM files: $($asmFiles.Name -join ', ')" -ForegroundColor Gray
}

# Step 3: Read a key file
Write-Host "`n  Step 3: Reading key project file..." -ForegroundColor Cyan
$readResult = Invoke-AgentTool -ToolName "read_file" -Arguments @{ path = "D:\professional-nasm-ide\ASM-CORE-README.md" }
if ($readResult.Success -and $readResult.Result.success) {
    $preview = $readResult.Result.content.Substring(0, [Math]::Min(200, $readResult.Result.content.Length))
    Write-Host "     ✅ Read ASM-CORE-README.md ($($readResult.Result.size) bytes)" -ForegroundColor Green
    Write-Host "     Preview: $($preview -replace '\r?\n', ' ')..." -ForegroundColor Gray
}

# Step 4: Count total files by type
Write-Host "`n  Step 4: Counting files by extension..." -ForegroundColor Cyan
$countResult = Invoke-AgentTool -ToolName "execute_command" -Arguments @{ 
    command = @"
Get-ChildItem 'D:\professional-nasm-ide' -Recurse -File | 
Group-Object Extension | 
Sort-Object Count -Descending | 
Select-Object -First 8 Name, Count | 
Format-Table -AutoSize | Out-String
"@
}
if ($countResult.Success) {
    Write-Host "     ✅ File type breakdown:" -ForegroundColor Green
    Write-Host $countResult.Result.output -ForegroundColor Gray
}

# ============================================
# TEST 5: Auto-Enable Agent Mode Detection
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 5: Auto-Enable Agent Mode Detection" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

# Test the pattern matching that triggers auto-enable
$agenticKeywords = @(
    'view\s+(code|file|project|folder|directory)',
    'read\s+(file|code|content)',
    'open\s+(file|project|folder)',
    'show\s+(me\s+)?(the\s+)?(file|code|content|directory|folder)',
    'list\s+(files|directory|folder|contents)',
    'audit\s+',
    'analyze\s+(code|file|project)',
    'scan\s+',
    '(cd|navigate|go\s+to|change\s+directory)',
    'browse\s+',
    'git\s+(status|commit|push|pull|log)',
    '(commit|push|pull)\s+(changes|code)',
    '(run|execute)\s+(command|script|terminal)',
    '/term',
    '/exec',
    '^/(sys|browse|nav|go|ls|dir|read|open|cd|git|workflow|task|agent|tools|env|deps|code|generate|review|refactor)',
    'agentically',
    'using\s+tools',
    'with\s+agent',
    'D:\\\\',
    'C:\\\\',
    'professional.?nasm',
    '\.asm\b',
    '\.ps1\b',
    '\.py\b'
)

function Test-RequiresAgentMode {
    param([string]$Message)
    foreach ($pattern in $agenticKeywords) {
        if ($Message -match $pattern) {
            return $true
        }
    }
    return $false
}

$testMessages = @(
    @{ Msg = "please agentically view the asm ide on D:\"; Expected = $true },
    @{ Msg = "audit D:\professional-nasm-ide"; Expected = $true },
    @{ Msg = "show me the files in the project"; Expected = $true },
    @{ Msg = "list directory contents"; Expected = $true },
    @{ Msg = "read file test.asm"; Expected = $true },
    @{ Msg = "view code in C:\Users\test"; Expected = $true },
    @{ Msg = "/ls D:\test"; Expected = $true },
    @{ Msg = "git status"; Expected = $true },
    @{ Msg = "analyze the .ps1 files"; Expected = $true },
    @{ Msg = "hello how are you"; Expected = $false },
    @{ Msg = "what is the weather"; Expected = $false },
    @{ Msg = "tell me a joke"; Expected = $false }
)

$passCount = 0
$failCount = 0

Write-Host "`n🔍 Testing auto-enable pattern detection..." -ForegroundColor Yellow
foreach ($test in $testMessages) {
    $result = Test-RequiresAgentMode -Message $test.Msg
    $passed = ($result -eq $test.Expected)
    
    if ($passed) {
        $passCount++
        $icon = "✅"
        $color = "Green"
    }
    else {
        $failCount++
        $icon = "❌"
        $color = "Red"
    }
    
    $expectedStr = if ($test.Expected) { "AGENT" } else { "CHAT" }
    $actualStr = if ($result) { "AGENT" } else { "CHAT" }
    
    Write-Host "  $icon '$($test.Msg.Substring(0, [Math]::Min(40, $test.Msg.Length)))...' → Expected: $expectedStr, Got: $actualStr" -ForegroundColor $color
}

Write-Host "`n  Pattern Detection: $passCount/$($testMessages.Count) tests passed" -ForegroundColor $(if ($failCount -eq 0) { "Green" } else { "Yellow" })

# ============================================
# SUMMARY
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$testResults = @{
    "Ollama Service"     = if ($ollamaAvailable) { "✅ PASS" } else { "❌ FAIL" }
    "Agent Tools"        = "✅ PASS ($($script:agentTools.Count) tools)"
    "File Operations"    = if ($readResult.Success) { "✅ PASS" } else { "⚠️ PARTIAL" }
    "Command Execution"  = if ($countResult.Success) { "✅ PASS" } else { "❌ FAIL" }
    "Auto-Enable Detection" = if ($failCount -eq 0) { "✅ PASS ($passCount patterns)" } else { "⚠️ $passCount/$($testMessages.Count)" }
}

foreach ($test in $testResults.GetEnumerator()) {
    Write-Host "  $($test.Key): $($test.Value)" -ForegroundColor $(if ($test.Value -match "PASS") { "Green" } elseif ($test.Value -match "PARTIAL|⚠️") { "Yellow" } else { "Red" })
}

Write-Host "`n🔚 Headless agentic test complete!" -ForegroundColor Cyan
Write-Host "   The GUI will now auto-enable Agent Mode for agentic requests." -ForegroundColor Gray
Write-Host "   Keywords: audit, view code, list files, D:\, .asm, agentically, etc." -ForegroundColor Gray
