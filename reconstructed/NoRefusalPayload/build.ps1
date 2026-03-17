#!/usr/bin/env pwsh
# ==============================================================================
# PowerShell Build Script for No-Refusal Payload Engine
# ==============================================================================

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    
    [switch]$Clean,
    [switch]$Run
)

$ProjectRoot = "D:\NoRefusalPayload"
$BuildDir = Join-Path $ProjectRoot "build"
$BinDir = Join-Path $BuildDir "bin"
$ExePath = Join-Path $BinDir "${Configuration}\NoRefusalEngine.exe"

function Write-Status {
    param([string]$Message)
    Write-Host "[*] $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[+] $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "[!] $Message" -ForegroundColor Red
}

Write-Host "========================================" -ForegroundColor Yellow
Write-Host "No-Refusal Payload Engine Build Script" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow
Write-Host ""

# Clean build if requested
if ($Clean) {
    Write-Status "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    Write-Status "Creating build directory..."
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Run CMake
Write-Status "Running CMake configuration ($Configuration)..."
Push-Location $BuildDir

$cmakeArgs = @(
    "-G", "`"Visual Studio 17 2022`"",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    $ProjectRoot
)

& cmake $cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
    Pop-Location
    exit 1
}

# Build project
Write-Status "Building project ($Configuration configuration)..."
& cmake --build . --config $Configuration --verbose

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    Pop-Location
    exit 1
}

Pop-Location

Write-Host ""
Write-Success "Build completed successfully!"
Write-Host ""

if (Test-Path $ExePath) {
    Write-Host "Output: $ExePath" -ForegroundColor Green
    
    if ($Run) {
        Write-Host ""
        Write-Status "Running executable..."
        & $ExePath
    }
} else {
    Write-Error "Executable not found at $ExePath"
}

Write-Host ""
