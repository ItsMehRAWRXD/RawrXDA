<#
.SYNOPSIS
    Initialize Thermal-Aware NVMe Array for Virtual VRAM Paging
    
.DESCRIPTION
    Configures the 5-drive NVMe array for thermal-aware paging:
    - DeviceID 0,1,2: CT1000P3PSSD8 (3x 931GB = 2.8TB)
    - DeviceID 4,5: Micron CT4000X10PROSSD9 (2x 3.7TB = 7.4TB)
    Total: ~10.2TB available for Virtual VRAM paging
    
    Creates SovereignControlBlock configuration for:
    - STRIPED_THERMAL_AWARE mode
    - Health-aware load balancing
    - 64MB page allocation
    - Dynamic drive selection based on temperature
    
.PARAMETER ConfigPath
    Path to store configuration files (default: D:\rawrxd\config)
    
.PARAMETER PageSizeMB
    Page size for Virtual VRAM in MB (default: 64)
    
.PARAMETER ThermalWarning
    Temperature threshold for warning in Celsius (default: 65)
    
.PARAMETER ThermalCritical
    Temperature threshold for critical in Celsius (default: 85)
    
.PARAMETER SkipCalibration
    Skip thermal baseline calibration
    
.EXAMPLE
    .\Initialize-ThermalNVMeArray.ps1 -PageSizeMB 64
    
.NOTES
    Author: RawrXD IDE Team
    Version: 1.0.0
    Date: 2026-01-26
#>

param (
    [string]$ConfigPath = "D:\rawrxd\config",
    [int]$PageSizeMB = 64,
    [double]$ThermalWarning = 65.0,
    [double]$ThermalCritical = 85.0,
    [switch]$SkipCalibration
)

$ErrorActionPreference = "Stop"
$Script:Version = "1.0.0"

# Color output helpers
function Write-Success { param($Message) Write-Host "✅ $Message" -ForegroundColor Green }
function Write-Warning2 { param($Message) Write-Host "⚠️ $Message" -ForegroundColor Yellow }
function Write-Error2 { param($Message) Write-Host "❌ $Message" -ForegroundColor Red }
function Write-Info { param($Message) Write-Host "ℹ️ $Message" -ForegroundColor Cyan }
function Write-Section { param($Message) 
    Write-Host ""
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "  $Message" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
}

# ═══════════════════════════════════════════════════════════════════════════════
# Header
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║  🔥 RawrXD IDE - Thermal NVMe Array Initialization v$Script:Version          ║" -ForegroundColor Magenta
Write-Host "║  STRIPED_THERMAL_AWARE Mode Configuration                             ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

# ═══════════════════════════════════════════════════════════════════════════════
# Detect Target Drives
# ═══════════════════════════════════════════════════════════════════════════════

Write-Section "Phase 1: NVMe Array Detection"

$targetDeviceIds = @(0, 1, 2, 4, 5)
$drives = @()
$totalCapacityGB = 0

foreach ($deviceId in $targetDeviceIds) {
    $disk = Get-PhysicalDisk | Where-Object { $_.DeviceId -eq $deviceId }
    
    if ($disk) {
        $health = Get-PhysicalDisk -UniqueId $disk.UniqueId | Get-StorageReliabilityCounter -ErrorAction SilentlyContinue
        
        $driveInfo = @{
            DeviceId = $disk.DeviceId
            FriendlyName = $disk.FriendlyName
            Model = $disk.Model
            SerialNumber = $disk.SerialNumber
            BusType = $disk.BusType
            SizeGB = [math]::Round($disk.Size / 1GB, 2)
            HealthStatus = $disk.HealthStatus
            Temperature = if ($health) { $health.Temperature } else { $null }
            Wear = if ($health) { $health.Wear } else { $null }
            PowerOnHours = if ($health) { $health.PowerOnHours } else { $null }
            ThermalBaseline = $null
            ThermalHeadroom = $null
        }
        
        $drives += $driveInfo
        $totalCapacityGB += $driveInfo.SizeGB
        
        $healthIcon = switch ($disk.HealthStatus) {
            "Healthy" { "✅" }
            "Warning" { "⚠️" }
            "Unhealthy" { "❌" }
            default { "❓" }
        }
        
        $tempStr = if ($driveInfo.Temperature) { "$($driveInfo.Temperature)°C" } else { "N/A" }
        Write-Host "  $healthIcon DeviceId $($driveInfo.DeviceId): $($driveInfo.FriendlyName) - $($driveInfo.SizeGB) GB | Temp: $tempStr" -ForegroundColor White
    }
    else {
        Write-Warning2 "DeviceId $deviceId not found"
    }
}

