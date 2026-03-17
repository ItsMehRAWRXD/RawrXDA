# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║ Install-NVMeOracleService.ps1 - Sovereign NVMe Thermal Sidecar Installer    ║
# ║                                                                              ║
# ║ Purpose: Install, uninstall, and manage the SovereignNVMeOracle service     ║
# ║                                                                              ║
# ║ Usage:                                                                       ║
# ║   .\Install-NVMeOracleService.ps1 -Install                                  ║
# ║   .\Install-NVMeOracleService.ps1 -Uninstall                                ║
# ║   .\Install-NVMeOracleService.ps1 -Status                                   ║
# ║   .\Install-NVMeOracleService.ps1 -Start                                    ║
# ║   .\Install-NVMeOracleService.ps1 -Stop                                     ║
# ║   .\Install-NVMeOracleService.ps1 -Test                                     ║
# ║                                                                              ║
# ║ Requires: Administrator privileges for service management                    ║
# ║                                                                              ║
# ║ Author: RawrXD IDE Team                                                      ║
# ║ Version: 2.0.0                                                               ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

[CmdletBinding()]
param(
    [switch]$Install,
    [switch]$Uninstall,
    [switch]$Status,
    [switch]$Start,
    [switch]$Stop,
    [switch]$Test,
    [switch]$TestMMF,
    [string]$BinaryPath = "",
    [string]$ServiceName = "SovereignNVMeOracle",
    [string]$DisplayName = "Sovereign NVMe Thermal Oracle",
    [string]$Description = "Provides real-time NVMe temperature telemetry via IOCTL to Local\SOVEREIGN_NVME_TEMPS memory-mapped file"
)

# ═══════════════════════════════════════════════════════════════════════════════
# Constants
# ═══════════════════════════════════════════════════════════════════════════════

$MMF_NAME = "Local\SOVEREIGN_NVME_TEMPS"
$SIGNATURE = 0x45564F53  # "SOVE"
$VERSION = 1

# ═══════════════════════════════════════════════════════════════════════════════
# Helper Functions
# ═══════════════════════════════════════════════════════════════════════════════

