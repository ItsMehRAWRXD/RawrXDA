# Windows Build Setup Script for RawrXD Model Loader
# Requires: CMake 3.20+, Visual Studio 2022, Clang, Vulkan SDK 1.3+

param(
    [string]$Configuration = "Release",
    [switch]$CleanBuild,
    [switch]$SkipShaderCompile,
    [switch]$UseClang = $true,  # Use Clang by default (faster compilation)
    [switch]$AutoInitMsvc       # Attempt automatic MSVC/SDK environment initialization
    , [string]$QtDir            # Optional explicit Qt6_DIR (e.g. C:\Qt\6.7.2\msvc2019_64\lib\cmake\Qt6)
)

$ErrorActionPreference = "Stop"

Write-Host "RawrXD Model Loader - Build Script" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Green
Write-Host "Compiler: $(if ($UseClang) { 'Clang' } else { 'MSVC' })" -ForegroundColor Green

# Check prerequisites
Write-Host "`nChecking prerequisites..." -ForegroundColor Yellow

# --- Environment Preflight: MSVC + Windows SDK ---
function Initialize-MsvcEnvironment {
    param(
        [switch]$Force
    )
    $alreadyHave = (Get-Command cl.exe -ErrorAction SilentlyContinue) -and $env:WindowsSdkDir -and $env:VCToolsInstallDir
    if ($alreadyHave -and -not $Force) {
        Write-Host "✓ MSVC environment already initialized" -ForegroundColor Green
        return $true
    }
    Write-Host "Attempting MSVC environment initialization (vcvars64.bat)..." -ForegroundColor Yellow
    $vcvarsCandidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
        'C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat'
    )
    $vcvarsPath = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $vcvarsPath) {
        Write-Host "✗ Could not locate vcvars64.bat in standard VS2022 installs" -ForegroundColor Red
        return $false
    }
    try {
        $envDump = cmd /c '"' + $vcvarsPath + '" & set'
        $envDump | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)$') {
                $name = $matches[1]; $val = $matches[2]
                Set-Item -Path Env:$name -Value $val
            }
        }
        if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
            Write-Host "✓ vcvars64 applied (cl available)" -ForegroundColor Green
            return $true
        } else {
            Write-Host "✗ cl still not found after vcvars64" -ForegroundColor Red
            return $false
        }
    } catch {
        Write-Host "✗ Failed invoking vcvars64.bat: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

if ($AutoInitMsvc) {
    Initialize-MsvcEnvironment | Out-Null
} else {
    if (-not ((Get-Command cl.exe -ErrorAction SilentlyContinue) -and $env:WindowsSdkDir)) {
        Write-Host "⚠ MSVC/SDK environment not initialized. Use -AutoInitMsvc or run Developer PowerShell for VS 2022." -ForegroundColor Yellow
        Write-Host "  Missing: cl.exe or WindowsSdkDir; builds may fail linking system libs." -ForegroundColor Yellow
    } else {
        Write-Host "✓ Detected MSVC toolchain and Windows SDK vars" -ForegroundColor Green
    }
}

# Check CMake
try {
    $cmake_version = cmake --version | Select-Object -First 1
    Write-Host "✓ CMake found: $cmake_version" -ForegroundColor Green
} catch {
    Write-Host "✗ CMake not found. Please install CMake 3.20+" -ForegroundColor Red
    exit 1
}

# Check Vulkan Runtime (no SDK required)
Write-Host "✓ Vulkan will be linked against system libraries" -ForegroundColor Green

# Optionally check for glslc for shader compilation
$glslc_path = if ($env:VULKAN_SDK) { "$env:VULKAN_SDK\bin\glslc.exe" } else { $null }
if ($glslc_path -and (Test-Path $glslc_path)) {
    Write-Host "✓ glslc compiler found" -ForegroundColor Green
} else {
    Write-Host "⚠ glslc not found (optional - shader compilation skipped)" -ForegroundColor Yellow
    $SkipShaderCompile = $true
}

# Check Visual Studio
Write-Host "`nChecking Visual Studio..." -ForegroundColor Yellow
$vs_path = "C:\Program Files\Microsoft Visual Studio\2022"
if (Test-Path $vs_path) {
    Write-Host "✓ Visual Studio 2022 found" -ForegroundColor Green
} else {
    Write-Host "⚠ Visual Studio 2022 not found at default location" -ForegroundColor Yellow
    Write-Host "  Make sure it's installed or update the path" -ForegroundColor Yellow
}

# Setup directories
$script_dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$build_dir = "$script_dir\build"
$shaders_dir = "$script_dir\shaders"

Write-Host "`nProject directory: $script_dir" -ForegroundColor Green

# Clean build if requested
if ($CleanBuild -and (Test-Path $build_dir)) {
    Write-Host "`nCleaning previous build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $build_dir
}

# Create build directory
if (!(Test-Path $build_dir)) {
    New-Item -ItemType Directory -Path $build_dir | Out-Null
}

# Compile shaders
if (!$SkipShaderCompile) {
    Write-Host "`n=== Compiling Shaders ===" -ForegroundColor Cyan
    
    $shaders = Get-ChildItem -Path $shaders_dir -Filter "*.glsl" -ErrorAction SilentlyContinue
    
    if ($shaders) {
        foreach ($shader in $shaders) {
            $spv_output = Join-Path $shaders_dir "$($shader.BaseName).spv"
            Write-Host "Compiling: $($shader.Name)" -ForegroundColor Yellow
            
            & $glslc_path $shader.FullName -o $spv_output
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "  ✓ Compiled to: $($shader.BaseName).spv" -ForegroundColor Green
            } else {
                Write-Host "  ✗ Failed to compile: $($shader.Name)" -ForegroundColor Red
            }
        }
    } else {
        Write-Host "No shaders found in: $shaders_dir" -ForegroundColor Yellow
    }
}

