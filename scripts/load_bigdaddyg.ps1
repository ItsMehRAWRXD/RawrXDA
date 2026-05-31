# ============================================================================
# load_bigdaddyg.ps1 - Load 70B+ Model with Mapped-Window Fallback
# ============================================================================
# Usage: .\load_bigdaddyg.ps1 -ModelPath "F:\models\bigdaddyg.gguf"
# ============================================================================

param(
    [Parameter(Mandatory=$true)]
    [string]$ModelPath,
    
    [string]$ConfigPath = "..\configs\bigdaddyg_70b_loader_config.json",
    [int]$ContextLength = 32768,
    [switch]$Benchmark,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

function Write-Status($Message, $Color = "White") {
    Write-Host "[BigDaddyG] $Message" -ForegroundColor $Color
}

function Test-ModelFile($Path) {
    if (-not (Test-Path $Path)) {
        throw "Model file not found: $Path"
    }
    
    $size = (Get-Item $Path).Length
    $sizeGB = [math]::Round($size / 1GB, 2)
    Write-Status "Model file found: $sizeGB GB" "Green"
    return $sizeGB
}

function Test-MemoryAvailability($RequiredGB) {
    $os = Get-CimInstance -ClassName Win32_OperatingSystem
    $availableGB = [math]::Round($os.FreePhysicalMemory / 1MB, 2)
    $totalGB = [math]::Round($os.TotalVisibleMemorySize / 1MB, 2)
    
    Write-Status "System Memory: $availableGB GB available / $totalGB GB total" "Cyan"
    
    if ($availableGB -lt $RequiredGB) {
        throw "Insufficient memory. Required: $RequiredGB GB, Available: $availableGB GB"
    }
    
    return $availableGB
}

function Enable-LargePageSupport() {
    # Check if large pages are available
    $privilege = @"
using System;
using System.Runtime.InteropServices;
public class LargePagePrivilege {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetLargePageMinimum();
}
"@
    
    try {
        Add-Type -TypeDefinition $privilege -Language CSharp
        $result = [LargePagePrivilege]::GetLargePageMinimum()
        if ($result) {
            Write-Status "Large page support detected" "Green"
            return $true
        }
    } catch {
        Write-Status "Large page support not available (normal for consumer Windows)" "Yellow"
    }
    return $false
}

function Set-ProcessPriority() {
    $process = Get-Process -Id $PID
    $process.PriorityClass = "High"
    Write-Status "Process priority set to HIGH" "Green"
}

function Invoke-ModelLoad() {
    param($ModelPath, $ConfigPath, $ContextLength)
    
    Write-Status "Loading model with mapped-window fallback..." "Cyan"
    Write-Status "Model: $ModelPath" "Gray"
    Write-Status "Config: $ConfigPath" "Gray"
    Write-Status "Context: $ContextLength tokens" "Gray"
    
    # Set environment variables for the loader
    $env:RAWRXD_MODEL_PATH = $ModelPath
    $env:RAWRXD_CONFIG_PATH = $ConfigPath
    $env:RAWRXD_CONTEXT_LENGTH = $ContextLength
    $env:RAWRXD_ENABLE_MMAP_FALLBACK = "1"
    $env:RAWRXD_SLIDING_WINDOW_SIZE = "2147483648"  # 2GB
    $env:RAWRXD_PREFETCH_AHEAD = "3"
    $env:RAWRXD_MAX_RESIDENT_LAYERS = "8"
    $env:RAWRXD_KV_CACHE_SIZE = "8589934592"  # 8GB
    $env:RAWRXD_LOG_MEMORY = if ($Verbose) { "1" } else { "0" }
    
    # Check for headless minimal mode (reduces overhead)
    $env:RAWRXD_HEADLESS_MINIMAL = "1"
    
    # Find the loader executable
    $loaderPath = "..\build\bin\RawrXD-Headless.exe"
    if (-not (Test-Path $loaderPath)) {
        $loaderPath = "..\build\bin\RawrXD-Win32IDE.exe"
    }
    if (-not (Test-Path $loaderPath)) {
        throw "RawrXD executable not found. Please build first."
    }
    
    Write-Status "Using loader: $loaderPath" "Gray"
    
    # Build arguments
    $args = @(
        "--model", $ModelPath,
        "--config", $ConfigPath,
        "--context", $ContextLength,
        "--mmap-fallback",
        "--sliding-window", "2gb",
        "--prefetch-ahead", "3",
        "--max-resident", "8"
    )
    
    if ($Benchmark) {
        $args += "--benchmark"
    }
    
    if ($Verbose) {
        $args += "--verbose"
    }
    
    # Start the loader with memory monitoring
    $startTime = Get-Date
    
    try {
        $process = Start-Process -FilePath $loaderPath -ArgumentList $args -PassThru -NoNewWindow
        
        Write-Status "Loader PID: $($process.Id)" "Cyan"
        
        # Monitor memory usage
        while (-not $process.HasExited) {
            Start-Sleep -Seconds 2
            
            try {
                $procInfo = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
                if ($procInfo) {
                    $workingSetGB = [math]::Round($procInfo.WorkingSet64 / 1GB, 2)
                    $privateBytesGB = [math]::Round($procInfo.PrivateMemorySize64 / 1GB, 2)
                    
                    if ($Verbose) {
                        Write-Status "Memory: WS=$workingSetGB GB, Private=$privateBytesGB GB" "Gray"
                    }
                }
            } catch {
                # Process may have exited
            }
        }
        
        $exitCode = $process.ExitCode
        $duration = (Get-Date) - $startTime
        
        if ($exitCode -eq 0) {
            Write-Status "Model loaded successfully in $($duration.TotalSeconds.ToString('F1')) seconds" "Green"
        } else {
            throw "Loader exited with code $exitCode"
        }
        
    } catch {
        throw "Failed to load model: $_"
    }
}

function Invoke-Benchmark() {
    param($ModelPath, $ConfigPath)
    
    Write-Status "Running benchmark..." "Cyan"
    
    $benchmarkArgs = @(
        "--model", $ModelPath,
        "--config", $ConfigPath,
        "--benchmark",
        "--turns", "10",
        "--tokens-per-turn", "128"
    )
    
    $loaderPath = "..\build\bin\RawrXD-Headless.exe"
    if (-not (Test-Path $loaderPath)) {
        $loaderPath = "..\build\bin\RawrXD-Win32IDE.exe"
    }
    
    & $loaderPath @benchmarkArgs 2>&1 | ForEach-Object {
        if ($_ -match "TPS:|tokens/sec|throughput") {
            Write-Status $_ "Green"
        } elseif ($_ -match "error|fail|exception") {
            Write-Status $_ "Red"
        } else {
            Write-Host $_
        }
    }
}

# ============================================================================
# Main Execution
# ============================================================================

Write-Status "BigDaddyG 70B+ Model Loader" "Cyan"
Write-Status "==========================" "Cyan"

# Verify model file
$modelSizeGB = Test-ModelFile $ModelPath

# Check memory (need ~48GB available for 70B model + overhead)
$requiredGB = 48
$availableGB = Test-MemoryAvailability $requiredGB

# Enable large page support if available
$largePages = Enable-LargePageSupport

# Set process priority
Set-ProcessPriority

# Load the model
Invoke-ModelLoad -ModelPath $ModelPath -ConfigPath $ConfigPath -ContextLength $ContextLength

# Run benchmark if requested
if ($Benchmark) {
    Invoke-Benchmark -ModelPath $ModelPath -ConfigPath $ConfigPath
}

Write-Status "Complete!" "Green"
