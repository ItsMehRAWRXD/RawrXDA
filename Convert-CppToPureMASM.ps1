#Requires -Version 5.1
<#
.SYNOPSIS
    Full C++ to pure x64 MASM conversion — THE SIMPLE HAS TO LEAVE.

.DESCRIPTION
    Ensures the five IDE modules are fully converted to pure x64 MASM with
    no partial, scaffolded, or "in a production impl this would be *" code.
    Scans MASM (and optionally C++) for forbidden patterns and fails if any found.
    Can build MASM modules with ml64.

    Source → MASM mapping:
      unified_overclock_governor.cpp/.h → src\asm\RawrXD_UnifiedOverclock_Governor.asm
      codebase_audit_system_impl.cpp    → src\asm\RawrXD_CodebaseAuditSystem.asm
      quantum_beaconism_backend.h       → src\asm\RawrXD_DualEngine_QuantumBeacon.asm
      dual_engine_system.h              → src\asm\RawrXD_DualEngine_QuantumBeacon.asm

.PARAMETER Validate
    Scan MASM (and target C++) for placeholder/stub/production-deferral patterns; exit 1 if any found.

.PARAMETER Build
    Run ml64 on each MASM module (requires ML64 in PATH or -ML64Path).

.PARAMETER ML64Path
    Path to ml64.exe (e.g. VS InstallDir\VC\Tools\MSVC\...\x64\ml64.exe).

.PARAMETER RootPath
    Repository root (default: script directory).

.PARAMETER ReportOnly
    Print report only; do not exit 1 on issues.

.EXAMPLE
    .\Convert-CppToPureMASM.ps1 -Validate
    .\Convert-CppToPureMASM.ps1 -Validate -Build -ML64Path "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.xx\x64\ml64.exe"
#>

[CmdletBinding()]
param(
    [switch]$Validate,
    [switch]$Build,
    [string]$ML64Path = "",
    [string]$RootPath = $PSScriptRoot,
    [switch]$ReportOnly
)

$ErrorActionPreference = "Stop"

# Source → MASM mapping (relative to RootPath)
$SourceToMASM = @(
    @{ Cpp = "src\core\unified_overclock_governor.cpp"; H = "src\core\unified_overclock_governor.h"; ASM = "src\asm\RawrXD_UnifiedOverclock_Governor.asm" },
    @{ Cpp = "src\audit\codebase_audit_system_impl.cpp"; H = "src\audit\codebase_audit_system.hpp"; ASM = "src\asm\RawrXD_CodebaseAuditSystem.asm" },
    @{ Cpp = $null; H = "src\core\quantum_beaconism_backend.h"; ASM = "src\asm\RawrXD_DualEngine_QuantumBeacon.asm" },
    @{ Cpp = $null; H = "src\core\dual_engine_system.h"; ASM = "src\asm\RawrXD_DualEngine_QuantumBeacon.asm" }
)

# Forbidden patterns in MASM — THE SIMPLE HAS TO LEAVE
$ForbiddenPatterns = @(
    @{ Regex = '(?i)in\s+a\s+production\s+(?:impl|implementation|system|version)\s+(?:this\s+)?would\s+be'; Category = "ProductionDeferral" },
    @{ Regex = '(?i)placeholder\s+(?:for|return|implementation|store)'; Category = "Placeholder" },
    @{ Regex = '(?i);\s*Placeholder\s*[—\-:]'; Category = "PlaceholderComment" },
    @{ Regex = '(?i)mov\s+(?:eax|rax)\s*,\s*42\s*(?:;.*)?'; Category = "StubReturn" },
    @{ Regex = '(?i)scaffold|scaffolded|scaffolding'; Category = "Scaffold" },
    @{ Regex = '(?i)stub\s+(?:export|placeholder|implementation|for)\s'; Category = "Stub" },
    @{ Regex = '(?i)TODO:\s*Implement'; Category = "TODOImplement" },
    @{ Regex = '(?i)for\s+(?:brevity|now|simplicity)\s*,\s*(?:skip|use)'; Category = "MinimalBypass" },
    @{ Regex = '(?i)production\s+would\s+use'; Category = "ProductionDeferral" },
    @{ Regex = '(?i)delegates?\s+to\s+C\+\+\s+(?:fallback|scalar)'; Category = "CppFallback" },
    @{ Regex = '(?i)full\s+(?:implementation|version)\s+would'; Category = "PartialImpl" },
    @{ Regex = '(?i)Store\s+confidence\s+placeholder'; Category = "PlaceholderStore" },
    @{ Regex = '(?i)would\s+track\s+actual'; Category = "DeferredTracking" },
    @{ Regex = '(?i)simulate\s+.*\s*\(placeholder\)'; Category = "SimulatedPlaceholder" },
    @{ Regex = '(?i);\s*Step\s+\d+:\s*.*\s*\(stub\)'; Category = "StepStub" },
    @{ Regex = '(?i);\s*Stub\s*[—\-:]'; Category = "StubComment" }
)

