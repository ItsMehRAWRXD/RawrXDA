#!/usr/bin/env powershell

<#
================================================================================
Build-Phase1.ps1 - Phase 1 Foundation Build Script
================================================================================

This script compiles the Phase 1 Foundation layer (assembly + C++):
1. Compiles Phase1_Master.asm using ml64.exe
2. Compiles Phase1_Foundation.cpp using cl.exe
3. Links into Phase1_Foundation.lib static library

USAGE:
    .\Build-Phase1.ps1 [-Release] [-Clean] [-Verbose]

OPTIONS:
    -Release    Build Release configuration (default: Debug)
    -Clean      Clean build (remove old .obj and .lib files)
    -Verbose    Show detailed compiler output
    -Help       Show this help message

REQUIREMENTS:
    - Visual Studio 2022 (C++ and MASM support)
    - ml64.exe (x64 MASM assembler) in PATH or VS installation
    - cl.exe (C++ compiler) in PATH or VS installation
    - lib.exe (static library tool) in PATH or VS installation

EXAMPLE:
    # Debug build
    .\Build-Phase1.ps1

    # Release build with verbose output
    .\Build-Phase1.ps1 -Release -Verbose

================================================================================
#>

[CmdletBinding()]
param(
    [switch]$Release,
    [switch]$Clean,
    [switch]$Verbose,
    [switch]$Help
)

function Show-Help {
    Get-Help -Full $MyInvocation.MyCommand.Path
    exit 0
}

if ($Help) { Show-Help }

# ============================================================================
# ENVIRONMENT SETUP
# ============================================================================

$ErrorActionPreference = "Stop"
$WarningPreference = if ($Verbose) { "Continue" } else { "SilentlyContinue" }

# Detect Visual Studio installation
function Find-VSTools {
    Write-Verbose "Searching for Visual Studio 2022 installation..."
    
    # Try vcvarsall.bat locations
    $vcvarsall = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    )
    
    foreach ($vc in $vcvarsall) {
        if (Test-Path $vc) {
            Write-Host "Found Visual Studio: $vc" -ForegroundColor Green
            return $vc
        }
    }
    
    Write-Error "Visual Studio 2022 not found"
    return $null
}

