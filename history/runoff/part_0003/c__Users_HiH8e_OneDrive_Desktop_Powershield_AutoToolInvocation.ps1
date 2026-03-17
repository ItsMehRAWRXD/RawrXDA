# ============================================
# AUTO TOOL INVOCATION SYSTEM
# ============================================
# This module adds intelligent tool detection and auto-invocation capabilities
# to the RawrXD IDE. It analyzes user messages and automatically calls the
# appropriate built-in tools without requiring manual /execute_tool commands.

# Track auto-tool execution for this session
if (-not $script:autoToolStats) {
    $script:autoToolStats = @{
        TotalInvocations = 0
        SuccessfulTools = 0
        FailedTools = 0
        ToolUsage = @{}
    }
}

function Get-IntentBasedToolCalls {
    <#
    .SYNOPSIS
        Automatically detects which tools should be invoked based on user intent
    .DESCRIPTION
        Analyzes natural language user messages and determines which built-in
        tools should be called, along with their parameters extracted from context.
    .PARAMETER UserMessage
        The natural language message from the user
    .PARAMETER CurrentWorkingDir
        Current working directory for resolving relative paths
    .OUTPUTS
        Array of tool calls with name and parameters
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$UserMessage,
        
        [Parameter(Mandatory = $false)]
        [string]$CurrentWorkingDir = $global:currentWorkingDir
    )
    
    if (-not $CurrentWorkingDir) {
        $CurrentWorkingDir = $PWD.Path
    }
    
    $toolCalls = @()
    $msgLower = $UserMessage.ToLower()
    
    # ============================================
    # FILE CREATION PATTERNS
    # ============================================
    if ($msgLower -match 'create\s+(a\s+)?(new\s+)?file\s+(?:called\s+|named\s+)?[''"]?([^''"]+?)[''"]?(?:\s|$)' -or
        $msgLower -match 'make\s+(a\s+)?(?:new\s+)?file\s+(?:called\s+|named\s+)?[''"]?([^''"]+?)[''"]?(?:\s|$)') {
        
        $fileName = $Matches[3] -replace '[''"]', ''
        $fileName = $fileName.Trim()
        
        # Resolve path
        $fullPath = if ([System.IO.Path]::IsPathRooted($fileName)) {
            $fileName
        } else {
            Join-Path $CurrentWorkingDir $fileName
        }
        
        # Extract content if provided
        $content = ""
        if ($msgLower -match 'with\s+content[:\s]+[''"](.+?)[''"]' -or
            $msgLower -match 'containing[:\s]+[''"](.+?)[''"]') {
            $content = $Matches[1]
        }
        
        $toolCalls += @{
            tool = "create_file"
            params = @{
                path = $fullPath
                content = $content
            }
            confidence = 0.95
        }
    }
    
    # ============================================
    # DIRECTORY CREATION PATTERNS
    # ============================================
    if ($msgLower -match 'create\s+(a\s+)?(new\s+)?(directory|folder|dir)\s+(?:called\s+|named\s+)?[''"]?([^''"]+?)[''"]?(?:\s|$)' -or
        $msgLower -match 'make\s+(a\s+)?(?:new\s+)?(directory|folder|dir)\s+(?:called\s+|named\s+)?[''"]?([^''"]+?)[''"]?(?:\s|$)' -or
        $msgLower -match 'mkdir\s+[''"]?([^''"]+?)[''"]?(?:\s|$)') {
        
        $dirName = if ($Matches[4]) { $Matches[4] } else { $Matches[1] }
        $dirName = $dirName -replace '[''"]', '' | Select-Object -First 1
        $dirName = $dirName.Trim()
        
        $fullPath = if ([System.IO.Path]::IsPathRooted($dirName)) {
            $dirName
        } else {
            Join-Path $CurrentWorkingDir $dirName
        }
        
        $toolCalls += @{
            tool = "create_directory"
            params = @{
                path = $fullPath
            }
            confidence = 0.95
        }
    }
    
    # ============================================
    # FILE READING PATTERNS
    # ============================================
    if ($msgLower -match '(?:read|show|display|open|view)\s+(?:the\s+)?(?:file|code|content)?\s*[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?' -or
        $msgLower -match 'what(?:''s|\s+is)\s+in\s+(?:the\s+)?(?:file\s+)?[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?') {
        
        $filePath = $Matches[1] -replace '[''"]', ''
        $filePath = $filePath.Trim()
        
        # Resolve relative paths
        if (-not [System.IO.Path]::IsPathRooted($filePath)) {
            $filePath = Join-Path $CurrentWorkingDir $filePath
        }
        
        # Detect line range if specified
        $startLine = 1
        $endLine = -1  # -1 means read all
        
        if ($msgLower -match 'lines?\s+(\d+)\s*-\s*(\d+)') {
            $startLine = [int]$Matches[1]
            $endLine = [int]$Matches[2]
        }
        elseif ($msgLower -match 'line\s+(\d+)') {
            $startLine = [int]$Matches[1]
            $endLine = $startLine
        }
        
        $toolCalls += @{
            tool = "read_file"
            params = @{
                path = $filePath
                startLine = $startLine
                endLine = $endLine
            }
            confidence = 0.92
        }
    }
    
    # ============================================
    # DIRECTORY LISTING PATTERNS
    # ============================================
    if ($msgLower -match '(?:list|show|display)\s+(?:the\s+)?(?:files?|contents?|directory)\s+(?:in\s+|of\s+)?[''"]?([a-z]:\\[^''"]+|[./][^''"]*)?[''"]?' -or
        $msgLower -match '(?:ls|dir)\s+[''"]?([a-z]:\\[^''"]+|[./][^''"]*)?[''"]?' -or
        $msgLower -match 'what(?:''s|\s+is)\s+in\s+(?:the\s+)?(?:directory|folder)\s+[''"]?([a-z]:\\[^''"]+|[./][^''"]*)?[''"]?') {
        
        $dirPath = if ($Matches[1]) { $Matches[1] -replace '[''"]', '' } else { $CurrentWorkingDir }
        $dirPath = $dirPath.Trim()
        
        if (-not [System.IO.Path]::IsPathRooted($dirPath)) {
            $dirPath = Join-Path $CurrentWorkingDir $dirPath
        }
        
        $toolCalls += @{
            tool = "list_directory"
            params = @{
                path = $dirPath
            }
            confidence = 0.90
        }
    }
    
    # ============================================
    # FILE EDITING PATTERNS
    # ============================================
    if ($msgLower -match 'edit\s+(?:the\s+)?(?:file\s+)?[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?' -or
        $msgLower -match 'modify\s+(?:the\s+)?(?:file\s+)?[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?' -or
        $msgLower -match 'change\s+(?:in\s+)?(?:file\s+)?[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?') {
        
        $filePath = $Matches[1] -replace '[''"]', ''
        $filePath = $filePath.Trim()
        
        if (-not [System.IO.Path]::IsPathRooted($filePath)) {
            $filePath = Join-Path $CurrentWorkingDir $filePath
        }
        
        # Try to extract what to replace and what to replace with
        $oldText = ""
        $newText = ""
        
        if ($msgLower -match 'replace\s+[''"](.+?)[''"]\s+with\s+[''"](.+?)[''"]') {
            $oldText = $Matches[1]
            $newText = $Matches[2]
        }
        elseif ($msgLower -match 'change\s+[''"](.+?)[''"]\s+to\s+[''"](.+?)[''"]') {
            $oldText = $Matches[1]
            $newText = $Matches[2]
        }
        
        if ($oldText -and $newText) {
            $toolCalls += @{
                tool = "edit_file"
                params = @{
                    path = $filePath
                    oldText = $oldText
                    newText = $newText
                }
                confidence = 0.88
            }
        }
    }
    
    # ============================================
    # TERMINAL/COMMAND EXECUTION PATTERNS
    # ============================================
    if ($msgLower -match '(?:run|execute)\s+(?:the\s+)?(?:command|script)?\s*[''"](.+?)[''"]' -or
        $msgLower -match '/exec\s+(.+)$' -or
        $msgLower -match '/term\s+(.+)$') {
        
        $command = $Matches[1].Trim()
        
        $toolCalls += @{
            tool = "run_terminal"
            params = @{
                command = $command
                workingDir = $CurrentWorkingDir
            }
            confidence = 0.93
        }
    }
    
    # ============================================
    # SEARCH PATTERNS
    # ============================================
    if ($msgLower -match '(?:search|find|grep)\s+(?:for\s+)?[''"](.+?)[''"](?:\s+in\s+(.+))?' -or
        $msgLower -match 'where\s+(?:is|are)\s+[''"](.+?)[''"]') {
        
        $searchTerm = $Matches[1]
        $searchPath = if ($Matches[2]) { 
            $Matches[2] -replace '[''"]', ''
        } else { 
            $CurrentWorkingDir 
        }
        
        if (-not [System.IO.Path]::IsPathRooted($searchPath)) {
            $searchPath = Join-Path $CurrentWorkingDir $searchPath
        }
        
        $toolCalls += @{
            tool = "search_files"
            params = @{
                query = $searchTerm
                path = $searchPath
            }
            confidence = 0.85
        }
    }
    
    # ============================================
    # FILE/DIRECTORY INFO PATTERNS
    # ============================================
    if ($msgLower -match 'what(?:''s|\s+is)\s+(?:the\s+)?(?:size|info|information)\s+of\s+[''"]?([^''"]+?)[''"]?' -or
        $msgLower -match 'show\s+(?:me\s+)?(?:info|information|details)\s+(?:about\s+|for\s+)?[''"]?([^''"]+?)[''"]?') {
        
        $targetPath = $Matches[1] -replace '[''"]', ''
        $targetPath = $targetPath.Trim()
        
        if (-not [System.IO.Path]::IsPathRooted($targetPath)) {
            $targetPath = Join-Path $CurrentWorkingDir $targetPath
        }
        
        $toolCalls += @{
            tool = "get_file_info"
            params = @{
                path = $targetPath
            }
            confidence = 0.87
        }
    }
    
    # ============================================
    # GIT STATUS PATTERNS
    # ============================================
    if ($msgLower -match '(?:git\s+)?(?:show|check|get)\s+(?:git\s+)?status' -or
        $msgLower -match 'what(?:''s|\s+is)\s+(?:the\s+)?git\s+status' -or
        $msgLower -match 'show\s+(?:me\s+)?(?:the\s+)?repo(?:sitory)?\s+status' -or
        $msgLower -match '/git\s+status') {
        
        $toolCalls += @{
            tool = "git_status"
            params = @{
                path = $CurrentWorkingDir
            }
            confidence = 0.92
        }
    }
    
    # ============================================
    # DELETE FILE PATTERNS
    # ============================================
    if ($msgLower -match 'delete\s+(?:the\s+)?(?:file\s+)?[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?' -or
        $msgLower -match 'remove\s+(?:the\s+)?(?:file\s+)?[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?' -or
        $msgLower -match 'rm\s+[''"]?([^''"]+?)[''"]?(?:\s|$)') {
        
        $filePath = $Matches[1] -replace '[''"]', ''
        $filePath = $filePath.Trim()
        
        if (-not [System.IO.Path]::IsPathRooted($filePath)) {
            $filePath = Join-Path $CurrentWorkingDir $filePath
        }
        
        $toolCalls += @{
            tool = "delete_file"
            params = @{
                path = $filePath
            }
            confidence = 0.85
        }
    }
    
    # ============================================
    # WRITE/SAVE FILE PATTERNS
    # ============================================
    if ($msgLower -match 'write\s+[''"](.+?)[''"]\s+to\s+(?:file\s+)?[''"]?([^''"]+?)[''"]?(?:\s|$)' -or
        $msgLower -match 'save\s+[''"](.+?)[''"]\s+(?:to|as)\s+[''"]?([^''"]+?)[''"]?(?:\s|$)') {
        
        $content = $Matches[1]
        $filePath = $Matches[2] -replace '[''"]', ''
        $filePath = $filePath.Trim()
        
        if (-not [System.IO.Path]::IsPathRooted($filePath)) {
            $filePath = Join-Path $CurrentWorkingDir $filePath
        }
        
        $toolCalls += @{
            tool = "write_file"
            params = @{
                path = $filePath
                content = $content
            }
            confidence = 0.90
        }
    }
    
    # ============================================
    # BROWSE/OPEN URL PATTERNS
    # ============================================
    if ($msgLower -match '(?:open|browse|go\s+to|navigate\s+to)\s+(?:url\s+)?[''"]?(https?://[^''"]+?)[''"]?(?:\s|$)' -or
        $msgLower -match 'search\s+(?:for\s+)?[''"](.+?)[''"]\s+(?:on\s+)?google') {
        
        $url = if ($Matches[1] -match '^https?://') { 
            $Matches[1] 
        } else { 
            "https://www.google.com/search?q=$([uri]::EscapeDataString($Matches[1]))" 
        }
        
        $toolCalls += @{
            tool = "browse_url"
            params = @{
                url = $url
            }
            confidence = 0.88
        }
    }
    
    # ============================================
    # ENVIRONMENT INFO PATTERNS
    # ============================================
    if ($msgLower -match '(?:show|get|what(?:''s|\s+is))\s+(?:the\s+)?(?:environment|env|system)\s+(?:info|information|details)?' -or
        $msgLower -match 'what(?:''s|\s+is)\s+(?:my\s+)?(?:powershell|ps)\s+version') {
        
        $toolCalls += @{
            tool = "get_environment"
            params = @{}
            confidence = 0.85
        }
    }
    
    # ============================================
    # TODO/TASK PATTERNS
    # ============================================
    if ($msgLower -match '(?:list|show)\s+(?:all\s+)?(?:the\s+)?todos?' -or
        $msgLower -match 'what(?:''s|\s+are)\s+(?:the\s+)?(?:remaining\s+)?todos?') {
        
        $toolCalls += @{
            tool = "list_todos"
            params = @{
                directory = $CurrentWorkingDir
            }
            confidence = 0.85
        }
    }
    
    # ============================================
    # PROJECT STRUCTURE PATTERNS
    # ============================================
    if ($msgLower -match '(?:show|display|get)\s+(?:the\s+)?project\s+(?:structure|tree|layout)' -or
        $msgLower -match 'what(?:''s|\s+is)\s+(?:the\s+)?(?:project|folder)\s+(?:structure|tree)') {
        
        $toolCalls += @{
            tool = "get_project_structure"
            params = @{
                path = $CurrentWorkingDir
                depth = 3
            }
            confidence = 0.88
        }
    }
    
    # ============================================
    # ANALYZE CODE PATTERNS
    # ============================================
    if ($msgLower -match '(?:analyze|check|lint)\s+(?:the\s+)?(?:code|file)\s+[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?' -or
        $msgLower -match 'find\s+(?:errors|issues|problems)\s+in\s+[''"]?([a-z]:\\[^''"]+|[./][^''"]+?)[''"]?') {
        
        $filePath = $Matches[1] -replace '[''"]', ''
        $filePath = $filePath.Trim()
        
        if (-not [System.IO.Path]::IsPathRooted($filePath)) {
            $filePath = Join-Path $CurrentWorkingDir $filePath
        }
        
        $toolCalls += @{
            tool = "analyze_code_errors"
            params = @{
                path = $filePath
            }
            confidence = 0.85
        }
    }
    
    return $toolCalls
}

