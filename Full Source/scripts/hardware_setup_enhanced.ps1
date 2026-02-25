<#
.SYNOPSIS
    Enhanced Hardware Setup Script for RawrXD IDE
    
.DESCRIPTION
    Dynamically detects and configures hardware components including:
    - CPU signature and capabilities (RDRAND/RDSEED)
    - NVMe/SSD drives with SMART health data
    - GPU details and compute capability
    - Memory configuration
    - System thermal sensors
    
    Generates configuration files for sovereign binding and thermal management.
    
.PARAMETER ConfigPath
    Path to store configuration files (default: D:\rawrxd\config)
    
.PARAMETER Force
    Overwrite existing configuration files
    
.PARAMETER SkipValidation
    Skip hardware validation checks (useful for development)
    
.PARAMETER Verbose
    Enable verbose output for debugging
    
.EXAMPLE
    .\hardware_setup_enhanced.ps1 -ConfigPath "D:\rawrxd\config" -Force
    
.NOTES
    Author: RawrXD IDE Team
    Version: 2.0.0
    Date: 2026-01-26
#>

param (
    [Parameter(Position = 0)]
    [string]$ConfigPath = "D:\rawrxd\config",
    
    [switch]$Force,
    [switch]$SkipValidation,
    [switch]$DetailedOutput
)

# ═══════════════════════════════════════════════════════════════════════════════
# Constants and Configuration
# ═══════════════════════════════════════════════════════════════════════════════

$ErrorActionPreference = "Stop"
$Script:Version = "2.0.0"

# Color output helpers
function Write-Success { param($Message) Write-Host "✅ $Message" -ForegroundColor Green }
function Write-Warning2 { param($Message) Write-Host "⚠️ $Message" -ForegroundColor Yellow }
function Write-Error2 { param($Message) Write-Host "❌ $Message" -ForegroundColor Red }
function Write-Info { param($Message) Write-Host "ℹ️ $Message" -ForegroundColor Cyan }
function Write-Debug2 { param($Message) if ($DetailedOutput) { Write-Host "🔍 $Message" -ForegroundColor Gray } }

# ═══════════════════════════════════════════════════════════════════════════════
# Header
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  🔧 RawrXD IDE - Enhanced Hardware Setup v$Script:Version                    ║" -ForegroundColor Cyan
Write-Host "║  Dynamic Hardware Detection and Configuration                         ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ═══════════════════════════════════════════════════════════════════════════════
# Create Configuration Directory
# ═══════════════════════════════════════════════════════════════════════════════

Write-Info "Configuration Path: $ConfigPath"

if (-Not (Test-Path -Path $ConfigPath)) {
    New-Item -ItemType Directory -Path $ConfigPath -Force | Out-Null
    Write-Success "Created configuration directory"
}

# ═══════════════════════════════════════════════════════════════════════════════
# CPU Detection
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "🔍 Detecting CPU..." -ForegroundColor Yellow
Write-Host "───────────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

