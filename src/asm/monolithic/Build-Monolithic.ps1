#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build-Monolithic.ps1 — Assemble & link the RawrXD monolithic x64 kernel.
    Auto-detects cl.exe, ml64.exe, link.exe via vswhere + directory probing.

.DESCRIPTION
    Assembles every .asm in src\asm\monolithic\ with ml64.exe, then links into
    a single RawrXD_Monolithic.exe (SUBSYSTEM:WINDOWS, ENTRY:WinMainCRTStartup).

    Toolchain resolution order:
      1. vswhere.exe (finds any VS 2022/2026 install)
      2. Known custom paths (C:\VS2022Enterprise, D:\Microsoft Visual Studio 2022)
      3. Standard paths (Program Files, Program Files (x86))

    Windows SDK resolution:
      Scans C:\Program Files (x86)\Windows Kits\10\Include\*\um\windows.h,
      picks the newest version.

.PARAMETER Config
    debug   — /Zi /DEBUG  (default)
    release — /O2 /LTCG

.PARAMETER Clean
    Remove build artifacts before building.

.EXAMPLE
    .\Build-Monolithic.ps1
    .\Build-Monolithic.ps1 -Config release
    .\Build-Monolithic.ps1 -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet("debug","release")]
    [string]$Config = "debug",
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ============================================================================
# Paths
# ============================================================================
$ScriptDir = $PSScriptRoot
$RootDir   = Resolve-Path (Join-Path $ScriptDir "..\..\..") # D:\rawrxd
$ObjDir    = Join-Path $RootDir "build\monolithic\obj"
$BinDir    = Join-Path $RootDir "build\monolithic\bin"
$OutExe    = Join-Path $BinDir  "RawrXD_Monolithic.exe"

# ============================================================================
# Clean
# ============================================================================
if ($Clean) {
    Write-Host "Cleaning monolithic build..." -ForegroundColor Magenta
    if (Test-Path $ObjDir) { Remove-Item -Recurse -Force $ObjDir }
    if (Test-Path $BinDir) { Remove-Item -Recurse -Force $BinDir }
    Write-Host "Clean done." -ForegroundColor Green
    exit 0
}

# ============================================================================
# Locate MSVC Toolchain (vswhere → directory probing)
# ============================================================================
function Find-MSVC {
    # --- Try vswhere first ---
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
        if ($vsPath -and (Test-Path "$vsPath\VC\Tools\MSVC")) {
            $ver = Get-ChildItem -Directory "$vsPath\VC\Tools\MSVC" |
                   Sort-Object Name -Descending | Select-Object -First 1
            if ($ver) { return $ver.FullName }
        }
    }

    # --- Fallback: probe known directories ---
    $candidates = @(
        "C:\VS2022Enterprise\VC\Tools\MSVC",
        "D:\Microsoft Visual Studio 2022\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
    )
    foreach ($base in $candidates) {
        if (Test-Path $base) {
            $ver = Get-ChildItem -Directory $base |
                   Sort-Object Name -Descending | Select-Object -First 1
            if ($ver) { return $ver.FullName }
        }
    }
    throw "MSVC toolchain not found. Install Visual Studio 2022+ with C++ Desktop workload."
}

function Find-WindowsSDK {
    $sdkBase = "C:\Program Files (x86)\Windows Kits\10"
    if (-not (Test-Path "$sdkBase\Include")) {
        throw "Windows SDK not found at $sdkBase"
    }
    $ver = Get-ChildItem -Directory "$sdkBase\Include" |
           Where-Object { Test-Path "$($_.FullName)\um\windows.h" } |
           Sort-Object Name -Descending | Select-Object -First 1
    if (-not $ver) { throw "No valid Windows SDK version found." }
    return @{ Root = $sdkBase; Ver = $ver.Name }
}

# Resolve
$MSVCRoot = Find-MSVC
$SDK      = Find-WindowsSDK
$SDKRoot  = $SDK.Root
$SDKVer   = $SDK.Ver

$ML64 = Join-Path $MSVCRoot "bin\Hostx64\x64\ml64.exe"
$CL   = Join-Path $MSVCRoot "bin\Hostx64\x64\cl.exe"
$LINK = Join-Path $MSVCRoot "bin\Hostx64\x64\link.exe"

