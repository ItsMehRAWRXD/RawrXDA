param(
    [string]$DllPath = "D:\rawrxd\build-sovereign\Release\RawrXD_Sovereign.dll"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $DllPath)) {
    throw "DLL not found: $DllPath"
}

$exports = @(
    'SovereignEnhancements_InitializeAll',
    'SovereignEnhancements_InferenceStep',
    'TieredOrchestrator_Initialize',
    'TieredOrchestrator_MigratePage',
    'DynamicQuant_Initialize',
    'DynamicQuant_HotPatchLevel',
    'DynamicQuant_AdaptiveLayer',
    'SpeculativeDecoding_Initialize',
    'SpeculativeDecoding_GenerateDrafts',
    'SpeculativeDecoding_VerifyDrafts',
    'KVCacheCompression_Initialize',
    'KVCacheCompression_UpdateScores',
    'KVCacheCompression_CompressTier',
    'ContinuousBatching_Initialize',
    'ContinuousBatching_ScheduleRequest',
    'ContinuousBatching_Step',
    'PagedKV_AllocatePages',
    'MoE_Initialize',
    'MoE_RouteTokens',
    'MoE_LoadBalance',
    'MoE_SparseActivate',
    'AsyncStreamingLoader_Initialize',
    'AsyncStreamingLoader_BeginStream',
    'AsyncIO_WorkerThread',
    'AsyncStreamingLoader_Cancel',
    'ThermalAwareThrottling_Initialize',
    'ThermalMonitor_Thread',
    'ThermalAwareThrottling_GetCurrentTarget'
)

$interopCode = @"
using System;
using System.Runtime.InteropServices;

public static class Native {
    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32", SetLastError=true)]
    public static extern bool FreeLibrary(IntPtr hModule);
}
"@

Add-Type -TypeDefinition $interopCode -Language CSharp

$h = [Native]::LoadLibrary($DllPath)
if ($h -eq [IntPtr]::Zero) {
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "LoadLibrary failed ($err): $DllPath"
}

$missing = @()
$resolved = @()

try {
    foreach ($name in $exports) {
        $p = [Native]::GetProcAddress($h, $name)
        if ($p -eq [IntPtr]::Zero) {
            $missing += $name
        } else {
            $resolved += [pscustomobject]@{ Export = $name; Address = ('0x{0:X}' -f $p.ToInt64()) }
        }
    }
}
finally {
    [void][Native]::FreeLibrary($h)
}

Write-Host "Sovereign export smoke test"
Write-Host "DLL: $DllPath"
Write-Host "Expected exports: $($exports.Count)"
Write-Host "Resolved exports: $($resolved.Count)"
Write-Host "Missing exports:  $($missing.Count)"

if ($resolved.Count -gt 0) {
    Write-Host ""
    Write-Host "Sample resolved addresses:"
    $resolved | Select-Object -First 6 | Format-Table -AutoSize | Out-String | Write-Host
}

if ($missing.Count -gt 0) {
    Write-Host "Missing names:" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}

Write-Host "All sovereign exports resolved." -ForegroundColor Green
