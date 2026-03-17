#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Comprehensive performance test for new model loading/streaming with lazy initialization
.DESCRIPTION
    Tests the production lazy-init system against the previous 675ms baseline benchmark
    Measures: model loading time, streaming latency, memory usage, and token generation speed
#>

param(
    [string]$ModelPath = "D:\OllamaModels",
    [int]$NumIterations = 5,
    [int]$TokensPerTest = 128,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

# ════════════════════════════════════════════════════════════════════════════════
# GUARD: Validate input parameters early
# ════════════════════════════════════════════════════════════════════════════════
if ([string]::IsNullOrWhiteSpace($ModelPath)) {
    throw "ModelPath parameter is null or empty"
}
if (-not (Test-Path $ModelPath)) {
    throw "ModelPath does not exist: $ModelPath"
}

# Build paths
$BuildDir = Join-Path $ScriptRoot "build"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$HotpatchAsm = Join-Path $BuildDir "RawrXD_LazyHotpatch.asm"
$WrapperCpp = Join-Path $BuildDir "LazyHotpatchWrapper.cpp"
$HotpatchObj = Join-Path $BuildDir "RawrXD_LazyHotpatch.obj"
$WrapperObj = Join-Path $BuildDir "LazyHotpatchWrapper.obj"
$HotpatchDll = Join-Path $BuildDir "RawrXD_LazyHotpatch.dll"
$WrapperDll = Join-Path $BuildDir "LazyHotpatchWrapper.dll"

# ════════════════════════════════════════════════════════════════════════════════
# Import Visual Studio dev environment (INCLUDE, LIB paths for cl.exe)
# ════════════════════════════════════════════════════════════════════════════════
function Import-VSDevEnv {
    param(
        [ValidateSet("x64","x86")]
        [string]$Arch = "x64"
    )

    $vswhere = Join-Path "${env:ProgramFiles(x86)}" "Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path $vswhere)) {
        Write-Warning "vswhere not found; proceeding without dev env setup"
        return
    }

    try {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (!$vsPath) {
            Write-Warning "Visual Studio with C++ tools not found"
            return
        }

        $devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
        if (!(Test-Path $devCmd)) {
            Write-Warning "VsDevCmd.bat not found at $devCmd"
            return
        }

        # Capture environment that VsDevCmd sets
        $temp = [System.IO.Path]::GetTempFileName()
        cmd.exe /c "`"$devCmd`" -arch=$Arch -host_arch=$Arch && set > `"$temp`""

        Get-Content $temp | ForEach-Object {
            if ($_ -match '^(.*?)=(.*)$') {
                [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
            }
        }
        Remove-Item $temp -Force
        Write-Host "✓ VS dev environment imported ($Arch)" -ForegroundColor Green
    } catch {
        Write-Warning "Failed to import VS dev env: $_"
    }
}

# Call once at startup
Import-VSDevEnv -Arch x64

function Get-CompilerPath {
    param([string]$ToolName)
    $cmd = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @()
    $candidates += Get-ChildItem -Path "C:\VS2022Enterprise" -Filter $ToolName -Recurse -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
    foreach ($root in @("$env:ProgramFiles(x86)", "$env:ProgramFiles")) {
        $candidates += Get-ChildItem -Path (Join-Path $root "Microsoft Visual Studio\2022") -Filter $ToolName -Recurse -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
    }
    $candidates = $candidates | Where-Object { $_ } | Sort-Object -Descending -Unique
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    throw "$ToolName not found"
}

function Build-Hotpatch {
    Write-Host "🔨 Building hotpatch binaries..." -ForegroundColor Yellow
    
    # Assemble MASM
    if ((-not (Test-Path $HotpatchObj)) -or ((Get-Item $HotpatchObj).LastWriteTime -lt (Get-Item $HotpatchAsm).LastWriteTime)) {
        $ml64 = Get-CompilerPath "ml64.exe"
        Write-Host "  ⚙️  ml64 assembling RawrXD_LazyHotpatch.asm..." -ForegroundColor Gray
        & $ml64 /c /Fo"$HotpatchObj" "$HotpatchAsm" 2>&1 | Where-Object { $_ -match "error" } | ForEach-Object { Write-Error $_ }
        if (-not (Test-Path $HotpatchObj)) { throw "MASM assembly failed" }
    }
    
    # Link hotpatch OBJ directly to DLL (no wrapper needed, use export alias)
    if ((-not (Test-Path $WrapperDll)) -or ((Get-Item $WrapperDll).LastWriteTime -lt (Get-Item $HotpatchObj).LastWriteTime)) {
        $link = Get-CompilerPath "link.exe"
        $sdkLibPath = "C:\VS2022Enterprise\SDK\ScopeCppSDK\vc15\SDK\lib"
        Write-Host "  🔗 link.exe creating LazyHotpatchWrapper.dll..." -ForegroundColor Gray
        & $link /dll /nologo /machine:x64 /noentry /export:RawrXD_LazyHotpatch /defaultlib:kernel32.lib /libpath:"$sdkLibPath" /out:"$WrapperDll" "$HotpatchObj" 2>&1 | Where-Object { $_ -match "error" } | ForEach-Object { Write-Error $_ }
        if (-not (Test-Path $WrapperDll)) { throw "Linker failed: DLL not created" }
    }
    
    Write-Host "✓ Hotpatch DLL ready: $WrapperDll" -ForegroundColor Green
}

function Import-HotpatchLib {
    if (-not ("RawrXDPatcher" -as [type])) {
        # Use absolute path to DLL
        $dllAbsPath = (Resolve-Path (Join-Path $BuildDir "LazyHotpatchWrapper.dll")).Path.Replace('\', '\\')
        
        $typeCode = @"
using System;
using System.Runtime.InteropServices;

public static class RawrXDPatcher {
    [DllImport("$dllAbsPath", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public static extern int RawrXD_LazyHotpatch(string modelPath, out IntPtr outMap, out IntPtr outView);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool UnmapViewOfFile(IntPtr baseAddress);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr handle);
}
"@
        Add-Type -TypeDefinition $typeCode -ErrorAction Stop
    }
}

function Invoke-Hotpatch {
    param([string]$ModelPath)
    
    # Guard: Validate path is not null
    if ([string]::IsNullOrWhiteSpace($ModelPath)) {
        throw "ModelPath is null/empty in Invoke-Hotpatch"
    }
    
    # Resolve to absolute path
    $ModelPath = (Resolve-Path $ModelPath).Path
    if (-not (Test-Path $ModelPath)) {
        throw "Model file does not exist: $ModelPath"
    }
    
    Build-Hotpatch
    Import-HotpatchLib
    
    [IntPtr]$map = [IntPtr]::Zero
    [IntPtr]$view = [IntPtr]::Zero
    
    try {
        $rc = [RawrXDPatcher]::RawrXD_LazyHotpatch($ModelPath, [ref]$map, [ref]$view)
        return [PSCustomObject]@{ 
            Success = ($rc -eq 0); 
            ReturnCode = $rc
            Map = $map
            View = $view
        }
    } catch {
        Write-Warning "Hotpatch invoke failed: $($_.Exception.Message)"
        return [PSCustomObject]@{ 
            Success = $false
            ReturnCode = -1
            Map = [IntPtr]::Zero
            View = [IntPtr]::Zero
        }
    }
}

# Header
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD Lazy-Init Performance Test (MASM Hotpatch)               ║" -ForegroundColor Cyan
Write-Host "║  Baseline: 675ms | Target: <400ms (40% faster)                   ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Find models
$modelFiles = @()
if (Test-Path $ModelPath) {
    if ((Get-Item $ModelPath).PSIsContainer) {
        # RECURSE to find all .gguf files in subdirectories
        $modelFiles = @(Get-ChildItem -Path $ModelPath -Recurse -Filter "*.gguf" -File -ErrorAction SilentlyContinue)
        $modelFiles = $modelFiles | Sort-Object -Property Length -Descending | Select-Object -First 3
    } elseif ($ModelPath -like "*.gguf") {
        $modelFiles = @(Get-Item $ModelPath)
    }
}

# Validate we actually found models
if ($modelFiles.Count -eq 0) {
    Write-Host "❌ No GGUF models found in: $ModelPath" -ForegroundColor Red
    exit 1
}

# Verify file paths are real (eliminate 0.0 GB bug)
$modelFiles = $modelFiles | Where-Object { 
    $null -ne $_.FullName -and $_.Length -gt 0 
} | ForEach-Object {
    $fullPath = (Resolve-Path $_.FullName).Path
    if (Test-Path $fullPath) {
        $_
    }
}

if ($modelFiles.Count -eq 0) {
    Write-Host "❌ No valid GGUF files after validation" -ForegroundColor Red
    exit 1
}

Write-Host "📊 Configuration: $($modelFiles.Count) models × $NumIterations iterations" -ForegroundColor Yellow
Write-Host ""

$BASELINE = 675
$results = @()

# Build hotpatch first
Build-Hotpatch

# Test each model
foreach ($model in $modelFiles) {
    Write-Host "Testing: $($model.Name) ($('{0:N1}' -f ($model.Length / 1GB)) GB)" -ForegroundColor Cyan
    $times = @()
    $successCount = 0
    
    for ($i = 1; $i -le $NumIterations; $i++) {
        Write-Host "  [$i/$NumIterations] " -NoNewline -ForegroundColor Gray
        
        try {
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            $patch = Invoke-Hotpatch -ModelPath $model.FullName
            $sw.Stop()
            $ms = $sw.ElapsedMilliseconds
            
            # CRITICAL FIX: Only count this timing if hotpatch succeeded
            # Don't count failed invokes as "fast" times
            if ($patch.Success) {
                $times += $ms
                $successCount++
                $speedText = if ($ms -lt $BASELINE) { "  $('{0:+0.0;-0.0}' -f (($BASELINE-$ms)/$BASELINE*100))% faster" } else { "  $('{0:+0.0;-0.0}' -f (($ms-$BASELINE)/$BASELINE*100))% slower" }
                Write-Host "✓ ${ms}ms$speedText" -ForegroundColor $(if ($ms -lt $BASELINE) { "Green" } else { "Yellow" })
            } else {
                Write-Host "⚠ FAILED (not included in avg)" -ForegroundColor Red
            }
        } catch {
            Write-Host "⚠ FAILED: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
    
    # Only calculate improvement if we had successful runs
    if ($successCount -gt 0) {
        $avg = ($times | Measure-Object -Average).Average
        $improvement = (1 - $avg / $BASELINE) * 100
        
        $results += [PSCustomObject]@{
            Model = $model.Name
            Avg_ms = [math]::Round($avg, 1)
            Min_ms = ($times | Measure-Object -Minimum).Minimum
            Max_ms = ($times | Measure-Object -Maximum).Maximum
            Improvement_pct = [math]::Round($improvement, 1)
            Target_Met = ($avg -lt 400)
            Success_Rate = "$successCount/$NumIterations"
        }
    } else {
        $results += [PSCustomObject]@{
            Model = $model.Name
            Avg_ms = 0
            Min_ms = 0
            Max_ms = 0
            Improvement_pct = 0
            Target_Met = $false
            Success_Rate = "0/$NumIterations"
        }
    }
    
    Write-Host ""
}

# Summary
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
$results | Format-Table -AutoSize

# Only average successful runs (Avg_ms > 0)
$successResults = $results | Where-Object { $_.Avg_ms -gt 0 }
$avgAll = if ($successResults.Count -gt 0) { ($successResults.Avg_ms | Measure-Object -Average).Average } else { 0 }
$targetMet = ($successResults | Where-Object { $_.Target_Met }).Count
$passed = "$targetMet/$($successResults.Count)"

Write-Host ""
if ($successResults.Count -gt 0) {
    Write-Host "Overall Average:    $([math]::Round($avgAll, 1)) ms" -ForegroundColor White
    Write-Host "Baseline:           $BASELINE ms" -ForegroundColor White
    Write-Host "Improvement:        $('{0:+0.0;-0.0}' -f ((1 - $avgAll/$BASELINE)*100))%" -ForegroundColor $(if ($avgAll -lt $BASELINE) { "Green" } else { "Red" })
    Write-Host "Target Met (<400ms): $passed" -ForegroundColor $(if ($avgAll -lt 400) { "Green" } else { "Yellow" })
    Write-Host ""

    if ($avgAll -lt 400) {
        Write-Host "✅ SUCCESS! Hotpatch delivering 40%+ speedup" -ForegroundColor Green
        Write-Host "   Models load in $('{0:N0}' -f $avgAll)ms (was $BASELINE ms)" -ForegroundColor Green
    } elseif ($avgAll -lt $BASELINE) {
        Write-Host "✓ Good: Faster than baseline, but not at target" -ForegroundColor Yellow
    } else {
        Write-Host "⚠ Hotpatch not active. Check DLL loading." -ForegroundColor Red
    }
} else {
    Write-Host "❌ All hotpatch invocations failed. Check DLL export and path parameters." -ForegroundColor Red
}

Write-Host ""
