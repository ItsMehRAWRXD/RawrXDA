#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Built-In Tools Module
    
.DESCRIPTION
    Provides core built-in tools for file operations, command execution,
    and system interactions within the agentic environment.
#>

# ============================================
# MODULE VARIABLES
# ============================================

$script:BuiltInTools = @{}
$script:ToolExecutionLog = @()
$script:MaxLogEntries = 500

# ============================================
# FILE SYSTEM TOOLS
# ============================================

function New-BuiltInTool {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Implementation,
        [string]$Description = "",
        [string[]]$Parameters = @()
    )
    
    $script:BuiltInTools[$Name] = @{
        Name = $Name
        Description = $Description
        Implementation = $Implementation
        Parameters = $Parameters
        Enabled = $true
        CreatedAt = Get-Date
    }
}

function Tool-ReadFile {
    <#
    .SYNOPSIS
        Read file contents
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$StartLine = 1,
        [int]$EndLine = -1
    )
    
    try {
        if (-not (Test-Path $Path)) {
            return @{ Success = $false; Error = "File not found: $Path" }
        }
        
        $content = Get-Content -Path $Path -Raw -ErrorAction Stop
        
        if ($EndLine -eq -1) {
            $lines = @($content -split "`n")
            $EndLine = $lines.Count
        }
        
        $lines = @(($content -split "`n")[($StartLine - 1)..($EndLine - 1)])
        
        Write-ToolLog -Tool "Tool-ReadFile" -Path $Path -Success $true
        
        return @{
            Success = $true
            Content = $lines -join "`n"
            LineCount = $lines.Count
            Path = $Path
        }
    }
    catch {
        Write-ToolLog -Tool "Tool-ReadFile" -Path $Path -Success $false -Error $_
        return @{ Success = $false; Error = $_ }
    }
}

function Tool-WriteFile {
    <#
    .SYNOPSIS
        Write content to file
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content,
        [switch]$Append
    )
    
    try {
        $dir = Split-Path $Path -Parent
        if (-not (Test-Path $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        
        if ($Append) {
            Add-Content -Path $Path -Value $Content -Encoding UTF8
        }
        else {
            Set-Content -Path $Path -Value $Content -Encoding UTF8
        }
        
        Write-ToolLog -Tool "Tool-WriteFile" -Path $Path -Success $true
        
        return @{
            Success = $true
            Path = $Path
            Message = "File written successfully"
        }
    }
    catch {
        Write-ToolLog -Tool "Tool-WriteFile" -Path $Path -Success $false -Error $_
        return @{ Success = $false; Error = $_ }
    }
}

function Tool-ListDirectory {
    <#
    .SYNOPSIS
        List directory contents
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string]$Filter = "*",
        [switch]$Recurse
    )
    
    try {
        if (-not (Test-Path $Path)) {
            return @{ Success = $false; Error = "Directory not found: $Path" }
        }
        
        $params = @{
            Path = $Path
            Filter = $Filter
            ErrorAction = "SilentlyContinue"
        }
        
        if ($Recurse) {
            $params.Recurse = $true
        }
        
        $items = Get-ChildItem @params | Select-Object Name, FullName, @{Name="Type"; Expression={if($_.PSIsContainer) {"Directory"} else {"File"}}}, Length, LastWriteTime
        
        Write-ToolLog -Tool "Tool-ListDirectory" -Path $Path -Success $true
        
        return @{
            Success = $true
            Path = $Path
            Items = $items
            Count = $items.Count
        }
    }
    catch {
        Write-ToolLog -Tool "Tool-ListDirectory" -Path $Path -Success $false -Error $_
        return @{ Success = $false; Error = $_ }
    }
}

# ============================================
# COMMAND EXECUTION TOOLS
# ============================================

function Tool-ExecuteCommand {
    <#
    .SYNOPSIS
        Execute a system command safely
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [string[]]$Arguments = @(),
        [int]$TimeoutSeconds = 30,
        [switch]$CaptureOutput
    )
    
    try {
        Write-ToolLog -Tool "Tool-ExecuteCommand" -Command $Command -Success $null
        
        $output = ""
        $exitCode = 0
        
        try {
            $proc = Start-Process -FilePath $Command -ArgumentList $Arguments -NoNewWindow -PassThru -Wait -TimeoutSec $TimeoutSeconds
            $exitCode = $proc.ExitCode
        }
        catch {
            if ($_.Exception.Message -like "*exceeded timeout*") {
                Write-ToolLog -Tool "Tool-ExecuteCommand" -Command $Command -Success $false -Error "Command timeout"
                return @{ Success = $false; Error = "Command execution timeout after ${TimeoutSeconds}s" }
            }
        }
        
        Write-ToolLog -Tool "Tool-ExecuteCommand" -Command $Command -Success $true
        
        return @{
            Success = $exitCode -eq 0
            ExitCode = $exitCode
            Output = $output
        }
    }
    catch {
        Write-ToolLog -Tool "Tool-ExecuteCommand" -Command $Command -Success $false -Error $_
        return @{ Success = $false; Error = $_; ExitCode = 1 }
    }
}

