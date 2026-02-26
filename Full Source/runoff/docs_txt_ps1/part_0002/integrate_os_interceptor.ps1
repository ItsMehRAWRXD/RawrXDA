#============================================================================
# OS Explorer Interceptor - MASM IDE Integration Script
# Integrates the interceptor into the MASM IDE with PowerShell streaming
#============================================================================

<#
.SYNOPSIS
    Integrates OS Explorer Interceptor into MASM IDE

.DESCRIPTION
    This script integrates the OS Explorer Interceptor into the MASM IDE,
    providing real-time PowerShell streaming and CLI control

.PARAMETER Build
    Build the interceptor components before integration

.PARAMETER Install
    Install the interceptor into the MASM IDE

.PARAMETER Test
    Test the integration

.PARAMETER ProcessId
    Process ID to intercept (for testing)

.PARAMETER ProcessName
    Process name to intercept (for testing)

.EXAMPLE
    .\integrate_os_interceptor.ps1 -Build -Install
    
.EXAMPLE
    .\integrate_os_interceptor.ps1 -Test -ProcessId 1234
    
.EXAMPLE
    .\integrate_os_interceptor.ps1 -Test -ProcessName "cursor"
#>

[CmdletBinding()]
param(
    [Parameter()]
    [switch]$Build,
    
    [Parameter()]
    [switch]$Install,
    
    [Parameter()]
    [switch]$Test,
    
    [Parameter()]
    [int]$ProcessId = 0,
    
    [Parameter()]
    [string]$ProcessName = ""
)

# Global variables
$script:InterceptorPath = Join-Path $PSScriptRoot "bin\os_explorer_interceptor.dll"
$script:CLIPath = Join-Path $PSScriptRoot "bin\os_interceptor_cli.exe"
$script:PowerShellModulePath = Join-Path $PSScriptRoot "modules\OSExplorerInterceptor.psm1"
$script:MASMIDEPath = "C:\MASM\IDE"  # Adjust this to your MASM IDE path

#============================================================================
# BUILD FUNCTION
#============================================================================

function Build-OSInterceptor {
    Write-Host "Building OS Explorer Interceptor..." -ForegroundColor Cyan
    
    # Check if build script exists
    $buildScript = Join-Path $PSScriptRoot "build_os_interceptor.bat"
    if (-not (Test-Path $buildScript)) {
        Write-Error "Build script not found: $buildScript"
        return $false
    }
    
    # Run build script
    try {
        & $buildScript
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[SUCCESS] Build completed successfully" -ForegroundColor Green
            return $true
        } else {
            Write-Error "Build failed with exit code: $LASTEXITCODE"
            return $false
        }
    } catch {
        Write-Error "Build failed: $_"
        return $false
    }
}

#============================================================================
# INSTALL FUNCTION
#============================================================================

function Install-OSInterceptor {
    Write-Host "Installing OS Explorer Interceptor into MASM IDE..." -ForegroundColor Cyan
    
    # Check if interceptor files exist
    $requiredFiles = @(
        $script:InterceptorPath,
        $script:CLIPath,
        $script:PowerShellModulePath
    )
    
    foreach ($file in $requiredFiles) {
        if (-not (Test-Path $file)) {
            Write-Error "Required file not found: $file"
            Write-Host "Run with -Build flag to build the components first" -ForegroundColor Yellow
            return $false
        }
    }
    
    # Check if MASM IDE path exists
    if (-not (Test-Path $script:MASMIDEPath)) {
        Write-Warning "MASM IDE path not found: $script:MASMIDEPath"
        Write-Host "Please update the MASMIDEPath variable in this script" -ForegroundColor Yellow
    }
    
    # Copy files to MASM IDE directory
    try {
        # Create directories if needed
        $ideBinPath = Join-Path $script:MASMIDEPath "bin"
        $ideModulesPath = Join-Path $script:MASMIDEPath "modules"
        
        if (-not (Test-Path $ideBinPath)) {
            New-Item -Path $ideBinPath -ItemType Directory -Force | Out-Null
        }
        
        if (-not (Test-Path $ideModulesPath)) {
            New-Item -Path $ideModulesPath -ItemType Directory -Force | Out-Null
        }
        
        # Copy interceptor DLL
        Copy-Item -Path $script:InterceptorPath -Destination $ideBinPath -Force
        Write-Host "  Copied: os_explorer_interceptor.dll" -ForegroundColor Green
        
        # Copy CLI executable
        Copy-Item -Path $script:CLIPath -Destination $ideBinPath -Force
        Write-Host "  Copied: os_interceptor_cli.exe" -ForegroundColor Green
        
        # Copy PowerShell module
        Copy-Item -Path $script:PowerShellModulePath -Destination $ideModulesPath -Force
        Write-Host "  Copied: OSExplorerInterceptor.psm1" -ForegroundColor Green
        
        # Create integration script in MASM IDE
        $integrationScript = @"
# OS Explorer Interceptor - MASM IDE Integration
# This script is automatically loaded by the MASM IDE

# Import the interceptor module
Import-Module "`$PSScriptRoot\modules\OSExplorerInterceptor.psm1" -Force

# Set up aliases for quick access
Set-Alias -Name osi -Value Start-OSInterceptor
Set-Alias -Name osx -Value Stop-OSInterceptor
Set-Alias -Name oss -Value Get-OSInterceptorStatus
Set-Alias -Name osst -Value Get-OSInterceptorStats
Set-Alias -Name osc -Value Clear-OSInterceptorLog
Set-Alias -Name osh -Value Show-OSInterceptorHelp

# Display welcome message
Write-Host "OS Explorer Interceptor integrated into MASM IDE" -ForegroundColor Green
Write-Host "Type 'osh' for help or 'osi -ProcessId <PID> -RealTimeStreaming' to start" -ForegroundColor Cyan

# Auto-start if process ID provided in environment
if (`$env:OSINTERCEPTOR_PID) {
    try {
        Start-OSInterceptor -ProcessId `$env:OSINTERCEPTOR_PID -RealTimeStreaming
    } catch {
        Write-Warning "Failed to auto-start interceptor: `$_"
    }
}
"@
        
        $integrationScriptPath = Join-Path $script:MASMIDEPath "os_interceptor_integration.ps1"
        $integrationScript | Out-File -FilePath $integrationScriptPath -Encoding UTF8
        Write-Host "  Created: os_interceptor_integration.ps1" -ForegroundColor Green
        
        Write-Host "[SUCCESS] Installation completed successfully" -ForegroundColor Green
        Write-Host ""
        Write-Host "To use in MASM IDE:" -ForegroundColor Cyan
        Write-Host "  1. Open MASM IDE PowerShell terminal" -ForegroundColor White
        Write-Host "  2. Run: .\os_interceptor_integration.ps1" -ForegroundColor White
        Write-Host "  3. Or use: Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming" -ForegroundColor White
        Write-Host ""
        Write-Host "Quick commands:" -ForegroundColor Cyan
        Write-Host "  osi 1234 -RealTimeStreaming  # Start interceptor" -ForegroundColor White
        Write-Host "  osx                          # Stop interceptor" -ForegroundColor White
        Write-Host "  oss                          # Show status" -ForegroundColor White
        Write-Host "  osst                         # Show statistics" -ForegroundColor White
        Write-Host "  osh                          # Show help" -ForegroundColor White
        
        return $true
        
    } catch {
        Write-Error "Installation failed: $_"
        return $false
    }
}