# Initialize Visual Studio environment
function Initialize-VSEnvironment {
    $vcvarsall = Find-VSTools
    if (-not $vcvarsall) {
        Write-Error "Cannot initialize Visual Studio environment"
        exit 1
    }
    
    Write-Verbose "Initializing Visual Studio environment..."
    
    # Call vcvarsall.bat to set up the environment
    & cmd /c "call `"$vcvarsall`" x64 && set" | ForEach-Object {
        if ($_ -match '=') {
            $name, $value = $_.split('=')
            [System.Environment]::SetEnvironmentVariable($name, $value)
        }
    }
}

# ============================================================================
# FILE PATHS
# ============================================================================

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = $scriptDir

$srcDir = Join-Path $projectRoot "src\foundation"
$includeDir = Join-Path $projectRoot "include"
$buildDir = Join-Path $projectRoot "build\phase1"
$libDir = Join-Path $projectRoot "lib"

$asmFile = Join-Path $srcDir "Phase1_Master.asm"
$cppFile = Join-Path $srcDir "Phase1_Foundation.cpp"

$asmObj = Join-Path $buildDir "Phase1_Master.obj"
$cppObj = Join-Path $buildDir "Phase1_Foundation.obj"
$libFile = Join-Path $libDir "Phase1_Foundation.lib"

# ============================================================================
# BUILD CONFIGURATION
# ============================================================================

$config = if ($Release) { "Release" } else { "Debug" }
$mlFlags = @(
    "/c"                # Compile only
    "/O2"               # Optimization level 2
    "/Zi"               # Debug information
    "/W3"               # Warning level 3
    "/nologo"           # Suppress logo
    "/D", "WIN64"       # Define WIN64
    "/D", "_WIN64"      # Define _WIN64
    "/DWIN32_LEAN_AND_MEAN"
)

$clFlags = @(
    "/c"                # Compile only
    "/O2"               # Optimization
    "/W4"               # Warning level 4
    "/WX"               # Warnings as errors
    "/Zi"               # Debug info
    "/fp:precise"       # Precise floating point
    "/arch:AVX2"        # AVX2 support
    "/std:c++17"        # C++17 standard
    "/nologo"           # Suppress logo
    "/I", $includeDir   # Include directory
    "/D", "WIN64"
    "/D", "_WIN64"
    "/D", "WIN32_LEAN_AND_MEAN"
    "/D", "_CRT_SECURE_NO_WARNINGS"
    "/D", "PHASE1_BUILDING_LIBRARY"
)

# ============================================================================
# BUILD FUNCTIONS
# ============================================================================

function Remove-BuildArtifacts {
    Write-Host "Cleaning old build artifacts..." -ForegroundColor Yellow
    
    if (Test-Path $buildDir) {
        Remove-Item -Path $buildDir -Recurse -Force | Out-Null
    }
    
    if (Test-Path $libFile) {
        Remove-Item -Path $libFile -Force | Out-Null
    }
}

function New-BuildDirectories {
    Write-Verbose "Creating build directories..."
    
    @($buildDir, $libDir) | ForEach-Object {
        if (-not (Test-Path $_)) {
            New-Item -Path $_ -ItemType Directory -Force | Out-Null
        }
    }
}

function Build-Assembly {
    Write-Host "Compiling Phase1_Master.asm..." -ForegroundColor Cyan
    
    if (-not (Test-Path $asmFile)) {
        Write-Error "Assembly file not found: $asmFile"
        exit 1
    }
    
    $mlCmd = @("ml64.exe") + $mlFlags + @("/Fo", $asmObj, $asmFile)
    
    Write-Verbose "ML64 Command: $($mlCmd -join ' ')"
    
    & $mlCmd[0] $mlCmd[1..($mlCmd.Count-1)]
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Assembly compilation failed with exit code: $LASTEXITCODE"
        exit 1
    }
    
    Write-Host "✓ Assembly compiled: $asmObj" -ForegroundColor Green
}

function Build-CppSource {
    Write-Host "Compiling Phase1_Foundation.cpp..." -ForegroundColor Cyan
    
    if (-not (Test-Path $cppFile)) {
        Write-Error "C++ file not found: $cppFile"
        exit 1
    }
    
    $clCmd = @("cl.exe") + $clFlags + @("/Fo", $cppObj, $cppFile)
    
    Write-Verbose "CL Command: $($clCmd -join ' ')"
    
    & $clCmd[0] $clCmd[1..($clCmd.Count-1)]
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "C++ compilation failed with exit code: $LASTEXITCODE"
        exit 1
    }
    
    Write-Host "✓ C++ compiled: $cppObj" -ForegroundColor Green
}

function Link-Library {
    Write-Host "Linking Phase1_Foundation.lib..." -ForegroundColor Cyan
    
    $libCmd = @(
        "lib.exe"
        "/OUT:$libFile"
        "/NOLOGO"
        $asmObj
        $cppObj
    )
    
    Write-Verbose "LIB Command: $($libCmd -join ' ')"
    
    & $libCmd[0] $libCmd[1..($libCmd.Count-1)]
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Library linking failed with exit code: $LASTEXITCODE"
        exit 1
    }
    
    Write-Host "✓ Library created: $libFile" -ForegroundColor Green
}

function Show-Statistics {
    if (Test-Path $libFile) {
        $size = (Get-Item $libFile).Length / 1KB
        Write-Host "Library size: $([math]::Round($size, 2)) KB" -ForegroundColor Cyan
    }
}

# ============================================================================
# MAIN BUILD PROCESS
# ============================================================================

function Invoke-Build {
    Write-Host "========================================" -ForegroundColor White
    Write-Host "Phase 1 Foundation Build Script" -ForegroundColor White
    Write-Host "========================================" -ForegroundColor White
    Write-Host "Configuration: $config" -ForegroundColor White
    Write-Host "========================================" -ForegroundColor White
    
    # Initialize Visual Studio environment
    Initialize-VSEnvironment
    
    # Clean if requested
    if ($Clean) {
        Remove-BuildArtifacts
    }
    
    # Create build directories
    New-BuildDirectories
    
    # Build assembly
    Build-Assembly
    
    # Build C++ source
    Build-CppSource
    
    # Link library
    Link-Library
    
    # Show statistics
    Show-Statistics
    
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "✓ Phase 1 Foundation build SUCCESSFUL" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Output: $libFile" -ForegroundColor Green
}

# ============================================================================
# ERROR HANDLING
# ============================================================================

try {
    Invoke-Build
}
catch {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "✗ Build FAILED" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
