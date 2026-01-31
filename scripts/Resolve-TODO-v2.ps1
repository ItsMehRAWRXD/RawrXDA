# ============================================================================
# RawrXD TODO Resolver v2.1 - Production with AVX-512 + Benchmarking + Rollback
# ============================================================================

#Requires -Version 7.0

param(
    [Parameter(Mandatory = $false)]
    [string]$SourcePath = "D:\lazy init ide\src",
    
    [Parameter(Mandatory = $false)]
    [string[]]$Extensions = @("*.ps1", "*.psm1", "*.cpp", "*.h", "*.asm", "*.cs", "*.py", "*.js", "*.ts"),
    
    [Parameter(Mandatory = $false)]
    [switch]$GenerateReport,
    
    [Parameter(Mandatory = $false)]
    [string]$ReportPath = "D:\lazy init ide\reports",
    
    [Parameter(Mandatory = $false)]
    [switch]$VerboseOutput,
    
    [Parameter(Mandatory = $false)]
    [switch]$Benchmark,
    
    [Parameter(Mandatory = $false)]
    [int]$BenchmarkIterations = 10000,
    
    [Parameter(Mandatory = $false)]
    [ValidateSet("BUG", "FIXME", "XXX", "TODO", "HACK", "REVIEW", "NOTE", "IDEA", "All")]
    [string]$FilterPattern = "All",
    
    [Parameter(Mandatory = $false)]
    [switch]$AutoFix,
    
    [Parameter(Mandatory = $false)]
    [switch]$CreateRollback,
    
    [Parameter(Mandatory = $false)]
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'

# ============================================================================
# Pattern Type Mapping
# ============================================================================
$PatternTypes = @{
    0 = @{ Name = "UNKNOWN"; Priority = 0; Color = "Gray"; Action = "Ignore" }
    1 = @{ Name = "TODO"; Priority = 1; Color = "Yellow"; Action = "Schedule" }
    2 = @{ Name = "FIXME"; Priority = 2; Color = "DarkYellow"; Action = "Plan Sprint" }
    3 = @{ Name = "XXX"; Priority = 2; Color = "Magenta"; Action = "Review" }
    4 = @{ Name = "HACK"; Priority = 1; Color = "DarkMagenta"; Action = "Refactor" }
    5 = @{ Name = "BUG"; Priority = 3; Color = "Red"; Action = "Create Issue" }
    6 = @{ Name = "NOTE"; Priority = 0; Color = "Cyan"; Action = "Document" }
    7 = @{ Name = "IDEA"; Priority = 0; Color = "Green"; Action = "Backlog" }
    8 = @{ Name = "REVIEW"; Priority = 1; Color = "Blue"; Action = "Code Review" }
}

# ============================================================================
# Load Native Bridge (AVX-512 SIMD version preferred)
# ============================================================================
$avxDllPath = "D:\lazy init ide\bin\RawrXD_AVX512_v2.dll"
$avxDllPathAlt = "D:\lazy init ide\bin\RawrXD_AVX512_SIMD.dll"
$scalarDllPath = "D:\lazy init ide\bin\RawrXD_PatternEngine_Real.dll"

# Prefer AVX-512 v2, then AVX-512, then scalar
if (Test-Path $avxDllPath) {
    $dllPath = $avxDllPath
    Write-Host "[Init] Using AVX-512 SIMD engine (v2)" -ForegroundColor Magenta
} elseif (Test-Path $avxDllPathAlt) {
    $dllPath = $avxDllPathAlt
    Write-Host "[Init] Using AVX-512 SIMD engine" -ForegroundColor Magenta
} elseif (Test-Path $scalarDllPath) {
    $dllPath = $scalarDllPath
    Write-Host "[Init] Using scalar engine (AVX-512 DLL not found)" -ForegroundColor Yellow
} else {
    throw "No pattern engine DLL found!"
}

$dllPathEscaped = $dllPath -replace '\\', '\\'

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class RawrXD_Engine
{
    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ClassifyPattern(IntPtr codeBuffer, int length, IntPtr context, out double confidence);

    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializePatternEngine();

    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ShutdownPatternEngine();

    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr GetPatternStats();
    
    [DllImport("$dllPathEscaped", CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetEngineInfo(IntPtr buffer);
}

// Engine info structure (must match ASM)
[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct EngineInfo
{
    public int Version;
    public int Mode;        // 1=scalar, 2=avx512
    public int Patterns;
    public int Reserved;
    public long TotalScans;
    public long TotalMatches;
    public long TotalBytes;
    public long TotalCycles;
    public long AvgNsPerOp;
    public long Padding;
}
"@

# ============================================================================
# Load Rollback Module (if available)
# ============================================================================
$rollbackModulePath = "D:\lazy init ide\scripts\TODO-RollbackSystem.psm1"
$script:RollbackAvailable = $false

if (Test-Path $rollbackModulePath) {
    try {
        Import-Module $rollbackModulePath -Force -ErrorAction Stop
        $script:RollbackAvailable = $true
        Write-Host "[Init] Rollback system loaded" -ForegroundColor Green
    }
    catch {
        Write-Warning "Failed to load rollback module: $_"
    }
}

# ============================================================================
# Helper Functions
# ============================================================================

function Get-EngineStats {
    # GetEngineInfo may not exist in all DLL versions
    try {
        $buffer = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(64)
        try {
            $result = [RawrXD_Engine]::GetEngineInfo($buffer)
            if ($result -eq 0) {
                $info = [System.Runtime.InteropServices.Marshal]::PtrToStructure($buffer, [Type][EngineInfo])
                return $info
            }
            return $null
        }
        finally {
            [System.Runtime.InteropServices.Marshal]::FreeHGlobal($buffer)
        }
    }
    catch {
        # GetEngineInfo not available
        return $null
    }
}

function Invoke-PatternClassification {
    param(
        [Parameter(Mandatory)]
        [string]$Text,
        
        [Parameter(Mandatory = $false)]
        [string]$Context = ""
    )
    
    $codeBytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $contextBytes = [System.Text.Encoding]::UTF8.GetBytes($Context)
    
    $codePtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($codeBytes.Length)
    $ctxPtr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([Math]::Max(1, $contextBytes.Length))
    
    try {
        [System.Runtime.InteropServices.Marshal]::Copy($codeBytes, 0, $codePtr, $codeBytes.Length)
        if ($contextBytes.Length -gt 0) {
            [System.Runtime.InteropServices.Marshal]::Copy($contextBytes, 0, $ctxPtr, $contextBytes.Length)
        }
        
        [double]$confidence = 0.0
        $type = [RawrXD_Engine]::ClassifyPattern($codePtr, $codeBytes.Length, $ctxPtr, [ref]$confidence)
        
        $patternInfo = $PatternTypes[$type]
        if (-not $patternInfo) { $patternInfo = $PatternTypes[0] }
        
        return @{
            Type = $type
            TypeName = $patternInfo.Name
            Priority = $patternInfo.Priority
            Confidence = $confidence
            Action = $patternInfo.Action
        }
    }
    finally {
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($codePtr)
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ctxPtr)
    }
}

function Scan-FileForPatterns {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath
    )
    
    $content = Get-Content -Path $FilePath -Raw -ErrorAction SilentlyContinue
    if (-not $content) {
        return @()
    }
    
    $patterns = @()
    $lines = $content -split "`n"
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        
        # Quick regex pre-filter (case-insensitive)
        if ($line -imatch '\b(TODO|FIXME|XXX|HACK|BUG|NOTE|IDEA|REVIEW)\b') {
            $result = Invoke-PatternClassification -Text $line -Context ([System.IO.Path]::GetExtension($FilePath))
            
            if ($result.Type -ne 0) {
                $patterns += [PSCustomObject]@{
                    File = $FilePath
                    Line = $i + 1
                    Type = $result.TypeName
                    Priority = $result.Priority
                    Confidence = [math]::Round($result.Confidence, 2)
                    Action = $result.Action
                    Content = $line.Trim()
                }
            }
        }
    }
    
    return $patterns
}

function Invoke-Benchmark {
    param([int]$Iterations)
    
    Write-Host ""
    Write-Host "=== Benchmark Mode ===" -ForegroundColor Cyan
    Write-Host "Iterations: $Iterations" -ForegroundColor Gray
    Write-Host ""
    
    # Test patterns
    $testCases = @(
        "// TODO: Fix this bug in the authentication module",
        "# FIXME: broken logic here needs refactoring",
        "/* BUG: crash on null pointer dereference */",
        "// NOTE: important implementation detail below",
        "// XXX: temporary workaround - needs proper fix",
        "This line has no pattern markers at all",
        "// HACK: quick fix for deadline - clean up later",
        "// IDEA: could optimize this with caching"
    )
    
    # Warm up
    Write-Host "[Warmup] Running 1000 warmup iterations..." -ForegroundColor Yellow
    foreach ($_ in 1..1000) {
        foreach ($test in $testCases) {
            $null = Invoke-PatternClassification -Text $test
        }
    }
    
    # Benchmark
    Write-Host "[Bench] Running $Iterations iterations..." -ForegroundColor Yellow
    
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $totalOps = 0
    
    foreach ($_ in 1..$Iterations) {
        foreach ($test in $testCases) {
            $null = Invoke-PatternClassification -Text $test
            $totalOps++
        }
    }
    
    $sw.Stop()
    
    # Get engine stats
    $stats = Get-EngineStats
    
    # Calculate metrics
    $elapsedMs = $sw.Elapsed.TotalMilliseconds
    $opsPerSec = [math]::Round($totalOps / ($elapsedMs / 1000), 0)
    $nsPerOp = [math]::Round(($elapsedMs * 1000000) / $totalOps, 2)
    
    Write-Host ""
    Write-Host "=== Benchmark Results ===" -ForegroundColor Green
    Write-Host ""
    
    if ($stats) {
        $modeStr = if ($stats.Mode -eq 2) { "AVX-512 SIMD" } else { "Scalar" }
        Write-Host "Engine Mode:    $modeStr" -ForegroundColor Cyan
        Write-Host "Total Scans:    $($stats.TotalScans)" -ForegroundColor White
        Write-Host "Total Matches:  $($stats.TotalMatches)" -ForegroundColor White
        Write-Host "Total Bytes:    $($stats.TotalBytes)" -ForegroundColor White
        Write-Host "Total Cycles:   $($stats.TotalCycles)" -ForegroundColor White
        Write-Host ""
    }
    
    Write-Host "PowerShell Timing:" -ForegroundColor Yellow
    Write-Host "  Total Operations: $totalOps" -ForegroundColor White
    Write-Host "  Elapsed Time:     $([math]::Round($elapsedMs, 2)) ms" -ForegroundColor White
    Write-Host "  Throughput:       $opsPerSec ops/sec" -ForegroundColor Green
    Write-Host "  Latency:          $nsPerOp ns/op" -ForegroundColor Green
    Write-Host ""
    
    # Compare to target
    $targetNs = 37785
    $speedup = [math]::Round($targetNs / $nsPerOp, 1)
    Write-Host "Performance vs Target (37,785 ns):" -ForegroundColor Yellow
    Write-Host "  Speedup: ${speedup}x faster" -ForegroundColor $(if ($speedup -gt 100) { "Green" } else { "Yellow" })
    Write-Host ""
    
    return @{
        TotalOps = $totalOps
        ElapsedMs = $elapsedMs
        OpsPerSec = $opsPerSec
        NsPerOp = $nsPerOp
        Speedup = $speedup
        EngineStats = $stats
    }
}

# ============================================================================
# Main Execution
# ============================================================================

Write-Host ""
Write-Host "=== RawrXD TODO Resolver v2.1 ===" -ForegroundColor Cyan
Write-Host "Source: $SourcePath" -ForegroundColor Gray
Write-Host "Extensions: $($Extensions -join ', ')" -ForegroundColor Gray
if ($FilterPattern -ne "All") {
    Write-Host "Filter: $FilterPattern" -ForegroundColor Gray
}
if ($AutoFix) {
    Write-Host "Mode: AUTO-FIX ENABLED" -ForegroundColor Red
}
Write-Host ""

# Initialize native engine
$initResult = [RawrXD_Engine]::InitializePatternEngine()
if ($initResult -ne 0) {
    throw "Failed to initialize pattern engine (error code: $initResult)"
}

# Check engine mode
$stats = Get-EngineStats
if ($stats) {
    $modeStr = if ($stats.Mode -eq 2) { "AVX-512 SIMD" } else { "Scalar" }
    Write-Host "[Init] Pattern engine initialized ($modeStr mode)" -ForegroundColor Green
} else {
    Write-Host "[Init] Pattern engine initialized" -ForegroundColor Green
}

# Run benchmark if requested
if ($Benchmark) {
    $benchResults = Invoke-Benchmark -Iterations $BenchmarkIterations
    
    # Cleanup
    [RawrXD_Engine]::ShutdownPatternEngine() | Out-Null
    Write-Host "=== Done ===" -ForegroundColor Green
    return $benchResults
}

# Collect files
$files = @()
foreach ($ext in $Extensions) {
    $files += Get-ChildItem -Path $SourcePath -Filter $ext -Recurse -File -ErrorAction SilentlyContinue
}

Write-Host "[Scan] Found $($files.Count) files to scan" -ForegroundColor Yellow
Write-Host ""

# Scan all files
$allPatterns = @()
$fileCount = 0
$sw = [System.Diagnostics.Stopwatch]::StartNew()

foreach ($file in $files) {
    $fileCount++
    
    if ($VerboseOutput) {
        Write-Progress -Activity "Scanning files" -Status "$fileCount / $($files.Count)" -PercentComplete (($fileCount / $files.Count) * 100)
    }
    
    $patterns = Scan-FileForPatterns -FilePath $file.FullName
    $allPatterns += $patterns
    
    if ($patterns.Count -gt 0 -and $VerboseOutput) {
        Write-Host "[+] $($file.Name): $($patterns.Count) pattern(s)" -ForegroundColor Green
    }
}

$sw.Stop()

if ($VerboseOutput) {
    Write-Progress -Activity "Scanning files" -Completed
}

# Display results
Write-Host "=== Scan Results ===" -ForegroundColor Cyan
Write-Host "Scan time: $([math]::Round($sw.Elapsed.TotalMilliseconds, 2)) ms" -ForegroundColor Gray
Write-Host ""

if ($allPatterns.Count -eq 0) {
    Write-Host "No patterns found!" -ForegroundColor Green
}
else {
    # Group by priority
    $critical = $allPatterns | Where-Object { $_.Priority -eq 3 }
    $high = $allPatterns | Where-Object { $_.Priority -eq 2 }
    $medium = $allPatterns | Where-Object { $_.Priority -eq 1 }
    $low = $allPatterns | Where-Object { $_.Priority -eq 0 }
    
    # Display critical first
    if ($critical.Count -gt 0) {
        Write-Host "CRITICAL ($($critical.Count)):" -ForegroundColor Red
        foreach ($p in $critical) {
            $relPath = $p.File -replace [regex]::Escape($SourcePath), ""
            Write-Host "  [$($p.Type)] $relPath`:$($p.Line)" -ForegroundColor Red
            Write-Host "    $($p.Content.Substring(0, [Math]::Min(80, $p.Content.Length)))" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    # High priority
    if ($high.Count -gt 0) {
        Write-Host "HIGH ($($high.Count)):" -ForegroundColor DarkYellow
        foreach ($p in $high | Select-Object -First 10) {
            $relPath = $p.File -replace [regex]::Escape($SourcePath), ""
            Write-Host "  [$($p.Type)] $relPath`:$($p.Line)" -ForegroundColor DarkYellow
        }
        if ($high.Count -gt 10) {
            Write-Host "  ... and $($high.Count - 10) more" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    # Medium priority
    if ($medium.Count -gt 0) {
        Write-Host "MEDIUM ($($medium.Count)):" -ForegroundColor Yellow
        foreach ($p in $medium | Select-Object -First 5) {
            $relPath = $p.File -replace [regex]::Escape($SourcePath), ""
            Write-Host "  [$($p.Type)] $relPath`:$($p.Line)" -ForegroundColor Yellow
        }
        if ($medium.Count -gt 5) {
            Write-Host "  ... and $($medium.Count - 5) more" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    # Summary
    Write-Host "=== Summary ===" -ForegroundColor Cyan
    Write-Host "Total patterns: $($allPatterns.Count)" -ForegroundColor White
    
    $groupedByType = $allPatterns | Group-Object -Property Type
    foreach ($group in $groupedByType | Sort-Object { -$PatternTypes[[int]($group.Name -replace '\D', '0')].Priority }) {
        $color = switch ($group.Name) {
            "BUG" { "Red" }
            "FIXME" { "DarkYellow" }
            "XXX" { "Magenta" }
            "TODO" { "Yellow" }
            "HACK" { "DarkMagenta" }
            "NOTE" { "Cyan" }
            "IDEA" { "Green" }
            "REVIEW" { "Blue" }
            default { "Gray" }
        }
        Write-Host "  $($group.Name): $($group.Count)" -ForegroundColor $color
    }
    
    # Filter by pattern if requested
    if ($FilterPattern -ne "All") {
        $allPatterns = $allPatterns | Where-Object { $_.Type -eq $FilterPattern }
        Write-Host ""
        Write-Host "[Filter] Filtered to $($allPatterns.Count) $FilterPattern pattern(s)" -ForegroundColor Yellow
    }
    
    # =========================================================================
    # Auto-Fix with Rollback Support
    # =========================================================================
    if ($AutoFix -and $allPatterns.Count -gt 0) {
        Write-Host ""
        Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Red
        Write-Host "║  ⚠️  AUTO-FIX MODE                                           ║" -ForegroundColor Red
        Write-Host "╠══════════════════════════════════════════════════════════════╣" -ForegroundColor Red
        Write-Host "║  This will modify $($allPatterns.Count.ToString().PadRight(5)) files with pattern fixes!              ║" -ForegroundColor Yellow
        Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Red
        
        $rollbackId = $null
        
        # Create rollback point
        if ($script:RollbackAvailable -and ($CreateRollback -or $true)) {
            $targetFiles = $allPatterns | Select-Object -ExpandProperty File -Unique
            $rollbackId = New-TODORollbackPoint -OperationName "TODO-AutoFix-$FilterPattern" `
                -TargetFiles $targetFiles `
                -Metadata @{
                    Pattern = $FilterPattern
                    Count = $allPatterns.Count
                    SourcePath = $SourcePath
                    Timestamp = (Get-Date).ToString("o")
                }
            Write-Host ""
            Write-Host "[Rollback] Created backup point: $rollbackId" -ForegroundColor Green
            Write-Host "           To undo: Restore-TODORollbackPoint -BackupId '$rollbackId'" -ForegroundColor Gray
        }
        elseif (-not $script:RollbackAvailable) {
            Write-Warning "Rollback module not available! Proceeding without backup."
        }
        
        if ($WhatIf) {
            Write-Host ""
            Write-Host "[WhatIf] Would process $($allPatterns.Count) patterns" -ForegroundColor Cyan
            foreach ($p in $allPatterns | Select-Object -First 10) {
                Write-Host "  - [$($p.Type)] $($p.File):$($p.Line)" -ForegroundColor Gray
            }
            if ($allPatterns.Count -gt 10) {
                Write-Host "  ... and $($allPatterns.Count - 10) more" -ForegroundColor Gray
            }
        }
        else {
            # Safety confirmation
            Write-Host ""
            $confirm = Read-Host "Type 'FIX' to proceed with auto-fix (or Enter to skip)"
            
            if ($confirm -eq "FIX") {
                $fixed = 0
                $skipped = 0
                
                foreach ($pattern in $allPatterns) {
                    # For now, mark as acknowledged (TODO: implement actual fixes)
                    # Future: call AI-powered fix generator here
                    Write-Host "  [ACK] $($pattern.Type) at $($pattern.File):$($pattern.Line)" -ForegroundColor Gray
                    $skipped++
                }
                
                Write-Host ""
                Write-Host "[AutoFix] Complete: $fixed fixed, $skipped acknowledged" -ForegroundColor Cyan
                if ($rollbackId) {
                    Write-Host "[Rollback] To undo: Restore-TODORollbackPoint -BackupId '$rollbackId'" -ForegroundColor Magenta
                }
            }
            else {
                Write-Host "[AutoFix] Skipped by user" -ForegroundColor Yellow
            }
        }
    }
}

# Generate report
if ($GenerateReport) {
    Write-Host ""
    Write-Host "[Report] Generating JSON report..." -ForegroundColor Yellow
    
    if (-not (Test-Path $ReportPath)) {
        New-Item -ItemType Directory -Path $ReportPath -Force | Out-Null
    }
    
    $reportFile = Join-Path $ReportPath "todo-scan-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
    
    $engineStats = Get-EngineStats
    
    $report = @{
        Timestamp = (Get-Date).ToString("o")
        SourcePath = $SourcePath
        ScanTimeMs = [math]::Round($sw.Elapsed.TotalMilliseconds, 2)
        TotalFiles = $files.Count
        TotalPatterns = $allPatterns.Count
        EngineMode = if ($engineStats -and $engineStats.Mode -eq 2) { "AVX-512" } else { "Scalar" }
        Patterns = $allPatterns | ForEach-Object {
            @{
                File = $_.File
                Line = $_.Line
                Type = $_.Type
                Priority = $_.Priority
                Confidence = $_.Confidence
                Action = $_.Action
                Content = $_.Content
            }
        }
        Statistics = @{
            Critical = ($allPatterns | Where-Object { $_.Priority -eq 3 }).Count
            High = ($allPatterns | Where-Object { $_.Priority -eq 2 }).Count
            Medium = ($allPatterns | Where-Object { $_.Priority -eq 1 }).Count
            Low = ($allPatterns | Where-Object { $_.Priority -eq 0 }).Count
        }
    }
    
    $report | ConvertTo-Json -Depth 10 | Out-File -FilePath $reportFile -Encoding UTF8
    Write-Host "[Report] Saved: $reportFile" -ForegroundColor Green
}

# Show engine stats
Write-Host ""
$finalStats = Get-EngineStats
if ($finalStats) {
    Write-Host "=== Engine Stats ===" -ForegroundColor Cyan
    Write-Host "Total scans:  $($finalStats.TotalScans)" -ForegroundColor Gray
    Write-Host "Total matches: $($finalStats.TotalMatches)" -ForegroundColor Gray
    Write-Host "Total bytes:  $($finalStats.TotalBytes)" -ForegroundColor Gray
}

# Cleanup
[RawrXD_Engine]::ShutdownPatternEngine() | Out-Null
Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Green
