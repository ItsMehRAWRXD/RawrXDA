<#
.SYNOPSIS
    Phase 1: RawrXD-SCC (Single-pass COFF64 Compiler) + MASM64 Bootstrap
.DESCRIPTION
    Compiles all .asm source files into COFF64 .obj files using ml64.exe.
    Parallel compilation with job-based scheduling.
    This is the first phase of the Genesis self-hosting pipeline.
    
    Canonical path: Once RawrXD-SCC can ingest its own source, this script
    invokes SCC instead of ml64. Until then, ml64 is the temporary scaffold.
.PARAMETER SourceDir
    Directory containing .asm source files (recursive search)
.PARAMETER OutputDir
    Directory for compiled .obj files
.PARAMETER UseSCC
    Use RawrXD-SCC instead of ml64.exe (self-hosting mode)
.EXAMPLE
    .\build_scc.ps1 -SourceDir "$env:LOCALAPPDATA\RawrXD\src" -UseSCC
#>
param(
    [string]$SourceDir = "$env:LOCALAPPDATA\RawrXD\src",
    [string]$OutputDir = "$env:LOCALAPPDATA\RawrXD\obj",
    [switch]$UseSCC
)

$ErrorActionPreference = "Stop"
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

function Write-Phase1Log {
    param([string]$Message, [string]$Level = "Info")
    $ts = Get-Date -Format "HH:mm:ss.fff"
    $colorMap = @{ Info="White"; Success="Green"; Warning="Yellow"; Error="Red" }
    Write-Host "[$ts] [PHASE1::SCC] $Message" -ForegroundColor $colorMap[$Level]
}

# ═══════════════════════════════════════════════════════════════
# Assembler Resolution (SCC → ml64 fallback chain)
# ═══════════════════════════════════════════════════════════════
$assembler = $null

if ($UseSCC) {
    $sccPath = "$env:LOCALAPPDATA\RawrXD\bin\rawrxd_scc.exe"
    if (Test-Path $sccPath) {
        $assembler = $sccPath
        Write-Phase1Log "Self-hosting: using RawrXD-SCC at $sccPath" "Success"
    } else {
        Write-Phase1Log "RawrXD-SCC not found at $sccPath, falling back to ml64" "Warning"
    }
}

if (!$assembler) {
    # ml64 resolution chain
    $ml64Candidates = @(
        "ml64.exe",                                                                                                # PATH
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\ml64.exe"
    )
    
    foreach ($candidate in $ml64Candidates) {
        if ($candidate -eq "ml64.exe") {
            $found = Get-Command $candidate -ErrorAction SilentlyContinue
            if ($found) { $assembler = $found.Source; break }
        } elseif (Test-Path $candidate) {
            $assembler = $candidate
            break
        }
    }
    
    if (!$assembler) {
        throw "FATAL: No assembler found. Install VS Build Tools or build RawrXD-SCC first."
    }
    Write-Phase1Log "Using ml64 scaffold: $assembler" "Info"
}

# ═══════════════════════════════════════════════════════════════
# Source Discovery
# ═══════════════════════════════════════════════════════════════
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

if (!(Test-Path $SourceDir)) {
    Write-Phase1Log "Source directory $SourceDir does not exist. Creating stub." "Warning"
    New-Item -ItemType Directory -Path $SourceDir -Force | Out-Null
    Write-Phase1Log "Place .asm files in $SourceDir and re-run." "Warning"
    return
}

$sources = Get-ChildItem -Path $SourceDir -Filter "*.asm" -Recurse | Where-Object { $_.Name -notmatch "_test\.asm$" }

if ($sources.Count -eq 0) {
    Write-Phase1Log "No .asm source files found in $SourceDir" "Warning"
    return
}

Write-Phase1Log "Found $($sources.Count) assembly source files" "Info"

# ═══════════════════════════════════════════════════════════════
# Parallel Compilation
# ═══════════════════════════════════════════════════════════════
$maxJobs = [Environment]::ProcessorCount
$compiled = 0
$failed = 0
$jobs = @()

foreach ($src in $sources) {
    $obj = Join-Path $OutputDir ($src.BaseName + ".obj")
    $asmPath = $src.FullName
    $asmExe = $assembler
    
    $job = Start-Job -ScriptBlock {
        param($asmExe, $asmPath, $obj, $srcName)
        try {
            $output = & $asmExe /c /Fo"$obj" /W3 /Zi /Zd "$asmPath" 2>&1
            if ($LASTEXITCODE -ne 0) {
                return @{ Success=$false; Name=$srcName; Output=($output -join "`n") }
            }
            return @{ Success=$true; Name=$srcName; Size=(Get-Item $obj -ErrorAction SilentlyContinue).Length }
        } catch {
            return @{ Success=$false; Name=$srcName; Output=$_.Exception.Message }
        }
    } -ArgumentList $asmExe, $asmPath, $obj, $src.Name
    
    $jobs += $job
    
    # Throttle to $maxJobs concurrent
    while (($jobs | Where-Object { $_.State -eq 'Running' }).Count -ge $maxJobs) {
        Start-Sleep -Milliseconds 100
    }
}

# Collect results
$jobs | Wait-Job | ForEach-Object {
    $result = Receive-Job $_
    if ($result.Success) {
        $compiled++
        $sizeKB = if ($result.Size) { [math]::Round($result.Size / 1KB, 1) } else { "?" }
        Write-Phase1Log "  OK: $($result.Name) ($sizeKB KB)" "Success"
    } else {
        $failed++
        Write-Phase1Log "  FAIL: $($result.Name): $($result.Output)" "Error"
    }
}
$jobs | Remove-Job -Force

$stopwatch.Stop()

# ═══════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════
$totalObj = (Get-ChildItem -Path $OutputDir -Filter "*.obj" -ErrorAction SilentlyContinue).Count
$totalSizeKB = [math]::Round(((Get-ChildItem -Path $OutputDir -Filter "*.obj" -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1KB), 1)

Write-Host ""
Write-Phase1Log "Phase 1 Complete" "Success"
Write-Phase1Log "  Compiled:   $compiled / $($sources.Count)" $(if ($failed -gt 0) { "Warning" } else { "Success" })
Write-Phase1Log "  Failed:     $failed" $(if ($failed -gt 0) { "Error" } else { "Success" })
Write-Phase1Log "  Objects:    $totalObj files ($totalSizeKB KB)" "Info"
Write-Phase1Log "  Elapsed:    $($stopwatch.Elapsed.ToString('mm\:ss\.fff'))" "Info"
Write-Phase1Log "  Output:     $OutputDir" "Info"
Write-Host ""

if ($failed -gt 0) {
    throw "Phase 1 failed: $failed compilation errors"
}
