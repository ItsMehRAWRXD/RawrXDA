$ErrorActionPreference = 'Stop'

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallDir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null | Select-Object -First 1

$MasmExe = Get-ChildItem "$vsInstallDir\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" | Select-Object -First 1 -ExpandProperty FullName
$LinkExe = Get-ChildItem "$vsInstallDir\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" | Select-Object -First 1 -ExpandProperty FullName

$masmDir = Split-Path $MasmExe -Parent
$msvcRoot = (Resolve-Path (Join-Path $masmDir "..\..\..")).Path
$msvcLibX64 = Join-Path $msvcRoot "lib\x64"

$sdkRoot = "C:\Program Files (x86)\Windows Kits\10\Lib"
$versions = Get-ChildItem $sdkRoot -Directory | Where-Object { $_.Name -match '^\d+\.' } | Sort-Object Name -Descending
$sdkUmX64 = Join-Path $versions[0].FullName "um\x64"
$sdkUcrtX64 = Join-Path $versions[0].FullName "ucrt\x64"

Write-Host "Building simple test..."
& $MasmExe /c /nologo test_simple_batch.asm
& $LinkExe /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main test_simple_batch.obj RawrXD_PE_Writer.obj kernel32.lib /LIBPATH:"$msvcLibX64" /LIBPATH:"$sdkUcrtX64" /LIBPATH:"$sdkUmX64"

if (Test-Path test_simple_batch.exe) {
    Write-Host "Running test..."
    & .\test_simple_batch.exe
}
