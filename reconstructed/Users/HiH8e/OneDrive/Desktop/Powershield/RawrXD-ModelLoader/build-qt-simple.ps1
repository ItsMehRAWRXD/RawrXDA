# Simple Qt build script for RawrXD-QtShell
param(
    [string]$Configuration = "Release",
    # Default QtDir updated to MSVC 2022 toolchain layout (adjust if custom install)
    [string]$QtDir = "C:\Qt\6.7.2\msvc2022_64\lib\cmake\Qt6",
    [string]$VsVcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    [string]$CMakeExe = "C:\Program Files\CMake\bin\cmake.exe"
)

$ErrorActionPreference = "Stop"

Write-Host "Building RawrXD Qt Shell..." -ForegroundColor Cyan
Write-Host "  Configuration: $Configuration" -ForegroundColor DarkGray
Write-Host "  Qt6 CMake dir: $QtDir" -ForegroundColor DarkGray

# Set up VS environment
$env:VSCMD_ARG_app_plat = "Desktop"
$env:VSCMD_ARG_TGT_ARCH = "x64"

# Initialize VS build tools environment
$vcvarsCmd = $VsVcVars
if (-not (Test-Path $vcvarsCmd)) { Write-Error "VS2022 vcvars64.bat not found: $vcvarsCmd" }

# Set environment for current session
cmd /c "`"$vcvarsCmd`" && set" | ForEach-Object {
    if ($_ -match "^([^=]+)=(.*)") {
        $varName = $matches[1]
        $varValue = $matches[2]
        [Environment]::SetEnvironmentVariable($varName, $varValue)
    }
}

# Find CMake
$cmake = $CMakeExe
if (-not (Test-Path $cmake)) { Write-Error "CMake not found: $cmake" }

# Clean and create build directory
$buildDir = "build-qt"
if (Test-Path $buildDir) {
    Remove-Item $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir | Out-Null
Set-Location $buildDir

try {
     # Export Qt6_DIR so top-level CMakeLists can pick it up
    $env:Qt6_DIR = $QtDir
    Write-Host "Configuring CMake..." -ForegroundColor Yellow
    & $cmake -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_SYSTEM_VERSION="10.0.22621.0" `
        -DQt6_DIR="$QtDir" `
        ..

    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed"
    }

    # Build Qt shell target only
    Write-Host "Building RawrXD-QtShell..." -ForegroundColor Yellow
    & $cmake --build . --config $Configuration --target RawrXD-QtShell

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Write-Host "✓ RawrXD-QtShell built successfully" -ForegroundColor Green
    $exePath = Join-Path -Path (Join-Path -Path "bin" -ChildPath $Configuration) -ChildPath "RawrXD-QtShell.exe"
    if (Test-Path $exePath) {
        Write-Host "Executable: $exePath" -ForegroundColor Cyan
    } else {
        Write-Host "Executable not found at expected path: $exePath" -ForegroundColor Yellow
    }

} catch {
    Write-Error $_.Exception.Message
} finally {
    Set-Location ..
}