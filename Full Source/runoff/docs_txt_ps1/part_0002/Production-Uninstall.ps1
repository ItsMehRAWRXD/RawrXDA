#Requires -Version 5.1
#Requires -RunAsAdministrator

<#
.SYNOPSIS
    RawrXD Production System Uninstaller

.DESCRIPTION
    Uninstalls RawrXD production system, removes services, and cleans up

.EXAMPLE
    .\Production-Uninstall.ps1 -InstallPath "C:\Program Files\RawrXD"
#>

param(
    [Parameter(Mandatory = $false)]
    [string]$InstallPath = "C:\Program Files\RawrXD",
    
    [Parameter(Mandatory = $false)]
    [string]$ServiceName = "RawrXDService",
    
    [Parameter(Mandatory = $false)]
    [switch]$KeepLogs = $false
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Red
Write-Host "║       RawrXD Production System Uninstaller               ║" -ForegroundColor Red
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Red
Write-Host ""

# Verify running as Administrator
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "ERROR: This script must be run as Administrator" -ForegroundColor Red
    exit 1
}

Write-Host "WARNING: This will remove RawrXD from:" -ForegroundColor Yellow
Write-Host "  $InstallPath" -ForegroundColor White
Write-Host ""
$confirmation = Read-Host "Continue with uninstallation? (yes/no)"
if ($confirmation -ne 'yes') {
    Write-Host "Uninstallation cancelled." -ForegroundColor Gray
    exit 0
}

# Phase 1: Stop and remove service
Write-Host "[1/4] Removing Windows Service..." -ForegroundColor Cyan
try {
    $service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
    if ($service) {
        if ($service.Status -eq 'Running') {
            Stop-Service -Name $ServiceName -Force
            Write-Host "  ✓ Service stopped" -ForegroundColor Green
        }
        
        $nssm = "C:\Windows\System32\nssm.exe"
        if (Test-Path $nssm) {
            & $nssm remove $ServiceName confirm
            Write-Host "  ✓ Service removed" -ForegroundColor Green
        } else {
            sc.exe delete $ServiceName
            Write-Host "  ✓ Service removed (fallback method)" -ForegroundColor Green
        }
    } else {
        Write-Host "  ⚠ Service not found (skipping)" -ForegroundColor Gray
    }
} catch {
    Write-Host "  ⚠ Failed to remove service: $_" -ForegroundColor Yellow
}

# Phase 2: Remove from PATH
Write-Host "[2/4] Removing from system PATH..." -ForegroundColor Cyan
try {
    $binPath = Join-Path $InstallPath "bin"
    $currentPath = [System.Environment]::GetEnvironmentVariable('PATH', [System.EnvironmentVariableTarget]::Machine)
    $newPath = ($currentPath -split ';' | Where-Object { $_ -ne $binPath }) -join ';'
    [System.Environment]::SetEnvironmentVariable('PATH', $newPath, [System.EnvironmentVariableTarget]::Machine)
    Write-Host "  ✓ Removed from PATH" -ForegroundColor Green
} catch {
    Write-Host "  ⚠ Failed to update PATH: $_" -ForegroundColor Yellow
}

# Phase 3: Remove environment variables
Write-Host "[3/4] Removing environment variables..." -ForegroundColor Cyan
try {
    [System.Environment]::SetEnvironmentVariable('LAZY_INIT_IDE_ROOT', $null, [System.EnvironmentVariableTarget]::Machine)
    Write-Host "  ✓ Removed LAZY_INIT_IDE_ROOT" -ForegroundColor Green
} catch {
    Write-Host "  ⚠ Failed to remove environment variable: $_" -ForegroundColor Yellow
}

# Phase 4: Remove installation directory
Write-Host "[4/4] Removing installation files..." -ForegroundColor Cyan
try {
    if (Test-Path $InstallPath) {
        if ($KeepLogs) {
            $logsPath = Join-Path $InstallPath "logs"
            if (Test-Path $logsPath) {
                $backupPath = Join-Path ([System.IO.Path]::GetTempPath()) "RawrXD_logs_backup_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
                Copy-Item -Path $logsPath -Destination $backupPath -Recurse -Force
                Write-Host "  ✓ Logs backed up to: $backupPath" -ForegroundColor Green
            }
        }
        
        Remove-Item -Path $InstallPath -Recurse -Force
        Write-Host "  ✓ Installation directory removed" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ Installation directory not found" -ForegroundColor Gray
    }
} catch {
    Write-Host "  ⚠ Failed to remove installation directory: $_" -ForegroundColor Yellow
}

# Remove desktop shortcuts
try {
    $desktopPath = [System.Environment]::GetFolderPath('Desktop')
    $shortcutPath = Join-Path $desktopPath "RawrXD.lnk"
    if (Test-Path $shortcutPath) {
        Remove-Item -Path $shortcutPath -Force
        Write-Host "  ✓ Removed desktop shortcut" -ForegroundColor Green
    }
} catch {
    Write-Host "  ⚠ Failed to remove shortcut: $_" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║         Uninstallation Completed Successfully!           ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""

Write-Host "RawrXD has been removed from your system." -ForegroundColor White
Write-Host "Thank you for using RawrXD!" -ForegroundColor Cyan
Write-Host ""
