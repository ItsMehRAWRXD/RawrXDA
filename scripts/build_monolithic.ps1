# build_monolithic.ps1 — Assemble 7 MASM64 sources and link to RawrXD.exe
# Run from repo root: .\scripts\build_monolithic.ps1

param(
    [string]$AsmDir = (Join-Path $PSScriptRoot "..\src\asm\monolithic"),
    [string]$ObjDir = (Join-Path $PSScriptRoot "..\build\monolithic\obj"),
    [string]$OutExe = (Join-Path $PSScriptRoot "..\build\monolithic\RawrXD.exe")
)

$ErrorActionPreference = "Stop"
$Red = "`e[31m"
$Green = "`e[32m"
$Cyan = "`e[36m"
$Reset = "`e[0m"

Write-Host "${Cyan}RawrXD Monolithic Build (MASM64)${Reset}`n"

# Find ml64 and link (VS2022 or PATH)
function Find-ML64 {
    $vswherePaths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "C:\VS2022Enterprise\Installer\vswhere.exe"
    )
    $vspath = $null
    foreach ($vw in $vswherePaths) {
        if (Test-Path $vw) {
            $vspath = & $vw -latest -property installationPath 2>$null
            if ($vspath) { break }
        }
    }
    if (-not $vspath -and (Test-Path "C:\VS2022Enterprise\VC\Tools\MSVC")) { $vspath = "C:\VS2022Enterprise" }
    if ($vspath -and (Test-Path "$vspath\VC\Tools\MSVC")) {
        $ml64 = Get-ChildItem -Path "$vspath\VC\Tools\MSVC" -Recurse -Filter "ml64.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "Hostx64\\x64" } | Select-Object -First 1
        if ($ml64 -and (Test-Path $ml64.FullName)) {
            $link = Join-Path $ml64.DirectoryName "link.exe"
            return @{ ml64 = $ml64.FullName; link = $link }
        }
    }
    $m = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($m) { return @{ ml64 = $m.Source; link = (Get-Command link.exe -ErrorAction SilentlyContinue).Source } }
    throw "ml64.exe not found. Install VS2022 with C++ x64 tools or run from a Developer Command Prompt."
}

$tools = Find-ML64
$ml64 = $tools.ml64
$link = $tools.link
Write-Host "${Green}✓${Reset} ml64: $ml64`n"

# Resolve lib path for link (Windows Kits um\x64)
$libPath = $null
if ($env:LIB) { $libPath = ($env:LIB -split ';' | Where-Object { $_ -match 'um\\x64|Lib\\.*\\um' } | Select-Object -First 1) }
if (-not $libPath -and (Test-Path "C:\Program Files (x86)\Windows Kits\10\Lib")) {
    $latest = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($latest) {
        $um64 = Join-Path $latest.FullName "um\x64"
        if (Test-Path $um64) { $libPath = $um64 }
    }
}

if (-not (Test-Path $ObjDir)) { New-Item -ItemType Directory -Path $ObjDir -Force | Out-Null }

$asms = @("main", "inference", "ui", "beacon", "lsp", "agent", "model_loader")
foreach ($name in $asms) {
    $asmPath = Join-Path $AsmDir "$name.asm"
    $objPath = Join-Path $ObjDir "$name.obj"
    if (-not (Test-Path $asmPath)) { throw "Missing: $asmPath" }
    & $ml64 /c /nologo /Fo $objPath $asmPath
    if ($LASTEXITCODE -ne 0) { throw "Assemble failed: $name.asm" }
    Write-Host "  $name.asm -> $name.obj"
}

Write-Host ""
$linkArgs = @{ ObjDir = $ObjDir; OutExe = $OutExe; LinkExe = $link }
if ($libPath) { $linkArgs['LibPath'] = $libPath; Write-Host "${Green}✓${Reset} Lib: $libPath`n" }
& (Join-Path $PSScriptRoot "genesis_final_link.ps1") @linkArgs
Write-Host "${Green}Done.${Reset} $OutExe"