function Get-ContentOrEmpty {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try { return [System.IO.File]::ReadAllText($Path) } catch { return $null }
}

function Find-ForbiddenInFile {
    param([string]$FilePath, [string]$Content, [string]$RelativePath)
    $issues = @()
    foreach ($pat in $ForbiddenPatterns) {
        $matches = [regex]::Matches($Content, $pat.Regex)
        foreach ($m in $matches) {
            $lineNum = 1
            $pos = 0
            $lines = $Content -split "`r?`n"
            foreach ($line in $lines) {
                if ($pos + $line.Length -ge $m.Index) { break }
                $pos += $line.Length + 1
                $lineNum++
            }
            $issues += [PSCustomObject]@{
                File     = $RelativePath
                Line     = $lineNum
                Category = $pat.Category
                Match    = $m.Value.Trim().Substring(0, [Math]::Min(100, $m.Value.Trim().Length))
            }
        }
    }
    return $issues
}

function Invoke-Validate {
    $allIssues = @()
    $asmScanned = @{}
    foreach ($entry in $SourceToMASM) {
        $asmPath = Join-Path $RootPath $entry.ASM
        if (-not $asmScanned[$asmPath]) {
            $asmScanned[$asmPath] = $true
            $rel = $entry.ASM
            $content = Get-ContentOrEmpty $asmPath
            if ($content) {
                $allIssues += Find-ForbiddenInFile -FilePath $asmPath -Content $content -RelativePath $rel
            }
        }
        if ($entry.Cpp) {
            $cppPath = Join-Path $RootPath $entry.Cpp
            $content = Get-ContentOrEmpty $cppPath
            if ($content) {
                $allIssues += Find-ForbiddenInFile -FilePath $cppPath -Content $content -RelativePath $entry.Cpp
            }
        }
        if ($entry.H) {
            $hPath = Join-Path $RootPath $entry.H
            $content = Get-ContentOrEmpty $hPath
            if ($content) {
                $allIssues += Find-ForbiddenInFile -FilePath $hPath -Content $content -RelativePath $entry.H
            }
        }
    }
    return $allIssues
}

function Write-Report {
    param([array]$Issues)
    $r = @"
================================================================================
RawrXD Pure x64 MASM Conversion — THE SIMPLE HAS TO LEAVE
Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Root: $RootPath
Issues: $($Issues.Count)
================================================================================

"@
    foreach ($i in ($Issues | Group-Object File)) {
        $r += "--- $($i.Name) ---`n"
        foreach ($j in $i.Group) {
            $r += "  L$($j.Line) [$($j.Category)] $($j.Match)`n"
        }
    }
    $r += "`n================================================================================`n"
    return $r
}

# Main
if ($Validate) {
    $issues = Invoke-Validate
    $report = Write-Report $issues
    Write-Host $report
    if ($issues.Count -gt 0 -and -not $ReportOnly) {
        Write-Host "FAIL: $($issues.Count) forbidden pattern(s). Fix MASM/C++ so the simple leaves." -ForegroundColor Red
        exit 1
    }
    if ($issues.Count -eq 0) {
        Write-Host "PASS: No placeholder/stub/production-deferral patterns in MASM or target sources." -ForegroundColor Green
    }
}

if ($Build) {
    $ml64 = "ml64.exe"
    if ($ML64Path) {
        $ml64 = $ML64Path
        if (-not (Test-Path -LiteralPath $ml64)) {
            Write-Host "ML64 not found: $ml64" -ForegroundColor Red
            exit 1
        }
    }
    $asmDir = Join-Path $RootPath "src\asm"
    $objDir = Join-Path $RootPath "build_ide\asm"
    $uniqueAsm = $SourceToMASM | ForEach-Object { $_.ASM } | Sort-Object -Unique
    foreach ($rel in $uniqueAsm) {
        $asmPath = Join-Path $RootPath $rel
        if (-not (Test-Path -LiteralPath $asmPath)) {
            Write-Host "Skip (missing): $rel" -ForegroundColor Yellow
            continue
        }
        $base = [System.IO.Path]::GetFileNameWithoutExtension($rel)
        $objPath = Join-Path $objDir "$base.obj"
        $dir = [System.IO.Path]::GetDirectoryName($objPath)
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        $asmFull = (Resolve-Path -LiteralPath $asmPath).Path
        Write-Host "Building $rel ..." -ForegroundColor Cyan
        & $ml64 /c /Zi /Fo $objPath $asmFull 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Build failed: $rel" -ForegroundColor Red
            exit 1
        }
    }
    Write-Host "MASM build complete." -ForegroundColor Green
}

if (-not ($Validate -or $Build)) {
    Write-Host "Usage: -Validate (scan for forbidden patterns), -Build (compile MASM with ml64)."
    Write-Host "Example: .\Convert-CppToPureMASM.ps1 -Validate -Build"
}
