# RawrXD Ultimate Production Deployment Script
# Version: 3.0.0
# Description: Complete production deployment orchestrator for RawrXD autonomous agent system

#Requires -Version 5.1

param(
    [string]$ConfigPath = "C:\RawrXD\Production\config.json",
    [string]$Mode = "Maximum",
    [switch]$SkipTesting,
    [switch]$SkipOptimization,
    [switch]$SkipSecurity,
    [switch]$DryRun
)

# Global variables
$Script:DeploymentState = @{
    Version = "3.0.0"
    StartTime = Get-Date
    Phase = "Initialization"
    Status = "Running"
    Mode = $Mode
    Errors = [System.Collections.Generic.List[string]]::new()
    Warnings = [System.Collections.Generic.List[string]]::new()
    Config = $null
}

# Write deployment log
function Write-DeploymentLog {
    param(
        [string]$Message,
        [string]$Level = "Info",
        [hashtable]$Data = @{}
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $color = switch ($Level) {
        "Success" { "Green" }
        "Info" { "White" }
        "Warning" { "Yellow" }
        "Error" { "Red" }
        "Critical" { "DarkRed" }
        default { "White" }
    }
    
    Write-Host "[$timestamp][$Level] $Message" -ForegroundColor $color
    
    # Log to file if configured
    if ($Script:DeploymentState.Config -and $Script:DeploymentState.Config.deployment.logPath) {
        $logPath = $Script:DeploymentState.Config.deployment.logPath
        $logDir = Split-Path $logPath -Parent
        if (-not (Test-Path $logDir)) {
            New-Item -Path $logDir -ItemType Directory -Force | Out-Null
        }
        
        $logEntry = "[$timestamp][$Level] $Message"
        if ($Data.Count -gt 0) {
            $logEntry += " | Data: $(ConvertTo-Json $Data -Compress)"
        }
        
        Add-Content -Path $logPath -Value $logEntry
    }
}

# Load configuration
function Load-Configuration {
    param([string]$ConfigPath)
    
    Write-DeploymentLog -Message "Loading configuration from: $ConfigPath" -Level Info
    
    if (-not (Test-Path $ConfigPath)) {
        throw "Configuration file not found: $ConfigPath"
    }
    
    try {
        $config = Get-Content $ConfigPath -Raw | ConvertFrom-Json
        $Script:DeploymentState.Config = $config
        Write-DeploymentLog -Message "Configuration loaded successfully" -Level Success
        return $config
    } catch {
        throw "Failed to load configuration: $_"
    }
}

# Validate system prerequisites
function Test-SystemPrerequisites {
    Write-DeploymentLog -Message "Testing system prerequisites" -Level Info
    
    $prerequisites = @{
        PowerShellVersion = $false
        AdminRights = $false
        ExecutionPolicy = $false
        DotNetFramework = $false
        DiskSpace = $false
        Memory = $false
    }
    
    # Check PowerShell version
    $psVersion = $PSVersionTable.PSVersion
    $minVersion = [version]"5.1"
    if ($psVersion -ge $minVersion) {
        $prerequisites.PowerShellVersion = $true
        Write-DeploymentLog -Message "PowerShell Version: $psVersion - ✓ PASS" -Level Success
    } else {
        Write-DeploymentLog -Message "PowerShell Version: $psVersion - ✗ FAIL (Minimum: $minVersion)" -Level Error
    }
    
    # Check admin rights
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]"Administrator")
    if ($isAdmin) {
        $prerequisites.AdminRights = $true
        Write-DeploymentLog -Message "Administrator Rights: ✓ PASS" -Level Success
    } else {
        Write-DeploymentLog -Message "Administrator Rights: ✗ FAIL" -Level Error
    }
    
    # Check execution policy
    $executionPolicy = Get-ExecutionPolicy
    if ($executionPolicy -ne "Restricted") {
        $prerequisites.ExecutionPolicy = $true
        Write-DeploymentLog -Message "Execution Policy: $executionPolicy - ✓ PASS" -Level Success
    } else {
        Write-DeploymentLog -Message "Execution Policy: $executionPolicy - ✗ FAIL" -Level Error
    }
    
    # Check .NET Framework
    $dotNetVersion = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" -Name Release -ErrorAction SilentlyContinue
    if ($dotNetVersion -and $dotNetVersion.Release -ge 461808) {
        $prerequisites.DotNetFramework = $true
        Write-DeploymentLog -Message ".NET Framework: 4.7.2+ ✓ PASS" -Level Success
    } else {
        Write-DeploymentLog -Message ".NET Framework: ✗ FAIL (Minimum: 4.7.2)" -Level Error
    }
    
    # Check disk space
    $drive = Get-PSDrive -Name "C"
    $freeSpaceGB = [Math]::Round($drive.Free / 1GB, 2)
    $minSpaceGB = $Script:DeploymentState.Config.system.minimumDiskSpaceGB
    if ($freeSpaceGB -gt $minSpaceGB) {
        $prerequisites.DiskSpace = $true
        Write-DeploymentLog -Message "Disk Space: $freeSpaceGB GB free - ✓ PASS" -Level Success
    } else {
        Write-DeploymentLog -Message "Disk Space: $freeSpaceGB GB free - ✗ FAIL (Minimum: $minSpaceGB GB)" -Level Error
    }
    
    # Check memory
    $memoryInfo = Get-CimInstance -ClassName Win32_OperatingSystem
    $freeMemoryGB = [Math]::Round($memoryInfo.FreePhysicalMemory / 1MB, 2)
    $minMemoryGB = $Script:DeploymentState.Config.system.minimumMemoryGB
    if ($freeMemoryGB -gt $minMemoryGB) {
        $prerequisites.Memory = $true
        Write-DeploymentLog -Message "Memory: $freeMemoryGB GB free - ✓ PASS" -Level Success
    } else {
        Write-DeploymentLog -Message "Memory: $freeMemoryGB GB free - ✗ FAIL (Minimum: $minMemoryGB GB)" -Level Error
    }
    
    # Check if all prerequisites are met
    $allPassed = $prerequisites.Values | Where-Object { $_ -eq $false } | Measure-Object | Select-Object -ExpandProperty Count
    $allPassed = $allPassed -eq 0
    
    if ($allPassed) {
        Write-DeploymentLog -Message "All system prerequisites passed" -Level Success
    } else {
        Write-DeploymentLog -Message "Some prerequisites failed. Check the errors above." -Level Error
    }
    
    return $prerequisites
}

