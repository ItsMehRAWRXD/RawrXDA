
# Cache for function results
$script:FunctionCache = @{}

function Get-FromCache {
    param([string]$Key)
    if ($script:FunctionCache.ContainsKey($Key)) {
        return $script:FunctionCache[$Key]
    }
    return $null
}

function Set-Cache {
    param([string]$Key, $Value)
    $script:FunctionCache[$Key] = $Value
}# RawrXD Agentic Commands Module
# Production-ready agentic command execution and automation

#Requires -Version 5.1

<#
.SYNOPSIS
    RawrXD.AgenticCommands - Agentic command execution and automation system

.DESCRIPTION
    Comprehensive agentic command system providing:
    - Terminal/shell command execution
    - File browser operations
    - Git integration
    - AI chat commands
    - Natural language processing
    - Command validation and security
    - Comprehensive logging and auditing

.LINK
    https://github.com/RawrXD/AgenticCommands

.NOTES
    Author: RawrXD Auto-Generation System
    Version: 1.0.0
    Requires: PowerShell 5.1+
    Last Updated: 2024-12-28
#>

# Import logging if available
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [string]$Function = $null,
            [hashtable]$Data = $null
        )
        $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        $caller = if ($Function) { $Function } else { (Get-PSCallStack)[1].FunctionName }
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$timestamp][$caller][$Level] $Message" -ForegroundColor $color
    }
}

# Agentic command registry
$script:AgenticCommandRegistry = @{
    TerminalCommands = @{
        '/term' = @{ Description = 'Execute terminal command'; Handler = 'Invoke-TerminalCommand'; Example = '/term ls' }
        '/exec' = @{ Description = 'Execute PowerShell command'; Handler = 'Invoke-PowerShellCommand'; Example = '/exec Get-ChildItem' }
        '/terminal' = @{ Description = 'Execute any terminal command'; Handler = 'Invoke-TerminalCommand'; Example = '/terminal whoami' }
    }
    FileBrowserCommands = @{
        '/ls' = @{ Description = 'List directory contents'; Handler = 'Invoke-FileListCommand'; Example = '/ls C:\Users' }
        '/cd' = @{ Description = 'Change directory'; Handler = 'Invoke-ChangeDirectoryCommand'; Example = '/cd C:\temp' }
        '/pwd' = @{ Description = 'Show current directory'; Handler = 'Invoke-ShowDirectoryCommand'; Example = '/pwd' }
        '/mkdir' = @{ Description = 'Create new folder'; Handler = 'Invoke-MakeDirectoryCommand'; Example = '/mkdir testfolder' }
        '/touch' = @{ Description = 'Create new file'; Handler = 'Invoke-TouchFileCommand'; Example = '/touch newfile.txt' }
        '/rm' = @{ Description = 'Delete file/folder'; Handler = 'Invoke-RemoveItemCommand'; Example = '/rm oldfile.txt' }
        '/open' = @{ Description = 'Open file in editor'; Handler = 'Invoke-OpenFileCommand'; Example = '/open file.txt' }
        '/write' = @{ Description = 'Write content to file'; Handler = 'Invoke-WriteFileCommand'; Example = '/write file.txt content' }
    }
    GitCommands = @{
        '/git status' = @{ Description = 'Git status'; Handler = 'Invoke-GitStatusCommand'; Example = '/git status' }
        '/git commit' = @{ Description = 'Stage & commit all'; Handler = 'Invoke-GitCommitCommand'; Example = '/git commit' }
        '/git branch' = @{ Description = 'List branches'; Handler = 'Invoke-GitBranchCommand'; Example = '/git branch' }
        '/git log' = @{ Description = 'Show git log'; Handler = 'Invoke-GitLogCommand'; Example = '/git log' }
    }
    AIChatCommands = @{
        'explain' = @{ Description = 'Explain code'; Handler = 'Invoke-ExplainCodeCommand'; Example = 'explain this code' }
        'analyze' = @{ Description = 'Analyze file'; Handler = 'Invoke-AnalyzeFileCommand'; Example = 'analyze file.ps1' }
    }
}

