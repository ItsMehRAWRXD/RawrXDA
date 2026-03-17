#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Auto Tool Invocation Module
    
.DESCRIPTION
    Automatically invokes tools based on agent requests with proper
    parameter binding and error handling.
#>

# ============================================
# MODULE VARIABLES
# ============================================

$script:ToolRegistry = @{}
$script:InvocationHistory = @()
$script:MaxHistoryEntries = 1000
$script:ToolTimeout = 30

# ============================================
# TOOL REGISTRATION
# ============================================

function Register-ToolInvocation {
    <#
    .SYNOPSIS
        Register a tool for automatic invocation
    #>
    param(
        [Parameter(Mandatory = $true)][string]$ToolName,
        [Parameter(Mandatory = $true)][scriptblock]$Handler,
        [string]$Description = "",
        [hashtable]$Parameters = @{}
    )
    
    $script:ToolRegistry[$ToolName] = @{
        Name = $ToolName
        Handler = $Handler
        Description = $Description
        Parameters = $Parameters
        Enabled = $true
        InvocationCount = 0
        LastInvoked = $null
    }
}

function Unregister-ToolInvocation {
    <#
    .SYNOPSIS
        Unregister a tool from automatic invocation
    #>
    param(
        [Parameter(Mandatory = $true)][string]$ToolName
    )
    
    if ($script:ToolRegistry.ContainsKey($ToolName)) {
        $script:ToolRegistry.Remove($ToolName)
        return $true
    }
    return $false
}

function Enable-ToolInvocation {
    <#
    .SYNOPSIS
        Enable automatic invocation for a tool
    #>
    param(
        [Parameter(Mandatory = $true)][string]$ToolName
    )
    
    if ($script:ToolRegistry.ContainsKey($ToolName)) {
        $script:ToolRegistry[$ToolName].Enabled = $true
    }
}

function Disable-ToolInvocation {
    <#
    .SYNOPSIS
        Disable automatic invocation for a tool
    #>
    param(
        [Parameter(Mandatory = $true)][string]$ToolName
    )
    
    if ($script:ToolRegistry.ContainsKey($ToolName)) {
        $script:ToolRegistry[$ToolName].Enabled = $false
    }
}

# ============================================
# TOOL INVOCATION
# ============================================

function Invoke-RegisteredTool {
    <#
    .SYNOPSIS
        Automatically invoke a registered tool
    #>
    param(
        [Parameter(Mandatory = $true)][string]$ToolName,
        [hashtable]$Parameters = @{},
        [int]$TimeoutSeconds = 30
    )
    
    try {
        if (-not $script:ToolRegistry.ContainsKey($ToolName)) {
            return @{
                Success = $false
                Error = "Tool not registered: $ToolName"
                Timestamp = Get-Date
            }
        }
        
        $tool = $script:ToolRegistry[$ToolName]
        
        if (-not $tool.Enabled) {
            return @{
                Success = $false
                Error = "Tool is disabled: $ToolName"
                Timestamp = Get-Date
            }
        }
        
        Write-Host "[AutoToolInvocation] Invoking tool: $ToolName with parameters: $($Parameters | ConvertTo-Json)" -ForegroundColor Cyan
        
        # Invoke the tool
        $startTime = Get-Date
        $result = & $tool.Handler @Parameters
        $duration = (Get-Date) - $startTime
        
        # Update statistics
        $tool.InvocationCount++
        $tool.LastInvoked = Get-Date
        
        # Log invocation
        $invocation = @{
            Timestamp = Get-Date
            ToolName = $ToolName
            Parameters = $Parameters
            Result = $result
            Duration = $duration.TotalSeconds
            Success = $true
        }
        
        $script:InvocationHistory += $invocation
        
        # Trim history if too large
        if ($script:InvocationHistory.Count -gt $script:MaxHistoryEntries) {
            $script:InvocationHistory = $script:InvocationHistory[-$script:MaxHistoryEntries..-1]
        }
        
        Write-Host "[AutoToolInvocation] Tool invocation completed in $($duration.TotalMilliseconds)ms" -ForegroundColor Green
        
        return @{
            Success = $true
            ToolName = $ToolName
            Result = $result
            Duration = $duration.TotalSeconds
            Timestamp = Get-Date
        }
    }
    catch {
        $invocation = @{
            Timestamp = Get-Date
            ToolName = $ToolName
            Parameters = $Parameters
            Error = $_.Exception.Message
            Success = $false
        }
        
        $script:InvocationHistory += $invocation
        
        Write-Host "[AutoToolInvocation] Tool invocation failed: $_" -ForegroundColor Red
        
        return @{
            Success = $false
            ToolName = $ToolName
            Error = $_.Exception.Message
            Timestamp = Get-Date
        }
    }
}

# ============================================
# BATCH INVOCATION
# ============================================

