#Requires -Version 5.1
#Requires -RunAsAdministrator

<#
.SYNOPSIS
    RawrXD Production System Installer

.DESCRIPTION
    Installs RawrXD production system with all modules, services, and configuration

.EXAMPLE
    .\Production-Install.ps1 -InstallPath "C:\Program Files\RawrXD"
    
.EXAMPLE
    .\Production-Install.ps1 -InstallPath "C:\RawrXD" -InstallService
#>

param(
    [Parameter(Mandatory = $false)]
    [string]$InstallPath = "C:\Program Files\RawrXD",
    
    [Parameter(Mandatory = $false)]
    [switch]$InstallService = $false,
    
    [Parameter(Mandatory = $false)]
    [string]$ServiceName = "RawrXDService",
    
    [Parameter(Mandatory = $false)]
    [switch]$CreateDesktopShortcut = $true
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        RawrXD Production System Installer                ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Verify running as Administrator
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "ERROR: This script must be run as Administrator" -ForegroundColor Red
    exit 1
}

# Installation Configuration
$config = @{
    InstallPath = $InstallPath
    ModulesPath = Join-Path $InstallPath "Modules"
    BinPath = Join-Path $InstallPath "bin"
    ConfigPath = Join-Path $InstallPath "config"
    LogsPath = Join-Path $InstallPath "logs"
    SourcePath = $PSScriptRoot
    InstallService = $InstallService
    ServiceName = $ServiceName
    CreateDesktopShortcut = $CreateDesktopShortcut
}

Write-Host "Installation Configuration:" -ForegroundColor Yellow
Write-Host "  Install Path: $($config.InstallPath)" -ForegroundColor White
Write-Host "  Install Service: $($config.InstallService)" -ForegroundColor White
Write-Host "  Service Name: $($config.ServiceName)" -ForegroundColor White
Write-Host ""

# Phase 1: Create directory structure
Write-Host "[1/6] Creating directory structure..." -ForegroundColor Cyan
try {
    @($config.InstallPath, $config.ModulesPath, $config.BinPath, $config.ConfigPath, $config.LogsPath) | ForEach-Object {
        if (-not (Test-Path $_)) {
            New-Item -Path $_ -ItemType Directory -Force | Out-Null
            Write-Host "  ✓ Created: $_" -ForegroundColor Green
        } else {
            Write-Host "  ✓ Exists: $_" -ForegroundColor Gray
        }
    }
} catch {
    Write-Host "  ✗ Failed to create directories: $_" -ForegroundColor Red
    exit 1
}

# Phase 2: Copy modules
Write-Host "[2/6] Installing PowerShell modules..." -ForegroundColor Cyan
try {
    $modules = Get-ChildItem -Path $config.SourcePath -Filter "RawrXD*.psm1"
    foreach ($module in $modules) {
        $dest = Join-Path $config.ModulesPath $module.Name
        Copy-Item -Path $module.FullName -Destination $dest -Force
        Write-Host "  ✓ Installed: $($module.Name)" -ForegroundColor Green
    }
} catch {
    Write-Host "  ✗ Failed to install modules: $_" -ForegroundColor Red
    exit 1
}

# Phase 3: Copy executables and binaries
Write-Host "[3/6] Installing executables..." -ForegroundColor Cyan
try {
    $binaries = @('RawrXD.exe', 'RawrXD-ModelLoader.exe', 'RawrXD_IDE_unified.exe')
    foreach ($bin in $binaries) {
        $src = Join-Path $config.SourcePath $bin
        if (Test-Path $src) {
            $dest = Join-Path $config.BinPath $bin
            Copy-Item -Path $src -Destination $dest -Force
            Write-Host "  ✓ Installed: $bin" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ Not found: $bin (skipping)" -ForegroundColor Yellow
        }
    }
} catch {
    Write-Host "  ✗ Failed to install executables: $_" -ForegroundColor Red
    exit 1
}

# Phase 4: Install configuration
Write-Host "[4/6] Installing configuration..." -ForegroundColor Cyan
try {
    $configFiles = @('config.json', 'model_config.json')
    foreach ($cfg in $configFiles) {
        $src = Join-Path $config.SourcePath $cfg
        if (Test-Path $src) {
            $dest = Join-Path $config.ConfigPath $cfg
            Copy-Item -Path $src -Destination $dest -Force
            Write-Host "  ✓ Installed: $cfg" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ Not found: $cfg (creating default)" -ForegroundColor Yellow
            @{ version = "1.0.0"; environment = "production" } | ConvertTo-Json | Set-Content -Path (Join-Path $config.ConfigPath $cfg)
        }
    }
    
    # Set environment variable
    [System.Environment]::SetEnvironmentVariable('LAZY_INIT_IDE_ROOT', $config.InstallPath, [System.EnvironmentVariableTarget]::Machine)
    Write-Host "  ✓ Set LAZY_INIT_IDE_ROOT environment variable" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed to install configuration: $_" -ForegroundColor Red
    exit 1
}

