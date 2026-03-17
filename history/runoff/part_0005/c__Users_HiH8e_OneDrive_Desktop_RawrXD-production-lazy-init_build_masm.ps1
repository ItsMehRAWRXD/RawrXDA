# RawrXD MASM IDE Build Script for Windows
# Builds the pure MASM + C++ Win32 IDE without Qt dependencies

param(
    [string]$BuildType = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$Clean = $false
)

$ProjectRoot = $PSScriptRoot
$BuildDir = "$ProjectRoot\build-masm"
$BinaryDir = "$BuildDir\bin"

Write-Host "=== RawrXD MASM IDE Build Script ===" -ForegroundColor Green
Write-Host "Project Root: $ProjectRoot"
Write-Host "Build Directory: $BuildDir"
Write-Host "Build Type: $BuildType"

# Clean build directory if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    Write-Host "Created build directory: $BuildDir" -ForegroundColor Green
}

# Configure CMake with MASM-specific configuration
Write-Host "Configuring MASM CMake project..." -ForegroundColor Cyan
Set-Location $BuildDir

$CmakeArgs = @(
    "-G", "$Generator",
    "-A", "x64"
)

$CmakeResult = cmake $CmakeArgs -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_PROJECT_NAME=RawrXDWin32MASM $ProjectRoot
if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build the project
Write-Host "Building project..." -ForegroundColor Cyan
$BuildResult = cmake --build . --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# Check if binary was created
$ExecutablePath = "$BinaryDir\RawrXDWin32MASM.exe"
if (Test-Path $ExecutablePath) {
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "Executable: $ExecutablePath" -ForegroundColor Yellow
    
    # Display file size
    $FileSize = (Get-Item $ExecutablePath).Length
    Write-Host "File size: $([math]::Round($FileSize/1MB, 2)) MB" -ForegroundColor Yellow
    
    # Test if executable runs
    Write-Host "Testing executable..." -ForegroundColor Cyan
    $TestResult = Start-Process -FilePath $ExecutablePath -ArgumentList "--version" -Wait -PassThru -NoNewWindow
    if ($TestResult.ExitCode -eq 0) {
        Write-Host "Executable test passed!" -ForegroundColor Green
    } else {
        Write-Host "Executable test failed (exit code: $($TestResult.ExitCode))" -ForegroundColor Yellow
    }
} else {
    Write-Host "Executable not found at expected path: $ExecutablePath" -ForegroundColor Red
    Write-Host "Available files in bin directory:" -ForegroundColor Yellow
    if (Test-Path $BinaryDir) {
        Get-ChildItem $BinaryDir | ForEach-Object { Write-Host "  $($_.Name)" }
    }
}

Write-Host "=== Build Complete ===" -ForegroundColor Green