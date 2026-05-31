#Requires -Version 5.1
<#
.SYNOPSIS
  Configure (if needed), build, and run the sovereign_assembler_smoke PE harness.

.DESCRIPTION
  Week-2 style gate: validates SovereignAssembler without manual cl.exe flags.
  Uses RAWRXD_REPO_ROOT or this repo's parent directory, optional -BuildDir.

.EXAMPLE
  .\scripts\Run-SovereignAssemblerSmoke.ps1
  .\scripts\Run-SovereignAssemblerSmoke.ps1 -BuildDir D:\rawrxd\build_ninja
#>
param(
    [string]$BuildDir = "",
    [string]$CmakeTarget = "sovereign_assembler_smoke_run",
    [switch]$SkipConfigure
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = $env:RAWRXD_REPO_ROOT
if (-not $repoRoot -or -not (Test-Path -LiteralPath $repoRoot)) {
    $repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "CMakeLists.txt"))) {
    throw "Repository root not found (set RAWRXD_REPO_ROOT or run from repo): $repoRoot"
}

if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build_sovereign_smoke"
}

$cmakeLists = Join-Path $repoRoot "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $cmakeLists)) {
    throw "Missing CMakeLists.txt at $cmakeLists"
}

if (-not $SkipConfigure -or -not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
    if (-not (Test-Path -LiteralPath $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    Write-Host "[Run-SovereignAssemblerSmoke] Configuring: $BuildDir"
    & cmake -S $repoRoot -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[Run-SovereignAssemblerSmoke] Ninja failed; trying Visual Studio 17 2022 x64..."
        & cmake -S $repoRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}

Write-Host "[Run-SovereignAssemblerSmoke] Building target $CmakeTarget..."
& cmake --build $BuildDir --config Release --target $CmakeTarget
exit $LASTEXITCODE
