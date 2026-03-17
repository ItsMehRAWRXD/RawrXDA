#!/usr/bin/env pwsh
# Test fixed hotpatch DLL

$code = @"
using System;
using System.Runtime.InteropServices;

public static class TestFixed {
    [DllImport("LazyHotpatchWrapper_Fixed.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public static extern int RawrXD_LazyHotpatch(string modelPath, out IntPtr outMap, out IntPtr outView);
}
"@

Add-Type -TypeDefinition $code 2>$null

$testModel = "D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf"
Write-Host "Testing fixed DLL with model: $testModel"

[IntPtr]$map = [IntPtr]::Zero
[IntPtr]$view = [IntPtr]::Zero

try {
    $rc = [TestFixed]::RawrXD_LazyHotpatch($testModel, [ref]$map, [ref]$view)
    Write-Host "✓ SUCCESS!"
    Write-Host "  Return code: $rc"
    Write-Host "  Map handle: $map"
    Write-Host "  View pointer: $view"
    
    if ($rc -eq 0) {
        Write-Host "  ✓ Return code 0 = SUCCESS"
    } else {
        Write-Host "  ⚠ Return code $rc = FAILURE"
    }
} catch {
    Write-Host "✗ FAILED: $($_.Exception.Message)"
}
