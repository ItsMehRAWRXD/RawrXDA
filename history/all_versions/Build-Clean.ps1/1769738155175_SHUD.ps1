# Build-Clean.ps1
# Execute clean build of RawrXD after Qt removal
# Supports Release and Debug configurations

param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release"
)

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              RAWRXD CLEAN BUILD - $Config" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

$startTime = Get-Date
$logFile = "D:\RawrXD\build_clean\logs\build_$Config`_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

# Initialize MSVC environment
Write-Host "`n🔧 Initializing MSVC environment..." -ForegroundColor Yellow

$vsInstallPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
)

$vcvarsPath = $null
foreach ($vsPath in $vsInstallPaths) {
    $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $candidate) {
        $vcvarsPath = $candidate
        break
    }
}

if (-not $vcvarsPath) {
    Write-Host "❌ MSVC environment script not found!" -ForegroundColor Red
    exit 1
}

# Resolve absolute path to CMake to ensure it works after PATH reset
$cmakePath = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmakePath) { $cmakePath = "cmake" }

# Create a wrapper batch file that RESETS PATH to avoid "input line too long" error
$wrapperBat = Join-Path $env:TEMP "run_with_msvc.bat"
# Warning: Resetting PATH to minimal set to fix 'input line too long' issues with vcvars64.bat in bloated environments
$batContent = "@echo off`r`nset PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem`r`ncall `"$vcvarsPath`"`r`n%*"
Set-Content -Path $wrapperBat -Value $batContent

# Verify cl.exe is available (via wrapper)
Write-Host "  Checking compiler accessibility..." -ForegroundColor DarkGray
$checkOutput = & $wrapperBat cl.exe 2>&1
$clTest = $checkOutput | Select-String "Microsoft"

if (-not $clTest) {
    Write-Host "❌ cl.exe not found in PATH or failed to allow execution!" -ForegroundColor Red
    Write-Host "  Wrapper output:" -ForegroundColor Yellow
    $checkOutput | Select-Object -First 20 | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
    exit 1
}
Write-Host "  ✅ Compiler ready: $($clTest[0])" -ForegroundColor Green

# Create/clear build directory
$buildDir = "D:\RawrXD\build_clean\$Config"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

# Check if CMake is available
$cmakeVersion = cmake --version 2>&1 | Select-String "cmake version"
if (-not $cmakeVersion) {
    Write-Host "`n❌ CMake not found! Install CMake 3.20+" -ForegroundColor Red
    exit 1
}
Write-Host "  ✅ CMake ready: $($cmakeVersion[0])" -ForegroundColor Green

# Change to build directory
Push-Location $buildDir

# Run CMake
Write-Host "`n📝 Running CMake configuration..." -ForegroundColor Yellow

# Updated to point to the pure Win32 'Ship' directory which contains the Qt-free CMakeLists.txt
$cmakeArgs = @(
    "../../Ship"
    "-G", "NMake Makefiles"
    "-DCMAKE_BUILD_TYPE=$Config"
)

Write-Host "  Command: cmake $($cmakeArgs -join ' ')" -ForegroundColor DarkGray

# Using wrapper batch file for execution
$cmdArgs = $cmakeArgs -join ' '
# Pass args carefully. Alternatively, use argument list splatting if possible, but for bat file we pass string.
& $wrapperBat $cmakePath $cmakeArgs *>> $logFile
$cmakeExit = $LASTEXITCODE

if ($cmakeExit -ne 0) {
    Write-Host "`n❌ CMake configuration failed! (Exit code: $cmakeExit)" -ForegroundColor Red
    Write-Host "📄 Log file: $logFile" -ForegroundColor Yellow
    
    # Show last 20 lines of log
    if (Test-Path $logFile) {
        $errors = Get-Content $logFile -Tail 20
        Write-Host "`nLast 20 lines of log:" -ForegroundColor Red
        $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkRed }
    }
    
    Pop-Location
    exit 1
}

Write-Host "  ✅ Configuration successful" -ForegroundColor Green

# Build
Write-Host "`n🔨 Building project ($Config)..." -ForegroundColor Yellow

$buildArgs = @(
    "--build", "."
    "--config", $Config
    "--parallel"
)

Write-Host "  Command: cmake $($buildArgs -join ' ')" -ForegroundColor DarkGray

& $wrapperBat $cmakePath $buildArgs *>> $logFile
$buildExit = $LASTEXITCODE

if ($buildExit -ne 0) {
    Write-Host "`n❌ Build failed! (Exit code: $buildExit)" -ForegroundColor Red
    Write-Host "📄 Log file: $logFile" -ForegroundColor Yellow
    
    if (Test-Path $logFile) {
        $errors = Get-Content $logFile -Tail 20
        Write-Host "`nLast 20 lines of log:" -ForegroundColor Red
        $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkRed }
    }
    
    Pop-Location
    exit 1
}
$endTime = Get-Date
$duration = $endTime - $startTime

Pop-Location

# Analyze results
Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "BUILD RESULTS" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

if ($buildExit -eq 0) {
    Write-Host "Status: ✅ BUILD SUCCESSFUL" -ForegroundColor Green
    Write-Host "Duration: $($duration.ToString('mm\:ss'))" -ForegroundColor Gray
    Write-Host "Config: $Config" -ForegroundColor Gray
    Write-Host "Output: $buildDir" -ForegroundColor Gray
    
    # Check output binary
    $exePath = "$buildDir\Release\RawrXD_IDE.exe"
    if (-not (Test-Path $exePath)) {
        $exePath = "$buildDir\Debug\RawrXD_IDE.exe"
    }
    if (-not (Test-Path $exePath)) {
        $exePath = "$buildDir\RawrXD_IDE.exe"
    }
    
    if (Test-Path $exePath) {
        $exeSize = (Get-Item $exePath).Length / 1MB
        Write-Host "Executable: $(Split-Path $exePath -Leaf)" -ForegroundColor Gray
        Write-Host "Size: $([math]::Round($exeSize, 2)) MB" -ForegroundColor Gray
    }
    
    Write-Host "`n✅ Ready for verification!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "Status: ❌ BUILD FAILED" -ForegroundColor Red
    Write-Host "Duration: $($duration.ToString('mm\:ss'))" -ForegroundColor Gray
    Write-Host "Log: $logFile" -ForegroundColor Yellow
    
    # Parse errors
    $errorLines = Get-Content $logFile -ErrorAction SilentlyContinue | Select-String "error" | Select-Object -First 20
    if ($errorLines) {
        Write-Host "`nFirst 20 errors:" -ForegroundColor Red
        $errorLines | ForEach-Object { Write-Host "  $($_.Line)" -ForegroundColor DarkRed }
    }
    
    exit 1
}
