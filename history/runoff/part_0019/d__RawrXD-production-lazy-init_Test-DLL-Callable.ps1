#!/usr/bin/env pwsh
# Minimal diagnostic: test DLL is actually callable

Write-Host "Testing DLL export visibility and signature..."

# First verify DLL exists and is loaded
$dllPath = "D:\RawrXD-production-lazy-init\build\LazyHotpatchWrapper.dll"
if (-not (Test-Path $dllPath)) {
    Write-Error "DLL not found: $dllPath"
    exit 1
}

Write-Host "✓ DLL exists at: $dllPath"

# Try different calling convention approaches
$cSharpCode = @"
using System;
using System.Runtime.InteropServices;

public static class TestHotpatch {
    // Attempt 1: Standard CDECL with Unicode marshaling
    [DllImport("LazyHotpatchWrapper.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public static extern int RawrXD_LazyHotpatch_Cdecl(string modelPath, out IntPtr outMap, out IntPtr outView);
    
    // Attempt 2: StdCall (though x64 should be CDECL)
    [DllImport("LazyHotpatchWrapper.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    public static extern int RawrXD_LazyHotpatch_StdCall(string modelPath, out IntPtr outMap, out IntPtr outView);
    
    // Attempt 3: With explicit entry point
    [DllImport("LazyHotpatchWrapper.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode, EntryPoint = "RawrXD_LazyHotpatch")]
    public static extern int RawrXD_LazyHotpatch_Explicit(string modelPath, out IntPtr outMap, out IntPtr outView);
    
    // Attempt 4: Pass as IntPtr (manual marshaling)
    [DllImport("LazyHotpatchWrapper.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int RawrXD_LazyHotpatch_IntPtr(IntPtr modelPathPtr, out IntPtr outMap, out IntPtr outView);
}
"@

Add-Type -TypeDefinition $cSharpCode -CompilerOptions "/nologo" 2>&1 | Tee-Object -Variable compileErrors

if ($compileErrors) {
    Write-Error "C# compilation failed"
    exit 1
}

Write-Host ""
Write-Host "Testing Attempt 1: CDECL with Unicode string..."
$testModel = "D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf"
Write-Host "  Path: $testModel"
Write-Host "  Exists: $(Test-Path $testModel)"

try {
    $map = [IntPtr]::Zero
    $view = [IntPtr]::Zero
    $result = [TestHotpatch]::RawrXD_LazyHotpatch_Cdecl($testModel, [ref]$map, [ref]$view)
    Write-Host "  ✓ CDECL Attempt succeeded: RC=$result, Map=$map, View=$view"
} catch {
    Write-Host "  ⚠ CDECL failed: $($_.Exception.Message)"
}

Write-Host ""
Write-Host "Testing Attempt 3: CDECL with explicit EntryPoint..."
try {
    $map = [IntPtr]::Zero
    $view = [IntPtr]::Zero
    $result = [TestHotpatch]::RawrXD_LazyHotpatch_Explicit($testModel, [ref]$map, [ref]$view)
    Write-Host "  ✓ Explicit EntryPoint succeeded: RC=$result, Map=$map, View=$view"
} catch {
    Write-Host "  ⚠ Explicit failed: $($_.Exception.Message)"
}

Write-Host ""
Write-Host "Testing Attempt 4: Manual IntPtr marshaling..."
try {
    $pathPtr = [System.Runtime.InteropServices.Marshal]::StringToHGlobalUni($testModel)
    $map = [IntPtr]::Zero
    $view = [IntPtr]::Zero
    $result = [TestHotpatch]::RawrXD_LazyHotpatch_IntPtr($pathPtr, [ref]$map, [ref]$view)
    Write-Host "  ✓ Manual marshaling succeeded: RC=$result, Map=$map, View=$view"
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($pathPtr)
} catch {
    Write-Host "  ⚠ Manual marshaling failed: $($_.Exception.Message)"
}
