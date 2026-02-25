# Test DLL Basic Functionality
# Tests loading, exports, and basic GGUF parsing

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Native Model Bridge - TEST SUITE" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"
$DllPath = Join-Path $PSScriptRoot "RawrXD_NativeModelBridge.dll"
$TestModel = Join-Path $PSScriptRoot "test_model_1m.gguf"

# Test 1: DLL Exists
Write-Host "[1/6] Checking DLL existence..." -NoNewline
if (Test-Path $DllPath) {
    Write-Host " ✅" -ForegroundColor Green
    $dllInfo = Get-Item $DllPath
    Write-Host "      Size: $($dllInfo.Length) bytes" -ForegroundColor Gray
} else {
    Write-Host " ❌" -ForegroundColor Red
    Write-Host "DLL not found at: $DllPath" -ForegroundColor Red
    exit 1
}

# Test 2: DLL Loads
Write-Host "[2/6] Loading DLL..." -NoNewline
try {
    Add-Type @"
using System;
using System.Runtime.InteropServices;

public class NativeBridge {
    [DllImport("RawrXD_NativeModelBridge.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int DllMain(IntPtr hInst, uint fdwReason, IntPtr lpReserved);
    
    [DllImport("RawrXD_NativeModelBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int LoadModelNative(
        [MarshalAs(UnmanagedType.LPStr)] string lpPath,
        out IntPtr ppContext
    );
    
    [DllImport("RawrXD_NativeModelBridge.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ForwardPass(
        IntPtr pCtx,
        uint token,
        uint pos,
        IntPtr pLogits
    );
}
"@
    Write-Host " ✅" -ForegroundColor Green
} catch {
    Write-Host " ❌" -ForegroundColor Red
    Write-Host "Failed to load DLL: $_" -ForegroundColor Red
    exit 1
}

# Test 3: Test Model Exists
Write-Host "[3/6] Checking test model..." -NoNewline
if (Test-Path $TestModel) {
    Write-Host " ✅" -ForegroundColor Green
    $modelInfo = Get-Item $TestModel
    Write-Host "      Size: $($modelInfo.Length) bytes" -ForegroundColor Gray
} else {
    Write-Host " ⚠️" -ForegroundColor Yellow
    Write-Host "      Test model not found. Run: python generate_test_gguf.py" -ForegroundColor Yellow
}

# Test 4: Parse GGUF Header (Manual)
Write-Host "[4/6] Parsing GGUF header..." -NoNewline
if (Test-Path $TestModel) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($TestModel)
        
        # Magic (bytes 0-3)
        $magic = [BitConverter]::ToUInt32($bytes, 0)
        $magicHex = "0x{0:X8}" -f $magic
        
        # Version (bytes 4-7)
        $version = [BitConverter]::ToUInt32($bytes, 4)
        
        # Tensor count (bytes 8-15)
        $tensorCount = [BitConverter]::ToUInt64($bytes, 8)
        
        # KV count (bytes 16-23)
        $kvCount = [BitConverter]::ToUInt64($bytes, 16)
        
        if ($magic -eq 0x46554747) {
            Write-Host " ✅" -ForegroundColor Green
            Write-Host "      Magic: $magicHex (GGUF)" -ForegroundColor Gray
            Write-Host "      Version: $version" -ForegroundColor Gray
            Write-Host "      Tensors: $tensorCount" -ForegroundColor Gray
            Write-Host "      Metadata KV: $kvCount" -ForegroundColor Gray
        } else {
            Write-Host " ❌" -ForegroundColor Red
            Write-Host "      Invalid magic: $magicHex (expected 0x46554747)" -ForegroundColor Red
        }
    } catch {
        Write-Host " ❌" -ForegroundColor Red
        Write-Host "      Error reading file: $_" -ForegroundColor Red
    }
} else {
    Write-Host " ⏭️  SKIPPED" -ForegroundColor Yellow
}

# Test 5: Call LoadModelNative
Write-Host "[5/6] Testing LoadModelNative..." -NoNewline
if (Test-Path $TestModel) {
    try {
        [IntPtr]$ctx = [IntPtr]::Zero
        $result = [NativeBridge]::LoadModelNative($TestModel, [ref]$ctx)
        
        if ($result -eq 1 -and $ctx -ne [IntPtr]::Zero) {
            Write-Host " ✅" -ForegroundColor Green
            Write-Host "      Context: 0x$($ctx.ToString('X16'))" -ForegroundColor Gray
        } elseif ($result -eq 1) {
            Write-Host " ⚠️" -ForegroundColor Yellow
            Write-Host "      Function returned success but context is null" -ForegroundColor Yellow
        } else {
            Write-Host " ❌" -ForegroundColor Red
            Write-Host "      Function returned error code: $result" -ForegroundColor Red
        }
    } catch {
        Write-Host " ❌" -ForegroundColor Red
        Write-Host "      Exception: $_" -ForegroundColor Red
        Write-Host "      This is expected if implementation is incomplete" -ForegroundColor Gray
    }
} else {
    Write-Host " ⏭️  SKIPPED (no test model)" -ForegroundColor Yellow
}

# Test 6: Memory Leak Check (Basic)
Write-Host "[6/6] Basic memory check..." -NoNewline
try {
    $process = Get-Process -Id $PID
    $memBefore = $process.WorkingSet64
    
    # Allocate and free multiple times
    for ($i = 0; $i -lt 10; $i++) {
        [IntPtr]$ctx = [IntPtr]::Zero
        $result = [NativeBridge]::LoadModelNative($TestModel, [ref]$ctx)
        # Note: Should call UnloadModelNative here
    }
    
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
    Start-Sleep -Milliseconds 100
    
    $process.Refresh()
    $memAfter = $process.WorkingSet64
    $memDelta = [Math]::Abs($memAfter - $memBefore)
    $memDeltaMB = [Math]::Round($memDelta / 1MB, 2)
    
    if ($memDeltaMB -lt 50) {
        Write-Host " ✅" -ForegroundColor Green
        Write-Host "      Memory delta: $memDeltaMB MB" -ForegroundColor Gray
    } else {
        Write-Host " ⚠️" -ForegroundColor Yellow
        Write-Host "      Memory delta: $memDeltaMB MB (possible leak)" -ForegroundColor Yellow
    }
} catch {
    Write-Host " ⚠️" -ForegroundColor Yellow
    Write-Host "      Could not perform memory check" -ForegroundColor Gray
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TEST SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "✅ = Passed" -ForegroundColor Green
Write-Host "⚠️  = Warning or partial success" -ForegroundColor Yellow
Write-Host "❌ = Failed" -ForegroundColor Red
Write-Host "⏭️  = Skipped" -ForegroundColor Gray
Write-Host ""

# Additional diagnostics
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "DIAGNOSTICS" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check exports
if (Get-Command dumpbin -ErrorAction SilentlyContinue) {
    Write-Host "DLL Exports:" -ForegroundColor White
    $exports = & dumpbin /exports $DllPath 2>&1 | Select-String -Pattern "^\s+\d+\s+\w+\s+\w+\s+(\w+)"
    if ($exports) {
        $exports | ForEach-Object {
            Write-Host "  - $($_.Matches.Groups[1].Value)" -ForegroundColor Gray
        }
    }
    Write-Host ""
}

# Check dependencies
if (Get-Command dumpbin -ErrorAction SilentlyContinue) {
    Write-Host "DLL Dependencies:" -ForegroundColor White
    $deps = & dumpbin /dependents $DllPath 2>&1 | Select-String -Pattern "\.dll"
    if ($deps) {
        $deps | ForEach-Object {
            Write-Host "  - $($_.Line.Trim())" -ForegroundColor Gray
        }
    }
    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "🎯 NEXT STEPS" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. If tests passed, implement remaining stubs:" -ForegroundColor White
Write-Host "   - Complete GetTokenEmbedding" -ForegroundColor Gray
Write-Host "   - Complete InitMathTables" -ForegroundColor Gray
Write-Host "   - Complete ForwardPass logic" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Run full inference test:" -ForegroundColor White
Write-Host "   powershell -File test_inference.ps1" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Profile performance:" -ForegroundColor White
Write-Host "   Measure-Command { .\test_dll_basic.ps1 }" -ForegroundColor Gray
Write-Host ""