# ============================================
# SEARCH & ANALYSIS TOOLS
# ============================================

function Tool-SearchText {
    <#
    .SYNOPSIS
        Search for text in files
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Path,
        [switch]$RegularExpression
    )
    
    try {
        $items = Get-ChildItem -Path $Path -File -Recurse -ErrorAction SilentlyContinue
        $results = @()
        
        foreach ($item in $items) {
            try {
                $content = Get-Content -Path $item.FullName -Raw -ErrorAction SilentlyContinue
                
                if ($RegularExpression) {
                    if ($content -match $Pattern) {
                        $results += @{
                            File = $item.FullName
                            Match = $true
                        }
                    }
                }
                else {
                    if ($content -like "*$Pattern*") {
                        $results += @{
                            File = $item.FullName
                            Match = $true
                        }
                    }
                }
            }
            catch { }
        }
        
        return @{
            Success = $true
            Pattern = $Pattern
            Results = $results
            Count = $results.Count
        }
    }
    catch {
        return @{ Success = $false; Error = $_ }
    }
}

# ============================================
# UTILITY FUNCTIONS
# ============================================

function Write-ToolLog {
    param(
        [string]$Tool,
        [string]$Path = "",
        [string]$Command = "",
        [bool]$Success = $null,
        [string]$Error = ""
    )
    
    $entry = @{
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        Tool = $Tool
        Path = $Path
        Command = $Command
        Success = $Success
        Error = $Error
    }
    
    $script:ToolExecutionLog += $entry
    
    if ($script:ToolExecutionLog.Count -gt $script:MaxLogEntries) {
        $script:ToolExecutionLog = $script:ToolExecutionLog[-$script:MaxLogEntries..-1]
    }
}

function Get-ToolExecutionLog {
    <#
    .SYNOPSIS
        Get tool execution logs
    #>
    param(
        [int]$Last = 50
    )
    
    return $script:ToolExecutionLog | Select-Object -Last $Last
}

function Get-RegisteredTools {
    <#
    .SYNOPSIS
        Get list of registered built-in tools
    #>
    return $script:BuiltInTools.Keys
}

function Initialize-BuiltInTools {
    <#
    .SYNOPSIS
        Initialize the built-in tools system
    .DESCRIPTION
        This function is called during startup to finalize tool registration
        and ensure all built-in tools are properly initialized
    #>
    # Note: Built-in tools are registered via the main RawrIDEPowershield.ps1 script
    # This function just ensures the tools are ready
    Write-Host "[RawrXD-BuiltInTools] Initialize-BuiltInTools called - $($(Get-RegisteredTools).Count) tools ready"
    return $true
}

# ============================================
# REGISTER BUILT-IN TOOLS
# ============================================

# Register tools in local registry first
New-BuiltInTool -Name "read_file" -Implementation ${function:Tool-ReadFile} -Description "Read file contents" -Parameters @("Path", "StartLine", "EndLine")
New-BuiltInTool -Name "write_file" -Implementation ${function:Tool-WriteFile} -Description "Write content to file" -Parameters @("Path", "Content", "Append")
New-BuiltInTool -Name "list_directory" -Implementation ${function:Tool-ListDirectory} -Description "List directory contents" -Parameters @("Path", "Filter", "Recurse")
New-BuiltInTool -Name "execute_command" -Implementation ${function:Tool-ExecuteCommand} -Description "Execute system command" -Parameters @("Command", "Arguments", "TimeoutSeconds")
New-BuiltInTool -Name "search_text" -Implementation ${function:Tool-SearchText} -Description "Search for text in files" -Parameters @("Pattern", "Path", "RegularExpression")

# Note: Agent system registration happens in RawrIDEPowershield.ps1 via main Register-AgentTool calls

# ============================================
# INITIALIZATION
# ============================================

Write-Host "[RawrXD-BuiltInTools] Module loaded successfully - $($(Get-RegisteredTools).Count) tools registered" -ForegroundColor Green

Export-ModuleMember -Function @(
    'Tool-ReadFile',
    'Tool-WriteFile',
    'Tool-ListDirectory',
    'Tool-ExecuteCommand',
    'Tool-SearchText',
    'Get-ToolExecutionLog',
    'Get-RegisteredTools',
    'Initialize-BuiltInTools',
    'New-BuiltInTool'
)
