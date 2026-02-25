# Build RawrXD Extension Host Hijacker (MASM64)
# Beacon-style injector that replaces VS Code/Cursor ExtensionHost with RawrXD native modules.
# Requires: VS 2022 (or Build Tools) x64, ML64 + LINK in PATH or under Program Files.
# Run as admin to inject into Code Helper / Cursor Helper.

param([switch]$Deploy)

$ErrorActionPreference = "Stop"
$rootDir = if ($PSScriptRoot) { $PSScriptRoot } else { "D:\rawrxd\extension-host" }
$outDir = Join-Path $rootDir "build"
$asmFile = Join-Path $rootDir "RawrXD_ExtensionHost_Hijacker.asm"
$objFile = Join-Path $outDir "RawrXD_ExtensionHost_Hijacker.obj"
$exeFile = Join-Path $outDir "RawrXD_ExtensionHost_Hijacker.exe"

Write-Host "RawrXD Extension Host Hijacker - MASM64 Build" -ForegroundColor Magenta
Write-Host "Target: Replace VS Code/Cursor ExtensionHost with native ASM" -ForegroundColor DarkGray

if (-not (Test-Path $asmFile)) {
    Write-Host "ERROR: $asmFile not found." -ForegroundColor Red
    exit 1
}
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Locate ML64 and LINK (VS 2022 or Build Tools)
$ml64 = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $ml64) {
    $ml64 = (Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
$link = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $link) {
    $link = (Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if (-not $ml64) {
    Write-Host "ERROR: ML64 not found. Install VS 2022 (or Build Tools) with C++ x64." -ForegroundColor Red
    Write-Host "  Or run from 'x64 Native Tools Command Prompt for VS 2022'." -ForegroundColor Gray
    exit 1
}

# Lib paths
$vcLib = ""
if ($link) {
    $vcDir = Split-Path (Split-Path (Split-Path $link))
    $vcLib = Join-Path $vcDir "lib\x64"
}
$sdkLib = ""
$sdkBase = "${env:ProgramFiles(x86)}\Windows Kits\10\Lib"
if (Test-Path $sdkBase) {
    $sdkVer = Get-ChildItem $sdkBase -Directory | Where-Object { $_.Name -match '^\d' } | Sort-Object Name -Descending | Select-Object -First 1
    if ($sdkVer) { $sdkLib = Join-Path $sdkVer.FullName "um\x64" }
}

Write-Host "[1/2] Assembling with ML64..." -ForegroundColor Cyan
& $ml64 /c /Fo"$objFile" /W3 /Zd "$asmFile"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Assembly FAILED." -ForegroundColor Red
    exit 1
}

if (-not $link) {
    Write-Host "LINK not found; .obj only." -ForegroundColor Yellow
    exit 0
}

Write-Host "[2/2] Linking..." -ForegroundColor Cyan
$linkArgs = @(
    "/OUT:$exeFile",
    "$objFile",
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:RawrXD_Entry",
    "kernel32.lib"
)
if ($sdkLib -and (Test-Path $sdkLib)) { $linkArgs += "/LIBPATH:$sdkLib" }
if ($vcLib -and (Test-Path $vcLib))   { $linkArgs += "/LIBPATH:$vcLib" }
& $link $linkArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "Link FAILED." -ForegroundColor Red
    exit 1
}

Write-Host "  Built: $exeFile" -ForegroundColor Green
Write-Host "  Run as Administrator to inject into Code Helper / Cursor Helper." -ForegroundColor DarkYellow

if ($Deploy) {
    $dest = "D:\rawrxd\build\RawrXD_ExtensionHost_Hijacker.exe"
    $buildRoot = "D:\rawrxd\build"
    if (-not (Test-Path $buildRoot)) { New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null }
    Copy-Item $exeFile $dest -Force
    Write-Host "  Deployed: $dest" -ForegroundColor Green
}