# Configure CMake
Write-Host "`n=== Configuring CMake ===" -ForegroundColor Cyan
Push-Location $build_dir

# Set clang compiler paths
$clang_cl = "C:\Program Files\LLVM\bin\clang-cl.exe"
if (-not (Test-Path $clang_cl)) {
    Write-Host "⚠ Using VS2022 Enterprise Clang" -ForegroundColor Yellow
    $clang_cl = "C:\VS2022Enterprise\VC\Tools\Llvm\bin\clang-cl.exe"
}

# Use Ninja if available, else Visual Studio
$generator = "Visual Studio 17 2022"
$ninja_path = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninja_path) {
    $generator = "Ninja"
}

$cmake_args = @(
    "..",
    "-G", $generator,
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_CONFIGURATION_TYPES=$Configuration",
    "-DCMAKE_C_COMPILER=$clang_cl",
    "-DCMAKE_CXX_COMPILER=$clang_cl",
    "-DCMAKE_CXX_FLAGS=/O2"
)

if ($generator -eq "Visual Studio 17 2022") {
    $cmake_args += @(
        "-A", "x64"
    )
}

if ($QtDir) {
    if (Test-Path $QtDir) {
        Write-Host "Using explicit Qt6_DIR: $QtDir" -ForegroundColor Yellow
        $cmake_args += @("-DQt6_DIR=$QtDir")
    } else {
        Write-Host "⚠ Provided QtDir does not exist: $QtDir" -ForegroundColor Yellow
    }
}

Write-Host "Running CMake with Clang from: $clang_cl" -ForegroundColor Yellow
Write-Host "Generator: $generator" -ForegroundColor Yellow

cmake @cmake_args

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ CMake configuration failed" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host "✓ CMake configuration successful" -ForegroundColor Green

# Build project with VS generator
Write-Host "`n=== Building Project ===" -ForegroundColor Cyan

cmake --build . --config $Configuration --verbose

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Build failed" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host "✓ Build successful" -ForegroundColor Green

Pop-Location

# Output summary
Write-Host "`n=== Build Summary ===" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Green
Write-Host "Build directory: $build_dir" -ForegroundColor Green
Write-Host "Output directory: $build_dir\bin" -ForegroundColor Green

$exe_path = "$build_dir\bin\Release\RawrXD-ModelLoader.exe"
if (Test-Path $exe_path) {
    Write-Host "Executable: $exe_path" -ForegroundColor Green
    Write-Host "`n✓ Build complete! Ready to run." -ForegroundColor Green
    
    # Offer to run
    $response = Read-Host "`nWould you like to run the application now? (y/n)"
    if ($response -eq "y") {
        & $exe_path
    }
} else {
    Write-Host "Executable not found at expected location" -ForegroundColor Yellow
    Write-Host "Check the build output above for errors" -ForegroundColor Yellow
}

Write-Host "`nFor more information, see README.md" -ForegroundColor Cyan
