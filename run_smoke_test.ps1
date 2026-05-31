# ============================================================================
# Smoke Test Runner - Slash Commands with Real Model Loaders
# ============================================================================

param(
    [string]$ModelPath = "",
    [string]$Workspace = "",
    [switch]$NoTitan,
    [switch]$NoStreaming,
    [switch]$Quiet,
    [switch]$Build,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

# Configuration
$RawrXDRoot = "d:\rawrxd"
$SrcDir = "$RawrXDRoot\src"
$BuildDir = "$RawrXDRoot\build\smoke_test"
$OutputDir = "$RawrXDRoot\bin\smoke_test"
$NinjaBuildDir = "$RawrXDRoot\build-ninja"

function Show-Help {
    Write-Host @"
RawrXD Slash Command Smoke Test Runner

Usage: .\run_smoke_test.ps1 [options]

Options:
    -ModelPath <path>    Path to GGUF model file
    -Workspace <path>    Test workspace directory
    -NoTitan            Disable Titan KV-Cache tests
    -NoStreaming        Use CPU inference instead of streaming
    -Quiet              Suppress verbose output
    -Build              Build only (don't run)
    -Help               Show this help message

Examples:
    .\run_smoke_test.ps1 -ModelPath d:\codestral22b.gguf
    .\run_smoke_test.ps1 -NoTitan -Quiet
    .\run_smoke_test.ps1 -Build

"@
}

function Find-Model {
    $modelPaths = @(
        "d:\codestral22b.gguf",
        "d:\ministral3.gguf",
        "d:\phi3mini.gguf",
        "f:\models\codestral22b.gguf",
        "g:\models\codestral22b.gguf"
    )
    
    foreach ($path in $modelPaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    return $null
}

function Build-SmokeTest {
    Write-Host "Building smoke test..." -ForegroundColor Cyan
    
    # Create directories
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    if (-not (Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    }
    
    # Find MSVC
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        if ($vsPath) {
            $vcvarsPath = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvarsPath) {
                Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Gray
            }
        }
    }
    
    # Try CMake build in the existing workspace build tree first.
    $cmakeLists = "$RawrXDRoot\CMakeLists.txt"
    if (Test-Path $cmakeLists) {
        Write-Host "Using CMake build..." -ForegroundColor Gray

        # Ensure build tree exists and is configured.
        if (-not (Test-Path $NinjaBuildDir)) {
            New-Item -ItemType Directory -Path $NinjaBuildDir -Force | Out-Null
        }

        Push-Location $NinjaBuildDir

        # Configure against top-level CMakeLists.txt.
        cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release $RawrXDRoot
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "CMake configuration failed" -ForegroundColor Red
            Pop-Location
            return $false
        }
        
        # Build
        cmake --build . --target slash_command_smoke_test --config Release -j 8
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "CMake build failed" -ForegroundColor Red
            Pop-Location
            return $false
        }
        
        # Copy built executable to expected output path used by runner.
        $builtExe = "$NinjaBuildDir\bin\slash_command_smoke_test.exe"
        if (Test-Path $builtExe) {
            Copy-Item -Path $builtExe -Destination "$OutputDir\slash_command_smoke_test.exe" -Force
        }

        Pop-Location
        return $true
    }
    
    # Fallback to direct compilation
    Write-Host "Using direct compilation..." -ForegroundColor Gray
    
    $cxxFlags = @(
        "/std:c++20",
        "/EHsc",
        "/W3",
        "/O2",
        "/DNDEBUG",
        "/DWIN32",
        "/D_WINDOWS",
        "/I`"$SrcDir`"",
        "/I`"$SrcDir\win32app`"",
        "/I`"$SrcDir\cli`"",
        "/I`"$SrcDir\ggml`"",
        "/I`"$SrcDir\inference`""
    )
    
    $sources = @(
        "$SrcDir\test\slash_command_smoke_test.cpp",
        "$SrcDir\cli\CLI_SlashRouter.cpp",
        "$SrcDir\gguf_loader.cpp",
        "$SrcDir\streaming_gguf_loader.cpp",
        "$SrcDir\cpu_inference_engine.cpp",
        "$SrcDir\win32app\Win32IDE_SlashRouter.cpp",
        "$SrcDir\win32app\Win32IDE_KVCacheCleanup.cpp"
    )
    
    $libs = @("user32.lib", "kernel32.lib")
    
    # Compile
    $clArgs = @("cl") + $cxxFlags + $sources + @("/c", "/Fo`"$BuildDir\smoke_test.obj`"")
    
    Push-Location $BuildDir
    
    & $clArgs[0] $clArgs[1..($clArgs.Length-1)]
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Compilation failed" -ForegroundColor Red
        Pop-Location
        return $false
    }
    
    # Link
    $linkArgs = @("link") + @("$BuildDir\smoke_test.obj") + $libs + @(
        "/Fe:`"$OutputDir\slash_command_smoke_test.exe`"",
        "/SUBSYSTEM:CONSOLE"
    )
    
    & $linkArgs[0] $linkArgs[1..($linkArgs.Length-1)]
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Linking failed" -ForegroundColor Red
        Pop-Location
        return $false
    }
    
    Pop-Location
    return $true
}

function Run-SmokeTest {
    param(
        [string]$Model,
        [string]$WorkDir,
        [bool]$UseTitan,
        [bool]$UseStreaming,
        [bool]$Verbose
    )
    
    $exePath = "$OutputDir\slash_command_smoke_test.exe"
    
    if (-not (Test-Path $exePath)) {
        Write-Host "Smoke test executable not found: $exePath" -ForegroundColor Red
        return $false
    }
    
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "  Running Smoke Test" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host ""
    
    $args = @()
    
    if ($Model) {
        $args += "--model", $Model
        Write-Host "Model: $Model" -ForegroundColor Gray
    }
    
    if ($WorkDir) {
        $args += "--workspace", $WorkDir
    }
    
    if (-not $UseTitan) {
        $args += "--no-titan"
    }
    
    if (-not $UseStreaming) {
        $args += "--no-streaming"
    }
    
    if (-not $Verbose) {
        $args += "--quiet"
    }
    
    # Run the test
    Push-Location $OutputDir
    
    & $exePath $args
    
    $exitCode = $LASTEXITCODE
    
    Pop-Location
    
    return $exitCode -eq 0
}

# Main
if ($Help) {
    Show-Help
    exit 0
}

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     RawrXD Slash Command Smoke Test - Real Model Loader     ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Find model if not specified
if (-not $ModelPath) {
    $ModelPath = Find-Model
    if ($ModelPath) {
        Write-Host "Auto-detected model: $ModelPath" -ForegroundColor Green
    }
}

# Build
$buildSuccess = Build-SmokeTest

if (-not $buildSuccess) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Build successful!" -ForegroundColor Green

if ($Build) {
    Write-Host "Build-only mode - skipping test execution" -ForegroundColor Yellow
    exit 0
}

# Run
$runSuccess = Run-SmokeTest `
    -Model $ModelPath `
    -WorkDir $Workspace `
    -UseTitan (-not $NoTitan) `
    -UseStreaming (-not $NoStreaming) `
    -Verbose (-not $Quiet)

if ($runSuccess) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host "  Smoke Test PASSED" -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Red
    Write-Host "  Smoke Test FAILED" -ForegroundColor Red
    Write-Host "============================================================" -ForegroundColor Red
    exit 1
}