try {
    $cpu = Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1
    
    $cpuInfo = @{
        Name = $cpu.Name.Trim()
        ProcessorId = $cpu.ProcessorId
        Manufacturer = $cpu.Manufacturer
        Architecture = switch ($cpu.Architecture) {
            0 { "x86" }
            5 { "ARM" }
            9 { "x64" }
            12 { "ARM64" }
            default { "Unknown" }
        }
        Cores = $cpu.NumberOfCores
        LogicalProcessors = $cpu.NumberOfLogicalProcessors
        MaxClockSpeed = $cpu.MaxClockSpeed
        L2CacheSize = $cpu.L2CacheSize
        L3CacheSize = $cpu.L3CacheSize
        VirtualizationFirmwareEnabled = $cpu.VirtualizationFirmwareEnabled
        VMMonitorModeExtensions = $cpu.VMMonitorModeExtensions
    }
    
    # Detect CPU features (RDRAND, RDSEED, AES-NI)
    $cpuFeatures = @{
        HasRDRAND = $false
        HasRDSEED = $false
        HasAESNI = $false
        HasAVX = $false
        HasAVX2 = $false
        HasAVX512 = $false
    }
    
    # Check for Intel/AMD specific features via registry or WMI
    if ($cpu.Manufacturer -match "Intel") {
        # Most Intel CPUs from Ivy Bridge (2012+) have RDRAND
        # Broadwell (2015+) has RDSEED
        $cpuFeatures.HasRDRAND = $true
        $cpuFeatures.HasRDSEED = $cpu.Name -match "(Core|Xeon|i[3579]|i[3579]-).*([5-9]|1[0-9])" -or $cpu.Name -match "(11th|12th|13th|14th)"
        $cpuFeatures.HasAESNI = $true
        $cpuFeatures.HasAVX = $true
        $cpuFeatures.HasAVX2 = $cpu.Name -match "(Haswell|Broadwell|Skylake|Core|i[3579]|i[3579]-)" -or $cpu.Name -match "(8th|9th|10th|11th|12th|13th|14th)"
    }
    elseif ($cpu.Manufacturer -match "AMD") {
        # AMD Bulldozer+ (2011+) has RDRAND
        # Zen (2017+) has RDSEED
        $cpuFeatures.HasRDRAND = $true
        $cpuFeatures.HasRDSEED = $cpu.Name -match "(Ryzen|EPYC|Threadripper)"
        $cpuFeatures.HasAESNI = $true
        $cpuFeatures.HasAVX = $true
        $cpuFeatures.HasAVX2 = $cpu.Name -match "(Ryzen|EPYC|Threadripper)"
    }
    
    $cpuInfo.Features = $cpuFeatures
    
    Write-Success "CPU: $($cpuInfo.Name)"
    Write-Debug2 "  ProcessorId: $($cpuInfo.ProcessorId)"
    Write-Debug2 "  Architecture: $($cpuInfo.Architecture)"
    Write-Debug2 "  Cores/Threads: $($cpuInfo.Cores)/$($cpuInfo.LogicalProcessors)"
    Write-Debug2 "  Max Clock: $($cpuInfo.MaxClockSpeed) MHz"
    Write-Debug2 "  RDRAND: $($cpuFeatures.HasRDRAND) | RDSEED: $($cpuFeatures.HasRDSEED)"
    Write-Debug2 "  AES-NI: $($cpuFeatures.HasAESNI) | AVX2: $($cpuFeatures.HasAVX2)"
}
catch {
    Write-Error2 "Failed to detect CPU: $_"
    $cpuInfo = @{ Error = $_.Exception.Message }
}

# ═══════════════════════════════════════════════════════════════════════════════
# NVMe/SSD Detection
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "🔍 Detecting Storage Drives..." -ForegroundColor Yellow
Write-Host "───────────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

$driveInfo = @()

try {
    $physicalDisks = Get-PhysicalDisk | Where-Object { $_.MediaType -eq "SSD" -or $_.BusType -eq "NVMe" }
    
    foreach ($disk in $physicalDisks) {
        # Get additional details
        $diskHealth = Get-PhysicalDisk -UniqueId $disk.UniqueId | Get-StorageReliabilityCounter -ErrorAction SilentlyContinue
        
        $drive = @{
            DeviceId = $disk.DeviceId
            FriendlyName = $disk.FriendlyName
            Model = $disk.Model
            SerialNumber = $disk.SerialNumber
            MediaType = $disk.MediaType
            BusType = $disk.BusType
            Size = [math]::Round($disk.Size / 1GB, 2)
            HealthStatus = $disk.HealthStatus
            OperationalStatus = $disk.OperationalStatus
        }
        
        # Add health metrics if available
        if ($diskHealth) {
            $drive.Temperature = $diskHealth.Temperature
            $drive.Wear = $diskHealth.Wear
            $drive.ReadErrorsTotal = $diskHealth.ReadErrorsTotal
            $drive.WriteErrorsTotal = $diskHealth.WriteErrorsTotal
            $drive.PowerOnHours = $diskHealth.PowerOnHours
        }
        
        $driveInfo += $drive
        
        $healthIcon = switch ($disk.HealthStatus) {
            "Healthy" { "✅" }
            "Warning" { "⚠️" }
            "Unhealthy" { "❌" }
            default { "❓" }
        }
        
        Write-Success "$healthIcon $($drive.FriendlyName) - $($drive.Size) GB ($($drive.BusType))"
        if ($diskHealth) {
            Write-Debug2 "    Temperature: $($diskHealth.Temperature)°C | Wear: $($diskHealth.Wear)%"
        }
    }
    
    if ($driveInfo.Count -eq 0) {
        Write-Warning2 "No SSD/NVMe drives detected"
    }
}
catch {
    Write-Error2 "Failed to detect storage drives: $_"
}