# Import deployment orchestrator module
function Import-DeploymentOrchestrator {
    param([string]$ModulePath)
    
    Write-DeploymentLog -Message "Importing deployment orchestrator module" -Level Info
    
    if (-not (Test-Path $ModulePath)) {
        throw "Deployment orchestrator module not found: $ModulePath"
    }
    
    try {
        Import-Module $ModulePath -Force -ErrorAction Stop
        Write-DeploymentLog -Message "Deployment orchestrator module imported successfully" -Level Success
    } catch {
        throw "Failed to import deployment orchestrator module: $_"
    }
}

# Execute deployment pipeline
function Invoke-DeploymentPipeline {
    param($Config)
    
    Write-DeploymentLog -Message "Starting deployment pipeline" -Level Info
    
    try {
        # Import the orchestrator module
        $modulePath = "C:\RawrXD\Production\RawrXD.DeploymentOrchestrator.psm1"
        Import-DeploymentOrchestrator -ModulePath $modulePath
        
        # Execute the complete deployment pipeline
        $result = Invoke-CompleteDeploymentPipeline @{
            SourcePath = $Config.deployment.sourcePath
            TargetPath = $Config.deployment.targetPath
            LogPath = $Config.deployment.logPath
            BackupPath = $Config.deployment.backupPath
            Mode = $Config.deployment.mode
            SkipTesting = $Config.deployment.skipTesting
            SkipOptimization = $Config.deployment.skipOptimization
            SkipSecurity = $Config.deployment.skipSecurity
        }
        
        Write-DeploymentLog -Message "Deployment pipeline completed successfully" -Level Success
        return $result
        
    } catch {
        Write-DeploymentLog -Message "Deployment pipeline failed: $_" -Level Critical
        throw
    }
}

# Main execution
function Start-UltimateDeployment {
    param(
        [string]$ConfigPath,
        [string]$Mode,
        [switch]$SkipTesting,
        [switch]$SkipOptimization,
        [switch]$SkipSecurity,
        [switch]$DryRun
    )
    
    Write-Host ""
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host "           RAW RX D ULTIMATE DEPLOYMENT" -ForegroundColor Cyan
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host "Version: 3.0.0 | Mode: $Mode | DryRun: $DryRun" -ForegroundColor White
    Write-Host ""
    
    try {
        # Update deployment state
        $Script:DeploymentState.StartTime = Get-Date
        $Script:DeploymentState.Status = "Running"
        
        # Load configuration
        $config = Load-Configuration -ConfigPath $ConfigPath
        
        # Override configuration with parameters
        if ($Mode -ne "Maximum") { $config.deployment.mode = $Mode }
        if ($SkipTesting) { $config.deployment.skipTesting = $true }
        if ($SkipOptimization) { $config.deployment.skipOptimization = $true }
        if ($SkipSecurity) { $config.deployment.skipSecurity = $true }
        
        # Test system prerequisites
        $prerequisites = Test-SystemPrerequisites
        
        # Check if all prerequisites passed
        $failedPrerequisites = $prerequisites.Values | Where-Object { $_ -eq $false } | Measure-Object | Select-Object -ExpandProperty Count
        
        if ($failedPrerequisites -gt 0 -and -not $DryRun) {
            throw "System prerequisites failed. Please fix the issues above and try again."
        }
        
        if ($DryRun) {
            Write-DeploymentLog -Message "Dry run mode enabled - skipping actual deployment" -Level Warning
            Write-DeploymentLog -Message "Configuration validated successfully" -Level Success
            return @{
                Success = $true
                DryRun = $true
                Message = "Configuration validated successfully in dry run mode"
            }
        }
        
        # Execute deployment pipeline
        $deploymentResult = Invoke-DeploymentPipeline -Config $config
        
        # Update deployment state
        $Script:DeploymentState.Status = "Completed"
        $Script:DeploymentState.EndTime = Get-Date
        
        Write-Host ""
        Write-Host "==================================================" -ForegroundColor Green
        Write-Host "           DEPLOYMENT COMPLETED SUCCESSFULLY" -ForegroundColor Green
        Write-Host "==================================================" -ForegroundColor Green
        Write-Host ""
        
        return $deploymentResult
        
    } catch {
        $Script:DeploymentState.Status = "Failed"
        $Script:DeploymentState.EndTime = Get-Date
        $Script:DeploymentState.Errors.Add($_)
        
        Write-Host ""
        Write-Host "==================================================" -ForegroundColor Red
        Write-Host "           DEPLOYMENT FAILED" -ForegroundColor Red
        Write-Host "==================================================" -ForegroundColor Red
        Write-Host "Error: $_" -ForegroundColor Red
        Write-Host ""
        
        throw
    }
}

# Execute main function
try {
    $result = Start-UltimateDeployment @PSBoundParameters
    exit 0
} catch {
    exit 1
}