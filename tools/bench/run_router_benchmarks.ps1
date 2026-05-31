param(
    [string]$PromptTokens = "1,15043",
    [int]$Runs = 5,
    [string]$BuildDir = "d:\rawrxd\build-ninja",
    [string]$SourceDir = "d:\rawrxd",
    [string]$OutDir = "d:\rawrxd\bench_out"
)

$ErrorActionPreference = "Stop"

$repoGuard = Join-Path $SourceDir "scripts\assert_repo_root.ps1"
if (-not (Test-Path $repoGuard)) {
    throw "Repo root guard script not found: $repoGuard"
}

& $repoGuard -ExpectedRoot "d:/rawrxd" -CheckPath $SourceDir
if ($LASTEXITCODE -ne 0) {
    throw "Repository root guard failed"
}

if ($Runs -lt 1) { $Runs = 1 }
if ($Runs -gt 10) { $Runs = 10 }

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$jsonOut = Join-Path $OutDir "router-bench-$timestamp.json"
$csvOut = Join-Path $OutDir "router-bench-$timestamp.csv"

Write-Host "[RouterBench] Building target RawrXD-RouterBenchRunner..."
& ninja -C $BuildDir RawrXD-RouterBenchRunner -j 8
if ($LASTEXITCODE -ne 0) {
    throw "Build failed for RawrXD-RouterBenchRunner"
}

$exe = Join-Path $BuildDir "bin\RawrXD-RouterBenchRunner.exe"
if (-not (Test-Path $exe)) {
    throw "Runner executable not found: $exe"
}

$cudaDll = Join-Path $env:WINDIR "System32\nvcuda.dll"
if (-not (Test-Path $cudaDll)) {
    throw "NVIDIA CUDA driver not found ($cudaDll). benchmark_generation/benchmark_long_context require NVIDIA_CUDA on this lane."
}

Write-Host "[RouterBench] Running benchmarks..."
& $exe --prompt $PromptTokens --runs $Runs --json $jsonOut --csv $csvOut
if ($LASTEXITCODE -ne 0) {
    throw "Benchmark runner failed"
}

Write-Host "[RouterBench] Done"
Write-Host "  JSON: $jsonOut"
Write-Host "  CSV:  $csvOut"
