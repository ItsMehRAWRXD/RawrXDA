<#
.SYNOPSIS
    Build RawrXD pure x64 MASM modules (Unified Governor, Quantum Beaconism, Dual Engine, Codebase Audit).
    No C++ runtime dependency for these modules; Win32 API only.
.DESCRIPTION
    Assembles all .asm in src/x64/ with ml64 and links into RawrXD_X64.dll.
    Requires: Visual Studio 2022 (or Build Tools) with MASM (ml64) and x64 tools on PATH,
    or run from "x64 Native Tools Command Prompt for VS".
.EXAMPLE
    .\Build-X64MASM.ps1
    .\Build-X64MASM.ps1 -Clean
    .\Build-X64MASM.ps1 -OutputDir build_ide\x64
#>
[CmdletBinding()]
param(
    [Parameter()]
    [string]$OutputDir = "build_ide\x64",
    [Parameter()]
    [switch]$Clean,
    [Parameter()]
    [switch]$NoLink
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir
$AsmDir = Join-Path $RepoRoot "src\x64"
$ObjDir = Join-Path $RepoRoot $OutputDir
$DllName = "RawrXD_X64.dll"

function Find-Masm64 {
    $paths = @(
        "D:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
    )
    foreach ($p in $paths) {
        $resolved = Get-Item $p -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($resolved) { return $resolved.FullName }
    }
    $envPath = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($envPath) { return $envPath.Source }
    throw "ml64.exe not found. Open 'x64 Native Tools Command Prompt for VS' or install VS 2022 with C++ x64 tools."
}

function Find-Link64 {
    $paths = @(
        "D:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
    )
    foreach ($p in $paths) {
        $resolved = Get-Item $p -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($resolved) { return $resolved.FullName }
    }
    $envPath = Get-Command link.exe -ErrorAction SilentlyContinue
    if ($envPath) { return $envPath.Source }
    throw "link.exe (x64) not found."
}

function Find-LibDir {
    $sdk = "C:\Program Files (x86)\Windows Kits\10"
    if (-not (Test-Path $sdk)) { $sdk = "D:\Program Files (x86)\Windows Kits\10" }
    $um = Get-Item (Join-Path $sdk "Lib\*\um\x64") -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    $ucrt = Get-Item (Join-Path $sdk "Lib\*\ucrt\x64") -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    if ($um -and $ucrt) { return @($um.FullName, $ucrt.FullName) }
    return @("$sdk\Lib\10.0.22621.0\um\x64", "$sdk\Lib\10.0.22621.0\ucrt\x64")
}

if ($Clean) {
    if (Test-Path $ObjDir) {
        Remove-Item -Path $ObjDir -Recurse -Force
        Write-Host "Cleaned $ObjDir"
    }
    exit 0
}

if (-not (Test-Path $AsmDir)) {
    New-Item -ItemType Directory -Path $AsmDir -Force
    Write-Warning "Created empty src\x64; add .asm files and re-run."
    exit 0
}

$ml64 = Find-Masm64
$link = Find-Link64
$libDirs = Find-LibDir
$libPath = ($libDirs -join ";")

New-Item -ItemType Directory -Path $ObjDir -Force | Out-Null

$asmFiles = Get-ChildItem -Path $AsmDir -Filter "*.asm"
if ($asmFiles.Count -eq 0) {
    Write-Warning "No .asm files in $AsmDir"
    exit 0
}

$objs = @()
foreach ($f in $asmFiles) {
    $objPath = Join-Path $ObjDir ($f.BaseName + ".obj")
    Write-Host "Assembling $($f.Name) -> $objPath"
    & $ml64 /nologo /c /Zi /Fo $objPath $f.FullName
    if ($LASTEXITCODE -ne 0) { throw "ml64 failed for $($f.Name)" }
    $objs += $objPath
}

if (-not $NoLink -and $objs.Count -gt 0) {
    $dllPath = Join-Path $ObjDir $DllName
    Write-Host "Linking $dllPath"
    $env:LIB = $libPath + ";$env:LIB"
    & $link /nologo /DLL /NOENTRY "/OUT:$dllPath" $objs kernel32.lib user32.lib advapi32.lib PowrProf.lib
    if ($LASTEXITCODE -ne 0) { throw "link failed" }
    Write-Host "Built: $dllPath"
}

Write-Host "Build-X64MASM.ps1 completed."
