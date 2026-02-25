param(
    [string]$Source = "titan_clean.asm",
    [string]$Object = "titan_clean.obj",
    [string]$Output = "titan_clean.exe"
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$msvcRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
$ml64 = Join-Path $msvcRoot "bin\Hostx64\x64\ml64.exe"
$link = Join-Path $msvcRoot "bin\Hostx64\x64\link.exe"
$sdkLib = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

if (!(Test-Path $ml64)) { throw "ml64 not found: $ml64" }
if (!(Test-Path $link)) { throw "link not found: $link" }
if (!(Test-Path $sdkLib)) { throw "Windows SDK lib path not found: $sdkLib" }
if (!(Test-Path $Source)) { throw "Source not found: $Source" }

& $ml64 /c /Fo $Object $Source
if ($LASTEXITCODE -ne 0) { throw "Assembly failed with exit code $LASTEXITCODE" }

& $link $Object /ENTRY:main /SUBSYSTEM:CONSOLE /OUT:$Output /LIBPATH:$sdkLib kernel32.lib
if ($LASTEXITCODE -ne 0) { throw "Link failed with exit code $LASTEXITCODE" }

Write-Host "Build succeeded: $Output"