#============================================================================
# TEST FUNCTION
#============================================================================

function Test-OSInterceptor {
    param(
        [int]$ProcessId = 0,
        [string]$ProcessName = ""
    )
    
    Write-Host "Testing OS Explorer Interceptor integration..." -ForegroundColor Cyan
    
    # Check if interceptor files exist
    $requiredFiles = @(
        $script:InterceptorPath,
        $script:CLIPath,
        $script:PowerShellModulePath
    )
    
    foreach ($file in $requiredFiles) {
        if (Test-Path $file) {
            Write-Host "  [OK] $file" -ForegroundColor Green
        } else {
            Write-Host "  [FAIL] $file not found" -ForegroundColor Red
            return $false
        }
    }
    
    # Import PowerShell module
    try {
        Import-Module $script:PowerShellModulePath -Force
        Write-Host "  [OK] PowerShell module imported" -ForegroundColor Green
    } catch {
        Write-Host "  [FAIL] Failed to import PowerShell module: $_" -ForegroundColor Red
        return $false
    }
    
    # Test with process if specified
    if ($ProcessId -gt 0 -or $ProcessName) {
        try {
            if ($ProcessName) {
                $process = Get-Process -Name $ProcessName -ErrorAction Stop | Select-Object -First 1
                if ($process) {
                    $ProcessId = $process.Id
                    Write-Host "  [OK] Found process: $ProcessName (PID: $ProcessId)" -ForegroundColor Green
                } else {
                    Write-Host "  [FAIL] Process '$ProcessName' not found" -ForegroundColor Red
                    return $false
                }
            } else {
                $process = Get-Process -Id $ProcessId -ErrorAction Stop
                Write-Host "  [OK] Found process: $($process.ProcessName) (PID: $ProcessId)" -ForegroundColor Green
            }
            
            # Quick test (don't actually inject, just verify)
            Write-Host "  [OK] Process verification successful" -ForegroundColor Green
            
        } catch {
            Write-Host "  [FAIL] Process verification failed: $_" -ForegroundColor Red
            return $false
        }
    }
    
    Write-Host ""
    Write-Host "[SUCCESS] All tests passed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "The OS Explorer Interceptor is ready to use." -ForegroundColor Cyan
    Write-Host "To start intercepting:" -ForegroundColor Cyan
    
    if ($ProcessId -gt 0) {
        Write-Host "  Start-OSInterceptor -ProcessId $ProcessId -RealTimeStreaming" -ForegroundColor White
    } else {
        Write-Host "  Start-OSInterceptor -ProcessId <PID> -RealTimeStreaming" -ForegroundColor White
    }
    
    return $true
}

#============================================================================
# MAIN EXECUTION
#============================================================================

# Handle parameters
if ($Build) {
    $buildResult = Build-OSInterceptor
    if (-not $buildResult) {
        exit 1
    }
}

if ($Install) {
    $installResult = Install-OSInterceptor
    if (-not $installResult) {
        exit 1
    }
}

if ($Test) {
    $testResult = Test-OSInterceptor -ProcessId $ProcessId -ProcessName $ProcessName
    if (-not $testResult) {
        exit 1
    }
}

# If no parameters, show help
if (-not $Build -and -not $Install -and -not $Test) {
    Write-Host @"
OS Explorer Interceptor - MASM IDE Integration

Usage:
    .\integrate_os_interceptor.ps1 [-Build] [-Install] [-Test] [-ProcessId <PID>] [-ProcessName <name>]

Parameters:
    -Build      Build the interceptor components
    -Install    Install into MASM IDE
    -Test       Test the integration
    -ProcessId  Process ID to test with (optional)
    -ProcessName Process name to test with (optional)

Examples:
    .\integrate_os_interceptor.ps1 -Build -Install
    .\integrate_os_interceptor.ps1 -Test -ProcessId 1234
    .\integrate_os_interceptor.ps1 -Test -ProcessName "cursor"

For help with commands after installation, type 'helpos' in PowerShell.
"@ -ForegroundColor Cyan
}
