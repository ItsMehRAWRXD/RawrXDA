# ═══════════════════════════════════════════════════════════════════════════════
# RAWRXD HARDWARE SETUP & DETECTION v2.0.0
# ═══════════════════════════════════════════════════════════════════════════════
# PURPOSE: Dynamic hardware detection and configuration for RawrXD IDE
#          Detects CPU, NVMe drives, GPU, and generates sovereign binding
# AUTHOR: RawrXD Sovereign Architect
# DATE: January 26, 2026
# ═══════════════════════════════════════════════════════════════════════════════

[CmdletBinding()]
param (
    [Parameter(Position = 0)]
    [string]$ConfigPath = "D:\rawrxd\config",
    
    [Parameter()]
    [switch]$Force,
    
    [Parameter()]
    [switch]$Verbose,
    
    [Parameter()]
    [switch]$SkipValidation,
    
    [Parameter()]
    [ValidateSet("Sustainable", "Hybrid", "Burst")]
    [string]$DefaultMode = "Sustainable"
)

$ErrorActionPreference = 'Stop'
$Script:CONFIG_VERSION = "2.0.0"

# ═══════════════════════════════════════════════════════════════════════════════
# BANNER
# ═══════════════════════════════════════════════════════════════════════════════

function Show-Banner {
    Write-Host ""
    Write-Host "  ╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "  ║  🔧 RAWRXD HARDWARE SETUP & DETECTION v2.0.0                 ║" -ForegroundColor Cyan
    Write-Host "  ║     Dynamic Configuration for Sovereign Thermal Management   ║" -ForegroundColor DarkCyan
    Write-Host "  ╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

# ═══════════════════════════════════════════════════════════════════════════════
# DIRECTORY SETUP
# ═══════════════════════════════════════════════════════════════════════════════

function Initialize-ConfigDirectory {
    param([string]$Path)
    
    Write-Host "📁 Initializing configuration directory..." -ForegroundColor Yellow
    
    $subDirs = @(
        $Path,
        "$Path\thermal",
        "$Path\hardware",
        "$Path\security",
        "$Path\backup"
    )
    
    foreach ($dir in $subDirs) {
        if (-not (Test-Path -Path $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
            Write-Host "   ✅ Created: $dir" -ForegroundColor Green
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# CPU DETECTION
# ═══════════════════════════════════════════════════════════════════════════════

function Get-CPUDetails {
    Write-Host "🔍 Detecting CPU..." -ForegroundColor Yellow
    
    try {
        $cpu = Get-CimInstance -ClassName Win32_Processor -ErrorAction Stop
        
        $cpuInfo = @{
            Name                = $cpu.Name.Trim()
            ProcessorId         = $cpu.ProcessorId
            Manufacturer        = $cpu.Manufacturer
            NumberOfCores       = $cpu.NumberOfCores
            NumberOfLogicalProcessors = $cpu.NumberOfLogicalProcessors
            MaxClockSpeed       = $cpu.MaxClockSpeed
            L2CacheSize         = $cpu.L2CacheSize
            L3CacheSize         = $cpu.L3CacheSize
            Architecture        = switch ($cpu.Architecture) {
                0 { "x86" }
                5 { "ARM" }
                9 { "x64" }
                12 { "ARM64" }
                default { "Unknown" }
            }
            VirtualizationEnabled = $cpu.VirtualizationFirmwareEnabled
            SecureBootCapable   = $cpu.SecureBootCapable
            EntropyHash         = Get-EntropyHash -ProcessorId $cpu.ProcessorId
        }
        
        Write-Host "   ✅ CPU: $($cpuInfo.Name)" -ForegroundColor Green
        Write-Host "      Cores: $($cpuInfo.NumberOfCores) | Threads: $($cpuInfo.NumberOfLogicalProcessors)" -ForegroundColor DarkGray
        Write-Host "      L3 Cache: $($cpuInfo.L3CacheSize) KB" -ForegroundColor DarkGray
        
        return $cpuInfo
    }
    catch {
        Write-Warning "Failed to detect CPU: $_"
        return @{
            Name = "Unknown"
            ProcessorId = "0000000000000000"
            NumberOfCores = 4
            NumberOfLogicalProcessors = 8
            EntropyHash = "FALLBACK_ENTROPY_" + [guid]::NewGuid().ToString("N").Substring(0, 16)
        }
    }
}

function Get-EntropyHash {
    param([string]$ProcessorId)
    
    # Generate entropy hash from processor ID and system info
    $entropySource = "$ProcessorId-$env:COMPUTERNAME-$env:USERNAME"
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $hash = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($entropySource))
    return [BitConverter]::ToString($hash).Replace("-", "").Substring(0, 32)
}

# ═══════════════════════════════════════════════════════════════════════════════
# NVME DRIVE DETECTION
# ═══════════════════════════════════════════════════════════════════════════════

function Get-NVMeDrives {
    Write-Host "🔍 Detecting NVMe drives..." -ForegroundColor Yellow
    
    $drives = @()
    
    try {
        # Get physical disks
        $physicalDisks = Get-PhysicalDisk -ErrorAction Stop | 
                         Where-Object { $_.MediaType -eq 'SSD' -and $_.BusType -eq 'NVMe' }
        
        foreach ($disk in $physicalDisks) {
            $driveInfo = @{
                Index           = $drives.Count
                DeviceId        = $disk.DeviceId
                FriendlyName    = $disk.FriendlyName
                Model           = $disk.Model
                SerialNumber    = $disk.SerialNumber
                MediaType       = $disk.MediaType
                BusType         = $disk.BusType
                Size            = $disk.Size
                SizeGB          = [math]::Round($disk.Size / 1GB, 2)
                HealthStatus    = $disk.HealthStatus
                OperationalStatus = $disk.OperationalStatus
                # Thermal settings
                MaxAllowedTemp  = Get-DriveMaxTemp -Model $disk.Model
                ThermalThreshold = 60.0  # Default sovereign threshold
                CurrentTemp     = 0.0    # Will be updated at runtime
            }
            
            $drives += $driveInfo
            
            Write-Host "   ✅ NVMe$($driveInfo.Index): $($driveInfo.FriendlyName)" -ForegroundColor Green
            Write-Host "      Size: $($driveInfo.SizeGB) GB | Health: $($driveInfo.HealthStatus)" -ForegroundColor DarkGray
        }
        
        # If no NVMe drives found, check for any SSDs
        if ($drives.Count -eq 0) {
            Write-Host "   ⚠️  No NVMe drives found, checking for SSDs..." -ForegroundColor Yellow
            
            $ssds = Get-PhysicalDisk | Where-Object { $_.MediaType -eq 'SSD' }
            
            foreach ($disk in $ssds) {
                $driveInfo = @{
                    Index           = $drives.Count
                    DeviceId        = $disk.DeviceId
                    FriendlyName    = $disk.FriendlyName
                    Model           = $disk.Model
                    SerialNumber    = $disk.SerialNumber
                    MediaType       = $disk.MediaType
                    BusType         = $disk.BusType
                    Size            = $disk.Size
                    SizeGB          = [math]::Round($disk.Size / 1GB, 2)
                    HealthStatus    = $disk.HealthStatus
                    OperationalStatus = $disk.OperationalStatus
                    MaxAllowedTemp  = 70.0  # SATA SSDs typically have higher tolerance
                    ThermalThreshold = 60.0
                    CurrentTemp     = 0.0
                }
                
                $drives += $driveInfo
                Write-Host "   ✅ SSD$($driveInfo.Index): $($driveInfo.FriendlyName)" -ForegroundColor Green
            }
        }
        
    }
    catch {
        Write-Warning "Failed to enumerate drives: $_"
    }
    
    # Ensure at least one drive entry
    if ($drives.Count -eq 0) {
        Write-Host "   ⚠️  Using fallback drive configuration" -ForegroundColor Yellow
        $drives += @{
            Index           = 0
            DeviceId        = "FALLBACK"
            FriendlyName    = "System Drive"
            Model           = "Generic SSD"
            MediaType       = "SSD"
            BusType         = "Unknown"
            SizeGB          = 500
            HealthStatus    = "Healthy"
            MaxAllowedTemp  = 70.0
            ThermalThreshold = 60.0
            CurrentTemp     = 0.0
        }
    }
    
    Write-Host "   📊 Total drives detected: $($drives.Count)" -ForegroundColor Cyan
    
    return $drives
}

function Get-DriveMaxTemp {
    param([string]$Model)
    
    # Known drive thermal limits (manufacturer specifications)
    $thermalLimits = @{
        "SK hynix"      = 70
        "Samsung 990"   = 70
        "Samsung 980"   = 70
        "WD Black"      = 70
        "Crucial T705"  = 70
        "Sabrent"       = 70
        "Seagate"       = 70
        "Intel"         = 70
        "Micron"        = 70
    }
    
    foreach ($key in $thermalLimits.Keys) {
        if ($Model -like "*$key*") {
            return $thermalLimits[$key]
        }
    }
    
    return 70  # Default safe maximum
}

# ═══════════════════════════════════════════════════════════════════════════════
# GPU DETECTION
# ═══════════════════════════════════════════════════════════════════════════════

function Get-GPUDetails {
    Write-Host "🔍 Detecting GPU..." -ForegroundColor Yellow
    
    try {
        $gpus = Get-CimInstance -ClassName Win32_VideoController -ErrorAction Stop
        
        $gpuList = @()
        
        foreach ($gpu in $gpus) {
            # Skip Microsoft Basic Display Adapter
            if ($gpu.Name -like "*Basic*" -or $gpu.Name -like "*Microsoft*") {
                continue
            }
            
            $gpuInfo = @{
                Name            = $gpu.Name
                DriverVersion   = $gpu.DriverVersion
                VideoProcessor  = $gpu.VideoProcessor
                AdapterRAM      = $gpu.AdapterRAM
                AdapterRAMGB    = if ($gpu.AdapterRAM) { [math]::Round($gpu.AdapterRAM / 1GB, 2) } else { 0 }
                CurrentResolution = "$($gpu.CurrentHorizontalResolution)x$($gpu.CurrentVerticalResolution)"
                RefreshRate     = $gpu.CurrentRefreshRate
                # Thermal settings
                MaxJunctionTemp = Get-GPUMaxTemp -Name $gpu.Name
                ThermalTarget   = 80.0
                CurrentTemp     = 0.0
            }
            
            $gpuList += $gpuInfo
            
            Write-Host "   ✅ GPU: $($gpuInfo.Name)" -ForegroundColor Green
            Write-Host "      VRAM: $($gpuInfo.AdapterRAMGB) GB | Driver: $($gpuInfo.DriverVersion)" -ForegroundColor DarkGray
        }
        
        if ($gpuList.Count -eq 0) {
            Write-Host "   ⚠️  No dedicated GPU detected" -ForegroundColor Yellow
            $gpuList += @{
                Name = "Integrated Graphics"
                MaxJunctionTemp = 100
                ThermalTarget = 80.0
                CurrentTemp = 0.0
            }
        }
        
        return $gpuList
    }
    catch {
        Write-Warning "Failed to detect GPU: $_"
        return @(@{
            Name = "Unknown"
            MaxJunctionTemp = 100
            ThermalTarget = 80.0
            CurrentTemp = 0.0
        })
    }
}

function Get-GPUMaxTemp {
    param([string]$Name)
    
    # Known GPU thermal limits
    if ($Name -like "*AMD*" -or $Name -like "*Radeon*") {
        return 110  # AMD Tjunction
    }
    elseif ($Name -like "*NVIDIA*" -or $Name -like "*GeForce*" -or $Name -like "*RTX*") {
        return 83   # NVIDIA typical max
    }
    elseif ($Name -like "*Intel*" -or $Name -like "*Arc*") {
        return 100
    }
    
    return 100  # Default
}

# ═══════════════════════════════════════════════════════════════════════════════
# MEMORY DETECTION
# ═══════════════════════════════════════════════════════════════════════════════

function Get-MemoryDetails {
    Write-Host "🔍 Detecting system memory..." -ForegroundColor Yellow
    
    try {
        $memory = Get-CimInstance -ClassName Win32_PhysicalMemory -ErrorAction Stop
        $totalMemory = ($memory | Measure-Object -Property Capacity -Sum).Sum
        
        $memInfo = @{
            TotalBytes      = $totalMemory
            TotalGB         = [math]::Round($totalMemory / 1GB, 2)
            ModuleCount     = $memory.Count
            Modules         = @($memory | ForEach-Object {
                @{
                    Manufacturer = $_.Manufacturer
                    Speed        = $_.Speed
                    Capacity     = $_.Capacity
                    CapacityGB   = [math]::Round($_.Capacity / 1GB, 2)
                    FormFactor   = $_.FormFactor
                }
            })
        }
        
        Write-Host "   ✅ RAM: $($memInfo.TotalGB) GB ($($memInfo.ModuleCount) modules)" -ForegroundColor Green
        
        return $memInfo
    }
    catch {
        Write-Warning "Failed to detect memory: $_"
        return @{
            TotalGB = 16
            ModuleCount = 2
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# SOVEREIGN BINDING GENERATION
# ═══════════════════════════════════════════════════════════════════════════════

function New-SovereignBinding {
    param(
        [hashtable]$CPU,
        [array]$NVMe,
        [array]$GPU,
        [hashtable]$Memory
    )
    
    Write-Host "🔐 Generating Sovereign Hardware Binding..." -ForegroundColor Yellow
    
    # Create unique hardware fingerprint
    $fingerprintSource = @(
        $CPU.ProcessorId,
        $CPU.Name,
        ($NVMe | ForEach-Object { $_.DeviceId }) -join "|",
        ($GPU | ForEach-Object { $_.Name }) -join "|"
    ) -join ":"
    
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $fingerprintHash = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($fingerprintSource))
    $fingerprint = [BitConverter]::ToString($fingerprintHash).Replace("-", "")
    
    $binding = @{
        Version             = $Script:CONFIG_VERSION
        GeneratedAt         = [DateTime]::UtcNow.ToString("o")
        Fingerprint         = $fingerprint
        EntropyHash         = $CPU.EntropyHash
        HardwareSummary     = @{
            CPU             = $CPU.Name
            Cores           = $CPU.NumberOfCores
            Threads         = $CPU.NumberOfLogicalProcessors
            NVMeCount       = $NVMe.Count
            TotalStorageGB  = ($NVMe | Measure-Object -Property SizeGB -Sum).Sum
            GPU             = ($GPU | Select-Object -First 1).Name
            MemoryGB        = $Memory.TotalGB
        }
        ThermalConfig       = @{
            DefaultMode         = $DefaultMode
            SustainableCeiling  = 59.5
            HybridCeiling       = 65.0
            BurstCeiling        = 75.0
            BurstDuration       = 60
            CooldownPeriod      = 300
        }
        SecurityConfig      = @{
            SessionKeyLength    = 128
            EntropySource       = "RDRAND"
            AuthRequired        = $true
        }
    }
    
    Write-Host "   ✅ Binding generated: $($fingerprint.Substring(0, 16))..." -ForegroundColor Green
    
    return $binding
}

# ═══════════════════════════════════════════════════════════════════════════════
# THERMAL GOVERNOR CONFIG
# ═══════════════════════════════════════════════════════════════════════════════

function New-ThermalGovernorConfig {
    param(
        [array]$NVMe,
        [array]$GPU
    )
    
    Write-Host "🌡️  Generating Thermal Governor configuration..." -ForegroundColor Yellow
    
    $config = @{
        Version             = $Script:CONFIG_VERSION
        GeneratedAt         = [DateTime]::UtcNow.ToString("o")
        
        # PID Controller settings
        PIDController       = @{
            Kp              = 2.5      # Proportional gain
            Ki              = 0.1      # Integral gain
            Kd              = 0.5      # Derivative gain
            SetPoint        = 55.0     # Target temperature
            OutputMin       = 0        # Minimum throttle %
            OutputMax       = 100      # Maximum throttle %
        }
        
        # EWMA Prediction settings
        Prediction          = @{
            Alpha           = 0.3
            HistorySize     = 20
            HorizonMs       = 5000
            MinConfidence   = 0.7
        }
        
        # Thermal zones for each drive
        DriveZones          = @($NVMe | ForEach-Object {
            @{
                Index           = $_.Index
                Name            = $_.FriendlyName
                CriticalTemp    = $_.MaxAllowedTemp
                ThrottleTemp    = $_.ThermalThreshold
                TargetTemp      = 55.0
                CurrentPolicy   = "Sustainable"
            }
        })
        
        # GPU thermal config
        GPUConfig           = @{
            Name            = ($GPU | Select-Object -First 1).Name
            MaxJunction     = ($GPU | Select-Object -First 1).MaxJunctionTemp
            ThrottlePoint   = 80.0
            TargetTemp      = 70.0
        }
        
        # Sampling configuration
        Sampling            = @{
            IntervalMs      = 1000
            BufferSize      = 100
            LogToFile       = $true
            LogPath         = "D:\rawrxd\logs\thermal.log"
        }
    }
    
    Write-Host "   ✅ Thermal governor configured for $($NVMe.Count) drives" -ForegroundColor Green
    
    return $config
}

# ═══════════════════════════════════════════════════════════════════════════════
# JITMAP CONFIG (LBA Mapping)
# ═══════════════════════════════════════════════════════════════════════════════

function New-JitMapConfig {
    param([array]$NVMe)
    
    Write-Host "📍 Generating JIT-LBA mapper configuration..." -ForegroundColor Yellow
    
    $config = @{
        Version             = $Script:CONFIG_VERSION
        GeneratedAt         = [DateTime]::UtcNow.ToString("o")
        
        # MMIO settings (simulated - real values require kernel driver)
        Controllers         = @($NVMe | ForEach-Object {
            @{
                Index               = $_.Index
                DeviceId            = $_.DeviceId
                # Simulated PCIe BAR0 addresses
                BAR0Address         = "0x{0:X16}" -f (0xF8000000 + ($_.Index * 0x10000))
                DoorbellBase        = "0x{0:X16}" -f (0xF8001000 + ($_.Index * 0x10000))
                QueueDepth          = 64
                MaxSQEntries        = 1024
                MaxCQEntries        = 1024
            }
        })
        
        # Tensor mapping settings
        TensorMapping       = @{
            MaxTensors          = 32768
            BlockSize           = 4096        # 4K alignment
            StripeSizeKB        = 256
            PrefetchDepth       = 8
        }
        
        # Buffer settings
        Buffers             = @{
            ReadAheadMB         = 64
            WriteBufferMB       = 32
            CommandBufferCount  = 16
        }
    }
    
    Write-Host "   ✅ JIT-LBA config generated for $($NVMe.Count) controllers" -ForegroundColor Green
    
    return $config
}

# ═══════════════════════════════════════════════════════════════════════════════
# PIPE BRIDGE CONFIG
# ═══════════════════════════════════════════════════════════════════════════════

function New-PipeBridgeConfig {
    Write-Host "🔌 Generating named pipe bridge configuration..." -ForegroundColor Yellow
    
    $config = @{
        Version             = $Script:CONFIG_VERSION
        GeneratedAt         = [DateTime]::UtcNow.ToString("o")
        
        Pipes               = @{
            SovereignPipe       = @{
                Name            = "\\.\pipe\RawrXD_Sovereign_Pipe"
                BufferSize      = 65536
                MaxInstances    = 4
                Timeout         = 5000
            }
            ThermalPipe         = @{
                Name            = "\\.\pipe\RawrXD_Thermal_Pipe"
                BufferSize      = 16384
                MaxInstances    = 2
                Timeout         = 1000
            }
            PluginLoaderPipe    = @{
                Name            = "\\.\pipe\RawrXD_PluginLoader_{0}"
                BufferSize      = 32768
                MaxInstances    = 1
                Timeout         = 5000
            }
        }
        
        Security            = @{
            RequireAdmin        = $false
            AllowRemote         = $false
            SecurityDescriptor  = "D:(A;;GA;;;BA)(A;;GA;;;SY)"
        }
    }
    
    Write-Host "   ✅ Pipe bridge configuration generated" -ForegroundColor Green
    
    return $config
}

# ═══════════════════════════════════════════════════════════════════════════════
# HUD CONFIG
# ═══════════════════════════════════════════════════════════════════════════════

function New-HUDConfig {
    Write-Host "🖥️  Generating HUD overlay configuration..." -ForegroundColor Yellow
    
    $config = @{
        Version             = $Script:CONFIG_VERSION
        GeneratedAt         = [DateTime]::UtcNow.ToString("o")
        
        Display             = @{
            Enabled             = $true
            Hotkey              = "Ctrl+Shift+S"
            RefreshRateMs       = 500
            Position            = "TopRight"
            Opacity             = 0.9
            Theme               = "Sovereign"
        }
        
        Widgets             = @{
            ThermalBars         = $true
            PredictionGraph     = $true
            LoadDistribution    = $true
            LatencyMonitor      = $true
            ThroughputGraph     = $true
            BurstModeIndicator  = $true
        }
        
        Alerts              = @{
            ThermalWarning      = 55.0
            ThermalCritical     = 60.0
            SoundEnabled        = $false
            FlashOnCritical     = $true
        }
    }
    
    Write-Host "   ✅ HUD configuration generated (Hotkey: Ctrl+Shift+S)" -ForegroundColor Green
    
    return $config
}

# ═══════════════════════════════════════════════════════════════════════════════
# SAVE CONFIGURATIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Save-Configuration {
    param(
        [string]$Path,
        [string]$Name,
        [object]$Data
    )
    
    $filePath = Join-Path $Path "$Name.json"
    
    # Backup existing config
    if (Test-Path $filePath) {
        $backupPath = Join-Path "$Path\backup" "$Name-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
        Copy-Item $filePath $backupPath -Force
    }
    
    $Data | ConvertTo-Json -Depth 10 | Out-File -FilePath $filePath -Encoding UTF8 -Force
    Write-Host "   💾 Saved: $filePath" -ForegroundColor DarkGray
}

# ═══════════════════════════════════════════════════════════════════════════════
# VALIDATION
# ═══════════════════════════════════════════════════════════════════════════════

function Test-Configuration {
    param([string]$Path)
    
    Write-Host ""
    Write-Host "🔍 Validating configuration..." -ForegroundColor Yellow
    
    $requiredFiles = @(
        "sovereign_binding.json",
        "thermal_governor.json",
        "jitmap_config.json",
        "pipe_bridge.json",
        "sovereign_hud.json"
    )
    
    $valid = $true
    
    foreach ($file in $requiredFiles) {
        $filePath = Join-Path $Path $file
        if (Test-Path $filePath) {
            try {
                $null = Get-Content $filePath | ConvertFrom-Json
                Write-Host "   ✅ $file" -ForegroundColor Green
            }
            catch {
                Write-Host "   ❌ $file (invalid JSON)" -ForegroundColor Red
                $valid = $false
            }
        }
        else {
            Write-Host "   ❌ $file (missing)" -ForegroundColor Red
            $valid = $false
        }
    }
    
    return $valid
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-HardwareSetup {
    Show-Banner
    
    # Check for existing configuration
    $existingConfig = Join-Path $ConfigPath "sovereign_binding.json"
    if ((Test-Path $existingConfig) -and (-not $Force)) {
        Write-Host "⚠️  Configuration already exists. Use -Force to regenerate." -ForegroundColor Yellow
        Write-Host "   Path: $existingConfig" -ForegroundColor DarkGray
        Write-Host ""
        
        $response = Read-Host "Regenerate configuration? (y/N)"
        if ($response -ne 'y' -and $response -ne 'Y') {
            Write-Host "Setup cancelled." -ForegroundColor Gray
            return
        }
    }
    
    # Initialize directories
    Initialize-ConfigDirectory -Path $ConfigPath
    
    Write-Host ""
    
    # Detect hardware
    $cpuInfo = Get-CPUDetails
    $nvmeDrives = Get-NVMeDrives
    $gpuInfo = Get-GPUDetails
    $memoryInfo = Get-MemoryDetails
    
    Write-Host ""
    
    # Generate configurations
    $sovereignBinding = New-SovereignBinding -CPU $cpuInfo -NVMe $nvmeDrives -GPU $gpuInfo -Memory $memoryInfo
    $thermalGovernor = New-ThermalGovernorConfig -NVMe $nvmeDrives -GPU $gpuInfo
    $jitmapConfig = New-JitMapConfig -NVMe $nvmeDrives
    $pipeBridgeConfig = New-PipeBridgeConfig
    $hudConfig = New-HUDConfig
    
    Write-Host ""
    
    # Save configurations
    Write-Host "💾 Saving configurations..." -ForegroundColor Yellow
    Save-Configuration -Path $ConfigPath -Name "sovereign_binding" -Data $sovereignBinding
    Save-Configuration -Path $ConfigPath -Name "thermal_governor" -Data $thermalGovernor
    Save-Configuration -Path $ConfigPath -Name "jitmap_config" -Data $jitmapConfig
    Save-Configuration -Path $ConfigPath -Name "pipe_bridge" -Data $pipeBridgeConfig
    Save-Configuration -Path $ConfigPath -Name "sovereign_hud" -Data $hudConfig
    
    # Validate if requested
    if (-not $SkipValidation) {
        $isValid = Test-Configuration -Path $ConfigPath
        
        if ($isValid) {
            Write-Host ""
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Green
            Write-Host " ✅ HARDWARE SETUP COMPLETE!" -ForegroundColor Green
            Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Green
            Write-Host ""
            Write-Host " Hardware Fingerprint: $($sovereignBinding.Fingerprint.Substring(0, 32))..." -ForegroundColor Cyan
            Write-Host " Default Mode: $DefaultMode" -ForegroundColor Cyan
            Write-Host " Config Path: $ConfigPath" -ForegroundColor Cyan
            Write-Host ""
            Write-Host " Next Steps:" -ForegroundColor Yellow
            Write-Host "   1. Run: .\Sovereign_Injection_Script.ps1 -Mode $DefaultMode" -ForegroundColor Gray
            Write-Host "   2. Launch IDE and press Ctrl+Shift+S for thermal HUD" -ForegroundColor Gray
            Write-Host ""
        }
        else {
            Write-Host ""
            Write-Host "❌ Configuration validation failed. Please check errors above." -ForegroundColor Red
        }
    }
}

# Run setup
Invoke-HardwareSetup
