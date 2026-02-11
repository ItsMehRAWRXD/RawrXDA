# ============================================================================
# RawrXD Benchmark - Compare Scalar vs AVX-512 SIMD
# ============================================================================

#Requires -Version 7.0

param(
    [Parameter(Mandatory = $false)]
    [int]$Iterations = 50000,
    
    [Parameter(Mandatory = $false)]
    [ValidateSet("Scalar", "AVX512", "Both")]
    [string]$Engine = "Both"
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
    "// IDEA: could optimize this with caching",
    "// REVIEW: security audit required",
    "Normal code without any markers here"
)

function Test-Engine {
    param(
        [string]$DllPath,
        [string]$EngineName,
        [int]$Iterations
    )
    
    if (-not (Test-Path $DllPath)) {
        Write-Host "[$EngineName] DLL not found: $DllPath" -ForegroundColor Red
        return $null
    }
    
    $dllPathEscaped = $DllPath -replace '\\', '\\'
    
    # Define the type with a unique name to avoid conflicts
    $typeName = "RawrXD_$($EngineName)_$([guid]::NewGuid().ToString('N').Substring(0,8))"
    
    Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class $typeName
{
    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ClassifyPattern(IntPtr codeBuffer, int length, IntPtr context, out double confidence);

    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializePatternEngine();

    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ShutdownPatternEngine();

    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetEngineInfo(IntPtr buffer);
}
"@
    
    $engineType = [Type]::GetType($typeName)
    
    # Initialize
    $initMethod = $engineType.GetMethod("InitializePatternEngine")
    $classifyMethod = $engineType.GetMethod("ClassifyPattern")
    $shutdownMethod = $engineType.GetMethod("ShutdownPatternEngine")
    $getInfoMethod = $engineType.GetMethod("GetEngineInfo")
    
    $initResult = $initMethod.Invoke($null, @())
    if ($initResult -ne 0) {
        Write-Host "[$EngineName] Failed to initialize (code: $initResult)" -ForegroundColor Red
        return $null
    }
    
    # Get engine info
    $infoBuffer = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(64)
    try {
        $infoResult = $getInfoMethod.Invoke($null, @($infoBuffer))
        if ($infoResult -eq 0) {
            $mode = [System.Runtime.InteropServices.Marshal]::ReadInt32($infoBuffer, 4)
            $modeStr = if ($mode -eq 2) { "AVX-512 SIMD" } else { "Scalar" }
            Write-Host "[$EngineName] Engine mode: $modeStr" -ForegroundColor Cyan
        }
    }
    finally {
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($infoBuffer)
    }
    
    # Warmup
    Write-Host "[$EngineName] Warming up (1000 iterations)..." -ForegroundColor Yellow
    foreach ($_ in 1..1000) {
        foreach ($pattern in $TestPatterns) {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($pattern)
            $ptr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($bytes.Length)
            $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(1)
            try {
                [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $ptr, $bytes.Length)
                [double]$conf = 0.0
                $null = $classifyMethod.Invoke($null, @($ptr, $bytes.Length, $ctxPtr, [ref]$conf))
            }
            finally {
                [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
                [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
            }
        }
    }
    
    # Benchmark
    Write-Host "[$EngineName] Running $Iterations iterations..." -ForegroundColor Yellow
    
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $totalOps = 0
    $matches = 0
    
    foreach ($_ in 1..$Iterations) {
        foreach ($pattern in $TestPatterns) {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($pattern)
            $ptr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($bytes.Length)
            $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(1)
            try {
                [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $ptr, $bytes.Length)
                [double]$conf = 0.0
                $result = $classifyMethod.Invoke($null, @($ptr, $bytes.Length, $ctxPtr, [ref]$conf))
                if ($result -gt 0) { $matches++ }
                $totalOps++
            }
            finally {
                [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
                [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
            }
        }
    }
    
    $sw.Stop()
    
    # Shutdown
    $shutdownMethod.Invoke($null, @()) | Out-Null
    
    # Calculate metrics
    $elapsedMs = $sw.Elapsed.TotalMilliseconds
    $opsPerSec = [math]::Round($totalOps / ($elapsedMs / 1000), 0)
    $nsPerOp = [math]::Round(($elapsedMs * 1000000) / $totalOps, 2)
    
    return @{
        Engine = $EngineName
        TotalOps = $totalOps
        Matches = $matches
        ElapsedMs = [math]::Round($elapsedMs, 2)
        OpsPerSec = $opsPerSec
        NsPerOp = $nsPerOp
    }
}

# ============================================================================
# Main
# ============================================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║       RawrXD Pattern Engine Benchmark                          ║" -ForegroundColor Cyan
Write-Host "║       Iterations: $($Iterations.ToString().PadRight(43))║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$results = @()

if ($Engine -eq "Scalar" -or $Engine -eq "Both") {
    $scalarResult = Test-Engine -DllPath "D:\lazy init ide\bin\RawrXD_PatternEngine_Real.dll" `
        -EngineName "Scalar" -Iterations $Iterations
    if ($scalarResult) { $results += $scalarResult }
    Write-Host ""
}

if ($Engine -eq "AVX512" -or $Engine -eq "Both") {
    $avxResult = Test-Engine -DllPath "D:\lazy init ide\bin\RawrXD_AVX512_SIMD.dll" `
        -EngineName "AVX512" -Iterations $Iterations
    if ($avxResult) { $results += $avxResult }
    Write-Host ""
}

# Summary
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                    BENCHMARK RESULTS                           ║" -ForegroundColor Green
Write-Host "╠════════════════════════════════════════════════════════════════╣" -ForegroundColor Green

foreach ($r in $results) {
    Write-Host "║  $($r.Engine.PadRight(10)) | $($r.OpsPerSec.ToString('N0').PadLeft(10)) ops/sec | $($r.NsPerOp.ToString('N0').PadLeft(8)) ns/op ║" -ForegroundColor White
}

Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green

# Comparison
if ($results.Count -eq 2) {
    $scalar = $results | Where-Object { $_.Engine -eq "Scalar" }
    $avx = $results | Where-Object { $_.Engine -eq "AVX512" }
    
    if ($scalar -and $avx) {
        $speedup = [math]::Round($avx.OpsPerSec / $scalar.OpsPerSec, 2)
        $latencyImprovement = [math]::Round($scalar.NsPerOp / $avx.NsPerOp, 2)
        
        Write-Host ""
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Yellow
        Write-Host "  AVX-512 vs Scalar Comparison:" -ForegroundColor Yellow
        Write-Host "    Throughput improvement: ${speedup}x faster" -ForegroundColor $(if ($speedup -gt 1.5) { "Green" } else { "Yellow" })
        Write-Host "    Latency improvement:    ${latencyImprovement}x faster" -ForegroundColor $(if ($latencyImprovement -gt 1.5) { "Green" } else { "Yellow" })
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Yellow
    }
}

Write-Host ""
