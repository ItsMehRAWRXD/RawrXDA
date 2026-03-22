#Requires -Version 5.1
<#
.SYNOPSIS
  Build (optional) and run RawrXD-TpsSmoke — TPS vs RAWRXD_TPS_REF (default 239).
  For every .gguf under a tree, use scripts/Benchmark-TpsSmoke-Models.ps1 (sets RAWRXD_TPS_MACHINE_JSON=1).

.PARAMETER BuildDir
  CMake build directory (default: build_smoke_auto).

.PARAMETER Config
  MSBuild/VS config (default: Release).

.PARAMETER ModelPath
  Path to .gguf (default: model/llama-7b-q4_0.gguf under repo root).

.PARAMETER MaxTokens
  Max decode steps cap (default: 8 — same as Benchmark-TpsSmoke-Models.ps1 / RAWRXD_TPS_REF=239 regime). Override for longer runs.

.PARAMETER RequireBeat
  If set, exit non-zero when TPS < ref (sets RAWRXD_TPS_REQUIRE_BEAT=1).

.PARAMETER DecodeProgress
  Sets RAWRXD_GGUF_DECODE_PROGRESS=1 so GGUFRunner logs each decode step (bash-style VAR=value does not work in PowerShell).

.PARAMETER SkipBuild
  Only run the exe if present.
#>
param(
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [string]$ModelPath = "",
    [int]$MaxTokens = 8,
    [switch]$RequireBeat,
    [switch]$DecodeProgress,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $repoRoot "CMakeLists.txt"))) {
    $walk = $PSScriptRoot
    while ($walk -and -not (Test-Path (Join-Path $walk "CMakeLists.txt"))) {
        $walk = Split-Path -Parent $walk
    }
    $repoRoot = $walk
}
if (-not $repoRoot -or -not (Test-Path (Join-Path $repoRoot "CMakeLists.txt"))) {
    throw "Could not find repository root (CMakeLists.txt)."
}

if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build_smoke_auto"
}

$candidates = @(
    (Join-Path $BuildDir "bin\$Config\RawrXD-TpsSmoke.exe"),
    (Join-Path $BuildDir "bin\RawrXD-TpsSmoke.exe"),
    (Join-Path $BuildDir "RawrXD-TpsSmoke.exe")
)
$exe = $null
foreach ($c in $candidates) {
    if (Test-Path $c) { $exe = $c; break }
}

if (-not $SkipBuild) {
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
        cmake -S $repoRoot -B $BuildDir
    }
    cmake --build $BuildDir --config $Config --target RawrXD-TpsSmoke
}

foreach ($c in $candidates) {
    if (Test-Path $c) { $exe = $c; break }
}

if (-not $exe -or -not (Test-Path $exe)) {
    throw "RawrXD-TpsSmoke not found under $BuildDir (build: cmake --build '$BuildDir' --config $Config --target RawrXD-TpsSmoke)."
}

if (-not $ModelPath) {
    $ModelPath = Join-Path $repoRoot "model\llama-7b-q4_0.gguf"
}

$env:RAWRXD_TPS_REF = if ($env:RAWRXD_TPS_REF) { $env:RAWRXD_TPS_REF } else { "239" }
if ($RequireBeat) {
    $env:RAWRXD_TPS_REQUIRE_BEAT = "1"
} else {
    Remove-Item Env:\RAWRXD_TPS_REQUIRE_BEAT -ErrorAction SilentlyContinue
}

if ($DecodeProgress) {
    $env:RAWRXD_GGUF_DECODE_PROGRESS = "1"
} else {
    Remove-Item Env:\RAWRXD_GGUF_DECODE_PROGRESS -ErrorAction SilentlyContinue
}

Push-Location $repoRoot
try {
    & $exe $ModelPath $MaxTokens
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
