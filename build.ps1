#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD IDE Build Script — Drives ml64.exe + cl.exe + link.exe
    No CMake required; finds MSVC toolchain automatically.

.DESCRIPTION
    Assembles all MASM x64 source files, compiles C++ wrapper, and links
    the final RawrXD_IDE.exe and RawrXD_Widget.exe without any external
    dependency beyond the Windows SDK and MSVC Build Tools.

.PARAMETER Target
    all       — build everything (default)
    ide       — build RawrXD_IDE.exe only
    widget    — build RawrXD_Widget.exe only
    clean     — remove all .obj and output binaries

.PARAMETER Config
    debug     — /DEBUG /Od /Zi (default for dev)
    release   — /O2 /GL /LTCG

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Target ide -Config release
    .\build.ps1 -Target clean
#>
[CmdletBinding()]
param(
    [ValidateSet("all","ide","widget","clean")]
    [string]$Target = "all",

    [ValidateSet("debug","release")]
    [string]$Config = "debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ============================================================================
# Paths
# ============================================================================
$Root      = $PSScriptRoot
$SrcDir    = Join-Path $Root "src"
$AgentDir  = Join-Path $SrcDir "agentic"
$ObjDir    = Join-Path $Root "build\obj"
$BinDir    = Join-Path $Root "build\bin"

# ============================================================================
# Locate MSVC Toolchain
# ============================================================================
function Find-Toolchain {
    $candidates = @(
        "D:\VS2022Enterprise\VC\Tools\MSVC",
        "C:\VS2022Enterprise\VC\Tools\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
    )
    foreach ($base in $candidates) {
        if (Test-Path $base) {
            $vers = Get-ChildItem -Directory $base | Sort-Object Name -Descending
            if ($vers.Count -gt 0) { return $vers[0].FullName }
        }
    }
    throw "MSVC toolchain not found. Install Visual Studio 2022 Build Tools."
}

function Find-WindowsSDK {
    $bases = @(
        "D:\Program Files (x86)\Windows Kits\10",
        "C:\Program Files (x86)\Windows Kits\10"
    )
    foreach ($base in $bases) {
        if (Test-Path "$base\Include") {
            $vers = Get-ChildItem -Directory "$base\Include" `
                    | Where-Object { Test-Path "$($_.FullName)\um\windows.h" } `
                    | Sort-Object Name -Descending
            if ($vers.Count -gt 0) { return @{ Root=$base; Ver=$vers[0].Name } }
        }
    }
    throw "Windows SDK not found."
}

$MSVCRoot = Find-Toolchain
$SDK      = Find-WindowsSDK
$SDKRoot  = $SDK.Root
$SDKVer   = $SDK.Ver

$ML64     = Join-Path $MSVCRoot "bin\Hostx64\x64\ml64.exe"
$CL       = Join-Path $MSVCRoot "bin\Hostx64\x64\cl.exe"
$LINK     = Join-Path $MSVCRoot "bin\Hostx64\x64\link.exe"

if (-not (Test-Path $ML64)) { throw "ml64.exe not found at: $ML64" }
if (-not (Test-Path $CL))   { throw "cl.exe not found at: $CL" }
if (-not (Test-Path $LINK)) { throw "link.exe not found at: $LINK" }

Write-Host "Toolchain: $MSVCRoot" -ForegroundColor Cyan
Write-Host "SDK:       $SDKRoot ($SDKVer)" -ForegroundColor Cyan
Write-Host "Target:    $Target  Config: $Config" -ForegroundColor Cyan

# ============================================================================
# Environment (INCLUDE / LIB)
# ============================================================================
$env:INCLUDE = [string]::Join(";", @(
    "$MSVCRoot\include",
    "$SDKRoot\Include\$SDKVer\ucrt",
    "$SDKRoot\Include\$SDKVer\shared",
    "$SDKRoot\Include\$SDKVer\um"
))
$env:LIB = [string]::Join(";", @(
    "$MSVCRoot\lib\x64",
    "$MSVCRoot\lib\onecore\x64",
    "$SDKRoot\Lib\$SDKVer\ucrt\x64",
    "$SDKRoot\Lib\$SDKVer\um\x64"
))

# ============================================================================
# Flags
# ============================================================================
$MasmFlags   = @("/nologo", "/W3", "/c", "/Cx", "/Zi", "/I$SrcDir", "/I$AgentDir")
$ClFlagsBase = @("/nologo", "/W4", "/WX", "/EHsc", "/MP",
                  "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
                  "/D_CRT_SECURE_NO_WARNINGS",
                  "/I$SrcDir")
$LinkLibs    = @("kernel32.lib","user32.lib","gdi32.lib","advapi32.lib",
                  "shell32.lib","shlwapi.lib","comctl32.lib","comdlg32.lib",
                  "wininet.lib","ws2_32.lib")

if ($Config -eq "release") {
    $ClFlagsBase += @("/O2", "/GL", "/DNDEBUG")
    $LinkFlagsExtra = @("/LTCG", "/OPT:REF", "/OPT:ICF")
} else {
    $ClFlagsBase += @("/Od", "/Zi", "/DDEBUG")
    $LinkFlagsExtra = @("/DEBUG")
}

$LinkFlagsBase = @("/nologo", "/MACHINE:X64", "/WX", "/INCREMENTAL:NO",
                    "/DYNAMICBASE", "/NXCOMPAT") + $LinkFlagsExtra

# ============================================================================
# Helpers
# ============================================================================
function Invoke-Exe {
    param([string]$Exe, [string[]]$Args, [string]$Desc)
    Write-Host "  [$Desc]" -ForegroundColor Yellow
    & $Exe @Args
    if ($LASTEXITCODE -ne 0) {
        throw "$Desc failed with exit code $LASTEXITCODE"
    }
}

function Assemble {
    param([string]$Source, [string]$ObjName)
    $obj = Join-Path $ObjDir $ObjName
    $args = $MasmFlags + @("/Fo$obj", $Source)
    Invoke-Exe $ML64 $args "ASM $([System.IO.Path]::GetFileName($Source))"
    return $obj
}

function Compile {
    param([string]$Source, [string]$ObjName)
    $obj = Join-Path $ObjDir $ObjName
    $args = $ClFlagsBase + @("/c", "/Fo$obj", $Source)
    Invoke-Exe $CL $args "CL  $([System.IO.Path]::GetFileName($Source))"
    return $obj
}

function Link-Exe {
    param([string]$OutExe, [string[]]$Objs, [string[]]$ExtraFlags)
    $args = $LinkFlagsBase + $ExtraFlags + @("/OUT:$OutExe") + $Objs + $LinkLibs
    Invoke-Exe $LINK $args "LINK $([System.IO.Path]::GetFileName($OutExe))"
}

# ============================================================================
# Clean
# ============================================================================
if ($Target -eq "clean") {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Magenta
    if (Test-Path $ObjDir) { Remove-Item -Recurse -Force $ObjDir }
    if (Test-Path $BinDir) { Remove-Item -Recurse -Force $BinDir }
    Write-Host "Clean complete." -ForegroundColor Green
    exit 0
}

# ============================================================================
# Create output dirs
# ============================================================================
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

# ============================================================================
# Build IPC / Widget core objects (shared by both targets)
# ============================================================================
Write-Host "`n=== Core Objects ===" -ForegroundColor Cyan
$ObjIPC     = Assemble (Join-Path $SrcDir "RawrXD_IPC_Bridge.asm")      "IPC_Bridge.obj"
$ObjWidget  = Assemble (Join-Path $SrcDir "RawrXD_WidgetEngine.asm")    "WidgetEngine.obj"
$ObjHW      = Assemble (Join-Path $SrcDir "RawrXD_HeadlessWidgets.asm") "HeadlessWidgets.obj"

$CoreObjs = @($ObjIPC, $ObjWidget, $ObjHW)

# ============================================================================
# Build GPU/DMA Engine objects (large files — may take several minutes)
# ============================================================================
Write-Host "`n=== Engine Objects ===" -ForegroundColor Cyan
$ObjGPU     = Assemble (Join-Path $AgentDir "gpu_dma_complete_final.asm") "gpu_dma.obj"
$ObjTitan   = Assemble (Join-Path $AgentDir "RawrXD_Titan_Master_GodSource_REVERSE_ENGINEERED.asm") "titan_master.obj"

$EngineObjs = @($ObjGPU, $ObjTitan)

# ============================================================================
# Build IDE Shell
# ============================================================================
if ($Target -in @("all","ide")) {
    Write-Host "`n=== IDE Shell ===" -ForegroundColor Cyan
    $ObjIDEShell  = Assemble (Join-Path $SrcDir "RawrXD_IDE_Shell.asm")    "IDE_Shell.obj"
    $ObjIDEWrap   = Compile  (Join-Path $SrcDir "RawrXD_IDE_Wrapper.cpp")  "IDE_Wrapper.obj"

    $IDEObjs = $CoreObjs + $EngineObjs + @($ObjIDEShell, $ObjIDEWrap)
    $IDEOut  = Join-Path $BinDir "RawrXD_IDE.exe"
    Link-Exe $IDEOut $IDEObjs @("/SUBSYSTEM:WINDOWS", "/ENTRY:IDEShellMain")
    Write-Host "  Built: $IDEOut" -ForegroundColor Green
}

# ============================================================================
# Build Widget Server
# ============================================================================
if ($Target -in @("all","widget")) {
    Write-Host "`n=== Widget Server ===" -ForegroundColor Cyan
    $WidgetOut = Join-Path $BinDir "RawrXD_Widget.exe"
    Link-Exe $WidgetOut $CoreObjs @("/SUBSYSTEM:CONSOLE", "/ENTRY:Widget_Main")
    Write-Host "  Built: $WidgetOut" -ForegroundColor Green
}

# ============================================================================
# Summary
# ============================================================================
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " RawrXD Build Complete ($Config)" -ForegroundColor Green
Get-ChildItem $BinDir -Filter "*.exe" | ForEach-Object {
    $sz = [math]::Round($_.Length / 1KB, 1)
    Write-Host "  $($_.Name)  $sz KB" -ForegroundColor White
}
Write-Host "========================================`n" -ForegroundColor Cyan
