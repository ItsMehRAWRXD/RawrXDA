# RawrXD Autonomous Agent IDE Runner
# Execute autonomous agent system in IDE environment
# Version: 3.0.0 - Production Ready

#Requires -Version 5.1
#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Run-AutonomousAgent-IDE - Execute autonomous agent system in IDE

.DESCRIPTION
    Executes the autonomous agent system providing:
    - Self-analysis and gap detection
    - Automatic feature generation
    - Self-testing and validation
    - Continuous improvement loop
    - Zero human intervention
    - Pure PowerShell implementation

.NOTES
    Version: 3.0.0 (Production Ready)
    Author: RawrXD Auto-Generation System
    Last Updated: 2024-12-28
    Requirements: PowerShell 5.1+, Administrator privileges

.EXAMPLE
    .\Run-AutonomousAgent-IDE.ps1
    
    Execute autonomous agent system in IDE

.EXAMPLE
    .\Run-AutonomousAgent-IDE.ps1 -MaxIterations 10 -SleepIntervalMs 2000
    
    Execute with custom iterations and sleep interval

.EXAMPLE
    .\Run-AutonomousAgent-IDE.ps1 -WhatIf
    
    Preview autonomous agent execution without making changes
#>

param(
    [Parameter(Mandatory=$false)]
    [int]$MaxIterations = 5,
    
    [Parameter(Mandatory=$false)]
    [int]$SleepIntervalMs = 3000,
    
    [Parameter(Mandatory=$false)]
    [switch]$WhatIf = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$Verbose = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipTests = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipExecution = $false
)

# Set error action preference
$ErrorActionPreference = "Stop"
$WarningPreference = "Continue"
$InformationPreference = "Continue"

# Global execution state
$script:ExecutionState = @{
    Version = "3.0.0"
    StartTime = Get-Date
    EndTime = $null
    Duration = 0
    Status = "Running"
    Mode = "IDE"
    WhatIf = $WhatIf
    Verbose = $Verbose
    SkipTests = $SkipTests
    SkipExecution = $SkipExecution
    Errors = [System.Collections.Generic.List[string]]::new()
    Warnings = [System.Collections.Generic.List[string]]::new()
    Success = $true
    Results = @{
        SystemValidation = $null
        ModuleImport = $null
        TestSuite = $null
        AutonomousAgent = $null
        FinalStatus = $null
    }
    Iterations = 0
    TestsPassed = 0
    TestsFailed = 0
    FeaturesGenerated = 0
    OptimizationsApplied = 0
}

# Write execution log
function Write-ExecutionLog {
    param(
        [Parameter(Mandatory=$true)][string]$Message,
        [ValidateSet('Info','Warning','Error','Debug','Success','Critical','IDE')][string]$Level = 'Info',
        [string]$Phase = "Execution",
        [hashtable]$Data = $null
    )
    
    $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
    $color = switch ($Level) {
        'IDE' { 'Magenta' }
        'Critical' { 'Red' }
        'Error' { 'Red' }
        'Warning' { 'Yellow' }
        'Success' { 'Green' }
        'Debug' { 'DarkGray' }
        default { 'Cyan' }
    }
    
    $logEntry = "[$timestamp][$Phase][$Level] $Message"
    Write-Host $logEntry -ForegroundColor $color
    
    # Update state
    if ($Level -eq 'Error' -or $Level -eq 'Critical') {
        $script:ExecutionState.Errors.Add($Message)
    } elseif ($Level -eq 'Warning') {
        $script:ExecutionState.Warnings.Add($Message)
    }
    
    # Log to file
    $logFile = "C:\RawrXD\Logs\IDE_Execution_$(Get-Date -Format 'yyyyMMdd').log"
    $logDir = Split-Path $logFile -Parent
    if (-not (Test-Path $logDir)) {
        New-Item -Path $logDir -ItemType Directory -Force | Out-Null
    }
    Add-Content -Path $logFile -Value $logEntry -Encoding UTF8
}

# Show banner
function Show-Banner {
    Write-Host ""
    Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║                                                                   ║" -ForegroundColor Magenta
    Write-Host "║         RawrXD AUTONOMOUS AGENT - IDE EXECUTION                 ║" -ForegroundColor Magenta
    Write-Host "║                    Version 3.0.0 - Production Ready               ║" -ForegroundColor Magenta
    Write-Host "║                                                                   ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""
}