if ($drives.Count -eq 0) {
    Write-Error2 "No target drives found! Aborting."
    exit 1
}

Write-Host ""
Write-Success "Detected $($drives.Count) drives with total capacity: $([math]::Round($totalCapacityGB, 2)) GB"

# ═══════════════════════════════════════════════════════════════════════════════
# Thermal Baseline Calibration
# ═══════════════════════════════════════════════════════════════════════════════

if (-not $SkipCalibration) {
    Write-Section "Phase 2: Thermal Baseline Calibration"
    Write-Info "Sampling temperatures over 10 seconds..."
    
    $samples = @{}
    foreach ($drive in $drives) {
        $samples[$drive.DeviceId] = @()
    }
    
    for ($i = 0; $i -lt 10; $i++) {
        Write-Host "  Sample $($i + 1)/10..." -NoNewline -ForegroundColor Gray
        
        foreach ($drive in $drives) {
            $health = Get-PhysicalDisk -DeviceId $drive.DeviceId | Get-StorageReliabilityCounter -ErrorAction SilentlyContinue
            if ($health -and $health.Temperature) {
                $samples[$drive.DeviceId] += $health.Temperature
            }
        }
        
        Write-Host " ✓" -ForegroundColor Green
        Start-Sleep -Seconds 1
    }
    
    Write-Host ""
    
    # Calculate baselines
    foreach ($drive in $drives) {
        $driveSamples = $samples[$drive.DeviceId]
        if ($driveSamples.Count -gt 0) {
            $baseline = ($driveSamples | Measure-Object -Average).Average
            $drive.ThermalBaseline = [math]::Round($baseline, 2)
            $drive.ThermalHeadroom = [math]::Round($ThermalWarning - $baseline, 2)
            Write-Host "  DeviceId $($drive.DeviceId): Baseline=$($drive.ThermalBaseline)°C, Headroom=$($drive.ThermalHeadroom)°C" -ForegroundColor Cyan
        }
        else {
            $drive.ThermalBaseline = 40.0
            $drive.ThermalHeadroom = $ThermalWarning - 40.0
            Write-Warning2 "DeviceId $($drive.DeviceId): Using default baseline (no temperature data)"
        }
    }
}
else {
    Write-Info "Skipping calibration - using default baselines"
    foreach ($drive in $drives) {
        $drive.ThermalBaseline = 40.0
        $drive.ThermalHeadroom = $ThermalWarning - 40.0
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# Calculate Virtual VRAM Pool
# ═══════════════════════════════════════════════════════════════════════════════

Write-Section "Phase 3: Virtual VRAM Pool Calculation"

$pageSizeBytes = $PageSizeMB * 1024 * 1024
$usableCapacityGB = [math]::Round($totalCapacityGB * 0.90, 2)  # 90% usable
$totalPages = [math]::Floor(($usableCapacityGB * 1024) / $PageSizeMB)

Write-Host "  Page Size: $PageSizeMB MB" -ForegroundColor White
Write-Host "  Total Raw Capacity: $([math]::Round($totalCapacityGB, 2)) GB" -ForegroundColor White
Write-Host "  Usable Capacity (90%): $usableCapacityGB GB" -ForegroundColor White
Write-Host "  Total Pages: $totalPages" -ForegroundColor Green
Write-Host ""

# Calculate pages per drive based on capacity ratio
$drivePages = @()
foreach ($drive in $drives) {
    $ratio = $drive.SizeGB / $totalCapacityGB
    $pages = [math]::Floor($totalPages * $ratio)
    $drivePages += @{
        DeviceId = $drive.DeviceId
        Pages = $pages
        CapacityMB = $pages * $PageSizeMB
    }
    Write-Host "  DeviceId $($drive.DeviceId): $pages pages ($([math]::Round($pages * $PageSizeMB / 1024, 2)) GB)" -ForegroundColor Cyan
}

# ═══════════════════════════════════════════════════════════════════════════════
# Generate SovereignControlBlock Configuration
# ═══════════════════════════════════════════════════════════════════════════════

Write-Section "Phase 4: SovereignControlBlock Configuration"

# Sort drives by thermal headroom (coolest first)
$sortedDrives = $drives | Sort-Object -Property ThermalHeadroom -Descending

Write-Info "Drive priority order (by thermal headroom):"
$priority = 0
foreach ($d in $sortedDrives) {
    Write-Host "  Priority $($priority): DeviceId $($d.DeviceId) (Headroom: $($d.ThermalHeadroom)°C)" -ForegroundColor White
    $priority++
}
Write-Host ""

# Create sovereign control block config
$scbConfig = @{
    version = "1.2.0"
    generated = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
    
    # Array configuration
    arrayMode = "STRIPED_THERMAL_AWARE"
    activeDrives = $drives.Count
    
    # Drive definitions
    drives = @()
    
    # Thermal thresholds
    thermal = @{
        warning = $ThermalWarning
        critical = $ThermalCritical
        shutdown = 95.0
        monitoringIntervalMs = 5000
    }
    
    # Load balancing
    loadBalancing = @{
        algorithm = "THERMAL_HEADROOM_WEIGHTED"
        thermalWeight = 0.50
        loadWeight = 0.30
        healthWeight = 0.20
        hysteresisThreshold = 0.05
        rebalanceIntervalMs = 10000
    }
    
    # Virtual VRAM pool
    virtualVRAM = @{
        pageSizeMB = $PageSizeMB
        totalPages = $totalPages
        usableCapacityGB = $usableCapacityGB
        prefetchPages = 4
        evictionPolicy = "LRU_THERMAL_AWARE"
    }
    
    # Prediction settings
    prediction = @{
        algorithm = "EWMA"
        alpha = 0.3
        horizonMs = 30000
        enableML = $false
    }
}

# Add drive details
$driveIndex = 0
foreach ($drive in $sortedDrives) {
    $pageAlloc = $drivePages | Where-Object { $_.DeviceId -eq $drive.DeviceId }
    
    $scbConfig.drives += @{
        index = $driveIndex
        deviceId = $drive.DeviceId
        model = $drive.Model
        busType = $drive.BusType
        capacityGB = $drive.SizeGB
        healthStatus = $drive.HealthStatus
        thermalBaseline = $drive.ThermalBaseline
        thermalHeadroom = $drive.ThermalHeadroom
        allocatedPages = $pageAlloc.Pages
        priority = $driveIndex
    }
    $driveIndex++
}

# Save configuration
$scbPath = Join-Path $ConfigPath "sovereign_control_block.json"
$scbConfig | ConvertTo-Json -Depth 10 | Out-File -FilePath $scbPath -Encoding UTF8
Write-Success "Created: sovereign_control_block.json"

# ═══════════════════════════════════════════════════════════════════════════════
# Generate Virtual VRAM Pool Configuration
# ═══════════════════════════════════════════════════════════════════════════════

$vramPoolConfig = @{
    version = "1.0.0"
    generated = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
    
    pool = @{
        totalCapacityGB = $usableCapacityGB
        pageSizeMB = $PageSizeMB
        totalPages = $totalPages
        
        # Page allocation table
        allocationTable = @{
            freePages = $totalPages
            usedPages = 0
            dirtyPages = 0
            evictablePages = 0
        }
        
        # Drive mapping
        driveMapping = @()
        
        # Performance hints
        hints = @{
            sequentialReadAheadPages = 8
            writeCoalescePages = 4
            maxConcurrentIO = 32
            preferredDriveType = "NVMe"
        }
    }
    
    eviction = @{
        policy = "LRU_THERMAL_AWARE"
        maxDirtyRatio = 0.30
        evictionThreshold = 0.85
        emergencyThreshold = 0.95
    }
    
    thermal = @{
        enableThermalAwareness = $true
        redistributeOnWarning = $true
        suspendDriveOnCritical = $true
        coolingPeriodMs = 30000
    }
}

# Add drive mappings
$pageOffset = 0
foreach ($dp in ($drivePages | Sort-Object { ($sortedDrives | Where-Object { $_.DeviceId -eq $_.DeviceId }).ThermalHeadroom } -Descending)) {
    $vramPoolConfig.pool.driveMapping += @{
        deviceId = $dp.DeviceId
        startPage = $pageOffset
        endPage = $pageOffset + $dp.Pages - 1
        pageCount = $dp.Pages
    }
    $pageOffset += $dp.Pages
}

$vramPoolPath = Join-Path $ConfigPath "virtual_vram_pool.json"
$vramPoolConfig | ConvertTo-Json -Depth 10 | Out-File -FilePath $vramPoolPath -Encoding UTF8
Write-Success "Created: virtual_vram_pool.json"

# ═══════════════════════════════════════════════════════════════════════════════
# Update thermal_config.json with calibrated baselines
# ═══════════════════════════════════════════════════════════════════════════════

$thermalConfigPath = Join-Path $ConfigPath "thermal_config.json"
if (Test-Path $thermalConfigPath) {
    $thermalConfig = Get-Content $thermalConfigPath | ConvertFrom-Json
    
    # Update drives with calibrated data
    $thermalConfig.drives = @()
    foreach ($drive in $sortedDrives) {
        $thermalConfig.drives += @{
            deviceId = $drive.DeviceId
            model = $drive.Model
            baseline = $drive.ThermalBaseline
            warningTemp = $ThermalWarning
            criticalTemp = $ThermalCritical
            ratedTBW = 0
        }
    }
    
    $thermalConfig.thresholds.warning = $ThermalWarning
    $thermalConfig.thresholds.critical = $ThermalCritical
    $thermalConfig.generated = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
    
    $thermalConfig | ConvertTo-Json -Depth 10 | Out-File -FilePath $thermalConfigPath -Encoding UTF8
    Write-Success "Updated: thermal_config.json with calibrated baselines"
}

# ═══════════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  ✅ Thermal NVMe Array Initialization Complete                        ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""

Write-Host "📊 Array Summary:" -ForegroundColor Cyan
Write-Host "  • Mode: STRIPED_THERMAL_AWARE" -ForegroundColor White
Write-Host "  • Active Drives: $($drives.Count)" -ForegroundColor White
Write-Host "  • Total Capacity: $([math]::Round($totalCapacityGB, 2)) GB" -ForegroundColor White
Write-Host "  • Usable Capacity: $usableCapacityGB GB" -ForegroundColor White
Write-Host "  • Page Size: $PageSizeMB MB" -ForegroundColor White
Write-Host "  • Total Pages: $totalPages" -ForegroundColor Green
Write-Host ""

Write-Host "🔥 Thermal Configuration:" -ForegroundColor Yellow
Write-Host "  • Warning Threshold: $ThermalWarning°C" -ForegroundColor White
Write-Host "  • Critical Threshold: $ThermalCritical°C" -ForegroundColor White
Write-Host "  • Coolest Drive: DeviceId $($sortedDrives[0].DeviceId) ($($sortedDrives[0].ThermalHeadroom)°C headroom)" -ForegroundColor White
Write-Host ""

Write-Host "📁 Configuration Files:" -ForegroundColor Cyan
Write-Host "  • $scbPath" -ForegroundColor Gray
Write-Host "  • $vramPoolPath" -ForegroundColor Gray
Write-Host "  • $thermalConfigPath" -ForegroundColor Gray
Write-Host ""

# Return result object
return @{
    Success = $true
    Drives = $drives
    TotalCapacityGB = $totalCapacityGB
    UsableCapacityGB = $usableCapacityGB
    TotalPages = $totalPages
    PageSizeMB = $PageSizeMB
    CoolestDrive = $sortedDrives[0].DeviceId
    ConfigPath = $ConfigPath
}
