# Quick-Install-NVMeService.ps1 - Run as Admin!
# Usage: Right-click > Run as Administrator

Write-Host "=== SOVEREIGN NVME ORACLE - QUICK INSTALLER ===" -ForegroundColor Cyan
Write-Host ""

# Check admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] Must run as Administrator!" -ForegroundColor Red
    Write-Host "        Right-click PowerShell > Run as Administrator" -ForegroundColor Yellow
    pause
    exit 1
}

# Configuration
$ServiceName = "SovereignNVMeOracle"
$DisplayName = "Sovereign NVMe Thermal Oracle"
$Description = "Real-time NVMe thermal telemetry via IOCTL - Global\SOVEREIGN_NVME_TEMPS MMF"
$SourceBinary = "D:\rawrxd\build\Release\nvme_oracle_service.exe"
$TargetDir = "C:\Sovereign"
$TargetBinary = "$TargetDir\nvme_oracle_service.exe"

# Create target directory
if (-not (Test-Path $TargetDir)) {
    Write-Host "[*] Creating $TargetDir..." -ForegroundColor Yellow
    New-Item -Path $TargetDir -ItemType Directory -Force | Out-Null
}

# Check source binary exists
if (-not (Test-Path $SourceBinary)) {
    Write-Host "[ERROR] Source binary not found: $SourceBinary" -ForegroundColor Red
    Write-Host "        Build it first: cmake --build D:\rawrxd\build --config Release" -ForegroundColor Yellow
    pause
    exit 1
}

# Stop existing service if running
Write-Host "[*] Checking for existing service..." -ForegroundColor Yellow
$existingSvc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($existingSvc) {
    Write-Host "[*] Stopping existing service..." -ForegroundColor Yellow
    sc.exe stop $ServiceName 2>$null | Out-Null
    Start-Sleep -Seconds 2
    
    Write-Host "[*] Deleting existing service..." -ForegroundColor Yellow
    sc.exe delete $ServiceName 2>$null | Out-Null
    Start-Sleep -Seconds 1
}

# Copy binary
Write-Host "[*] Copying binary to $TargetDir..." -ForegroundColor Yellow
Copy-Item -Path $SourceBinary -Destination $TargetBinary -Force

# Verify copy
if (-not (Test-Path $TargetBinary)) {
    Write-Host "[ERROR] Failed to copy binary!" -ForegroundColor Red
    pause
    exit 1
}

# Create service
Write-Host "[*] Creating service..." -ForegroundColor Yellow
$result = sc.exe create $ServiceName binPath= "`"$TargetBinary`"" start= auto obj= "LocalSystem" DisplayName= "$DisplayName"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Failed to create service: $result" -ForegroundColor Red
    pause
    exit 1
}

# Set description
sc.exe description $ServiceName "$Description" | Out-Null

# Configure recovery options (restart on failure)
sc.exe failure $ServiceName reset= 86400 actions= restart/5000/restart/10000/restart/30000 | Out-Null

# Start service
Write-Host "[*] Starting service..." -ForegroundColor Yellow
sc.exe start $ServiceName | Out-Null
Start-Sleep -Seconds 3

# Verify service status
$svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($svc -and $svc.Status -eq "Running") {
    Write-Host "[SUCCESS] Service is RUNNING!" -ForegroundColor Green
} else {
    Write-Host "[WARNING] Service status: $($svc.Status)" -ForegroundColor Yellow
    Write-Host "          Check Event Viewer > Windows Logs > System" -ForegroundColor Yellow
}

# Test MMF
Write-Host ""
Write-Host "[*] Testing MMF access..." -ForegroundColor Yellow
Start-Sleep -Seconds 2

try {
    $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting("Global\SOVEREIGN_NVME_TEMPS")
    $accessor = $mmf.CreateViewAccessor(0, 152)
    
    $signature = $accessor.ReadUInt32(0)
    $version = $accessor.ReadUInt32(4)
    $driveCount = $accessor.ReadUInt32(8)
    
    Write-Host ""
    Write-Host "=== MMF DATA ===" -ForegroundColor Cyan
    Write-Host "Signature: 0x$($signature.ToString('X8')) $(if($signature -eq 0x534F5645){'✓ VALID'}else{'✗ INVALID'})" -ForegroundColor $(if($signature -eq 0x534F5645){'Green'}else{'Red'})
    Write-Host "Version: $version"
    Write-Host "Drive Count: $driveCount"
    Write-Host ""
    Write-Host "Drive Temperatures:" -ForegroundColor Cyan
    
    for ($i = 0; $i -lt $driveCount -and $i -lt 16; $i++) {
        $temp = $accessor.ReadInt32(16 + ($i * 4))
        $color = if($temp -eq -1){'Gray'}elseif($temp -gt 70){'Red'}elseif($temp -gt 55){'Yellow'}else{'Green'}
        $status = if($temp -eq -1){"(not NVMe)"}elseif($temp -eq 0){"(ERROR)"}else{""}
        Write-Host "  Drive ${i}: $temp °C $status" -ForegroundColor $color
    }
    
    $timestamp = $accessor.ReadInt64(144)
    Write-Host ""
    Write-Host "Last Update: $timestamp ms" -ForegroundColor Cyan
    
    $accessor.Dispose()
    $mmf.Dispose()
    
    Write-Host ""
    Write-Host "[SUCCESS] Installation complete! MMF is accessible." -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Could not read MMF: $_" -ForegroundColor Red
    Write-Host "        The service may need more time to initialize." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== INSTALLATION SUMMARY ===" -ForegroundColor Cyan
Write-Host "Service Name: $ServiceName"
Write-Host "Binary Path:  $TargetBinary"
Write-Host "MMF Name:     Global\SOVEREIGN_NVME_TEMPS"
Write-Host ""
Write-Host "Commands:"
Write-Host "  sc.exe query $ServiceName     - Check status"
Write-Host "  sc.exe stop $ServiceName      - Stop service"
Write-Host "  sc.exe start $ServiceName     - Start service"
Write-Host "  sc.exe delete $ServiceName    - Remove service"
Write-Host ""
pause
