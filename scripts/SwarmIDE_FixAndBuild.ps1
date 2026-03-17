#requires -Version 7.0
<#
.SYNOPSIS
    Swarm IDE Fix & Full Build — configure once, then build all ~1600 sources.

.DESCRIPTION
    Ensures CMake configure runs (fixes "rules.ninja missing"), then builds
    the full RawrXD tree including swarm (SwarmPanel, swarm_worker, swarm_coordinator,
    swarm_orchestrator, etc.). Use after pulling or when build dir is stale.

.PARAMETER Configure
    Force re-run CMake configure even if build dir exists.

.PARAMETER Clean
    Remove build dir before configure (full clean).

.PARAMETER Generator
    CMake generator: Ninja (default) or "Visual Studio 17 2022 -A x64".

.PARAMETER Verbose
    Show full CMake and build output.

.EXAMPLE
    .\scripts\SwarmIDE_FixAndBuild.ps1
    .\scripts\SwarmIDE_FixAndBuild.ps1 -Configure -Verbose
    .\scripts\SwarmIDE_FixAndBuild.ps1 -Clean
#>
param(
    [switch]$Configure,
    [switch]$Clean,
    [string]$Generator = "Ninja",
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Project root = repo root (parent of scripts/)
$root = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { "D:\rawrxd" }
if (-not (Test-Path $root)) { $root = "D:\rawrxd" }

$buildDir = Join-Path $root "build"
$cmakeList = Join-Path $root "CMakeLists.txt"

function Log { param([string]$m, [string]$c = 'White') Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $m" -ForegroundColor $c }

Log "╔═══════════════════════════════════════════════════════════════════════╗" Cyan
Log "║  RawrXD Swarm IDE — Fix & Full Build (~1600 files)                    ║" Cyan
Log "╚═══════════════════════════════════════════════════════════════════════╝" Cyan
Log "Root: $root" Gray
Log "Build: $buildDir" Gray
Log "Generator: $Generator" Gray

if (-not (Test-Path $cmakeList)) {
    Log "ERROR: CMakeLists.txt not found at $cmakeList" Red
    exit 1
}

# Clean if requested
if ($Clean -and (Test-Path $buildDir)) {
    Log "Cleaning build dir..." Yellow
    Remove-Item $buildDir -Recurse -Force
}

# Ensure build dir exists
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force | Out-Null }

# Decide if we need to configure
$needConfigure = $Configure
$ninjaRules = Join-Path $buildDir "CMakeFiles\rules.ninja"
$vsSln = Join-Path $buildDir "RawrXD.sln"
if (-not $needConfigure) {
    if ($Generator -eq "Ninja" -and -not (Test-Path $ninjaRules)) { $needConfigure = $true }
    if ($Generator -like "Visual Studio*" -and -not (Test-Path $vsSln)) { $needConfigure = $true }
}

Push-Location $buildDir
try {
    if ($needConfigure) {
        Log "Running CMake configure..." Cyan
        $cmakeArgs = @(
            "..",
            "-G", $Generator,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        )
        if ($Generator -like "Visual Studio*") {
            $cmakeArgs += "-A", "x64"
        }
        if ($Verbose) {
            & cmake @cmakeArgs
        } else {
            & cmake @cmakeArgs 2>&1 | ForEach-Object { Log $_ Gray }
        }
        if ($LASTEXITCODE -ne 0) {
            Log "CMake configure failed (exit $LASTEXITCODE). Fix errors above and re-run." Red
            exit $LASTEXITCODE
        }
        Log "Configure OK" Green
    } else {
        Log "Using existing configure (use -Configure to re-run)" Gray
    }

    Log "Building all targets..." Cyan
    if ($Generator -eq "Ninja") {
        if ($Verbose) { & ninja }
        else { & ninja 2>&1 | ForEach-Object { if ($_ -match "error|FAILED") { Log $_ Red } else { Write-Host $_ } } }
    } else {
        $msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        if (-not (Test-Path $msbuild)) {
            $msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
        }
        if (-not (Test-Path $msbuild)) {
            $msbuild = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
        }
        if (Test-Path $msbuild) {
            & $msbuild RawrXD.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
        } else {
            Log "MSBuild not found. Use Generator Ninja or open build\RawrXD.sln in Visual Studio." Yellow
            exit 1
        }
    }

    if ($LASTEXITCODE -ne 0) {
        Log "Build failed (exit $LASTEXITCODE). Check errors above." Red
        exit $LASTEXITCODE
    }

    # Report artifacts
    $ideExe = Get-ChildItem $buildDir -Filter "RawrXD*.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    $goldExe = Get-ChildItem $buildDir -Filter "RawrXD_Gold*.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    $win32Exe = Get-ChildItem $buildDir -Filter "RawrXD-Win32IDE*.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1

    Log "" White
    Log "╔═══════════════════════════════════════════════════════════════════════╗" Green
    Log "║  BUILD COMPLETE                                                       ║" Green
    Log "╚═══════════════════════════════════════════════════════════════════════╝" Green
    if ($ideExe) { Log "  IDE:    $($ideExe.FullName)" Green }
    if ($goldExe) { Log "  Gold:   $($goldExe.FullName)" Green }
    if ($win32Exe) { Log "  Win32:  $($win32Exe.FullName)" Green }
    Log "  Swarm:  Win32IDE_SwarmPanel, swarm_worker, swarm_coordinator, swarm_orchestrator (in linked binary)" Gray
    Log "  Next:   Run IDE from build\bin\ or open build\RawrXD.sln" Gray
} finally {
    Pop-Location
}
