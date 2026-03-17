# Build Win32 IDE with MASM CLI compiler chain
$ErrorActionPreference = "Stop"

$CL = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" -Recurse -Filter "cl.exe" -ErrorAction SilentlyContinue | Where-Object { $_.Directory.Name -eq "x64" } | Select-Object -First 1

if (-not $CL) {
    Write-Host "ERROR: cl.exe not found"
    exit 1
}

$ClPath = $CL.FullName
$VCToolsPath = $CL.Directory.Parent.Parent.Parent.FullName
$WinSDK = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0"

$env:INCLUDE = "$VCToolsPath\include;$WinSDK\ucrt;$WinSDK\um;$WinSDK\shared"
$env:LIB = "$VCToolsPath\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

Push-Location $PSScriptRoot

& $ClPath /nologo /EHsc /std:c++17 /MD /O2 /W3 /D_CRT_SECURE_NO_WARNINGS `
    /Fe:RawrXD_Win32_IDE.exe RawrXD_Win32_IDE.cpp `
    user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib wininet.lib shlwapi.lib

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Built RawrXD_Win32_IDE.exe"
} else {
    Write-Host "✗ Build failed"
    Pop-Location
    exit 1
}

Pop-Location