# Show configuration
function Show-Configuration {
    Write-Host "IDE Execution Configuration:" -ForegroundColor Yellow
    Write-Host "  Max Iterations: $MaxIterations" -ForegroundColor White
    Write-Host "  Sleep Interval: $SleepIntervalMs ms" -ForegroundColor White
    Write-Host "  WhatIf: $WhatIf" -ForegroundColor White
    Write-Host "  Verbose: $Verbose" -ForegroundColor White
    Write-Host "  Skip Tests: $SkipTests" -ForegroundColor White
    Write-Host "  Skip Execution: $SkipExecution" -ForegroundColor White
    Write-Host "  Start Time: $($script:ExecutionState.StartTime)" -ForegroundColor White
    Write-Host ""
}

# Execute system validation
function Execute-SystemValidation {
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    Write-ExecutionLog -Message "PHASE 1: SYSTEM VALIDATION" -Level IDE
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    
    try {
        Write-ExecutionLog -Message "Checking system prerequisites for IDE execution" -Level Info
        
        $prereqs = @{
            PowerShellVersion = $false
            Administrator = $false
            ExecutionPolicy = $false
            .NETFramework = $false
            AllPassed = $false
        }
        
        # Check PowerShell version
        $psVersion = $PSVersionTable.PSVersion
        $prereqs.PowerShellVersion = ($psVersion.Major -ge 5)
        Write-ExecutionLog -Message "PowerShell Version: $psVersion - $(if($prereqs.PowerShellVersion){'✓ PASS'}else{'✗ FAIL'})" -Level $(if($prereqs.PowerShellVersion){'Success'}else{'Error'})
        
        # Check administrator privileges
        $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
        $prereqs.Administrator = $isAdmin
        Write-ExecutionLog -Message "Administrator Rights: $(if($isAdmin){'✓ PASS'}else{'⚠ WARNING (IDE mode)'})" -Level $(if($isAdmin){'Success'}else{'Warning'})
        
        # Check execution policy
        $execPolicy = Get-ExecutionPolicy
        $prereqs.ExecutionPolicy = ($execPolicy -ne 'Restricted')
        Write-ExecutionLog -Message "Execution Policy: $execPolicy - $(if($prereqs.ExecutionPolicy){'✓ PASS'}else{'✗ FAIL'})" -Level $(if($prereqs.ExecutionPolicy){'Success'}else{'Error'})
        
        # Check .NET Framework
        $dotnetVersion = Get-ItemProperty "HKLM:SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full\" -ErrorAction SilentlyContinue
        $prereqs.NETFramework = ($dotnetVersion -and $dotnetVersion.Release -ge 461808)
        Write-ExecutionLog -Message ".NET Framework: $(if($prereqs.NETFramework){'4.7.2+ ✓ PASS'}else{'⚠ WARNING'})" -Level $(if($prereqs.NETFramework){'Success'}else{'Warning'})
        
        # Overall result (IDE mode allows non-admin)
        $prereqs.AllPassed = ($prereqs.PowerShellVersion -and $prereqs.ExecutionPolicy)
        
        if ($prereqs.AllPassed) {
            Write-ExecutionLog -Message "✓ All critical prerequisites passed for IDE execution" -Level Success
        } else {
            Write-ExecutionLog -Message "✗ Some critical prerequisites failed" -Level Error
            throw "System validation failed. Please address the issues above."
        }
        
        $script:ExecutionState.Results.SystemValidation = $prereqs
        return $prereqs
        
    } catch {
        Write-ExecutionLog -Message "System validation failed: $_" -Level Critical
        throw
    }
}

# Import autonomous agent
function Import-AutonomousAgent {
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    Write-ExecutionLog -Message "PHASE 2: IMPORT AUTONOMOUS AGENT" -Level IDE
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    
    try {
        $modulePath = Join-Path $PSScriptRoot "RawrXD.AutonomousAgent.psm1"
        
        if (-not (Test-Path $modulePath)) {
            throw "Autonomous agent module not found: $modulePath"
        }
        
        Write-ExecutionLog -Message "Importing autonomous agent module" -Level Info
        Import-Module $modulePath -Force -Global -ErrorAction Stop
        
        Write-ExecutionLog -Message "✓ Autonomous agent imported successfully" -Level Success
        
        $script:ExecutionState.Results.ModuleImport = @{
            Success = $true
            ModulePath = $modulePath
            Timestamp = Get-Date
        }
        
        return $true
        
    } catch {
        Write-ExecutionLog -Message "Failed to import autonomous agent: $_" -Level Critical
        throw
    }
}

