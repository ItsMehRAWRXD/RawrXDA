$ErrorActionPreference = "Stop"
Write-Host "=== RawrXD Self-Hosting Build Script ==="
Write-Host "Emitting RawrXD_Native_Core.dll via MASM..."

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installPath) {
    Write-Host "Could not find Visual Studio installation."
    $installPath = "C:\VS2022Enterprise"
}

$cl = "$installPath\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe" | Resolve-Path | Select-Object -ExpandProperty Path -First 1
if (-not $cl) {
    $cl = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
}
Write-Host "Using C++:   $cl"

$ml64 = "$installPath\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" | Resolve-Path | Select-Object -ExpandProperty Path -First 1
$link = "$installPath\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" | Resolve-Path | Select-Object -ExpandProperty Path -First 1

if (-not $ml64 -or -not $link) {
    $ml64 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
    $link = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
}

$libPaths = @(
    "C:\Program Files (x86)\Windows Kits\10\Lib\*\um\x64" | Resolve-Path | Select-Object -ExpandProperty Path | Select-Object -Last 1
)

Write-Host "Using ASM:   $ml64"
Write-Host "Using LINK:  $link"

$outDir = "D:\rawrxd\build\bin"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

$srcAsm = "D:\rawrxd\src\self_hosting\NativeIO_Core.asm"
$srcUI = "D:\rawrxd\src\self_hosting\RawrXD_Native_UI.asm"
$srcExplorer = "D:\rawrxd\src\self_hosting\RawrXD_Native_Explorer.asm"
$defFile = "D:\rawrxd\src\self_hosting\RawrXD_Native_Core.def"
$objFile = "D:\rawrxd\src\self_hosting\NativeIO_Core.obj"
$objUI = "D:\rawrxd\src\self_hosting\RawrXD_Native_UI.obj"
$objExplorer = "D:\rawrxd\src\self_hosting\RawrXD_Native_Explorer.obj"
$outDll = "$outDir\RawrXD_Native_Core.dll"

$srcDock = "D:\rawrxd\src\self_hosting\RawrXD_DockManager.cpp"
$objDock = "D:\rawrxd\src\self_hosting\RawrXD_DockManager.obj"

$srcSwarm = "D:\rawrxd\src\compute\SwarmLink_HotSwap.cpp"
$objSwarm = "D:\rawrxd\src\compute\SwarmLink_HotSwap.obj"

$inc1 = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um"
$inc2 = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared"
$inc3 = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt"
$inc4 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include"

& $cl /c /nologo /EHsc /I$inc1 /I$inc2 /I$inc3 /I$inc4 /Fo$objDock $srcDock
if ($LASTEXITCODE -ne 0) { exit 1 }

& $cl /c /nologo /EHsc /I$inc1 /I$inc2 /I$inc3 /I$inc4 /Fo$objSwarm $srcSwarm
if ($LASTEXITCODE -ne 0) { exit 1 }

& $ml64 /c /nologo "/Fo$objFile" $srcAsm
if ($LASTEXITCODE -ne 0) { exit 1 }
& $ml64 /c /nologo "/Fo$objUI" $srcUI
if ($LASTEXITCODE -ne 0) { exit 1 }
& $ml64 /c /nologo "/Fo$objExplorer" $srcExplorer

if ($LASTEXITCODE -ne 0) {
    Write-Host "MASM compilation failed."
    exit 1
}

$linkArgs = @(
    "/NOLOGO",
    "/DLL",
    "/DEF:$defFile",
    "/OUT:$outDll",
    "/ENTRY:DllMain",
    $objFile,
    $objDock,
    $objSwarm,
    "kernel32.lib", "user32.lib", "comctl32.lib", "userenv.lib", "gdi32.lib", "libvcruntime.lib", "libucrt.lib", "libcmt.lib", "libcpmt.lib",
    $objUI, $objExplorer
)

$env:LIB += ";C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"
$env:LIB += ";C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64"
$env:LIB += ";" + ($libPaths -join ";")

& $link $linkArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Successfully emitted $outDll"
} else {
    Write-Host "Linker failed."
    exit 1
}
