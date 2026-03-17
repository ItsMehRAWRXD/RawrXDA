param(
    [string]$BuildDir = "D:\rawrxd\build",
    [string]$SrcDir = "D:\rawrxd\src\direct_io"
)

$ErrorActionPreference = 'Stop'

function Find-Tool($name, $fallbacks) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Path }
    foreach ($path in $fallbacks) {
        if (Test-Path $path) { return $path }
    }
    throw "Tool $name not found. Ensure VS Build Tools are installed and in PATH."
}

# Try to locate ml64.exe, link.exe, cl.exe
$ml64 = Find-Tool 'ml64.exe' @(
    'C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe'
)
$link = Find-Tool 'link.exe' @(
    'C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe'
)
$cl = Find-Tool 'cl.exe' @(
    'C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe'
)

# Find Windows SDK lib path
$libRoot = 'C:\Program Files (x86)\Windows Kits\10\Lib'
if (-not (Test-Path $libRoot)) { throw "Windows 10 SDK Lib not found at $libRoot" }
$verDir = Get-ChildItem -Directory $libRoot | Sort-Object Name -Descending | Select-Object -First 1
$umx64 = Join-Path $verDir.FullName 'um\x64'
$kernel32 = Join-Path $umx64 'Kernel32.lib'
if (-not (Test-Path $kernel32)) { throw "Kernel32.lib not found at $kernel32" }

Write-Host "Building MASM DLL..."
$asmPath = Join-Path $SrcDir 'nvme_query.asm'
$objPath = Join-Path $BuildDir 'nvme_query.obj'
$dllPath = Join-Path $BuildDir 'nvme_query.dll'

& $ml64 /c $asmPath /Fo $objPath
if ($LASTEXITCODE -ne 0) { throw "MASM assembly failed" }

& $link /DLL /EXPORT:QueryNVMeTemp /OUT:$dllPath $objPath $kernel32
if ($LASTEXITCODE -ne 0) { throw "MASM linking failed" }

Write-Host "Building C++ Service Host..."
$cppPath = Join-Path $SrcDir 'SovereignNVMeOracle.cpp'
$exePath = Join-Path $BuildDir 'SovereignNVMeOracle.exe'

# Compile C++
& $cl /O2 /Fe:$exePath $cppPath $advapi32
if ($LASTEXITCODE -ne 0) { throw "C++ compilation failed" }

Write-Host "Hybrid service build complete!"
Write-Host "DLL: $dllPath"
Write-Host "EXE: $exePath"