function Invoke-AutoToolCalling {
    <#
    .SYNOPSIS
        Automatically detects and invokes tools based on user message
    .DESCRIPTION
        Main entry point for auto tool invocation. Analyzes user intent,
        detects appropriate tools, executes them, and returns enhanced
        context for the AI response.
    .PARAMETER UserMessage
        The user's natural language message
    .PARAMETER CurrentWorkingDir
        Current working directory
    .OUTPUTS
        Hashtable with executedTools, results, and enhancedPrompt
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$UserMessage,
        
        [Parameter(Mandatory = $false)]
        [string]$CurrentWorkingDir = $global:currentWorkingDir,
        
        [Parameter(Mandatory = $false)]
        [double]$ConfidenceThreshold = 0.80
    )
    
    $detectedTools = Get-IntentBasedToolCalls -UserMessage $UserMessage -CurrentWorkingDir $CurrentWorkingDir
    
    # Filter by confidence
    $toolsToExecute = $detectedTools | Where-Object { $_.confidence -ge $ConfidenceThreshold }
    
    if ($toolsToExecute.Count -eq 0) {
        return @{
            executedTools = @()
            results = @()
            enhancedPrompt = $UserMessage
            autoInvoked = $false
        }
    }
    
    $results = @()
    $contextAdditions = @()
    
    foreach ($toolCall in $toolsToExecute) {
        try {
            Write-DevConsole "[AUTO-TOOL] Invoking $($toolCall.tool) (confidence: $($toolCall.confidence))" "INFO"
            
            $result = Invoke-AgentTool -ToolName $toolCall.tool -Parameters $toolCall.params
            
            $results += @{
                tool = $toolCall.tool
                params = $toolCall.params
                result = $result
                success = $result.success
            }
            
            # Add result to context
            if ($result.success) {
                $contextAdditions += "`n[AUTO-EXECUTED: $($toolCall.tool)]"
                $resultSummary = $result | ConvertTo-Json -Depth 3 -Compress
                $contextAdditions += "Result: $($resultSummary.Substring(0, [Math]::Min(500, $resultSummary.Length)))"
            }
            
            Write-DevConsole "[AUTO-TOOL] $($toolCall.tool) completed: success=$($result.success)" "SUCCESS"
        }
        catch {
            Write-DevConsole "[AUTO-TOOL] $($toolCall.tool) failed: $_" "ERROR"
            $results += @{
                tool = $toolCall.tool
                params = $toolCall.params
                error = $_.Exception.Message
                success = $false
            }
        }
    }
    
    # Enhance the prompt with tool results
    $enhancedPrompt = $UserMessage
    if ($contextAdditions.Count -gt 0) {
        $enhancedPrompt += "`n`n" + ($contextAdditions -join "`n")
    }
    
    return @{
        executedTools = $toolsToExecute
        results = $results
        enhancedPrompt = $enhancedPrompt
        autoInvoked = $true
    }
    
    # Update stats
    $script:autoToolStats.TotalInvocations++
    foreach ($result in $results) {
        if ($result.success) {
            $script:autoToolStats.SuccessfulTools++
        } else {
            $script:autoToolStats.FailedTools++
        }
        
        if (-not $script:autoToolStats.ToolUsage[$result.tool]) {
            $script:autoToolStats.ToolUsage[$result.tool] = 0
        }
        $script:autoToolStats.ToolUsage[$result.tool]++
    }
}

