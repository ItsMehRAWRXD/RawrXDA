#Requires -Version 5.1
<#
.SYNOPSIS
    Editor Diagnostics Module
    
.DESCRIPTION
    Provides diagnostics and troubleshooting tools for the RawrXD IDE editor,
    including error detection, performance monitoring, and debugging utilities.
#>

# ============================================
# MODULE VARIABLES
# ============================================

$script:DiagnosticLog = @()
$script:EditorErrors = @()
$script:PerformanceMetrics = @{}
$script:MaxDiagnosticEntries = 500

# ============================================
# DIAGNOSTIC LOGGING
# ============================================

function Write-EditorDiagnostic {
    <#
    .SYNOPSIS
        Write a diagnostic entry
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [ValidateSet("DEBUG", "INFO", "WARNING", "ERROR")]
        [string]$Level = "INFO"
    )
    
    $entry = @{
        Timestamp = Get-Date
        Level = $Level
        Message = $Message
    }
    
    $script:DiagnosticLog += $entry
    
    if ($script:DiagnosticLog.Count -gt $script:MaxDiagnosticEntries) {
        $script:DiagnosticLog = $script:DiagnosticLog[-$script:MaxDiagnosticEntries..-1]
    }
    
    return @{
        Success = $true
        Timestamp = $entry.Timestamp
    }
}

function Get-EditorDiagnostics {
    <#
    .SYNOPSIS
        Get editor diagnostics
    #>
    param(
        [int]$Last = 100,
        [string]$Level = ""
    )
    
    $diagnostics = $script:DiagnosticLog
    
    if ($Level) {
        $diagnostics = $diagnostics | Where-Object { $_.Level -eq $Level }
    }
    
    return $diagnostics | Select-Object -Last $Last
}

function Get-EditorErrorReport {
    <#
    .SYNOPSIS
        Get comprehensive error report
    #>
    return @{
        ErrorCount = $script:EditorErrors.Count
        Errors = $script:EditorErrors
        DiagnosticEntriesCount = $script:DiagnosticLog.Count
        Timestamp = Get-Date
    }
}

# ============================================
# INITIALIZATION
# ============================================

Write-Host "[editor-diagnostics] Module loaded successfully" -ForegroundColor Green

# Note: This is a dot-sourced PS1 script, not a PSM1 module
# Functions are automatically available in parent scope
