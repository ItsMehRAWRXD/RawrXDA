<#
.SYNOPSIS
    Installs RawrXD SafeMode CLI to system PATH for global access.

.DESCRIPTION
    Creates a global 'rawr' command accessible from any terminal.
    Provides full IDE functionality via command line when GUI fails.

.PARAMETER Uninstall
    Remove the global rawr command.

.EXAMPLE
    .\Install-RawrSafeMode.ps1
    .\Install-RawrSafeMode.ps1 -Uninstall
#>

param(
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

# Configuration
$InstallDir = "$env:LOCALAPPDATA\RawrXD"
$ScriptDir = $PSScriptRoot
$SourceScript = Join-Path $ScriptDir "rawr.ps1"
$TargetScript = Join-Path $InstallDir "rawr.ps1"
$ShimBat = Join-Path $InstallDir "rawr.cmd"
$ShimPs1 = Join-Path $InstallDir "rawr-loader.ps1"

# Colors
$Colors = @{
    Green = "`e[32m"
    Cyan = "`e[36m"
    Yellow = "`e[33m"
    Red = "`e[31m"
    Reset = "`e[0m"
}

function Write-Success { param($Msg) Write-Host "$($Colors.Green)✓ $Msg$($Colors.Reset)" }
function Write-Info { param($Msg) Write-Host "$($Colors.Cyan)ℹ $Msg$($Colors.Reset)" }
function Write-Warn { param($Msg) Write-Host "$($Colors.Yellow)⚠ $Msg$($Colors.Reset)" }
function Write-Err { param($Msg) Write-Host "$($Colors.Red)✗ $Msg$($Colors.Reset)" }

function Show-Banner {
    Write-Host @"
$($Colors.Cyan)
 ____                     __  ______  
|  _ \ __ ___      ___ __|  \/  |  _ \ 
| |_) / _`` \ \ /\ / / '__| |\/| | | | |
|  _ < (_| |\ V  V /| |  | |  | | |_| |
|_| \_\__,_| \_/\_/ |_|  |_|  |_|____/ 
$($Colors.Reset)
$($Colors.Yellow)SafeMode CLI Installer$($Colors.Reset)
"@
}

function Test-InPath {
    param([string]$Dir)
    $path = [Environment]::GetEnvironmentVariable("Path", "User")
    return $path -split ";" | Where-Object { $_ -eq $Dir }
}

function Add-ToPath {
    param([string]$Dir)
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if (-not (Test-InPath $Dir)) {
        [Environment]::SetEnvironmentVariable("Path", "$currentPath;$Dir", "User")
        $env:Path = "$env:Path;$Dir"
        return $true
    }
    return $false
}

function Remove-FromPath {
    param([string]$Dir)
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $newPath = ($currentPath -split ";" | Where-Object { $_ -ne $Dir }) -join ";"
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    $env:Path = ($env:Path -split ";" | Where-Object { $_ -ne $Dir }) -join ";"
}

function Install-RawrSafeMode {
    Show-Banner
    Write-Host ""
    
    # Create install directory
    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
        Write-Success "Created install directory: $InstallDir"
    }
    
    # Check source exists
    if (-not (Test-Path $SourceScript)) {
        Write-Err "Source script not found: $SourceScript"
        exit 1
    }
    
    # Copy main script
    Copy-Item $SourceScript $TargetScript -Force
    Write-Success "Installed rawr.ps1"
    
    # Create CMD shim for PowerShell
    $cmdContent = @"
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0rawr.ps1" %*
"@
    Set-Content -Path $ShimBat -Value $cmdContent
    Write-Success "Created CMD shim"
    
    # Create PowerShell profile loader
    $loaderContent = @"
# RawrXD SafeMode CLI Loader
# Auto-generated - do not edit

function global:rawr {
    & "$InstallDir\rawr.ps1" @args
}

# Alias for quick access
Set-Alias -Name rx -Value rawr -Scope Global

Write-Host "`e[32m[RawrXD]`e[0m SafeMode CLI loaded. Use 'rawr help' or 'rx help'"
"@
    Set-Content -Path $ShimPs1 -Value $loaderContent
    Write-Success "Created PowerShell loader"
    
    # Add to PATH
    if (Add-ToPath $InstallDir) {
        Write-Success "Added to PATH"
    } else {
        Write-Info "Already in PATH"
    }
    
    # Check for PowerShell profile
    $profilePath = $PROFILE.CurrentUserAllHosts
    $profileContent = if (Test-Path $profilePath) { Get-Content $profilePath -Raw } else { "" }
    
    if ($profileContent -notmatch "rawr-loader\.ps1") {
        $importLine = ". `"$ShimPs1`""
        
        Write-Host ""
        Write-Warn "To enable 'rawr' function in PowerShell sessions, add to your profile:"
        Write-Host ""
        Write-Host "    $($Colors.Cyan)$importLine$($Colors.Reset)"
        Write-Host ""
        
        $addToProfile = Read-Host "Add to PowerShell profile now? (y/n)"
        if ($addToProfile -eq "y") {
            if (-not (Test-Path $profilePath)) {
                New-Item -ItemType File -Path $profilePath -Force | Out-Null
            }
            Add-Content -Path $profilePath -Value "`n# RawrXD SafeMode CLI`n$importLine"
            Write-Success "Added to PowerShell profile"
        }
    }
    
    Write-Host ""
    Write-Success "Installation complete!"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  $($Colors.Green)rawr help$($Colors.Reset)          - Show all commands"
    Write-Host "  $($Colors.Green)rawr run <model>$($Colors.Reset)   - Load a model (ollama-style)"
    Write-Host "  $($Colors.Green)rawr chat$($Colors.Reset)          - Interactive chat"
    Write-Host "  $($Colors.Green)rawr agent <task>$($Colors.Reset)  - Run agentic task with tools"
    Write-Host "  $($Colors.Green)rawr tools$($Colors.Reset)         - List available tools"
    Write-Host "  $($Colors.Green)rawr repl$($Colors.Reset)          - Enter REPL mode"
    Write-Host ""
    Write-Info "Restart your terminal or run: `$env:Path = [Environment]::GetEnvironmentVariable('Path', 'User')"
}

function Uninstall-RawrSafeMode {
    Show-Banner
    Write-Host ""
    Write-Info "Uninstalling RawrXD SafeMode CLI..."
    Write-Host ""
    
    # Remove from PATH
    Remove-FromPath $InstallDir
    Write-Success "Removed from PATH"
    
    # Remove profile entry
    $profilePath = $PROFILE.CurrentUserAllHosts
    if (Test-Path $profilePath) {
        $content = Get-Content $profilePath -Raw
        if ($content -match "rawr-loader\.ps1") {
            $content = $content -replace "(?m)^# RawrXD SafeMode CLI\r?\n.*rawr-loader\.ps1.*\r?\n?", ""
            Set-Content -Path $profilePath -Value $content.TrimEnd()
            Write-Success "Removed from PowerShell profile"
        }
    }
    
    # Remove install directory
    if (Test-Path $InstallDir) {
        Remove-Item -Path $InstallDir -Recurse -Force
        Write-Success "Removed install directory"
    }
    
    Write-Host ""
    Write-Success "Uninstall complete!"
}

# Main
if ($Uninstall) {
    Uninstall-RawrSafeMode
} else {
    Install-RawrSafeMode
}
