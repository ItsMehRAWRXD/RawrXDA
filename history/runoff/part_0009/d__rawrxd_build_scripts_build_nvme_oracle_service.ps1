param(
    [string]$AsmPath = "D:\rawrxd\src\direct_io\nvme_oracle_service.asm",
    [string]$OutDir = "D:\rawrxd\build"
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

# Try to locate ml64.exe and link.exe
$ml64 = Find-Tool 'ml64.exe' @(
    'C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe'
)
$link = Find-Tool 'link.exe' @(
    'C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe'
)

# Find latest Windows SDK lib path
$libRoot = 'C:\Program Files (x86)\Windows Kits\10\Lib'
if (-not (Test-Path $libRoot)) { throw "Windows 10 SDK Lib not found at $libRoot" }
$verDir = Get-ChildItem -Directory $libRoot | Sort-Object Name -Descending | Select-Object -First 1
$umx64 = Join-Path $verDir.FullName 'um\x64'
$advapi32 = Join-Path $umx64 'Advapi32.lib'
$kernel32 = Join-Path $umx64 'Kernel32.lib'
if (-not (Test-Path $advapi32)) { throw "Advapi32.lib not found at $advapi32" }
if (-not (Test-Path $kernel32)) { throw "Kernel32.lib not found at $kernel32" }
$user32 = Join-Path $umx64 'User32.lib'

# Prepare output paths
$outObj = Join-Path $OutDir 'nvme_oracle_service.obj'
$outExe = Join-Path $OutDir 'nvme_oracle_service.exe'

Write-Host "Assembling $AsmPath -> $outObj"
& $ml64 /c $AsmPath /Fo $outObj

Write-Host "Linking -> $outExe"
& $link $outObj $advapi32 $kernel32 $user32 /SUBSYSTEM:windows /ENTRY:ServiceEntry /OUT:$outExe

Write-Host "Build complete: $outExe"