function Test-RequiresAutoTooling {
    <#
    .SYNOPSIS
        Quick test to determine if a message likely needs auto-tooling
    .DESCRIPTION
        Performs a fast regex check to see if the message contains
        patterns that suggest tool invocation would be beneficial.
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$UserMessage
    )
    
    $msgLower = $UserMessage.ToLower()
    
    $autoToolPatterns = @(
        # File operations
        'create\s+(a\s+)?(new\s+)?(file|directory|folder)',
        '(read|show|display|view|open)\s+(?:the\s+)?(?:file|code)',
        '(list|show)\s+(?:the\s+)?(?:files|directory|contents)',
        '(edit|modify|change)\s+(?:the\s+)?file',
        '(delete|remove)\s+(?:the\s+)?(?:file|folder)',
        '(write|save)\s+.+?\s+(to|as)\s+',
        
        # Terminal/commands
        '(run|execute)\s+(?:the\s+)?(?:command|script)',
        '/exec\s+',
        '/term\s+',
        
        # Search
        '(search|find|grep)\s+(?:for\s+)?',
        
        # Directory navigation
        'mkdir\s+',
        '\bls\b',
        '\bdir\b',
        '\brm\s+',
        
        # Git
        'git\s+status',
        'repo(?:sitory)?\s+status',
        
        # Browser
        '(open|browse|navigate)\s+.+?(url|http)',
        'search\s+.+?\s+google',
        
        # Environment/info
        'environment\s+info',
        'system\s+info',
        'powershell\s+version',
        
        # Todos/tasks
        '(list|show)\s+.+?todos?',
        
        # Project structure
        'project\s+(structure|tree|layout)',
        'folder\s+(structure|tree)',
        
        # Code analysis
        '(analyze|check|lint)\s+.+?(code|file)',
        'find\s+(errors|issues|problems)'
    )
    
    foreach ($pattern in $autoToolPatterns) {
        if ($msgLower -match $pattern) {
            return $true
        }
    }
    
    return $false
}

