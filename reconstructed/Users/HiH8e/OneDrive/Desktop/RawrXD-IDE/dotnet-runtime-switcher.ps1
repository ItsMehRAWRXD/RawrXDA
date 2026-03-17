#Requires -Version 5.1
<#
.SYNOPSIS
    .NET Runtime Switcher Module
    
.DESCRIPTION
    Provides utilities to switch between different .NET runtime versions
    for development and debugging purposes.
#>

# ============================================
# MODULE VARIABLES
# ============================================

$script:AvailableRuntimes = @()
$script:CurrentRuntime = $null
$script:RuntimeSwitchHistory = @()

# ============================================
# RUNTIME DETECTION
# ============================================

function Detect-DotNetRuntimes {
    <#
    .SYNOPSIS
        Detect installed .NET runtimes
    #>
    try {
        $dotnetPath = $null
        
        # Try to find dotnet CLI
        $dotnet = Get-Command dotnet -ErrorAction SilentlyContinue
        if ($dotnet) {
            $dotnetPath = $dotnet.Source
        }
        
        if ($dotnetPath) {
            $runtimes = & dotnet --list-runtimes 2>$null
            
            $parsed = @()
            foreach ($runtime in $runtimes) {
                if ($runtime) {
                    $parsed += @{
                        Name = $runtime
                        Installed = $true
                    }
                }
            }
            
            $script:AvailableRuntimes = $parsed
            
            return @{
                Success = $true
                RuntimeCount = $parsed.Count
                Runtimes = $parsed
            }
        }
        else {
            return @{
                Success = $false
                Message = ".NET CLI not found"
            }
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

function Get-CurrentRuntime {
    <#
    .SYNOPSIS
        Get the current .NET runtime version
    #>
    try {
        $current = & dotnet --version 2>$null
        
        return @{
            Success = $true
            Version = $current
            Timestamp = Get-Date
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

# ============================================
# INITIALIZATION
# ============================================

Write-Host "[dotnet-runtime-switcher] Module loaded successfully" -ForegroundColor Green

# Note: This is a dot-sourced PS1 script, not a PSM1 module
# Functions are automatically available in parent scope
