# RawrXD Unified MASM64 IDE Build Script (PowerShell)
# Assembles and links the unified offensive security toolkit

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Unified MASM64 IDE Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Find ml64.exe
$ml64Paths = @(
    "C:\VS2022Enterprise\SDK\ScopeCppSDK\vc15\VC\bin\ml64.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
)

$ml64 = $null
foreach ($path in $ml64Paths) {
    if (Test-Path $path) {
        $ml64 = $path
        break
    }
}

# If not found, search for it
if (-not $ml64) {
    Write-Host "Searching for ml64.exe..." -ForegroundColor Yellow
    $found = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio" -Recurse -Filter "ml64.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ml64 = $found.FullName
    }
}

if (-not $ml64) {
    $found = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "ml64.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ml64 = $found.FullName
    }
}

if (-not $ml64) {
    $found = Get-ChildItem "C:\VS2022Enterprise" -Recurse -Filter "ml64.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ml64 = $found.FullName
    }
}

if (-not $ml64) {
    Write-Host "ERROR: ml64.exe not found. Please install Visual Studio with C++ tools." -ForegroundColor Red
    Write-Host ""
    pause
    exit 1
}

Write-Host "Found ml64.exe: $ml64" -ForegroundColor Green

# Find link.exe (in same directory as ml64.exe)
$linker = Join-Path (Split-Path $ml64) "link.exe"
if (-not (Test-Path $linker)) {
    Write-Host "ERROR: link.exe not found in same directory as ml64.exe" -ForegroundColor Red
    exit 1
}

# Set up environment (lib paths)
$vcToolsPath = Split-Path (Split-Path (Split-Path $ml64))
$libPath = Join-Path $vcToolsPath "lib\x64"

# Find Windows SDK
$sdkPaths = @(
    "C:\Program Files (x86)\Windows Kits\10\Lib\*\um\x64",
    "C:\Program Files\Windows Kits\10\Lib\*\um\x64"
)

$sdkLib = $null
foreach ($pattern in $sdkPaths) {
    $found = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $sdkLib = $found.FullName
        break
    }
}

$libPaths = @($libPath)
if ($sdkLib) {
    $libPaths += $sdkLib
    Write-Host "Found Windows SDK: $sdkLib" -ForegroundColor Green
}

$env:LIB = $libPaths -join ";"

# Change to script directory
Set-Location $PSScriptRoot

# Assemble
Write-Host ""
Write-Host "[1/2] Assembling RawrXD_IDE_unified.asm..." -ForegroundColor Yellow
& $ml64 /c /Zi /nologo /Fo"RawrXD_IDE_unified.obj" "RawrXD_IDE_unified.asm"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Assembly failed." -ForegroundColor Red
    Write-Host ""
    pause
    exit 1
}

# Link
Write-Host "[2/2] Linking RawrXD_IDE_unified.obj..." -ForegroundColor Yellow
& $linker /SUBSYSTEM:CONSOLE /NOLOGO /ENTRY:_start_entry /OUT:"RawrXD_IDE_unified.exe" "RawrXD_IDE_unified.obj" kernel32.lib user32.lib advapi32.lib shell32.lib


if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Linking failed." -ForegroundColor Red
    Write-Host ""
    pause
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Build Complete!" -ForegroundColor Green
Write-Host "Output: RawrXD_IDE_unified.exe" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Usage:" -ForegroundColor Cyan
Write-Host "  GUI Mode: .\RawrXD_IDE_unified.exe" -ForegroundColor White
Write-Host "  CLI Mode: .\RawrXD_IDE_unified.exe -compile" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -encrypt" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -inject" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -uac" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -persist" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -sideload" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -avscan" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -entropy" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -stubgen" -ForegroundColor White
Write-Host "            .\RawrXD_IDE_unified.exe -trace" -ForegroundColor White
Write-Host ""