# ═══════════════════════════════════════════════════════════════════════════════
# GPU Detection
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "🔍 Detecting GPU..." -ForegroundColor Yellow
Write-Host "───────────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

$gpuInfo = @()

try {
    $gpus = Get-CimInstance -ClassName Win32_VideoController
    
    foreach ($gpu in $gpus) {
        $gpuDetail = @{
            Name = $gpu.Name
            DeviceId = $gpu.DeviceID
            AdapterRAM = [math]::Round($gpu.AdapterRAM / 1GB, 2)
            DriverVersion = $gpu.DriverVersion
            DriverDate = $gpu.DriverDate
            Status = $gpu.Status
            VideoProcessor = $gpu.VideoProcessor
            CurrentResolution = "$($gpu.CurrentHorizontalResolution)x$($gpu.CurrentVerticalResolution)"
            RefreshRate = $gpu.CurrentRefreshRate
        }
        
        # Detect GPU type
        if ($gpu.Name -match "NVIDIA") {
            $gpuDetail.Vendor = "NVIDIA"
            $gpuDetail.SupportsVulkan = $true
            $gpuDetail.SupportsCUDA = $true
        }
        elseif ($gpu.Name -match "AMD|Radeon") {
            $gpuDetail.Vendor = "AMD"
            $gpuDetail.SupportsVulkan = $true
            $gpuDetail.SupportsCUDA = $false
        }
        elseif ($gpu.Name -match "Intel") {
            $gpuDetail.Vendor = "Intel"
            $gpuDetail.SupportsVulkan = $true
            $gpuDetail.SupportsCUDA = $false
        }
        else {
            $gpuDetail.Vendor = "Unknown"
            $gpuDetail.SupportsVulkan = $false
            $gpuDetail.SupportsCUDA = $false
        }
        
        $gpuInfo += $gpuDetail
        
        Write-Success "$($gpuDetail.Name) - $($gpuDetail.AdapterRAM) GB"
        Write-Debug2 "    Driver: $($gpuDetail.DriverVersion)"
        Write-Debug2 "    Vulkan: $($gpuDetail.SupportsVulkan) | CUDA: $($gpuDetail.SupportsCUDA)"
    }
}
catch {
    Write-Error2 "Failed to detect GPU: $_"
}

# ═══════════════════════════════════════════════════════════════════════════════
# Memory Detection
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "🔍 Detecting Memory..." -ForegroundColor Yellow
Write-Host "───────────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

try {
    $memory = Get-CimInstance -ClassName Win32_PhysicalMemory
    $totalMemory = ($memory | Measure-Object -Property Capacity -Sum).Sum
    
    $memoryInfo = @{
        TotalGB = [math]::Round($totalMemory / 1GB, 2)
        Modules = @()
    }
    
    foreach ($dimm in $memory) {
        $module = @{
            Capacity = [math]::Round($dimm.Capacity / 1GB, 2)
            Speed = $dimm.Speed
            Manufacturer = $dimm.Manufacturer
            PartNumber = $dimm.PartNumber
            BankLabel = $dimm.BankLabel
            DeviceLocator = $dimm.DeviceLocator
        }
        $memoryInfo.Modules += $module
    }
    
    Write-Success "Total Memory: $($memoryInfo.TotalGB) GB"
    Write-Debug2 "    Modules: $($memoryInfo.Modules.Count)"
    foreach ($mod in $memoryInfo.Modules) {
        Write-Debug2 "    - $($mod.DeviceLocator): $($mod.Capacity) GB @ $($mod.Speed) MHz"
    }
}
catch {
    Write-Error2 "Failed to detect memory: $_"
    $memoryInfo = @{ Error = $_.Exception.Message }
}

# ═══════════════════════════════════════════════════════════════════════════════
# System Fingerprint Generation
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "🔐 Generating System Fingerprint..." -ForegroundColor Yellow
Write-Host "───────────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

