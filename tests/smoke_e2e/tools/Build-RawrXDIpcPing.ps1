# Build RawrXDIpcPing.exe against production Win32IDE_ChatIpcProtocol.h (header-only).
param(
    [string]$RepoRoot = "D:\rawrxd",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
if (-not $OutDir) { $OutDir = $PSScriptRoot }

$src = Join-Path $OutDir "RawrXDIpcPing.cpp"
$exe = Join-Path $OutDir "RawrXDIpcPing.exe"
$include = Join-Path $RepoRoot "src\win32app"

if (-not (Test-Path $src)) {
    throw "Missing source: $src"
}

$ninjaBin = Join-Path $RepoRoot "..\rxdn_ninja\bin\RawrXDIpcPing.exe"
$ninjaRoot = "D:\rxdn_ninja"
if (Test-Path (Join-Path $ninjaRoot "build.ninja")) {
    Write-Host "Building via ninja RawrXDIpcPing in $ninjaRoot..."
    Push-Location $ninjaRoot
    ninja RawrXDIpcPing 2>&1 | Write-Host
    Pop-Location
    if (Test-Path $ninjaBin) {
        Copy-Item -Path $ninjaBin -Destination $exe -Force
        Write-Host "Copied $ninjaBin -> $exe" -ForegroundColor Green
    }
}

if (-not (Test-Path $exe)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vcvars = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "VC\Auxiliary\Build\vcvars64.bat" | Select-Object -First 1
        if ($vcvars -and (Test-Path $vcvars)) {
            $buildCmd = "call `"$vcvars`" >nul && cl /nologo /EHsc /std:c++17 /O2 /I `"$include`" `"$src`" /Fe:`"$exe`""
            cmd /c $buildCmd | Write-Host
        }
    }
}

if (-not (Test-Path $exe)) {
    throw "Build succeeded but $exe was not produced"
}

Write-Host "Built: $exe" -ForegroundColor Green
