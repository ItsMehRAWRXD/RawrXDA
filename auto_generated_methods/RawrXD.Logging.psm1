
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
}# RawrXD Logging Module
# Production-ready structured logging

#Requires -Version 5.1

<#
.SYNOPSIS
    RawrXD.Logging - Structured logging system

.DESCRIPTION
    Comprehensive structured logging system providing:
    - Structured log output
    - Multiple log levels
    - File and console logging
    - Performance tracking
    - No external dependencies

.LINK
    https://github.com/RawrXD/Logging

.NOTES
    Author: RawrXD Auto-Generation System
    Version: 1.0.0
    Requires: PowerShell 5.1+
    Last Updated: 2024-12-28
#>

# Global logging configuration
$script:LoggingConfig = @{
    Enabled = $true
    Level = "Info"
    FilePath = $null
    Console = $true
    Colors = @{
        Debug = "DarkGray"
        Info = "Cyan"
        Warning = "Yellow"
        Error = "Red"
    }
    Timestamps = $true
    FunctionNames = $true
}

# Write structured log entry
function Write-StructuredLog {
    <#
    .SYNOPSIS
        Write structured log entry
    
    .DESCRIPTION
        Write a structured log entry with timestamp, level, function name, and optional data
    
    .PARAMETER Message
        Log message
    
    .PARAMETER Level
        Log level (Debug, Info, Warning, Error)
    
    .PARAMETER Function
        Function name (auto-detected if not provided)
    
    .PARAMETER Data
        Additional data to log (hashtable)
    
    .EXAMPLE
        Write-StructuredLog -Message "Starting operation" -Level Info -Function "MyFunction"
        
        Write info log
    
    .EXAMPLE
        Write-StructuredLog -Message "Error occurred" -Level Error -Data @{ ErrorCode = 500 }
        
        Write error log with data
    
    .OUTPUTS
        None
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Message,
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('Debug', 'Info', 'Warning', 'Error')]
        [string]$Level = "Info",
        
        [Parameter(Mandatory=$false)]
        [string]$Function = $null,
        
        [Parameter(Mandatory=$false)]
        [hashtable]$Data = $null
    )
    
    # Check if logging is enabled
    if (-not $script:LoggingConfig.Enabled) {
        return
    }
    
    # Check log level
    $levels = @("Debug", "Info", "Warning", "Error")
    $currentLevelIndex = [Array]::IndexOf($levels, $script:LoggingConfig.Level)
    $messageLevelIndex = [Array]::IndexOf($levels, $Level)
    
    if ($messageLevelIndex -lt $currentLevelIndex) {
        return
    }
    
    # Get function name if not provided
    if (-not $Function) {
        $callStack = Get-PSCallStack
        if ($callStack.Count -gt 1) {
            $Function = $callStack[1].FunctionName
        } else {
            $Function = "Unknown"
        }
    }
    
    # Build log entry
    $timestamp = if ($script:LoggingConfig.Timestamps) {
        Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    } else {
        ""
    }
    
    $functionName = if ($script:LoggingConfig.FunctionNames) {
        $Function
    } else {
        ""
    }
    
    $logParts = @()
    
    if ($timestamp) {
        $logParts += "[$timestamp]"
    }
    
    if ($functionName) {
        $logParts += "[$functionName]"
    }
    
    $logParts += "[$Level]"
    $logParts += $Message
    
    $logEntry = $logParts -join " "
    
    # Add data if provided
    if ($Data) {
        try {
            $dataJson = $Data | ConvertTo-Json -Compress -Depth 3
            $logEntry += " | Data: $dataJson"
        } catch {
            # If JSON conversion fails, skip data
        }
    }
    
    # Write to console
    if ($script:LoggingConfig.Console) {
        $color = $script:LoggingConfig.Colors[$Level]
        Write-Host $logEntry -ForegroundColor $color
    }
    
    # Write to file
    if ($script:LoggingConfig.FilePath) {
        try {
            $logFile = $script:LoggingConfig.FilePath
            
            # If FilePath is a directory, create dated log file
            if (Test-Path $logFile -PathType Container) {
                $date = Get-Date -Format "yyyy-MM-dd"
                $logFile = Join-Path $logFile "RawrXD_$date.log"
            }
            
            # Ensure directory exists
            $logDir = Split-Path $logFile -Parent
            if (-not (Test-Path $logDir)) {
                New-Item -Path $logDir -ItemType Directory -Force | Out-Null
            }
            
            # Append to log file
            Add-Content -Path $logFile -Value $logEntry -Encoding UTF8
        } catch {
            # Silently fail file logging errors
        }
    }
}

