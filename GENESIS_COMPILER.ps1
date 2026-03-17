# RAWRXD GENESIS v1.0 - Self-Hosting Build System
# Phase 1: ML64 assembles RawrXD_Genesis.asm -> genesis.exe
# Phase 2: genesis.exe writes minimal PE stub (self-compile)
# Phase 3: genesis.exe <file.asm> assembles external .asm (when parser is full)
# Phase 4: genesis.exe --link produces RawrXD.exe from .obj set

param([switch]$SelfCompile, [string]$Target = "RawrXD.exe", [string]$AsmFile = "")
$ErrorActionPreference = "Stop"
$rootDir = if ($PSScriptRoot) { $PSScriptRoot } else { "D:\rawrxd" }
$genesisDir = Join-Path $rootDir "genesis"
$buildDir = Join-Path $rootDir "build_prod"

Write-Host "GENESIS PROTOCOL INITIATED" -ForegroundColor Magenta
Write-Host "Target: Self-hosting MASM64 bootstrap" -ForegroundColor DarkGray
$genesisAsm = Join-Path $genesisDir "RawrXD_Genesis.asm"
if (-not (Test-Path $genesisAsm)) {
    Write-Host "ERROR: genesis\RawrXD_Genesis.asm not found." -ForegroundColor Red
    exit 1
}
New-Item -ItemType Directory -Force -Path $genesisDir | Out-Null
New-Item -ItemType Directory -Force -Path $buildDir -ErrorAction SilentlyContinue | Out-Null

# Locate ML64 / LINK (VS2022 or VS Build Tools)
$ml64 = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $ml64) {
    $ml64 = (Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
$link = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $link) {
    $link = (Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if (-not $ml64) {
    Write-Host "ERROR: ML64 not found. Install VS2022 (or Build Tools) with C++ x64 tools." -ForegroundColor Red
    Write-Host "  Or run from 'Developer Command Prompt for VS 2022' and ensure ml64 is in PATH." -ForegroundColor Gray
    exit 1
}

# Resolve lib paths so kernel32.lib can be found (VC lib + Windows SDK um\x64)
$vcLib = ""
if ($link) {
    $vcLib = [System.IO.Path]::GetFullPath((Join-Path (Split-Path (Split-Path (Split-Path $link))) "lib\x64"))
}
$sdkLib = ""
$sdkBase = "${env:ProgramFiles(x86)}\Windows Kits\10\Lib"
if (Test-Path $sdkBase) {
    $sdkVerDir = Get-ChildItem $sdkBase -Directory | Where-Object { $_.Name -match '^\d' } | Sort-Object Name -Descending | Select-Object -First 1
    if ($sdkVerDir) {
        $sdkLib = Join-Path $sdkVerDir.FullName "um\x64"
    }
}

Write-Host "[Phase 1] Bootstrapping Genesis with ML64..." -ForegroundColor Yellow
& $ml64 /c /Fo"$genesisDir\genesis.obj" /W3 /Zd "$genesisAsm"
if ($LASTEXITCODE -ne 0) { Write-Host "Assembly FAILED"; exit 1 }
if (-not $link) { Write-Host "LINK not found, skipping link"; exit 1 }

$linkArgs = @("/OUT:$genesisDir\genesis.exe", "$genesisDir\genesis.obj", "/SUBSYSTEM:CONSOLE", "/ENTRY:GenesisMain", "kernel32.lib")
if ($sdkLib -and (Test-Path $sdkLib)) { $linkArgs += "/LIBPATH:$sdkLib" }
if ($vcLib -and (Test-Path $vcLib))   { $linkArgs += "/LIBPATH:$vcLib" }

& $link $linkArgs
if ($LASTEXITCODE -ne 0) { Write-Host "Link FAILED"; exit 1 }
Write-Host "  genesis.exe built" -ForegroundColor Green

if ($SelfCompile) {
    Write-Host "[Phase 2] Genesis self-compilation..." -ForegroundColor Yellow
    Push-Location $genesisDir
    try { & ".\genesis.exe" } finally { Pop-Location }
    if (Test-Path (Join-Path $genesisDir "RawrXD_Genesis.exe")) {
        Write-Host "  Stub written: genesis\RawrXD_Genesis.exe" -ForegroundColor Green
    }
}

if ($AsmFile -ne "") {
    Write-Host "[Phase 3] Assembling external .asm..." -ForegroundColor Yellow
    $fullPath = if ([System.IO.Path]::IsPathRooted($AsmFile)) { $AsmFile } else { Join-Path $rootDir $AsmFile }
    if (Test-Path $fullPath) {
        Push-Location $genesisDir
        try { & ".\genesis.exe" $fullPath } finally { Pop-Location }
    } else {
        Write-Host "  File not found: $fullPath" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "GENESIS COMPLETE" -ForegroundColor Green
Write-Host "  Phase 1: ML64 -> genesis.exe" -ForegroundColor Gray
Write-Host "  -SelfCompile: genesis writes RawrXD_Genesis.exe stub" -ForegroundColor Gray
Write-Host "  -AsmFile <path>: genesis assembles file (Phase 3)" -ForegroundColor Gray
Write-Host "Run: .\GENESIS_COMPILER.ps1 -SelfCompile" -ForegroundColor White