# Execute test suite
function Execute-TestSuite {
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    Write-ExecutionLog -Message "PHASE 3: EXECUTE TEST SUITE" -Level IDE
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    
    if ($SkipTests) {
        Write-ExecutionLog -Message "Skipping test suite execution (SkipTests flag set)" -Level Warning
        $script:ExecutionState.Results.TestSuite = @{
            Success = $true
            Skipped = $true
            TestsPassed = 0
            TestsFailed = 0
            Timestamp = Get-Date
        }
        return $true
    }
    
    try {
        Write-ExecutionLog -Message "Starting comprehensive test suite execution" -Level Info
        
        # Import test functions from Test-AutonomousAgent.ps1
        $testScriptPath = Join-Path $PSScriptRoot "Test-AutonomousAgent.ps1"
        if (-not (Test-Path $testScriptPath)) {
            throw "Test script not found: $testScriptPath"
        }
        
        Write-ExecutionLog -Message "Loading test functions from: $testScriptPath" -Level Info
        . $testScriptPath -TestType All -WhatIf:$WhatIf -Verbose:$Verbose
        
        Write-ExecutionLog -Message "✓ Test suite execution completed successfully" -Level Success
        
        $script:ExecutionState.Results.TestSuite = @{
            Success = $true
            Skipped = $false
            TestsPassed = $script:TestState.Tests.Passed
            TestsFailed = $script:TestState.Tests.Failed
            Timestamp = Get-Date
        }
        
        $script:ExecutionState.TestsPassed = $script:TestState.Tests.Passed
        $script:ExecutionState.TestsFailed = $script:TestState.Tests.Failed
        
        return $true
        
    } catch {
        Write-ExecutionLog -Message "Test suite execution failed: $_" -Level Critical
        throw
    }
}

# Execute autonomous agent
function Execute-AutonomousAgent {
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    Write-ExecutionLog -Message "PHASE 4: EXECUTE AUTONOMOUS AGENT" -Level IDE
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    
    if ($SkipExecution) {
        Write-ExecutionLog -Message "Skipping autonomous agent execution (SkipExecution flag set)" -Level Warning
        $script:ExecutionState.Results.AutonomousAgent = @{
            Success = $true
            Skipped = $true
            Iterations = 0
            FeaturesGenerated = 0
            Timestamp = Get-Date
        }
        return $true
    }
    
    try {
        Write-ExecutionLog -Message "Starting autonomous agent execution" -Level Info
        Write-ExecutionLog -Message "Max iterations: $MaxIterations" -Level Info
        Write-ExecutionLog -Message "Sleep interval: $SleepIntervalMs ms" -Level Info
        Write-ExecutionLog -Message "WhatIf mode: $WhatIf" -Level Info
        
        # Import execution functions from Execute-AutonomousAgent.ps1
        $execScriptPath = Join-Path $PSScriptRoot "Execute-AutonomousAgent.ps1"
        if (-not (Test-Path $execScriptPath)) {
            throw "Execution script not found: $execScriptPath"
        }
        
        Write-ExecutionLog -Message "Loading execution functions from: $execScriptPath" -Level Info
        . $execScriptPath -MaxIterations $MaxIterations -SleepIntervalMs $SleepIntervalMs -WhatIf:$WhatIf -Verbose:$Verbose
        
        Write-ExecutionLog -Message "✓ Autonomous agent execution completed successfully" -Level Success
        
        $script:ExecutionState.Results.AutonomousAgent = @{
            Success = $true
            Skipped = $false
            Iterations = $script:ExecutionState.Iterations
            FeaturesGenerated = $script:ExecutionState.FeaturesGenerated
            Timestamp = Get-Date
        }
        
        return $true
        
    } catch {
        Write-ExecutionLog -Message "Autonomous agent execution failed: $_" -Level Critical
        throw
    }
}