# Set logging configuration
function Set-LoggingConfig {
    <#
    .SYNOPSIS
        Set logging configuration
    
    .DESCRIPTION
        Configure logging settings
    
    .PARAMETER Enabled
        Whether logging is enabled
    
    .PARAMETER Level
        Log level (Debug, Info, Warning, Error)
    
    .PARAMETER FilePath
        Path for log files
    
    .PARAMETER Console
        Whether to log to console
    
    .PARAMETER Timestamps
        Whether to include timestamps
    
    .PARAMETER FunctionNames
        Whether to include function names
    
    .EXAMPLE
        Set-LoggingConfig -Level Debug -FilePath "C:\\Logs" -Console $true
        
        Configure logging
    
    .OUTPUTS
        None
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [bool]$Enabled = $null,
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('Debug', 'Info', 'Warning', 'Error')]
        [string]$Level = $null,
        
        [Parameter(Mandatory=$false)]
        [string]$FilePath = $null,
        
        [Parameter(Mandatory=$false)]
        [bool]$Console = $null,
        
        [Parameter(Mandatory=$false)]
        [bool]$Timestamps = $null,
        
        [Parameter(Mandatory=$false)]
        [bool]$FunctionNames = $null
    )
    
    if ($Enabled -ne $null) {
        $script:LoggingConfig.Enabled = $Enabled
    }
    
    if ($Level) {
        $script:LoggingConfig.Level = $Level
    }
    
    if ($FilePath) {
        $script:LoggingConfig.FilePath = $FilePath
    }
    
    if ($Console -ne $null) {
        $script:LoggingConfig.Console = $Console
    }
    
    if ($Timestamps -ne $null) {
        $script:LoggingConfig.Timestamps = $Timestamps
    }
    
    if ($FunctionNames -ne $null) {
        $script:LoggingConfig.FunctionNames = $FunctionNames
    }
    
    Write-StructuredLog -Message "Logging configuration updated" -Level Info -Function "Set-LoggingConfig" -Data $script:LoggingConfig
}

# Get logging configuration
function Get-LoggingConfig {
    <#
    .SYNOPSIS
        Get logging configuration
    
    .DESCRIPTION
        Get current logging configuration
    
    .EXAMPLE
        Get-LoggingConfig
        
        Get logging configuration
    
    .OUTPUTS
        Logging configuration hashtable
    #>
    [CmdletBinding()]
    param()
    
    return $script:LoggingConfig.Clone()
}

# Start performance timer
function Start-PerformanceTimer {
    <#
    .SYNOPSIS
        Start performance timer
    
    .DESCRIPTION
        Start a performance timer for measuring operation duration
    
    .PARAMETER Name
        Timer name
    
    .EXAMPLE
        $timer = Start-PerformanceTimer -Name "MyOperation"
        # ... do work ...
        Stop-PerformanceTimer -Timer $timer
        
        Measure operation duration
    
    .OUTPUTS
        Timer object
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name
    )
    
    return [PSCustomObject]@{
        Name = $Name
        StartTime = Get-Date
        StopTime = $null
        Duration = $null
    }
}

