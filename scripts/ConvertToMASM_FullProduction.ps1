#Requires -Version 7.0
<#
.SYNOPSIS
    Full production MASM conversion — no stubs, no "in a production impl", no minimal scaffolding.
    Enforces pure x64 MASM implementations across unified_overclock_governor, codebase_audit_system,
    quantum_beaconism_backend, dual_engine_system.

.DESCRIPTION
    The simple HAS TO LEAVE. All stub/placeholder/minimal implementations are rejected.
    Run -Validate to check MASM files for remaining stubs. Run -Convert to apply CMake migration.
    Run -Report to list what must be completed in each .asm file.
#>

param(
    [string]$RepoRoot = (Get-Location).Path,
    [switch]$Validate,
    [switch]$Convert,
    [switch]$Report,
    [switch]$GlobalScan
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:RepoRoot = if ([System.IO.Path]::IsPathRooted($RepoRoot)) { $RepoRoot } else { (Resolve-Path $RepoRoot).Path }

function Write-Info([string]$m) { Write-Host "[INFO] $m" -ForegroundColor Cyan }
function Write-Ok([string]$m)   { Write-Host "[OK]   $m" -ForegroundColor Green }
function Write-Warn([string]$m) { Write-Host "[WARN] $m" -ForegroundColor Yellow }
function Write-Fail([string]$m) { Write-Host "[FAIL] $m" -ForegroundColor Red }

# Stub/placeholder patterns that MUST NOT exist in production MASM
$script:StubPatterns = @(
    'Stub exports',
    'full impl would',
    'in a production impl',
    'production implementation would',
    'placeholder',
    'scaffold',
    'TODO:',
    'FIXME:',
    'minimal implementation',
    'no-op',
    'Not implemented',
    '; stub',
    'bridge can call'
)

# Required MASM modules and their C++/header origins
$script:Modules = @(
    @{
        Name   = "UnifiedOverclockGovernor"
        Cpp    = "src/core/unified_overclock_governor.cpp"
        Header = "src/core/unified_overclock_governor.h"
        Asm    = "src/asm/RawrXD_UnifiedOverclock_Governor.asm"
        RequiredExports = @(
            "Governor_Init", "Governor_Shutdown", "Governor_ApplyOffset",
            "Governor_GetDomainTelemetry", "Governor_GetSystemTelemetry",
            "Governor_SaveSession", "Governor_LoadSession", "Governor_DispatchCLI",
            "Governor_EnableAutoTune", "Governor_DisableAutoTune", "Governor_Overclock", "Governor_Underclock"
        )
    },
    @{
        Name   = "CodebaseAuditSystem"
        Cpp    = "src/audit/codebase_audit_system_impl.cpp"
        Header = $null
        Asm    = "src/asm/RawrXD_CodebaseAuditSystem.asm"
        RequiredExports = @(
            "CodebaseAudit_Initialize", "CodebaseAudit_Shutdown",
            "CodebaseAudit_AuditFullProject", "CodebaseAudit_AnalyzeSourceFile",
            "CodebaseAudit_AnalyzeCppQuality", "CodebaseAudit_AnalyzeCppSecurity",
            "CodebaseAudit_CollectSourceFiles", "CodebaseAudit_ReadFileContent"
        )
    },
    @{
        Name   = "QuantumBeaconismBackend"
        Cpp    = "src/core/quantum_beaconism_backend.cpp"
        Header = "src/core/quantum_beaconism_backend.h"
        Asm    = @("src/asm/quantum_beaconism_backend.asm", "src/asm/RawrXD_DualEngine_QuantumBeacon.asm")
        RequiredExports = @("qb_masm_normalize_qubit", "qb_masm_weighted_fitness", "qb_masm_entangle_pair")
    },
    @{
        Name   = "DualEngineSystem"
        Cpp    = "src/core/dual_engine_system.cpp"
        Header = "src/core/dual_engine_system.h"
        Asm    = "src/asm/RawrXD_DualEngine_QuantumBeacon.asm"
        RequiredExports = @("DualEngine_InitAll", "DualEngine_ShutdownAll", "DualEngine_ExecuteOnAll", "DualEngine_DispatchCLI")
    }
)

function Get-RepoPath([string]$rel) {
    $p = Join-Path $script:RepoRoot $rel
    if (Test-Path -LiteralPath $p) { return (Microsoft.PowerShell.Management\Resolve-Path $p).Path }
    return $p
}

function Get-AsmContent([string]$asmPath) {
    $full = Get-RepoPath $asmPath
    if (-not (Test-Path -LiteralPath $full)) { return $null }
    return [System.IO.File]::ReadAllText($full)
}

function Test-StubInContent([string]$content) {
    $hits = New-Object System.Collections.Generic.List[string]
    foreach ($pat in $script:StubPatterns) {
        if ($content -match $pat) { $hits.Add($pat) }
    }
    return $hits
}

function Get-ExportPresent([string]$content, [string[]]$exports) {
    $present = @()
    foreach ($e in $exports) {
        if ($content -match "PUBLIC\s+$e\b" -or $content -match "$e\s+PROC") { $present += $e }
    }
    return $present
}

function Invoke-Validate {
    Write-Info "Validating pure MASM — no stubs allowed"
    Set-Location $script:RepoRoot
    $violations = New-Object System.Collections.Generic.List[object]
    $missing = New-Object System.Collections.Generic.List[string]

    foreach ($mod in $script:Modules) {
        $asms = if ($mod.Asm -is [array]) { $mod.Asm } else { @($mod.Asm) }
        foreach ($a in $asms) {
            $p = Get-RepoPath $a
            if (-not (Test-Path -LiteralPath $p)) {
                $missing.Add($a)
                Write-Fail "Missing: $a"
                continue
            }
            $text = Get-AsmContent $a
            if (-not $text) { continue }
            $stubs = Test-StubInContent $text
            foreach ($s in $stubs) {
                $violations.Add([PSCustomObject]@{ File = $a; Pattern = $s })
            }
        }
    }

    if ($violations.Count -gt 0) {
        Write-Fail "Stub/placeholder violations (the simple HAS TO LEAVE):"
        $violations | ForEach-Object { Write-Host "  $($_.File) :: $($_.Pattern)" -ForegroundColor Red }
        return 1
    }
    if ($missing.Count -gt 0) {
        Write-Fail "Missing MASM modules: $($missing -join ', ')"
        return 2
    }
    Write-Ok "All MASM modules present; no stub patterns detected"
    return 0
}

function Invoke-Report {
    Write-Info "Reporting MASM completion status"
    Set-Location $script:RepoRoot
    foreach ($mod in $script:Modules) {
        Write-Host ""
        Write-Host "=== $($mod.Name) ===" -ForegroundColor Cyan
        $asms = if ($mod.Asm -is [array]) { $mod.Asm } else { @($mod.Asm) }
        foreach ($a in $asms) {
            $text = Get-AsmContent $a
            if (-not $text) {
                Write-Fail "  $a — NOT FOUND"
                continue
            }
            $present = Get-ExportPresent $text $mod.RequiredExports
            $total = $mod.RequiredExports.Count
            $got = @($present).Count
            $pct = [math]::Round(100.0 * $got / [math]::Max(1, $total), 0)
            Write-Host "  $a — Exports: $got/$total ($pct%)" -ForegroundColor $(if ($pct -eq 100) { "Green" } else { "Yellow" })
        }
    }
    return 0
}

function Invoke-Convert {
    Write-Info "Applying CMake MASM migration"
    $cmakePath = Join-Path $script:RepoRoot "CMakeLists.txt"
    if (-not (Test-Path -LiteralPath $cmakePath)) {
        Write-Fail "CMakeLists.txt not found"
        return 1
    }
    $cmake = [System.IO.File]::ReadAllText($cmakePath)
    $replacements = @(
        @{ From = "src/core/unified_overclock_governor.cpp"; To = "src/asm/RawrXD_UnifiedOverclock_Governor.asm" },
        @{ From = "src/core/dual_engine_system.cpp"; To = "src/asm/RawrXD_DualEngine_QuantumBeacon.asm" },
        @{ From = "src/core/quantum_beaconism_backend.cpp"; To = "src/asm/quantum_beaconism_backend.asm" }
    )
    foreach ($r in $replacements) {
        if ($cmake -match [regex]::Escape($r.From)) {
            $cmake = $cmake -replace [regex]::Escape($r.From), $r.To
            Write-Ok "Mapped $($r.From) -> $($r.To)"
        }
    }
    [System.IO.File]::WriteAllText($cmakePath, $cmake, [System.Text.Encoding]::UTF8)
    Write-Ok "CMake migration complete"
    return 0
}

# Main
if ($Validate) { exit Invoke-Validate }
if ($Report)   { exit Invoke-Report }
if ($Convert)  { exit Invoke-Convert }

Write-Host @"

ConvertToMASM_FullProduction.ps1 — Full production MASM conversion

Usage:
  .\ConvertToMASM_FullProduction.ps1 -Validate   # Fail if stubs/placeholders detected
  .\ConvertToMASM_FullProduction.ps1 -Report     # Show completion status per module
  .\ConvertToMASM_FullProduction.ps1 -Convert    # Apply CMake source migration

Rule: THE SIMPLE HAS TO LEAVE. No partial implementations, no "in a production impl",
      no scaffolded minimal stubs. All exports must have full production logic.

"@
exit 0
