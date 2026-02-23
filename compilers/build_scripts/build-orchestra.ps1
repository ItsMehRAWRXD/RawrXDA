# Build script for Autonomous IDE with Orchestra CLI
# This script configures CMake and builds both GUI and CLI targets

param(
    [string]$BuildType = "Release",
    [int]$Parallel = 4,
    [switch]$Clean,
    [switch]$Help
)

if ($Help) {
    Write-Host @"
Autonomous IDE - Orchestra Build Script

Usage: .\build-orchestra.ps1 [OPTIONS]

Options:
  -BuildType    Release (default) or Debug
  -Parallel     Number of parallel build jobs (default: 4)
  -Clean        Clean build directory before building
  -Help         Show this help message

Examples:
  .\build-orchestra.ps1
  .\build-orchestra.ps1 -Clean -Parallel 8
  .\build-orchestra.ps1 -BuildType Debug
"@
    exit 0
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Autonomous IDE - Orchestra Build Script" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# Check if CMake is installed
try {
    $cmake = Get-Command cmake -ErrorAction Stop
    Write-Host "[✓] CMake found: $($cmake.Source)" -ForegroundColor Green
} catch {
    Write-Host "[✗] CMake not found. Please install CMake 3.20 or later." -ForegroundColor Red
    exit 1
}

# Create or clean build directory
if ($Clean -and (Test-Path "build")) {
    Write-Host "[1/4] Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item "build" -Recurse -Force
}

if (-not (Test-Path "build")) {
    Write-Host "[1/4] Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "build" | Out-Null
} else {
    Write-Host "[1/4] Build directory already exists" -ForegroundColor Yellow
}

# Configure with CMake
Write-Host "[2/4] Configuring CMake..." -ForegroundColor Yellow
Push-Location "build"
$configCmd = "cmake .. -DCMAKE_BUILD_TYPE=$BuildType"
Write-Host "  Running: $configCmd" -ForegroundColor Gray
Invoke-Expression $configCmd
if ($LASTEXITCODE -ne 0) {
    Write-Host "[✗] CMake configuration failed" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

# Build project
Write-Host "[3/4] Building project (with $Parallel parallel jobs)..." -ForegroundColor Yellow
$buildCmd = "cmake --build build --config $BuildType --parallel $Parallel"
Write-Host "  Running: $buildCmd" -ForegroundColor Gray
Invoke-Expression $buildCmd
if ($LASTEXITCODE -ne 0) {
    Write-Host "[✗] Build failed" -ForegroundColor Red
    exit 1
}

# Verify executables
Write-Host "[4/4] Verifying build artifacts..." -ForegroundColor Yellow
$guiExe = "build\Release\AutonomousIDE.exe"
$cliExe = "build\Release\orchestra-cli.exe"

$guiExists = Test-Path $guiExe
$cliExists = Test-Path $cliExe

if ($guiExists) {
    Write-Host "  [✓] GUI executable: $guiExe" -ForegroundColor Green
} else {
    Write-Host "  [✗] GUI executable not found: $guiExe" -ForegroundColor Red
}

if ($cliExists) {
    Write-Host "  [✓] CLI executable: $cliExe" -ForegroundColor Green
} else {
    Write-Host "  [✗] CLI executable not found: $cliExe" -ForegroundColor Red
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Build Complete!" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "Usage:" -ForegroundColor Cyan
Write-Host "  GUI (Graphical Interface):" -ForegroundColor Yellow
Write-Host "    $guiExe" -ForegroundColor Gray
Write-Host ""
Write-Host "  CLI (Interactive Mode):" -ForegroundColor Yellow
Write-Host "    $cliExe interactive" -ForegroundColor Gray
Write-Host ""
Write-Host "  CLI (Demo Mode):" -ForegroundColor Yellow
Write-Host "    $cliExe demo" -ForegroundColor Gray
Write-Host ""
Write-Host "  CLI (Batch Mode - Submit Task):" -ForegroundColor Yellow
Write-Host '    $cliExe batch submit-task task1 "Analyze code" main.cpp' -ForegroundColor Gray
Write-Host ""
Write-Host "  CLI (Batch Mode - Execute):" -ForegroundColor Yellow
Write-Host "    $cliExe batch execute 4" -ForegroundColor Gray
Write-Host ""
