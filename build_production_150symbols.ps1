$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Write-Host "`n========================================================================"
Write-Host "RawrXD Symbol Batch Resolver - Full Production Build"
Write-Host "150 symbols across 10 batches - Zero stubs, direct resolution"
Write-Host "========================================================================`n"

# Locate Visual Studio
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallDir = $null
if (Test-Path $vswhere) {
    $vsInstallDir = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null | Select-Object -First 1
}
if (-not $vsInstallDir) {
    $knownPaths = @(
        'D:\VS2022Enterprise',
        'C:\VS2022Enterprise', 
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise',
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
    )
    foreach ($p in $knownPaths) {
        if (Test-Path "$p\VC\Auxiliary\Build\vcvars64.bat") {
            $vsInstallDir = $p; break
        }
    }
}

if (-not $vsInstallDir) {
    Write-Error 'Visual Studio 2022 x64 tools not found.'
    exit 1
}

# Find tools
$MasmExe = Get-ChildItem "$vsInstallDir\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" | Select-Object -First 1 -ExpandProperty FullName
$LinkExe = Get-ChildItem "$vsInstallDir\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" | Select-Object -First 1 -ExpandProperty FullName

if (-not (Test-Path $MasmExe)) {
    Write-Error "ml64.exe not found"
    exit 1
}

# Get library paths
$masmDir = Split-Path $MasmExe -Parent
$msvcRoot = (Resolve-Path (Join-Path $masmDir "..\..\..")).Path
$msvcLibX64 = Join-Path $msvcRoot "lib\x64"

$sdkRoot = if (Test-Path "C:\Program Files (x86)\Windows Kits\10\Lib") {
    "C:\Program Files (x86)\Windows Kits\10\Lib"
} elseif (Test-Path "D:\Program Files (x86)\Windows Kits\10\Lib") {
    "D:\Program Files (x86)\Windows Kits\10\Lib"
} else { $null }

$sdkUmX64 = $null
$sdkUcrtX64 = $null
if ($sdkRoot) {
    $versions = Get-ChildItem $sdkRoot -Directory | Where-Object { $_.Name -match '^\d+\.' } | Sort-Object Name -Descending
    foreach ($v in $versions) {
        $um = Join-Path $v.FullName "um\x64"
        $ucrt = Join-Path $v.FullName "ucrt\x64"
        if ((Test-Path $um) -and (Test-Path $ucrt)) {
            $sdkUmX64 = $um
            $sdkUcrtX64 = $ucrt
            break
        }
    }
}

$libPaths = @()
foreach ($p in @($msvcLibX64, $sdkUcrtX64, $sdkUmX64)) {
    if ($p -and (Test-Path $p)) { $libPaths += $p }
}

Write-Host "[1/4] Assembling PE Writer Core..."
& $MasmExe /c /nologo /Fo"RawrXD_PE_Writer.obj" "RawrXD_PE_Writer.asm"
if ($LASTEXITCODE -ne 0) { Write-Error "Core assembly failed"; exit 1 }

Write-Host "[2/4] Assembling Symbol Batch Resolver (150 symbols)..."
& $MasmExe /c /nologo /Fo"symbol_batch_resolver_production.obj" "symbol_batch_resolver_production.asm"
if ($LASTEXITCODE -ne 0) { Write-Error "Resolver assembly failed"; exit 1 }

Write-Host "[3/4] Assembling Production Driver..."
& $MasmExe /c /nologo /Fo"production_driver_150symbols.obj" "production_driver_150symbols.asm"
if ($LASTEXITCODE -ne 0) { Write-Error "Driver assembly failed"; exit 1 }

Write-Host "[4/4] Linking Production Executable..."
$linkArgs = @(
    "/NOLOGO", "/SUBSYSTEM:CONSOLE", "/ENTRY:main", "/MACHINE:X64"
    "/OUT:RawrXD_SymbolResolver_Production.exe"
    "production_driver_150symbols.obj"
    "symbol_batch_resolver_production.obj"
    "RawrXD_PE_Writer.obj"
)
foreach ($lp in $libPaths) {
    $linkArgs += "/LIBPATH:`"$lp`""
}
$linkArgs += "kernel32.lib"

& $LinkExe $linkArgs
if ($LASTEXITCODE -ne 0) { Write-Error "Link failed"; exit 1 }

Write-Host "`n========================================================================"
Write-Host "BUILD SUCCESSFUL"
Write-Host "========================================================================`n"
Write-Host "Executable: RawrXD_SymbolResolver_Production.exe`n"
Write-Host "Run to generate PE with 150 resolved symbols:"
Write-Host "  > .\RawrXD_SymbolResolver_Production.exe`n"
Write-Host "Output: production_150symbols.exe"
Write-Host "========================================================================`n"
