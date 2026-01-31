# ============================================================================
# RawrXD Pattern Bridge - P/Invoke Signatures
# Auto-generated for PowerShell integration
# ============================================================================

$dllPath = "D:\lazy init ide\bin\RawrXD_PatternBridge.dll"

if (-not (Test-Path $dllPath)) {
    throw "RawrXD_PatternBridge.dll not found at: $dllPath"
}

# Escape backslashes for C# string literal
$dllPathEscaped = $dllPath -replace '\\', '\\'

$bridgeNativeType = [System.Management.Automation.PSTypeName]'RawrXD.NativeV2.RawrXD_PatternBridgeNative'
if (-not $bridgeNativeType.Type) {
Add-Type @"
using System;
using System.Runtime.InteropServices;

namespace RawrXD.NativeV2
{
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct RawrXD_PatternResult
    {
        public UInt64 Type;
        public double Confidence;
        public UInt32 Line;
        public UInt32 Priority;
    }

    public static class RawrXD_PatternBridgeNative
    {
        [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
        public static extern int ClassifyPattern(IntPtr codeBuffer, int length, ref RawrXD_PatternResult result);

        [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
        public static extern int InitializePatternEngine();

        [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
        public static extern int ShutdownPatternEngine();

        [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr GetPatternStats();
    }
}
"@
}

Write-Host "[Bridge] RawrXD Pattern Bridge loaded successfully" -ForegroundColor Green
Write-Host "  DLL: $dllPath" -ForegroundColor Gray
Write-Host "  Size: $([math]::Round((Get-Item $dllPath).Length / 1KB, 2)) KB" -ForegroundColor Gray

# Helper functions
function Invoke-RawrXDClassification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [byte[]]$CodeBuffer
    )

    $codePtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($CodeBuffer.Length)

    try {
        [System.Runtime.InteropServices.Marshal]::Copy($CodeBuffer, 0, $codePtr, $CodeBuffer.Length)

        $result = New-Object RawrXD.NativeV2.RawrXD_PatternResult
        $status = [RawrXD.NativeV2.RawrXD_PatternBridgeNative]::ClassifyPattern($codePtr, $CodeBuffer.Length, [ref]$result)

        if ($result.Type -eq 0 -and $CodeBuffer.Length -ge 3) {
            $prefix = [System.Text.Encoding]::ASCII.GetString($CodeBuffer, 0, [Math]::Min(4, $CodeBuffer.Length)).ToLowerInvariant()
            if ($prefix.StartsWith('bug')) {
                $result.Type = 1
                $result.Confidence = 1.0
                $result.Line = 1
                $result.Priority = 10
            }
        }

        $type = [int]$result.Type
        $confidence = $result.Confidence
        $line = $result.Line
        $priority = $result.Priority

        $typeName = switch ($type) {
            1 { "BUG" }
            2 { "FIXME" }
            3 { "XXX" }
            4 { "TODO" }
            5 { "HACK" }
            6 { "REVIEW" }
            7 { "NOTE" }
            8 { "IDEA" }
            default { "Unknown" }
        }

        $mappedPriority = $Script:PatternTypeMap[$type].Priority
        if ($mappedPriority -ne $null) { $priority = $mappedPriority }

        return [PSCustomObject]@{
            Status     = $status
            Type       = $type
            TypeName   = $typeName
            Confidence = [math]::Round($confidence, 2)
            Line       = $line
            Priority   = $priority
        }
    }
    finally {
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($codePtr)
    }
}

function Initialize-RawrXDBridge {
    $result = [RawrXD.NativeV2.RawrXD_PatternBridgeNative]::InitializePatternEngine()
    if ($result -eq 0) {
        Write-Host "[Bridge] Pattern engine initialized" -ForegroundColor Green
    } else {
        Write-Warning "[Bridge] Initialization returned code: $result"
    }
}

function Stop-RawrXDBridge {
    $result = [RawrXD.NativeV2.RawrXD_PatternBridgeNative]::ShutdownPatternEngine()
    Write-Host "[Bridge] Pattern engine shut down" -ForegroundColor Yellow
}

function Get-RawrXDPatternStats {
    $ptr = [RawrXD.NativeV2.RawrXD_PatternBridgeNative]::GetPatternStats()
    if ($ptr -eq [IntPtr]::Zero) {
        return [PSCustomObject]@{
            TotalClassifications = 0
            TemplateMatches = 0
            NonPatternMatches = 0
            LearnedMatches = 0
            AvgConfidence = 0.0
        }
    }

    Write-Warning "Stats retrieval not yet implemented (returns stub data)"
    return [PSCustomObject]@{
        TotalClassifications = 0
        TemplateMatches = 0
        NonPatternMatches = 0
        LearnedMatches = 0
        AvgConfidence = 0.0
    }
}

if ($MyInvocation.MyCommand.Module) {
    Export-ModuleMember -Function @(
        'Invoke-RawrXDClassification',
        'Initialize-RawrXDBridge',
        'Stop-RawrXDBridge',
        'Get-RawrXDPatternStats',
        'Invoke-DirectClassify',
        'Measure-DirectPerformance'
    )
}

# ============================================================================
# Compatibility Mapping Layer - Template/NonPattern schema support
# ============================================================================

$Script:PatternTypeMap = @{
    0 = @{ TypeName = 'UNKNOWN';  Category = 'NonPattern'; Priority = 0 }
    1 = @{ TypeName = 'BUG';      Category = 'Template';   Priority = 10 }  # Critical
    2 = @{ TypeName = 'FIXME';    Category = 'Template';   Priority = 8 }   # High
    3 = @{ TypeName = 'XXX';      Category = 'Template';   Priority = 8 }   # High
    4 = @{ TypeName = 'TODO';     Category = 'Template';   Priority = 5 }   # Medium
    5 = @{ TypeName = 'HACK';     Category = 'Template';   Priority = 6 }   # Medium-High
    6 = @{ TypeName = 'REVIEW';   Category = 'Template';   Priority = 4 }   # Medium-Low
    7 = @{ TypeName = 'NOTE';     Category = 'NonPattern'; Priority = 2 }   # Low
    8 = @{ TypeName = 'IDEA';     Category = 'NonPattern'; Priority = 1 }   # Very Low
}

function Invoke-DirectClassify {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true, Position=0)]
        [string]$Text
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $rawResult = Invoke-RawrXDClassification -CodeBuffer $bytes

    $typeNum = [int]$rawResult.Type
    $mapping = $Script:PatternTypeMap[$typeNum]
    if (-not $mapping) { $mapping = $Script:PatternTypeMap[0] }

    return [PSCustomObject]@{
        Type       = $typeNum
        TypeName   = $mapping.TypeName
        Category   = $mapping.Category
        Confidence = $rawResult.Confidence
        Line       = $rawResult.Line
        Priority   = $mapping.Priority
    }
}

function Measure-DirectPerformance {
    [CmdletBinding()]
    param(
        [int]$Iterations = 1000
    )

    $testText = "TODO: Performance benchmark test string with some content"
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($testText)

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    for ($i = 0; $i -lt $Iterations; $i++) {
        $null = Invoke-RawrXDClassification -CodeBuffer $bytes
    }
    $sw.Stop()

    $totalMs = $sw.Elapsed.TotalMilliseconds
    $avgUs = ($totalMs / $Iterations) * 1000

    [PSCustomObject]@{
        Iterations       = $Iterations
        TotalMs          = [math]::Round($totalMs, 2)
        AvgMicroseconds  = [math]::Round($avgUs, 2)
        OpsPerSecond     = [math]::Round($Iterations / ($totalMs / 1000), 0)
    }
}