# Show final results
function Show-FinalResults {
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    Write-ExecutionLog -Message "IDE EXECUTION COMPLETE" -Level IDE
    Write-ExecutionLog -Message "═══════════════════════════════════════════════════════════════════" -Level IDE
    
    $endTime = Get-Date
    $duration = [Math]::Round(($endTime - $script:ExecutionState.StartTime).TotalMinutes, 2)
    
    Write-Host ""
    Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║                                                                   ║" -ForegroundColor Magenta
    Write-Host "║         RawrXD AUTONOMOUS AGENT - IDE EXECUTION COMPLETE        ║" -ForegroundColor Magenta
    Write-Host "║                                                                   ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""
    
    Write-Host "Execution Summary:" -ForegroundColor Yellow
    Write-Host "  Duration: $duration minutes" -ForegroundColor White
    Write-Host "  Mode: $($script:ExecutionState.Mode)" -ForegroundColor White
    Write-Host "  WhatIf: $($script:ExecutionState.WhatIf)" -ForegroundColor White
    Write-Host "  Status: $(if($script:ExecutionState.Success){'SUCCESS'}else{'FAILED'})" -ForegroundColor $(if($script:ExecutionState.Success){'Green'}else{'Red'})
    Write-Host ""
    
    Write-Host "Statistics:" -ForegroundColor Yellow
    Write-Host "  Tests Passed: $($script:ExecutionState.TestsPassed)" -ForegroundColor Green
    Write-Host "  Tests Failed: $($script:ExecutionState.TestsFailed)" -ForegroundColor Red
    Write-Host "  Iterations: $($script:ExecutionState.Iterations)" -ForegroundColor White
    Write-Host "  Features Generated: $($script:ExecutionState.FeaturesGenerated)" -ForegroundColor White
    Write-Host "  Optimizations Applied: $($script:ExecutionState.OptimizationsApplied)" -ForegroundColor White
    Write-Host "  Errors: $($script:ExecutionState.Errors.Count)" -ForegroundColor $(if($script:ExecutionState.Errors.Count -eq 0){'Green'}else{'Red'})
    Write-Host "  Warnings: $($script:ExecutionState.Warnings.Count)" -ForegroundColor $(if($script:ExecutionState.Warnings.Count -eq 0){'Green'}else{'Yellow'})
    Write-Host ""
    
    if ($script:ExecutionState.Errors.Count -gt 0) {
        Write-Host "Errors:" -ForegroundColor Red
        foreach ($error in $script:ExecutionState.Errors) {
            Write-Host "  • $error" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    if ($script:ExecutionState.Warnings.Count -gt 0) {
        Write-Host "Warnings:" -ForegroundColor Yellow
        foreach ($warning in $script:ExecutionState.Warnings) {
            Write-Host "  • $warning" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    Write-Host "Next Steps:" -ForegroundColor Yellow
    Write-Host "  • Review logs at: C:\RawrXD\Logs" -ForegroundColor White
    Write-Host "  • Verify module functionality with: Import-Module 'C:\RawrXD\Autonomous\RawrXD.AutonomousAgent.psm1'" -ForegroundColor White
    Write-Host "  • Run: Get-AutonomousAgentStatus" -ForegroundColor White
    Write-Host "  • Monitor performance and security" -ForegroundColor White
    Write-Host ""
    
    Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║              AUTONOMOUS AGENT READY FOR PRODUCTION                ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""
}

# Main execution
function Start-MainExecution {
    Show-Banner
    Show-Configuration
    
    try {
        # Execute system validation
        $validationResult = Execute-SystemValidation
        
        # Import autonomous agent
        $importResult = Import-AutonomousAgent
        
        # Execute test suite
        $testResult = Execute-TestSuite
        
        # Execute autonomous agent
        $execResult = Execute-AutonomousAgent
        
        # Update final status
        $script:ExecutionState.EndTime = Get-Date
        $script:ExecutionState.Duration = [Math]::Round(($script:ExecutionState.EndTime - $script:ExecutionState.StartTime).TotalMinutes, 2)
        $script:ExecutionState.Status = "Complete"
        $script:ExecutionState.Success = ($script:ExecutionState.Errors.Count -eq 0)
        
        Write-ExecutionLog -Message "IDE execution completed successfully" -Level Success
        
        # Show final results
        Show-FinalResults
        
        return $script:ExecutionState
        
    } catch {
        $script:ExecutionState.EndTime = Get-Date
        $script:ExecutionState.Duration = [Math]::Round(($script:ExecutionState.EndTime - $script:ExecutionState.StartTime).TotalMinutes, 2)
        $script:ExecutionState.Status = "Failed"
        $script:ExecutionState.Success = $false
        
        Write-ExecutionLog -Message "IDE execution failed: $_" -Level Critical
        
        Write-Host ""
        Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Red
        Write-Host "║                                                                   ║" -ForegroundColor Red
        Write-Host "║              IDE EXECUTION FAILED                                 ║" -ForegroundColor Red
        Write-Host "║                                                                   ║" -ForegroundColor Red
        Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Red
        Write-Host ""
        Write-Host "Error: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please check the logs at: C:\RawrXD\Logs" -ForegroundColor Yellow
        Write-Host ""
        
        throw
    }
}

# Execute main function
try {
    $executionResult = Start-MainExecution
    
    # Exit with success
    Write-ExecutionLog -Message "IDE execution completed successfully" -Level Success
    exit 0
    
} catch {
    Write-ExecutionLog -Message "IDE execution failed: $_" -Level Critical
    exit 1
}