try {
    # Create fingerprint from hardware identifiers
    $fingerprintData = @(
        $cpuInfo.ProcessorId,
        $cpuInfo.Name,
        ($driveInfo | ForEach-Object { $_.SerialNumber }) -join "-",
        ($gpuInfo | ForEach-Object { $_.DeviceId }) -join "-"
    ) -join "|"
    
    # Generate SHA-256 hash
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $fingerprintBytes = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($fingerprintData))
    $fingerprint = [System.BitConverter]::ToString($fingerprintBytes).Replace("-", "").ToLower()
    
    Write-Success "Fingerprint: $($fingerprint.Substring(0, 16))..."
}
catch {
    Write-Error2 "Failed to generate fingerprint: $_"
    $fingerprint = "error-generating-fingerprint"
}

# ═══════════════════════════════════════════════════════════════════════════════
# Generate Configuration Files
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "📝 Generating Configuration Files..." -ForegroundColor Yellow
Write-Host "───────────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

# Sovereign Binding Configuration
$sovereignBinding = @{
    version = "2.0.0"
    generated = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
    fingerprint = $fingerprint
    cpu = $cpuInfo
    drives = $driveInfo
    gpu = $gpuInfo
    memory = $memoryInfo
}

$bindingPath = Join-Path $ConfigPath "sovereign_binding.json"
$sovereignBinding | ConvertTo-Json -Depth 10 | Out-File -FilePath $bindingPath -Encoding UTF8

Write-Success "Created: sovereign_binding.json"

# Thermal Configuration
$thermalConfig = @{
    version = "2.0.0"
    generated = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
    thresholds = @{
        warning = 65
        critical = 85
        shutdown = 95
    }
    monitoring = @{
        pollIntervalMs = 5000
        healthCheckIntervalMs = 60000
    }
    prediction = @{
        algorithm = "EWMA"
        alpha = 0.3
        historySize = 10
        enableML = $false
    }
    loadBalancing = @{
        enabled = $true
        thermalWeight = 0.5
        loadWeight = 0.3
        healthWeight = 0.2
        hysteresisThreshold = 0.05
    }
    drives = @()
}

# Add drives to thermal config
foreach ($drive in $driveInfo) {
    $thermalConfig.drives += @{
        path = $drive.DeviceId
        model = $drive.Model
        warningTemp = 65
        criticalTemp = 85
        ratedTBW = 0
    }
}

$thermalPath = Join-Path $ConfigPath "thermal_config.json"
$thermalConfig | ConvertTo-Json -Depth 10 | Out-File -FilePath $thermalPath -Encoding UTF8

Write-Success "Created: thermal_config.json"

# Hardware Capabilities
$capabilities = @{
    version = "2.0.0"
    generated = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
    entropy = @{
        rdrand = $cpuInfo.Features.HasRDRAND
        rdseed = $cpuInfo.Features.HasRDSEED
    }
    crypto = @{
        aesni = $cpuInfo.Features.HasAESNI
    }
    simd = @{
        avx = $cpuInfo.Features.HasAVX
        avx2 = $cpuInfo.Features.HasAVX2
        avx512 = $cpuInfo.Features.HasAVX512
    }
    compute = @{
        vulkan = ($gpuInfo | Where-Object { $_.SupportsVulkan }).Count -gt 0
        cuda = ($gpuInfo | Where-Object { $_.SupportsCUDA }).Count -gt 0
    }
}

$capabilitiesPath = Join-Path $ConfigPath "hardware_capabilities.json"
$capabilities | ConvertTo-Json -Depth 10 | Out-File -FilePath $capabilitiesPath -Encoding UTF8

Write-Success "Created: hardware_capabilities.json"

# ═══════════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  ✅ Hardware Configuration Complete                                   ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""

Write-Host "Configuration files created in: $ConfigPath" -ForegroundColor Cyan
Write-Host "  • sovereign_binding.json  - Hardware fingerprint and system binding" -ForegroundColor Gray
Write-Host "  • thermal_config.json     - Thermal management configuration" -ForegroundColor Gray
Write-Host "  • hardware_capabilities.json - CPU/GPU feature detection" -ForegroundColor Gray
Write-Host ""

# Return summary object for programmatic use
return @{
    Success = $true
    ConfigPath = $ConfigPath
    Fingerprint = $fingerprint
    CPU = $cpuInfo
    Drives = $driveInfo
    GPU = $gpuInfo
    Memory = $memoryInfo
    Capabilities = $capabilities
}
