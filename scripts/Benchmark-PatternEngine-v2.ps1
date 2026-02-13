# ============================================================================
# RawrXD Benchmark v2 - Compare Scalar vs AVX-512 SIMD
# ============================================================================

#Requires -Version 7.0

param(
    [Parameter(Mandatory = $false)]
    [int]$Iterations = 20000
)

$ErrorActionPreference = 'Stop'

# Test patterns
$TestPatterns = @(
    "// TODO: Fix this bug in the authentication module",
    "# FIXME: broken logic here needs refactoring",
    "/* BUG: crash on null pointer dereference */",
    "// NOTE: important implementation detail below",
    "// XXX: temporary workaround - needs proper fix",
    "This line has no pattern markers at all",
    "// HACK: quick fix for deadline - clean up later",
    "// IDEA: could optimize this with caching"
)

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║       RawrXD Pattern Engine Benchmark v2                       ║" -ForegroundColor Cyan
Write-Host "║       Iterations: $($Iterations.ToString().PadRight(43))║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ============================================================================
# Test 1: Scalar Engine
# ============================================================================
Write-Host "[SCALAR] Testing RawrXD_PatternEngine_Real.dll..." -ForegroundColor Yellow

$scalarDll = "D:\lazy init ide\bin\RawrXD_PatternEngine_Real.dll"
$scalarEscaped = $scalarDll -replace '\\', '\\\\'

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class ScalarEngine
{
    [DllImport("$scalarEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ClassifyPattern(IntPtr codeBuffer, int length, IntPtr context, out double confidence);

    [DllImport("$scalarEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializePatternEngine();

    [DllImport("$scalarEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ShutdownPatternEngine();
}
"@

[ScalarEngine]::InitializePatternEngine() | Out-Null
Write-Host "[SCALAR] Engine initialized" -ForegroundColor Green

# Warmup
Write-Host "[SCALAR] Warming up..." -ForegroundColor Gray
foreach ($_ in 1..1000) {
    foreach ($pattern in $TestPatterns) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($pattern)
        $ptr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($bytes.Length)
        $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(1)
        [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $ptr, $bytes.Length)
        [double]$conf = 0.0
        [ScalarEngine]::ClassifyPattern($ptr, $bytes.Length, $ctxPtr, [ref]$conf) | Out-Null
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
    }
}

# Benchmark
Write-Host "[SCALAR] Running $Iterations iterations..." -ForegroundColor Yellow
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$scalarOps = 0

foreach ($_ in 1..$Iterations) {
    foreach ($pattern in $TestPatterns) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($pattern)
        $ptr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($bytes.Length)
        $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(1)
        [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $ptr, $bytes.Length)
        [double]$conf = 0.0
        [ScalarEngine]::ClassifyPattern($ptr, $bytes.Length, $ctxPtr, [ref]$conf) | Out-Null
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
        $scalarOps++
    }
}
$sw.Stop()
[ScalarEngine]::ShutdownPatternEngine() | Out-Null

$scalarMs = $sw.Elapsed.TotalMilliseconds
$scalarOpsPerSec = [math]::Round($scalarOps / ($scalarMs / 1000), 0)
$scalarNsPerOp = [math]::Round(($scalarMs * 1000000) / $scalarOps, 2)

Write-Host "[SCALAR] Complete: $scalarOpsPerSec ops/sec, $scalarNsPerOp ns/op" -ForegroundColor Green
Write-Host ""

# ============================================================================
# Test 2: AVX-512 SIMD Engine
# ============================================================================
Write-Host "[AVX512] Testing RawrXD_AVX512_SIMD.dll..." -ForegroundColor Yellow

$avxDll = "D:\lazy init ide\bin\RawrXD_AVX512_SIMD.dll"
$avxEscaped = $avxDll -replace '\\', '\\\\'

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class AVX512Engine
{
    [DllImport("$avxEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ClassifyPattern(IntPtr codeBuffer, int length, IntPtr context, out double confidence);

    [DllImport("$avxEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializePatternEngine();

    [DllImport("$avxEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ShutdownPatternEngine();
    
    [DllImport("$avxEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetEngineInfo(IntPtr buffer);
}
"@

[AVX512Engine]::InitializePatternEngine() | Out-Null

# Check engine mode
$infoBuffer = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(64)
[AVX512Engine]::GetEngineInfo($infoBuffer) | Out-Null
$mode = [System.Runtime.InteropServices.Marshal]::ReadInt32($infoBuffer, 4)
[System.Runtime.InteropServices.Marshal]::FreeHGlobal($infoBuffer)
$modeStr = if ($mode -eq 2) { "AVX-512 SIMD ACTIVE" } else { "Scalar Fallback" }
Write-Host "[AVX512] Engine mode: $modeStr" -ForegroundColor $(if ($mode -eq 2) { "Magenta" } else { "Yellow" })

# Warmup
Write-Host "[AVX512] Warming up..." -ForegroundColor Gray
foreach ($_ in 1..1000) {
    foreach ($pattern in $TestPatterns) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($pattern)
        $ptr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($bytes.Length)
        $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(1)
        [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $ptr, $bytes.Length)
        [double]$conf = 0.0
        [AVX512Engine]::ClassifyPattern($ptr, $bytes.Length, $ctxPtr, [ref]$conf) | Out-Null
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
    }
}

# Benchmark
Write-Host "[AVX512] Running $Iterations iterations..." -ForegroundColor Yellow
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$avxOps = 0

foreach ($_ in 1..$Iterations) {
    foreach ($pattern in $TestPatterns) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($pattern)
        $ptr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($bytes.Length)
        $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(1)
        [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $ptr, $bytes.Length)
        [double]$conf = 0.0
        [AVX512Engine]::ClassifyPattern($ptr, $bytes.Length, $ctxPtr, [ref]$conf) | Out-Null
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
        $avxOps++
    }
}
$sw.Stop()
[AVX512Engine]::ShutdownPatternEngine() | Out-Null

$avxMs = $sw.Elapsed.TotalMilliseconds
$avxOpsPerSec = [math]::Round($avxOps / ($avxMs / 1000), 0)
$avxNsPerOp = [math]::Round(($avxMs * 1000000) / $avxOps, 2)

Write-Host "[AVX512] Complete: $avxOpsPerSec ops/sec, $avxNsPerOp ns/op" -ForegroundColor Green
Write-Host ""

# ============================================================================
# Results
# ============================================================================
$speedup = [math]::Round($avxOpsPerSec / $scalarOpsPerSec, 2)
$latencyGain = [math]::Round($scalarNsPerOp / $avxNsPerOp, 2)

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    BENCHMARK RESULTS                           ║" -ForegroundColor Cyan
Write-Host "╠════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  Engine      │ Throughput      │ Latency                       ║" -ForegroundColor White
Write-Host "╠════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  Scalar      │ $($scalarOpsPerSec.ToString('N0').PadLeft(10)) ops/s │ $($scalarNsPerOp.ToString('N0').PadLeft(8)) ns/op                ║" -ForegroundColor White
Write-Host "║  AVX-512     │ $($avxOpsPerSec.ToString('N0').PadLeft(10)) ops/s │ $($avxNsPerOp.ToString('N0').PadLeft(8)) ns/op                ║" -ForegroundColor $(if ($mode -eq 2) { "Magenta" } else { "Yellow" })
Write-Host "╠════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  Speedup:    ${speedup}x                                            ║" -ForegroundColor $(if ($speedup -gt 1.5) { "Green" } else { "Yellow" })
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

if ($mode -ne 2) {
    Write-Host "⚠️  AVX-512 not detected - CPU may not support it or XGETBV check failed" -ForegroundColor Yellow
}