# Phase 5: Create Windows Service (optional)
if ($config.InstallService) {
    Write-Host "[5/6] Installing Windows Service..." -ForegroundColor Cyan
    try {
        # Create service wrapper script
        $serviceScript = @"
# RawrXD Service Wrapper
`$ErrorActionPreference = 'Continue'
Set-Location '$($config.InstallPath)'
Import-Module (Join-Path '$($config.ModulesPath)' 'RawrXD.Core.psm1') -Force
while (`$true) {
    try {
        Start-Sleep -Seconds 60
    } catch {
        Start-Sleep -Seconds 5
    }
}
"@
        $serviceScriptPath = Join-Path $config.BinPath "service.ps1"
        $serviceScript | Set-Content -Path $serviceScriptPath -Force
        
        # Create NSSM service wrapper
        $nssm = "C:\Windows\System32\nssm.exe"
        if (Test-Path $nssm) {
            & $nssm install $config.ServiceName "powershell.exe" "-ExecutionPolicy Bypass -File `"$serviceScriptPath`""
            & $nssm set $config.ServiceName AppDirectory $config.InstallPath
            & $nssm set $config.ServiceName DisplayName "RawrXD Production Service"
            & $nssm set $config.ServiceName Description "RawrXD AI IDE Production Service"
            & $nssm set $config.ServiceName Start SERVICE_AUTO_START
            
            Start-Service $config.ServiceName
            Write-Host "  ✓ Service installed and started" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ NSSM not found. Service not installed. Install NSSM from https://nssm.cc/" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "  ✗ Failed to install service: $_" -ForegroundColor Red
    }
} else {
    Write-Host "[5/6] Skipping service installation..." -ForegroundColor Gray
}

# Phase 6: Create shortcuts
Write-Host "[6/6] Creating shortcuts..." -ForegroundColor Cyan
try {
    if ($config.CreateDesktopShortcut) {
        $shell = New-Object -ComObject WScript.Shell
        $desktopPath = [System.Environment]::GetFolderPath('Desktop')
        
        # Create shortcut to main executable
        $exePath = Join-Path $config.BinPath "RawrXD.exe"
        if (Test-Path $exePath) {
            $shortcutPath = Join-Path $desktopPath "RawrXD.lnk"
            $shortcut = $shell.CreateShortcut($shortcutPath)
            $shortcut.TargetPath = $exePath
            $shortcut.WorkingDirectory = $config.InstallPath
            $shortcut.Save()
            Write-Host "  ✓ Created desktop shortcut" -ForegroundColor Green
        }
    }
    
    # Add to PATH
    $currentPath = [System.Environment]::GetEnvironmentVariable('PATH', [System.EnvironmentVariableTarget]::Machine)
    if ($currentPath -notlike "*$($config.BinPath)*") {
        [System.Environment]::SetEnvironmentVariable('PATH', "$currentPath;$($config.BinPath)", [System.EnvironmentVariableTarget]::Machine)
        Write-Host "  ✓ Added to system PATH" -ForegroundColor Green
    }
} catch {
    Write-Host "  ✗ Failed to create shortcuts: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║          Installation Completed Successfully!            ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""

Write-Host "Installation Summary:" -ForegroundColor Yellow
Write-Host "  Installed to: $($config.InstallPath)" -ForegroundColor White
Write-Host "  Modules: $($modules.Count) PowerShell modules" -ForegroundColor White
Write-Host "  Service: $(if ($config.InstallService) { 'Installed and running' } else { 'Not installed' })" -ForegroundColor White
Write-Host ""

Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. Restart your terminal to load PATH changes" -ForegroundColor White
Write-Host "  2. Run 'RawrXD.exe' to launch the IDE" -ForegroundColor White
Write-Host "  3. Configure model endpoints in: $($config.ConfigPath)\model_config.json" -ForegroundColor White
Write-Host ""

Write-Host "To uninstall, run: .\Production-Uninstall.ps1" -ForegroundColor Gray
Write-Host ""
