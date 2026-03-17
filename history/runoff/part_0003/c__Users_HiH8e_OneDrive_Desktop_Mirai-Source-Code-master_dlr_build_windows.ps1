# PowerShell build script for DLR component with advanced options
# Usage: .\build_windows.ps1 [-BuildType Debug|Release] [-Architecture x86|x64] [-StaticLinking] [-XPSupport] [-Clean]

param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Release",
    
    [ValidateSet("x86", "x64")]
    [string]$Architecture = "x64",
    
    [switch]$StaticLinking,
    [switch]$XPSupport,
    [switch]$Clean,
    [switch]$Help
)

function Show-Help {
    Write-Host "DLR Windows Build Script" -ForegroundColor Green
    Write-Host "Usage: .\build_windows.ps1 [OPTIONS]" -ForegroundColor White
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -BuildType      Debug or Release (default: Release)"
    Write-Host "  -Architecture   x86 or x64 (default: x64)"
    Write-Host "  -StaticLinking  Enable static linking"
    Write-Host "  -XPSupport      Enable Windows XP compatibility"
    Write-Host "  -Clean          Clean previous builds before building"
    Write-Host "  -Help           Show this help message"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Cyan
    Write-Host "  .\build_windows.ps1"
    Write-Host "  .\build_windows.ps1 -BuildType Debug -Architecture x86"
    Write-Host "  .\build_windows.ps1 -StaticLinking -XPSupport"
}

if ($Help) {
    Show-Help
    exit 0
}

# Check for CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Error "CMake not found. Please install CMake and add it to PATH."
    exit 1
}

Write-Host "Building DLR for Windows $Architecture - $BuildType" -ForegroundColor Green

# Clean if requested
if ($Clean -and (Test-Path "build")) {
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location "build"

try {
    # Prepare CMake arguments
    $cmakeArgs = @("..")
    
    # Architecture
    if ($Architecture -eq "x86") {
        $cmakeArgs += "-A", "Win32"
    } else {
        $cmakeArgs += "-A", "x64"
    }
    
    # Build type
    $cmakeArgs += "-DCMAKE_BUILD_TYPE=$BuildType"
    
    # Optional features
    if ($StaticLinking) {
        $cmakeArgs += "-DENABLE_STATIC_LINKING=ON"
    }
    
    if ($XPSupport) {
        $cmakeArgs += "-DENABLE_XP_SUPPORT=ON"
    }
    
    # Configure
    Write-Host "Configuring CMake..." -ForegroundColor Cyan
    Write-Host "Command: cmake $($cmakeArgs -join ' ')" -ForegroundColor DarkGray
    
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    
    # Build
    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build . --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    
    Set-Location ".."
    
    # Check for output
    $binaryPath = "build\$BuildType\dlr_win.exe"
    if (Test-Path $binaryPath) {
        Write-Host "Build successful: $binaryPath" -ForegroundColor Green
        
        # Create release directory
        if (-not (Test-Path "release")) {
            New-Item -ItemType Directory -Path "release" | Out-Null
        }
        
        # Copy binary
        $outputName = "dlr.$Architecture.exe"
        Copy-Item $binaryPath "release\$outputName"
        Write-Host "Binary copied to release\$outputName" -ForegroundColor Green
        
        # Get file info
        $fileInfo = Get-Item "release\$outputName"
        $sizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
        
        Write-Host ""
        Write-Host "Build Summary:" -ForegroundColor Yellow
        Write-Host "  Target: $outputName"
        Write-Host "  Size: $sizeKB KB"
        Write-Host "  Build Type: $BuildType"
        Write-Host "  Architecture: $Architecture"
        Write-Host "  Static Linking: $StaticLinking"
        Write-Host "  XP Support: $XPSupport"
        
    } else {
        throw "Build failed - binary not found at $binaryPath"
    }
    
    Write-Host ""
    Write-Host "DLR Windows build completed successfully!" -ForegroundColor Green
    
} catch {
    Set-Location ".."
    Write-Error "Build failed: $_"
    exit 1
}