function Write-Header {
    Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║     SOVEREIGN NVMe Thermal Oracle Service Manager            ║" -ForegroundColor Cyan
    Write-Host "║     Version 2.0.0                                            ║" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Require-Administrator {
    if (-not (Test-Administrator)) {
        Write-Host "[ERROR] This operation requires Administrator privileges." -ForegroundColor Red
        Write-Host "        Please run PowerShell as Administrator." -ForegroundColor Yellow
        exit 1
    }
}

function Find-SidecarBinary {
    param([string]$PreferredPath)
    
    $searchPaths = @(
        $PreferredPath,
        "$PSScriptRoot\build\nvme_thermal_sidecar_clean.exe",
        "$PSScriptRoot\..\build\nvme_thermal_sidecar_clean.exe",
        "D:\rawrxd\build\nvme_thermal_sidecar_clean.exe",
        "C:\Sovereign\nvme_thermal_sidecar_clean.exe",
        "$env:ProgramFiles\RawrXD\nvme_thermal_sidecar_clean.exe"
    )
    
    foreach ($path in $searchPaths) {
        if ($path -and (Test-Path $path)) {
            return (Resolve-Path $path).Path
        }
    }
    
    return $null
}

function Get-ServiceStatus {
    try {
        $service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
        if ($service) {
            return @{
                Exists = $true
                Status = $service.Status
                StartType = $service.StartType
                DisplayName = $service.DisplayName
            }
        }
    } catch {}
    
    return @{
        Exists = $false
        Status = "NotInstalled"
        StartType = "N/A"
        DisplayName = "N/A"
    }
}

function Test-MMFExists {
    try {
        $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($MMF_NAME)
        $mmf.Dispose()
        return $true
    } catch {
        return $false
    }
}

function Read-MMFData {
    try {
        $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($MMF_NAME)
        $accessor = $mmf.CreateViewAccessor(0, 152)  # Size of SovereignThermalData
        
        $signature = $accessor.ReadUInt32(0)
        $version = $accessor.ReadUInt32(4)
        $driveCount = $accessor.ReadUInt32(8)
        $timestamp = $accessor.ReadInt64(144)
        
        $temps = @()
        for ($i = 0; $i -lt $driveCount -and $i -lt 16; $i++) {
            $temp = $accessor.ReadInt32(16 + ($i * 4))
            $temps += $temp
        }
        
        $accessor.Dispose()
        $mmf.Dispose()
        
        return @{
            Success = $true
            Signature = $signature
            SignatureValid = ($signature -eq $SIGNATURE)
            Version = $version
            DriveCount = $driveCount
            Temps = $temps
            Timestamp = $timestamp
        }
    } catch {
        return @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# Service Operations
# ═══════════════════════════════════════════════════════════════════════════════

function Install-NVMeOracleService {
    Require-Administrator
    
    Write-Host "[*] Installing $ServiceName service..." -ForegroundColor Yellow
    
    # Find binary
    $binary = Find-SidecarBinary -PreferredPath $BinaryPath
    if (-not $binary) {
        Write-Host "[ERROR] Could not find nvme_thermal_sidecar_clean.exe" -ForegroundColor Red
        Write-Host "        Specify path with -BinaryPath parameter" -ForegroundColor Yellow
        Write-Host "        Or build it: cmake --build . --target nvme_thermal_sidecar_clean" -ForegroundColor Yellow
        return $false
    }
    
    Write-Host "        Binary: $binary" -ForegroundColor Gray
    
    # Check if already installed
    $status = Get-ServiceStatus
    if ($status.Exists) {
        Write-Host "[WARNING] Service already exists (Status: $($status.Status))" -ForegroundColor Yellow
        Write-Host "          Use -Uninstall first, or -Start/-Stop to manage" -ForegroundColor Yellow
        return $false
    }
    
    # Create service
    Write-Host "[*] Creating service..." -ForegroundColor Yellow
    $result = sc.exe create $ServiceName `
        binPath= "`"$binary`"" `
        start= auto `
        obj= "NT AUTHORITY\SYSTEM" `
        DisplayName= "$DisplayName"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Failed to create service: $result" -ForegroundColor Red
        return $false
    }
    
    # Set description
    sc.exe description $ServiceName "$Description" | Out-Null
    
    # Set failure actions (restart on failure)
    sc.exe failure $ServiceName reset= 86400 actions= restart/5000/restart/10000/restart/30000 | Out-Null
    
    Write-Host "[SUCCESS] Service installed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To start the service:" -ForegroundColor Cyan
    Write-Host "  sc start $ServiceName" -ForegroundColor White
    Write-Host "  OR" -ForegroundColor Gray
    Write-Host "  .\Install-NVMeOracleService.ps1 -Start" -ForegroundColor White
    
    return $true
}

function Uninstall-NVMeOracleService {
    Require-Administrator
    
    Write-Host "[*] Uninstalling $ServiceName service..." -ForegroundColor Yellow
    
    $status = Get-ServiceStatus
    if (-not $status.Exists) {
        Write-Host "[INFO] Service is not installed" -ForegroundColor Gray
        return $true
    }
    
    # Stop if running
    if ($status.Status -eq "Running") {
        Write-Host "[*] Stopping service..." -ForegroundColor Yellow
        sc.exe stop $ServiceName | Out-Null
        Start-Sleep -Seconds 2
    }
    
    # Delete service
    Write-Host "[*] Removing service..." -ForegroundColor Yellow
    $result = sc.exe delete $ServiceName
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Failed to delete service: $result" -ForegroundColor Red
        return $false
    }
    
    Write-Host "[SUCCESS] Service uninstalled successfully!" -ForegroundColor Green
    return $true
}

function Start-NVMeOracleService {
    Require-Administrator
    
    $status = Get-ServiceStatus
    if (-not $status.Exists) {
        Write-Host "[ERROR] Service is not installed. Use -Install first." -ForegroundColor Red
        return $false
    }
    
    if ($status.Status -eq "Running") {
        Write-Host "[INFO] Service is already running" -ForegroundColor Green
        return $true
    }
    
    Write-Host "[*] Starting $ServiceName service..." -ForegroundColor Yellow
    sc.exe start $ServiceName | Out-Null
    Start-Sleep -Seconds 2
    
    $status = Get-ServiceStatus
    if ($status.Status -eq "Running") {
        Write-Host "[SUCCESS] Service started successfully!" -ForegroundColor Green
        return $true
    } else {
        Write-Host "[ERROR] Service failed to start (Status: $($status.Status))" -ForegroundColor Red
        Write-Host "        Check Event Viewer for details" -ForegroundColor Yellow
        return $false
    }
}

function Stop-NVMeOracleService {
    Require-Administrator
    
    $status = Get-ServiceStatus
    if (-not $status.Exists) {
        Write-Host "[INFO] Service is not installed" -ForegroundColor Gray
        return $true
    }
    
    if ($status.Status -ne "Running") {
        Write-Host "[INFO] Service is not running (Status: $($status.Status))" -ForegroundColor Gray
        return $true
    }
    
    Write-Host "[*] Stopping $ServiceName service..." -ForegroundColor Yellow
    sc.exe stop $ServiceName | Out-Null
    Start-Sleep -Seconds 2
    
    $status = Get-ServiceStatus
    if ($status.Status -ne "Running") {
        Write-Host "[SUCCESS] Service stopped successfully!" -ForegroundColor Green
        return $true
    } else {
        Write-Host "[WARNING] Service may still be stopping..." -ForegroundColor Yellow
        return $true
    }
}

function Show-ServiceStatus {
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host " SERVICE STATUS" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    $status = Get-ServiceStatus
    
    if ($status.Exists) {
        $statusColor = switch ($status.Status) {
            "Running" { "Green" }
            "Stopped" { "Yellow" }
            default { "Red" }
        }
        
        Write-Host "  Service Name:  $ServiceName" -ForegroundColor White
        Write-Host "  Display Name:  $($status.DisplayName)" -ForegroundColor White
        Write-Host "  Status:        $($status.Status)" -ForegroundColor $statusColor
        Write-Host "  Start Type:    $($status.StartType)" -ForegroundColor White
    } else {
        Write-Host "  Service:       NOT INSTALLED" -ForegroundColor Red
    }
    
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host " MMF STATUS" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    if (Test-MMFExists) {
        Write-Host "  MMF Name:      $MMF_NAME" -ForegroundColor White
        Write-Host "  MMF Status:    EXISTS" -ForegroundColor Green
        
        $data = Read-MMFData
        if ($data.Success) {
            $sigStatus = if ($data.SignatureValid) { "VALID" } else { "INVALID" }
            $sigColor = if ($data.SignatureValid) { "Green" } else { "Red" }
            
            Write-Host "  Signature:     0x$($data.Signature.ToString('X8')) ($sigStatus)" -ForegroundColor $sigColor
            Write-Host "  Version:       $($data.Version)" -ForegroundColor White
            Write-Host "  Drive Count:   $($data.DriveCount)" -ForegroundColor White
            Write-Host "  Timestamp:     $($data.Timestamp) ms" -ForegroundColor White
            Write-Host ""
            Write-Host "  Temperatures:" -ForegroundColor Cyan
            for ($i = 0; $i -lt $data.Temps.Count; $i++) {
                $temp = $data.Temps[$i]
                $tempColor = if ($temp -eq -1) { "Gray" } elseif ($temp -gt 60) { "Red" } elseif ($temp -gt 45) { "Yellow" } else { "Green" }
                $tempStr = if ($temp -eq -1) { "N/A" } else { "$temp°C" }
                Write-Host "    Drive ${i}: $tempStr" -ForegroundColor $tempColor
            }
        } else {
            Write-Host "  Read Error:    $($data.Error)" -ForegroundColor Red
        }
    } else {
        Write-Host "  MMF Name:      $MMF_NAME" -ForegroundColor White
        Write-Host "  MMF Status:    NOT FOUND" -ForegroundColor Red
        Write-Host ""
        Write-Host "  The sidecar is not running or failed to create the MMF." -ForegroundColor Yellow
    }
    
    Write-Host ""
}

function Test-ThermalSystem {
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host " THERMAL SYSTEM TEST" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    # Test 1: Service Installation
    Write-Host ""
    Write-Host "[Test 1] Service Installation" -ForegroundColor Yellow
    $status = Get-ServiceStatus
    if ($status.Exists) {
        Write-Host "  [PASS] Service is installed" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Service is NOT installed" -ForegroundColor Red
        Write-Host "         Run: .\Install-NVMeOracleService.ps1 -Install" -ForegroundColor Gray
    }
    
    # Test 2: Service Running
    Write-Host ""
    Write-Host "[Test 2] Service Running" -ForegroundColor Yellow
    if ($status.Status -eq "Running") {
        Write-Host "  [PASS] Service is running" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Service is NOT running (Status: $($status.Status))" -ForegroundColor Red
        Write-Host "         Run: sc start $ServiceName" -ForegroundColor Gray
    }
    
    # Test 3: MMF Exists
    Write-Host ""
    Write-Host "[Test 3] Memory-Mapped File" -ForegroundColor Yellow
    if (Test-MMFExists) {
        Write-Host "  [PASS] MMF '$MMF_NAME' exists" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] MMF '$MMF_NAME' NOT found" -ForegroundColor Red
    }
    
    # Test 4: MMF Data Valid
    Write-Host ""
    Write-Host "[Test 4] MMF Data Validity" -ForegroundColor Yellow
    $data = Read-MMFData
    if ($data.Success) {
        if ($data.SignatureValid) {
            Write-Host "  [PASS] Signature valid (0x$($data.Signature.ToString('X8')))" -ForegroundColor Green
        } else {
            Write-Host "  [FAIL] Invalid signature (0x$($data.Signature.ToString('X8')))" -ForegroundColor Red
        }
        
        if ($data.Version -eq $VERSION) {
            Write-Host "  [PASS] Version correct ($($data.Version))" -ForegroundColor Green
        } else {
            Write-Host "  [WARN] Version mismatch (Expected: $VERSION, Got: $($data.Version))" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  [FAIL] Could not read MMF: $($data.Error)" -ForegroundColor Red
    }
    
    # Test 5: Temperature Data
    Write-Host ""
    Write-Host "[Test 5] Temperature Readings" -ForegroundColor Yellow
    if ($data.Success -and $data.Temps.Count -gt 0) {
        $validTemps = $data.Temps | Where-Object { $_ -ne -1 }
        if ($validTemps.Count -gt 0) {
            Write-Host "  [PASS] $($validTemps.Count)/$($data.Temps.Count) drives reporting valid temperatures" -ForegroundColor Green
            foreach ($temp in $validTemps) {
                $status = if ($temp -gt 60) { "HOT" } elseif ($temp -gt 45) { "WARM" } else { "OK" }
                Write-Host "         $temp°C ($status)" -ForegroundColor Gray
            }
        } else {
            Write-Host "  [WARN] No valid temperature readings (all -1)" -ForegroundColor Yellow
            Write-Host "         This may indicate IOCTL permission issues" -ForegroundColor Gray
            Write-Host "         Ensure service runs as NT AUTHORITY\SYSTEM" -ForegroundColor Gray
        }
    } else {
        Write-Host "  [FAIL] No temperature data available" -ForegroundColor Red
    }
    
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
}

# ═══════════════════════════════════════════════════════════════════════════════
# Main Entry Point
# ═══════════════════════════════════════════════════════════════════════════════

Write-Header

if ($Install) {
    Install-NVMeOracleService
} elseif ($Uninstall) {
    Uninstall-NVMeOracleService
} elseif ($Start) {
    Start-NVMeOracleService
} elseif ($Stop) {
    Stop-NVMeOracleService
} elseif ($Test) {
    Test-ThermalSystem
} elseif ($TestMMF) {
    Show-ServiceStatus
} elseif ($Status) {
    Show-ServiceStatus
} else {
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\Install-NVMeOracleService.ps1 -Install     Install the service" -ForegroundColor White
    Write-Host "  .\Install-NVMeOracleService.ps1 -Uninstall   Remove the service" -ForegroundColor White
    Write-Host "  .\Install-NVMeOracleService.ps1 -Start       Start the service" -ForegroundColor White
    Write-Host "  .\Install-NVMeOracleService.ps1 -Stop        Stop the service" -ForegroundColor White
    Write-Host "  .\Install-NVMeOracleService.ps1 -Status      Show service and MMF status" -ForegroundColor White
    Write-Host "  .\Install-NVMeOracleService.ps1 -Test        Run diagnostic tests" -ForegroundColor White
    Write-Host ""
    Write-Host "Optional Parameters:" -ForegroundColor Yellow
    Write-Host "  -BinaryPath <path>    Specify path to nvme_thermal_sidecar_clean.exe" -ForegroundColor White
    Write-Host "  -ServiceName <name>   Override service name (default: SovereignNVMeOracle)" -ForegroundColor White
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  # Install and start the service" -ForegroundColor Gray
    Write-Host "  .\Install-NVMeOracleService.ps1 -Install" -ForegroundColor White
    Write-Host "  .\Install-NVMeOracleService.ps1 -Start" -ForegroundColor White
    Write-Host ""
    Write-Host "  # Check if temperatures are being read" -ForegroundColor Gray
    Write-Host "  .\Install-NVMeOracleService.ps1 -Test" -ForegroundColor White
}
