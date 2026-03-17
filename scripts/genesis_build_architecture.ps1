# genesis_build_architecture.ps1
# Builds RawrXD canonical architecture: MASM64 kernel_suite -> .obj; optional C++ bridge.
# Usage: .\scripts\genesis_build_architecture.ps1 [-Config Release|Debug] [-OutDir path] [-KernelsOnly]

param(
    [string]$Config = "Release",
    [string]$OutDir = "",
    [switch]$KernelsOnly
)

$ErrorActionPreference = "Stop"
$rootDir = if ($PSScriptRoot) { Split-Path $PSScriptRoot } else { "D:\rawrxd" }
if (-not $OutDir) { $OutDir = Join-Path $rootDir "build_prod" }
$asmDir = Join-Path $rootDir "src\asm\kernel_suite"
$startTime = Get-Date

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Toolchain discovery
$ml64 = (Get-Command ml64.exe -ErrorAction SilentlyContinue).Source
if (-not $ml64) {
    $ml64 = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if (-not $ml64) {
    Write-Host "ERROR: ml64.exe not found." -ForegroundColor Red
    exit 1
}

Write-Host "RawrXD Architecture Builder" -ForegroundColor Cyan
Write-Host "  MASM64: $ml64"
Write-Host "  ASM:    $asmDir"
Write-Host "  Out:    $OutDir"

# MASM64 kernel_suite (only files that exist)
$asmFiles = @(
    "RawrXD_InferenceCore_SGEMM_AVX2",
    "RawrXD_FlashAttention_AVX512",
    "RawrXD_KQuant_Dequant"
)

$objs = @()
foreach ($name in $asmFiles) {
    $asmPath = Join-Path $asmDir "$name.asm"
    if (-not (Test-Path $asmPath)) {
        Write-Host "  SKIP: $name.asm not found" -ForegroundColor Yellow
        continue
    }
    $objPath = Join-Path $OutDir "$name.obj"
    Write-Host "  Assembling $name.asm ..." -ForegroundColor Gray
    & $ml64 /c /nologo /Fo"$objPath" /W3 /Zi /Zf "$asmPath"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  FAIL: $name.asm" -ForegroundColor Red
        exit 1
    }
    $objs += $objPath
}

Write-Host "  Kernel .obj count: $($objs.Count)" -ForegroundColor Green

if ($KernelsOnly) {
    $elapsed = (Get-Date) - $startTime
    Write-Host "Done (kernels only) in $([math]::Round($elapsed.TotalSeconds,1))s" -ForegroundColor Cyan
    exit 0
}

# Optional: link to DLL if link.exe available
$link = (Get-Command link.exe -ErrorAction SilentlyContinue).Source
if (-not $link) {
    $link = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if ($link -and $objs.Count -gt 0) {
    $dllPath = Join-Path $OutDir "RawrXD_Kernel.dll"
    $libPath = [System.IO.Path]::GetDirectoryName($link) + "\..\lib\x64"
    $sdkLib = "${env:ProgramFiles(x86)}\Windows Kits\10\Lib\*\um\x64"
    Write-Host "  Linking $dllPath ..." -ForegroundColor Gray
    & $link /DLL /NOLOGO /OUT:"$dllPath" $objs /MACHINE:X64 /LIBPATH:"$libPath" kernel32.lib
    if ($LASTEXITCODE -eq 0) {
        $sizeKB = [math]::Round((Get-Item $dllPath).Length / 1KB, 2)
        Write-Host "  Built: $dllPath ($sizeKB KB)" -ForegroundColor Green
    }
}

$elapsed = (Get-Date) - $startTime
Write-Host "Build complete in $([math]::Round($elapsed.TotalSeconds,1))s" -ForegroundColor Cyan