function Invoke-ToolBatch {
    <#
    .SYNOPSIS
        Invoke multiple tools in sequence or parallel
    #>
    param(
        [Parameter(Mandatory = $true)][hashtable[]]$ToolRequests,
        [switch]$Parallel,
        [int]$ThrottleLimit = 5
    )
    
    try {
        $results = @()
        
        if ($Parallel) {
            $results = $ToolRequests | ForEach-Object -Parallel {
                $toolName = $_.ToolName
                $parameters = $_.Parameters
                
                if ($script:ToolRegistry.ContainsKey($toolName)) {
                    & $script:ToolRegistry[$toolName].Handler @parameters
                }
            } -ThrottleLimit $ThrottleLimit
        }
        else {
            foreach ($request in $ToolRequests) {
                $result = Invoke-RegisteredTool -ToolName $request.ToolName -Parameters $request.Parameters
                $results += $result
            }
        }
        
        return @{
            Success = $true
            BatchSize = $ToolRequests.Count
            Results = $results
            Timestamp = Get-Date
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
            Timestamp = Get-Date
        }
    }
}

# ============================================
# TOOL MANAGEMENT
# ============================================

function Get-RegisteredToolInvocations {
    <#
    .SYNOPSIS
        Get list of registered tool invocations
    #>
    param(
        [switch]$IncludeDisabled
    )
    
    $tools = $script:ToolRegistry.Values
    
    if (-not $IncludeDisabled) {
        $tools = $tools | Where-Object { $_.Enabled }
    }
    
    return $tools | Select-Object Name, Description, Enabled, InvocationCount, LastInvoked
}

function Get-ToolInvocationHistory {
    <#
    .SYNOPSIS
        Get tool invocation history
    #>
    param(
        [int]$Last = 50,
        [string]$ToolName = ""
    )
    
    $history = $script:InvocationHistory
    
    if ($ToolName) {
        $history = $history | Where-Object { $_.ToolName -eq $ToolName }
    }
    
    return $history | Select-Object -Last $Last
}

function Clear-ToolInvocationHistory {
    <#
    .SYNOPSIS
        Clear invocation history
    #>
    $script:InvocationHistory = @()
    
    return @{
        Success = $true
        Message = "Invocation history cleared"
    }
}

function Get-ToolInvocationStats {
    <#
    .SYNOPSIS
        Get statistics on tool invocations
    #>
    return @{
        RegisteredTools = $script:ToolRegistry.Count
        EnabledTools = ($script:ToolRegistry.Values | Where-Object { $_.Enabled }).Count
        TotalInvocations = $script:InvocationHistory.Count
        AverageInvocationTime = if ($script:InvocationHistory.Count -gt 0) { 
            ([math]::Round(($script:InvocationHistory | Measure-Object -Property Duration -Average).Average, 3))
        } else { 0 }
        SuccessfulInvocations = ($script:InvocationHistory | Where-Object { $_.Success }).Count
        FailedInvocations = ($script:InvocationHistory | Where-Object { -not $_.Success }).Count
    }
}

# ============================================
# PARAMETER VALIDATION
# ============================================

function Test-ToolParameters {
    <#
    .SYNOPSIS
        Validate parameters for a tool
    #>
    param(
        [Parameter(Mandatory = $true)][string]$ToolName,
        [hashtable]$Parameters = @{}
    )
    
    if (-not $script:ToolRegistry.ContainsKey($ToolName)) {
        return @{
            Valid = $false
            Message = "Tool not registered: $ToolName"
        }
    }
    
    $tool = $script:ToolRegistry[$ToolName]
    $missingParams = @()
    
    # Check for required parameters
    foreach ($param in $tool.Parameters.Keys) {
        if ($tool.Parameters[$param].Required -and -not $Parameters.ContainsKey($param)) {
            $missingParams += $param
        }
    }
    
    if ($missingParams.Count -gt 0) {
        return @{
            Valid = $false
            Message = "Missing required parameters: $($missingParams -join ', ')"
            MissingParameters = $missingParams
        }
    }
    
    return @{
        Valid = $true
        Message = "Parameters are valid"
    }
}

# ============================================
# INITIALIZATION
# ============================================

Write-Host "[RawrXD-AutoToolInvocation] Module loaded successfully" -ForegroundColor Green

Export-ModuleMember -Function @(
    'Register-ToolInvocation',
    'Unregister-ToolInvocation',
    'Enable-ToolInvocation',
    'Disable-ToolInvocation',
    'Invoke-RegisteredTool',
    'Invoke-ToolBatch',
    'Get-RegisteredToolInvocations',
    'Get-ToolInvocationHistory',
    'Clear-ToolInvocationHistory',
    'Get-ToolInvocationStats',
    'Test-ToolParameters'
)