function Invoke-TerminalCommand {
    <#
    .SYNOPSIS
        Execute terminal/shell command
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Command,
        
        [Parameter(Mandatory=$false)]
        [string]$WorkingDirectory = $PWD
    )
    
    $functionName = 'Invoke-TerminalCommand'
    
    try {
        Write-StructuredLog -Message "Executing terminal command: $Command" -Level Info -Function $functionName -Data @{
            Command = $Command
            WorkingDirectory = $WorkingDirectory
        }
        
        # Validate command for security
        $blockedCommands = @('format', 'del', 'rd', 'rm -rf', 'shutdown', 'reboot')
        foreach ($blocked in $blockedCommands) {
            if ($Command -match "^\s*$blocked\s") {
                throw "Command '$blocked' is blocked for security reasons"
            }
        }
        
        # Execute command
        $output = switch -Regex ($Command) {
            '^ls\b' { 
                $path = if ($Command -match '^ls\s+(.+)$') { $matches[1] } else { $WorkingDirectory }
                Get-ChildItem -Path $path -ErrorAction Stop | Format-Table -AutoSize | Out-String
            }
            '^dir\b' { 
                $path = if ($Command -match '^dir\s+(.+)$') { $matches[1] } else { $WorkingDirectory }
                Get-ChildItem -Path $path -ErrorAction Stop | Format-Table -AutoSize | Out-String
            }
            default { 
                $result = Invoke-Expression $Command 2>&1
                if ($result -is [System.Array]) { $result | Out-String } else { $result.ToString() }
            }
        }
        
        Write-StructuredLog -Message "Command executed successfully" -Level Info -Function $functionName
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error executing terminal command: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-PowerShellCommand {
    <#
    .SYNOPSIS
        Execute PowerShell command
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Command,
        
        [Parameter(Mandatory=$false)]
        [string]$WorkingDirectory = $PWD
    )
    
    $functionName = 'Invoke-PowerShellCommand'
    
    try {
        Write-StructuredLog -Message "Executing PowerShell command: $Command" -Level Info -Function $functionName -Data @{
            Command = $Command
            WorkingDirectory = $WorkingDirectory
        }
        
        # Validate command for security
        $blockedCommands = @('Remove-Item', 'Format-Volume', 'Restart-Computer', 'Stop-Computer')
        foreach ($blocked in $blockedCommands) {
            if ($Command -match "^\s*$blocked\b") {
                throw "Command '$blocked' is blocked for security reasons"
            }
        }
        
        # Execute command
        $result = Invoke-Expression $Command 2>&1
        $output = if ($result -is [System.Array]) { $result | Out-String } else { $result.ToString() }
        
        Write-StructuredLog -Message "PowerShell command executed successfully" -Level Info -Function $functionName
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error executing PowerShell command: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-FileListCommand {
    <#
    .SYNOPSIS
        List directory contents
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$Path = $PWD
    )
    
    $functionName = 'Invoke-FileListCommand'
    
    try {
        Write-StructuredLog -Message "Listing directory contents: $Path" -Level Info -Function $functionName -Data @{
            Path = $Path
        }
        
        if (-not (Test-Path $Path)) {
            throw "Path not found: $Path"
        }
        
        $items = Get-ChildItem -Path $Path -ErrorAction Stop
        $output = $items | Format-Table -AutoSize | Out-String
        
        Write-StructuredLog -Message "Listed $($items.Count) items" -Level Info -Function $functionName -Data @{
            ItemCount = $items.Count
        }
        
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error listing directory: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-ChangeDirectoryCommand {
    <#
    .SYNOPSIS
        Change current directory
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Path
    )
    
    $functionName = 'Invoke-ChangeDirectoryCommand'
    
    try {
        Write-StructuredLog -Message "Changing directory to: $Path" -Level Info -Function $functionName -Data @{
            Path = $Path
        }
        
        if (-not (Test-Path $Path)) {
            throw "Directory not found: $Path"
        }
        
        Set-Location -Path $Path -ErrorAction Stop
        $newPath = Get-Location
        
        Write-StructuredLog -Message "Directory changed to: $newPath" -Level Info -Function $functionName -Data @{
            NewPath = $newPath.Path
        }
        
        return "Changed to directory: $newPath"
        
    } catch {
        Write-StructuredLog -Message "Error changing directory: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-ShowDirectoryCommand {
    <#
    .SYNOPSIS
        Show current directory
    #>
    [CmdletBinding()]
    param()
    
    $functionName = 'Invoke-ShowDirectoryCommand'
    
    try {
        $currentPath = Get-Location
        Write-StructuredLog -Message "Showing current directory: $currentPath" -Level Info -Function $functionName -Data @{
            CurrentPath = $currentPath.Path
        }
        
        return "Current directory: $currentPath"
        
    } catch {
        Write-StructuredLog -Message "Error showing directory: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-MakeDirectoryCommand {
    <#
    .SYNOPSIS
        Create new directory
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$DirectoryName
    )
    
    $functionName = 'Invoke-MakeDirectoryCommand'
    
    try {
        Write-StructuredLog -Message "Creating new directory: $DirectoryName" -Level Info -Function $functionName -Data @{
            DirectoryName = $DirectoryName
        }
        
        $fullPath = Join-Path (Get-Location) $DirectoryName
        if (Test-Path $fullPath) {
            throw "Directory already exists: $fullPath"
        }
        
        New-Item -ItemType Directory -Path $fullPath -ErrorAction Stop | Out-Null
        
        Write-StructuredLog -Message "Directory created successfully: $fullPath" -Level Info -Function $functionName -Data @{
            FullPath = $fullPath
        }
        
        return "Created directory: $fullPath"
        
    } catch {
        Write-StructuredLog -Message "Error creating directory: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-TouchFileCommand {
    <#
    .SYNOPSIS
        Create new file
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$FileName
    )
    
    $functionName = 'Invoke-TouchFileCommand'
    
    try {
        Write-StructuredLog -Message "Creating new file: $FileName" -Level Info -Function $functionName -Data @{
            FileName = $FileName
        }
        
        $fullPath = Join-Path (Get-Location) $FileName
        if (Test-Path $fullPath) {
            throw "File already exists: $fullPath"
        }
        
        New-Item -ItemType File -Path $fullPath -ErrorAction Stop | Out-Null
        
        Write-StructuredLog -Message "File created successfully: $fullPath" -Level Info -Function $functionName -Data @{
            FullPath = $fullPath
        }
        
        return "Created file: $fullPath"
        
    } catch {
        Write-StructuredLog -Message "Error creating file: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-RemoveItemCommand {
    <#
    .SYNOPSIS
        Remove file or directory
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Path,
        
        [Parameter(Mandatory=$false)]
        [switch]$Force
    )
    
    $functionName = 'Invoke-RemoveItemCommand'
    
    try {
        Write-StructuredLog -Message "Removing item: $Path" -Level Info -Function $functionName -Data @{
            Path = $Path
            Force = $Force
        }
        
        if (-not (Test-Path $Path)) {
            throw "Item not found: $Path"
        }
        
        # Confirm deletion if not forced
        if (-not $Force) {
            $item = Get-Item $Path
            $response = Read-Host "Delete $($item.FullName)? (Y/N)"
            if ($response -ne 'Y' -and $response -ne 'y') {
                return "Deletion cancelled"
            }
        }
        
        Remove-Item -Path $Path -Recurse -Force -ErrorAction Stop
        
        Write-StructuredLog -Message "Item removed successfully: $Path" -Level Info -Function $functionName -Data @{
            Path = $Path
        }
        
        return "Removed: $Path"
        
    } catch {
        Write-StructuredLog -Message "Error removing item: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-OpenFileCommand {
    <#
    .SYNOPSIS
        Open file in editor
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$FilePath
    )
    
    $functionName = 'Invoke-OpenFileCommand'
    
    try {
        Write-StructuredLog -Message "Opening file: $FilePath" -Level Info -Function $functionName -Data @{
            FilePath = $FilePath
        }
        
        if (-not (Test-Path $FilePath)) {
            throw "File not found: $FilePath"
        }
        
        # Try to open with default application
        Start-Process -FilePath $FilePath -ErrorAction Stop
        
        Write-StructuredLog -Message "File opened successfully: $FilePath" -Level Info -Function $functionName -Data @{
            FilePath = $FilePath
        }
        
        return "Opened file: $FilePath"
        
    } catch {
        Write-StructuredLog -Message "Error opening file: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-WriteFileCommand {
    <#
    .SYNOPSIS
        Write content to file
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$FilePath,
        
        [Parameter(Mandatory=$true)]
        [string]$Content
    )
    
    $functionName = 'Invoke-WriteFileCommand'
    
    try {
        Write-StructuredLog -Message "Writing content to file: $FilePath" -Level Info -Function $functionName -Data @{
            FilePath = $FilePath
            ContentLength = $Content.Length
        }
        
        # Create directory if it doesn't exist
        $directory = Split-Path $FilePath -Parent
        if ($directory -and -not (Test-Path $directory)) {
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
        }
        
        # Write content to file
        $Content | Set-Content -Path $FilePath -Encoding UTF8 -Force
        
        Write-StructuredLog -Message "Content written successfully to: $FilePath" -Level Info -Function $functionName -Data @{
            FilePath = $FilePath
            BytesWritten = $Content.Length
        }
        
        return "Written to file: $FilePath"
        
    } catch {
        Write-StructuredLog -Message "Error writing to file: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-GitStatusCommand {
    <#
    .SYNOPSIS
        Git status command
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$RepositoryPath = $PWD
    )
    
    $functionName = 'Invoke-GitStatusCommand'
    
    try {
        Write-StructuredLog -Message "Getting git status for: $RepositoryPath" -Level Info -Function $functionName -Data @{
            RepositoryPath = $RepositoryPath
        }
        
        if (-not (Test-Path "$RepositoryPath\.git")) {
            throw "Not a git repository: $RepositoryPath"
        }
        
        $output = git -C $RepositoryPath status 2>&1
        
        Write-StructuredLog -Message "Git status retrieved successfully" -Level Info -Function $functionName
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error getting git status: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-GitCommitCommand {
    <#
    .SYNOPSIS
        Stage and commit all changes
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$RepositoryPath = $PWD,
        
        [Parameter(Mandatory=$false)]
        [string]$Message = "Auto-commit via RawrXD"
    )
    
    $functionName = 'Invoke-GitCommitCommand'
    
    try {
        Write-StructuredLog -Message "Committing changes in: $RepositoryPath" -Level Info -Function $functionName -Data @{
            RepositoryPath = $RepositoryPath
            Message = $Message
        }
        
        if (-not (Test-Path "$RepositoryPath\.git")) {
            throw "Not a git repository: $RepositoryPath"
        }
        
        # Stage all changes
        git -C $RepositoryPath add . 2>&1 | Out-Null
        
        # Commit changes
        $output = git -C $RepositoryPath commit -m $Message 2>&1
        
        Write-StructuredLog -Message "Changes committed successfully" -Level Info -Function $functionName
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error committing changes: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-GitBranchCommand {
    <#
    .SYNOPSIS
        List git branches
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$RepositoryPath = $PWD
    )
    
    $functionName = 'Invoke-GitBranchCommand'
    
    try {
        Write-StructuredLog -Message "Listing git branches for: $RepositoryPath" -Level Info -Function $functionName -Data @{
            RepositoryPath = $RepositoryPath
        }
        
        if (-not (Test-Path "$RepositoryPath\.git")) {
            throw "Not a git repository: $RepositoryPath"
        }
        
        $output = git -C $RepositoryPath branch 2>&1
        
        Write-StructuredLog -Message "Git branches listed successfully" -Level Info -Function $functionName
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error listing git branches: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-GitLogCommand {
    <#
    .SYNOPSIS
        Show git log
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$RepositoryPath = $PWD
    )
    
    $functionName = 'Invoke-GitLogCommand'
    
    try {
        Write-StructuredLog -Message "Showing git log for: $RepositoryPath" -Level Info -Function $functionName -Data @{
            RepositoryPath = $RepositoryPath
        }
        
        if (-not (Test-Path "$RepositoryPath\.git")) {
            throw "Not a git repository: $RepositoryPath"
        }
        
        $output = git -C $RepositoryPath log --oneline -10 2>&1
        
        Write-StructuredLog -Message "Git log retrieved successfully" -Level Info -Function $functionName
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error showing git log: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-ExplainCodeCommand {
    <#
    .SYNOPSIS
        Explain code using AI
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Code
    )
    
    $functionName = 'Invoke-ExplainCodeCommand'
    
    try {
        Write-StructuredLog -Message "Explaining code snippet" -Level Info -Function $functionName -Data @{
            CodeLength = $Code.Length
        }
        
        # Simple code explanation (in production, this would call an AI service)
        $explanation = @"
Code Explanation:
================
The provided code appears to be a PowerShell script with $($Code.Length) characters.

Key observations:
- Uses PowerShell syntax and cmdlets
- Follows standard PowerShell conventions
- Can be executed in a PowerShell environment

To get a more detailed explanation, please provide:
1. The specific code you want explained
2. Any particular aspects you're interested in
3. The context or purpose of the code
"@
        
        Write-StructuredLog -Message "Code explanation generated" -Level Info -Function $functionName
        return $explanation
        
    } catch {
        Write-StructuredLog -Message "Error explaining code: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-AnalyzeFileCommand {
    <#
    .SYNOPSIS
        Analyze file using AI
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$FilePath
    )
    
    $functionName = 'Invoke-AnalyzeFileCommand'
    
    try {
        Write-StructuredLog -Message "Analyzing file: $FilePath" -Level Info -Function $functionName -Data @{
            FilePath = $FilePath
        }
        
        if (-not (Test-Path $FilePath)) {
            throw "File not found: $FilePath"
        }
        
        $content = Get-Content $FilePath -Raw
        $lines = (Get-Content $FilePath).Count
        $size = (Get-Item $FilePath).Length
        
        # Simple file analysis (in production, this would call an AI service)
        $analysis = @"
File Analysis Report
====================
File: $FilePath
Size: $size bytes
Lines: $lines

Content Preview:
----------------
$(if ($content.Length -gt 500) { $content.Substring(0, 500) + "..." } else { $content })

Basic Analysis:
---------------
- File type: PowerShell script (.ps1)
- Size: $size bytes
- Lines of code: $lines
- Can be executed in PowerShell environment

To get a more detailed analysis, please provide:
1. The specific aspects you want analyzed
2. The purpose or context of the file
3. Any particular concerns or questions
"@
        
        Write-StructuredLog -Message "File analysis completed" -Level Info -Function $functionName -Data @{
            FilePath = $FilePath
            Lines = $lines
            Size = $size
        }
        
        return $analysis
        
    } catch {
        Write-StructuredLog -Message "Error analyzing file: $_" -Level Error -Function $functionName
        throw
    }
}

function Invoke-AgenticCommand {
    <#
    .SYNOPSIS
        Main entry point for agentic command execution
    
    .DESCRIPTION
        Execute agentic commands with comprehensive support for:
        - Terminal/shell commands
        - File browser operations
        - Git integration
        - AI chat commands
        - Natural language processing
        - Command validation and security
    
    .PARAMETER Command
        Agentic command to execute
    
    .PARAMETER WorkingDirectory
        Working directory for command execution
    
    .PARAMETER Force
        Force execution without confirmation
    
    .EXAMPLE
        Invoke-AgenticCommand -Command "/term ls"
        
        Execute terminal command
    
    .EXAMPLE
        Invoke-AgenticCommand -Command "/ls C:\\Users"
        
        List directory contents
    
    .EXAMPLE
        Invoke-AgenticCommand -Command "/git status"
        
        Get git status
    
    .EXAMPLE
        Invoke-AgenticCommand -Command "analyze file.ps1"
        
        Analyze PowerShell file
    
    .OUTPUTS
        Command execution results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Command,
        
        [Parameter(Mandatory=$false)]
        [string]$WorkingDirectory = $PWD,
        
        [Parameter(Mandatory=$false)]
        [switch]$Force
    )
    
    $functionName = 'Invoke-AgenticCommand'
    $startTime = Get-Date
    
    try {
        Write-StructuredLog -Message "Executing agentic command" -Level Info -Function $functionName -Data @{
            Command = $Command
            WorkingDirectory = $WorkingDirectory
        }
        
        # Parse command
        $commandParts = $Command -split '\s+'
        $commandName = $commandParts[0]
        $arguments = $commandParts[1..$($commandParts.Length - 1)] -join ' '
        
        # Route to appropriate handler
        $output = switch -Wildcard ($commandName) {
            '/term' { Invoke-TerminalCommand -Command $arguments -WorkingDirectory $WorkingDirectory }
            '/exec' { Invoke-PowerShellCommand -Command $arguments -WorkingDirectory $WorkingDirectory }
            '/terminal' { Invoke-TerminalCommand -Command $arguments -WorkingDirectory $WorkingDirectory }
            '/ls' { Invoke-FileListCommand -Path $(if($arguments){$arguments}else{$WorkingDirectory}) }
            '/cd' { Invoke-ChangeDirectoryCommand -Path $arguments }
            '/pwd' { Invoke-ShowDirectoryCommand }
            '/mkdir' { Invoke-MakeDirectoryCommand -DirectoryName $arguments }
            '/touch' { Invoke-TouchFileCommand -FileName $arguments }
            '/rm' { Invoke-RemoveItemCommand -Path $arguments -Force:$Force }
            '/open' { Invoke-OpenFileCommand -FilePath $arguments }
            '/write' { 
                $parts = $arguments -split '\s+', 2
                Invoke-WriteFileCommand -FilePath $parts[0] -Content $parts[1]
            }
            '/git' {
                $gitCommand = $arguments -split '\s+' | Select-Object -First 1
                switch ($gitCommand) {
                    'status' { Invoke-GitStatusCommand -RepositoryPath $WorkingDirectory }
                    'commit' { Invoke-GitCommitCommand -RepositoryPath $WorkingDirectory }
                    'branch' { Invoke-GitBranchCommand -RepositoryPath $WorkingDirectory }
                    'log' { Invoke-GitLogCommand -RepositoryPath $WorkingDirectory }
                    default { throw "Unknown git command: $gitCommand" }
                }
            }
            'explain' { Invoke-ExplainCodeCommand -Code $arguments }
            'analyze' { Invoke-AnalyzeFileCommand -FilePath $arguments }
            default { throw "Unknown agentic command: $commandName" }
        }
        
        $duration = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        Write-StructuredLog -Message "Agentic command executed successfully in ${duration}s" -Level Info -Function $functionName -Data @{
            Duration = $duration
        }
        
        return $output
        
    } catch {
        Write-StructuredLog -Message "Error executing agentic command: $_" -Level Error -Function $functionName
        throw
    }
}

function Get-AgenticCommandHelp {
    <#
    .SYNOPSIS
        Get help for agentic commands
    #>
    [CmdletBinding()]
    param()
    
    $help = @"
═══════════════════════════════════════════════════════════════
  🤖 RawrXD AGENTIC COMMANDS - HELP
═══════════════════════════════════════════════════════════════

TERMINAL / SHELL COMMANDS:
  /term <command>              - Execute terminal command
  /exec <command>              - Execute PowerShell command
  /terminal <command>          - Execute any terminal command

FILE BROWSER COMMANDS:
  /ls [path]                   - List directory contents
  /cd <path>                   - Change directory
  /pwd                         - Show current directory
  /mkdir <name>                - Create new folder
  /touch <filename>            - Create new file
  /rm <path>                   - Delete file/folder
  /open <filepath>             - Open file in editor
  /write <filepath> <content>  - Write content to file

GIT COMMANDS:
  /git status                  - Git status
  /git commit                  - Stage & commit all
  /git branch                  - List branches
  /git log                     - Show git log

AI CHAT COMMANDS:
  explain <code>               - Explain code
  analyze <filepath>           - Analyze file

═══════════════════════════════════════════════════════════════
"@
    
    return $help
}

# Export main functions
Export-ModuleMember -Function Invoke-AgenticCommand, Get-AgenticCommandHelp, Invoke-TerminalCommand, Invoke-PowerShellCommand, Invoke-FileListCommand, Invoke-ChangeDirectoryCommand, Invoke-ShowDirectoryCommand, Invoke-MakeDirectoryCommand, Invoke-TouchFileCommand, Invoke-RemoveItemCommand, Invoke-OpenFileCommand, Invoke-WriteFileCommand, Invoke-GitStatusCommand, Invoke-GitCommitCommand, Invoke-GitBranchCommand, Invoke-GitLogCommand, Invoke-ExplainCodeCommand, Invoke-AnalyzeFileCommand