foreach ($tool in @($ML64, $CL, $LINK)) {
    if (-not (Test-Path $tool)) { throw "Missing: $tool" }
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " RawrXD Monolithic Kernel Builder"           -ForegroundColor White
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  MSVC:  $MSVCRoot"      -ForegroundColor Gray
Write-Host "  SDK:   $SDKRoot ($SDKVer)" -ForegroundColor Gray
Write-Host "  ml64:  $ML64"          -ForegroundColor Gray
Write-Host "  cl:    $CL"            -ForegroundColor Gray
Write-Host "  link:  $LINK"          -ForegroundColor Gray
Write-Host "  Config: $Config"       -ForegroundColor Gray
Write-Host "============================================" -ForegroundColor Cyan

# ============================================================================
# Environment (INCLUDE / LIB)
# ============================================================================
$env:INCLUDE = [string]::Join(";", @(
    "$MSVCRoot\include",
    "$SDKRoot\Include\10.0.22621.0\ucrt",
    "$SDKRoot\Include\$SDKVer\shared",
    "$SDKRoot\Include\$SDKVer\um"
))
$env:LIB = [string]::Join(";", @(
    "$MSVCRoot\lib\x64",
    "$MSVCRoot\lib\onecore\x64",
    "$SDKRoot\Lib\10.0.22621.0\ucrt\x64",
    "$SDKRoot\Lib\$SDKVer\um\x64"
))

# ============================================================================
# Build Flags
# ============================================================================
$MasmFlags = @("/nologo", "/W3", "/c", "/Cx")
if ($Config -eq "debug") {
    $MasmFlags += "/Zi"
    $LinkFlags  = @("/nologo", "/MACHINE:X64", "/SUBSYSTEM:WINDOWS",
                    "/ENTRY:WinMainCRTStartup", "/DEBUG",
                    "/DYNAMICBASE", "/NXCOMPAT", "/INCREMENTAL:NO")
} else {
    $MasmFlags += "/Zd"
    $LinkFlags  = @("/nologo", "/MACHINE:X64", "/SUBSYSTEM:WINDOWS",
                    "/ENTRY:WinMainCRTStartup", "/LTCG", "/OPT:REF", "/OPT:ICF",
                    "/DYNAMICBASE", "/NXCOMPAT", "/INCREMENTAL:NO")
}

$LinkLibs = @(
    "kernel32.lib", "user32.lib", "gdi32.lib", "advapi32.lib",
    "shell32.lib", "shlwapi.lib", "comctl32.lib", "comdlg32.lib",
    "ws2_32.lib", "wininet.lib", "ole32.lib"
)

# ============================================================================
# Helpers
# ============================================================================
function Invoke-Tool {
    param([string]$Exe, [string[]]$ToolArgs, [string]$Label)
    Write-Host "  [$Label]" -ForegroundColor Yellow
    $output = & $Exe @ToolArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host ($output -join "`n") -ForegroundColor Red
        throw "$Label failed (exit $LASTEXITCODE)"
    }
    $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
}

# ============================================================================
# Create output dirs
# ============================================================================
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

# ============================================================================
# Assemble all .asm files
# ============================================================================
Write-Host "`n=== Skipping C++ compile (Cached) ===
     Compiling: quantum_agent_orchestrator.cpp
     # & $CL /c /O2 /EHsc /std:c++17 /Fo$ObjDir\stubs.obj D:\rawrxd\src\agent\quantum_agent_orchestrator.cpp /I D:\rawrxd\src\agent

     Compiling: quantum_agent_orchestrator_thunks.cpp
     # & $CL /c /O2 /EHsc /std:c++17 /Fo$ObjDir\thunks.obj D:\rawrxd\src\agent\quantum_agent_orchestrator_thunks.cpp /I D:\rawrxd\src\agent

=== Assembling monolithic kernel ===" -ForegroundColor Cyan

$asmFiles = Get-ChildItem -Path $ScriptDir -Filter "*.asm" -File |
            Where-Object { $_.Name -notin @(
                "kv_pruning_test.asm",   # standalone test
                "pe_test_harness.asm",   # standalone PE test (duplicates WinMainCRTStartup)
                "stress_harness.asm",    # standalone stress test (duplicates g_hInstance/WinMainCRTStartup)
                "stream_bench.asm",      # standalone benchmark (needs all WebView2/ExtHost)
                "ollama_sovereign_proxy.asm"  # standalone proxy (has own main PROC)
            ) }

$objFiles = @()
foreach ($asm in $asmFiles) {
    $objName = [System.IO.Path]::ChangeExtension($asm.Name, ".obj")
    $objPath = Join-Path $ObjDir $objName
    $asmArgs = $MasmFlags + @("/Fo$objPath", $asm.FullName)
    Invoke-Tool $ML64 $asmArgs "ml64 $($asm.Name)"
    $objFiles += $objPath
}

# --- INJECTED LINKER ASSETS ---
$objFiles += Join-Path $ObjDir "thunks.obj"
# removed redundant stubs.obj

Write-Host "  Assembled $($objFiles.Count) modules." -ForegroundColor Green

# ============================================================================
# Link
# ============================================================================
Write-Host "`n=== Linking ===" -ForegroundColor Cyan
$linkArgs = $LinkFlags + @("/OUT:$OutExe") + $objFiles + $LinkLibs
Invoke-Tool $LINK $linkArgs "link RawrXD_Monolithic.exe"

# ============================================================================
# Summary
# ============================================================================
if (Test-Path $OutExe) {
    $sz = [math]::Round((Get-Item $OutExe).Length / 1KB, 1)
    Write-Host "`n============================================" -ForegroundColor Green
    Write-Host " BUILD SUCCESS: $OutExe ($sz KB)" -ForegroundColor White
    Write-Host "============================================" -ForegroundColor Green
} else {
    throw "Link completed but output not found: $OutExe"
}