# Stop performance timer
function Stop-PerformanceTimer {
    <#
    .SYNOPSIS
        Stop performance timer
    
    .DESCRIPTION
        Stop a performance timer and log the duration
    
    .PARAMETER Timer
        Timer object from Start-PerformanceTimer
    
    .EXAMPLE
        $timer = Start-PerformanceTimer -Name "MyOperation"
        # ... do work ...
        Stop-PerformanceTimer -Timer $timer
        
        Stop timer and log duration
    
    .OUTPUTS
        Timer duration in seconds
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [PSCustomObject]$Timer
    )
    
    $Timer.StopTime = Get-Date
    $Timer.Duration = [Math]::Round(($Timer.StopTime - $Timer.StartTime).TotalSeconds, 3)
    
    Write-StructuredLog -Message "Operation '$($Timer.Name)' completed in $($Timer.Duration)s" -Level Info -Function "Stop-PerformanceTimer" -Data @{
        Operation = $Timer.Name
        Duration = $Timer.Duration
    }
    
    return $Timer.Duration
}

# Log error with exception details
function Write-ErrorLog {
    <#
    .SYNOPSIS
        Log error with exception details
    
    .DESCRIPTION
        Log an error with full exception details including stack trace
    
    .PARAMETER Message
        Error message
    
    .PARAMETER Exception
        Exception object
    
    .PARAMETER Function
        Function name
    
    .EXAMPLE
        try {
            # ... code ...
        } catch {
            Write-ErrorLog -Message "Operation failed" -Exception $_ -Function "MyFunction"
        }
        
        Log error with exception details
    
    .OUTPUTS
        None
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Message,
        
        [Parameter(Mandatory=$true)]
        [System.Exception]$Exception,
        
        [Parameter(Mandatory=$false)]
        [string]$Function = $null
    )
    
    $errorData = @{
        Message = $Exception.Message
        Type = $Exception.GetType().Name
        StackTrace = $Exception.StackTrace
    }
    
    if ($Exception.InnerException) {
        $errorData.InnerException = $Exception.InnerException.Message
    }
    
    Write-StructuredLog -Message $Message -Level Error -Function $Function -Data $errorData
}

# Log operation start
function Write-OperationStart {
    <#
    .SYNOPSIS
        Log operation start
    
    .DESCRIPTION
        Log the start of an operation
    
    .PARAMETER Operation
        Operation name
    
    .PARAMETER Data
        Operation data
    
    .EXAMPLE
        Write-OperationStart -Operation "DeploySystem" -Data @{ Target = "Production" }
        
        Log operation start
    
    .OUTPUTS
        None
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Operation,
        
        [Parameter(Mandatory=$false)]
        [hashtable]$Data = $null
    )
    
    $callStack = Get-PSCallStack
    $functionName = if ($callStack.Count -gt 1) { $callStack[1].FunctionName } else { "Unknown" }
    
    Write-StructuredLog -Message "Starting operation: $Operation" -Level Info -Function $functionName -Data $Data
}

# Log operation complete
function Write-OperationComplete {
    <#
    .SYNOPSIS
        Log operation complete
    
    .DESCRIPTION
        Log the completion of an operation
    
    .PARAMETER Operation
        Operation name
    
    .PARAMETER Duration
        Operation duration in seconds
    
    .PARAMETER Data
        Operation results
    
    .EXAMPLE
        Write-OperationComplete -Operation "DeploySystem" -Duration 12.5 -Data @{ Success = $true }
        
        Log operation complete
    
    .OUTPUTS
        None
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Operation,
        
        [Parameter(Mandatory=$true)]
        [double]$Duration,
        
        [Parameter(Mandatory=$false)]
        [hashtable]$Data = $null
    )
    
    $callStack = Get-PSCallStack
    $functionName = if ($callStack.Count -gt 1) { $callStack[1].FunctionName } else { "Unknown" }
    
    $message = "Operation '$Operation' completed in ${Duration}s"
    
    Write-StructuredLog -Message $message -Level Info -Function $functionName -Data $Data
}

# Export functions
Export-ModuleMember -Function Write-StructuredLog, Set-LoggingConfig, Get-LoggingConfig, Start-PerformanceTimer, Stop-PerformanceTimer, Write-ErrorLog, Write-OperationStart, Write-OperationComplete