# Log successful loading (only if Write-StartupLog is available)
if (Get-Command Write-StartupLog -ErrorAction SilentlyContinue) {
    Write-StartupLog "✅ Auto Tool Invocation module loaded" "SUCCESS"
}

function Get-AutoToolStats {
    <#
    .SYNOPSIS
        Get statistics about auto-tool invocations
    .DESCRIPTION
        Returns a summary of auto-tool usage during the current session
    #>
    return $script:autoToolStats
}

function Reset-AutoToolStats {
    <#
    .SYNOPSIS
        Reset auto-tool statistics
    #>
    $script:autoToolStats = @{
        TotalInvocations = 0
        SuccessfulTools = 0
        FailedTools = 0
        ToolUsage = @{}
    }
}

function Get-AvailableAutoTools {
    <#
    .SYNOPSIS
        Get list of tools that can be auto-invoked
    #>
    return @(
        @{ Name = "create_file"; Description = "Create a new file"; Patterns = @("create file", "make file", "new file") }
        @{ Name = "create_directory"; Description = "Create a directory"; Patterns = @("create directory", "mkdir", "create folder") }
        @{ Name = "read_file"; Description = "Read file contents"; Patterns = @("read file", "show file", "open file", "view file") }
        @{ Name = "list_directory"; Description = "List directory contents"; Patterns = @("list files", "ls", "dir", "show directory") }
        @{ Name = "edit_file"; Description = "Edit file contents"; Patterns = @("edit file", "modify file", "replace in file") }
        @{ Name = "delete_file"; Description = "Delete a file"; Patterns = @("delete file", "remove file", "rm") }
        @{ Name = "write_file"; Description = "Write content to file"; Patterns = @("write to file", "save to file") }
        @{ Name = "run_terminal"; Description = "Run terminal command"; Patterns = @("run command", "execute", "/exec", "/term") }
        @{ Name = "search_files"; Description = "Search for text in files"; Patterns = @("search for", "find", "grep") }
        @{ Name = "get_file_info"; Description = "Get file information"; Patterns = @("file info", "file size", "file details") }
        @{ Name = "git_status"; Description = "Get git repository status"; Patterns = @("git status", "repo status") }
        @{ Name = "browse_url"; Description = "Open URL in browser"; Patterns = @("open url", "browse", "navigate to") }
        @{ Name = "get_environment"; Description = "Get environment info"; Patterns = @("environment info", "system info", "ps version") }
        @{ Name = "list_todos"; Description = "List TODO items"; Patterns = @("list todos", "show todos") }
        @{ Name = "get_project_structure"; Description = "Show project structure"; Patterns = @("project structure", "project tree") }
        @{ Name = "analyze_code_errors"; Description = "Analyze code for errors"; Patterns = @("analyze code", "check code", "find errors") }
    )
}

# Export functions for external use
$script:AutoToolInvocationLoaded = $true
