# Build and run microbenchmark
$cl = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
$inc = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include"
$winsdk = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0"

$env:INCLUDE = "$inc;$winsdk\ucrt;$winsdk\um;$winsdk\shared"
$env:LIB = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"

Write-Host "Building scalar version..." -ForegroundColor Cyan
& $cl /O2 /TC microbench.c /Fe:bench_scalar.exe /link 2>&1 | Out-Null

Write-Host "Building AVX2 version..." -ForegroundColor Cyan
& $cl /O2 /arch:AVX2 /TC /D__AVX2__ microbench.c /Fe:bench_avx2.exe /link 2>&1 | Out-Null

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  MATMUL BENCHMARK RESULTS (4096×4096)" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

if (Test-Path "bench_scalar.exe") {
    Write-Host "🔹 Scalar Implementation:" -ForegroundColor Green
    & .\bench_scalar.exe
    Write-Host ""
}

if (Test-Path "bench_avx2.exe") {
    Write-Host "🔹 AVX2 Implementation:" -ForegroundColor Green
    & .\bench_avx2.exe
}

Write-Host ""
