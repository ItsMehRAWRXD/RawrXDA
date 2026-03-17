#!/usr/bin/env pwsh

Write-Host "Testing LazyHotpatchWrapper.dll..."
Write-Host "Current directory: $(Get-Location)"

$dllPath = Join-Path (Get-Location) "build\LazyHotpatchWrapper.dll"
Write-Host "DLL full path: $dllPath"
Write-Host "DLL exists: $(Test-Path $dllPath)"

# Add build directory to PATH so DLL can be found
$env:PATH = "$(Get-Location)\build;$env:PATH"
Write-Host "Updated PATH to include build directory"
Write-Host ""

# Create proper P/Invoke wrapper - use ABSOLUTE path for DllImport
$dllPathEscaped = $dllPath -replace '\\', '\\\\'
$csharpSignature = @"
using System;
using System.Runtime.InteropServices;

public static class HotpatchDLL {
    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public static extern int RawrXD_LazyHotpatch(string modelPath, out IntPtr outMap, out IntPtr outView);
}
"@

Add-Type -TypeDefinition $csharpSignature -ErrorAction Stop

$testFile = "D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf"
Write-Host "Test file: $testFile"
Write-Host "Test file exists: $(Test-Path $testFile)"

Write-Host ""
Write-Host "Calling RawrXD_LazyHotpatch..."

[IntPtr] $map = [IntPtr]::Zero
[IntPtr] $view = [IntPtr]::Zero

try {
    $returnCode = [HotpatchDLL]::RawrXD_LazyHotpatch($testFile, [ref]$map, [ref]$view)
    
    Write-Host "✓ Call succeeded!"
    Write-Host "  Return Code: $returnCode"
    Write-Host "  Map handle: $map (0x$($map.ToString('X')))"
    Write-Host "  View ptr: $view (0x$($view.ToString('X')))"
    
    if ($returnCode -eq 0 -and $map -ne 0 -and $view -ne 0) {
        Write-Host ""
        Write-Host "✅ HOTPATCH WORKING! File opened, mapped, and view created successfully."
    } else {
        Write-Host ""
        Write-Host "⚠ Return code indicates failure or incomplete operation"
    }
} catch {
    Write-Host "✗ Call failed: $($_.Exception.Message)"
    $_.Exception.InnerException | ForEach-Object { Write-Host "  Inner: $_" }
}
