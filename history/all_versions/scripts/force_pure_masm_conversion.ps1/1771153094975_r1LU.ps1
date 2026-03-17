#Requires -Version 7.0
param(
    [string]$RepoRoot = "d:/rawrxd",
    [switch]$Apply,
    [switch]$GlobalScan,
    [switch]$ArchiveCpp
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Info([string]$msg) { Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Ok([string]$msg) { Write-Host "[OK]   $msg" -ForegroundColor Green }
function Write-Warn([string]$msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Fail([string]$msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red }

function Resolve-RepoPath([string]$relativePath) {
    if ([System.IO.Path]::IsPathRooted($relativePath)) { return $relativePath }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRootResolved $relativePath))
}

function Read-Text([string]$path) {
    $fullPath = Resolve-RepoPath $path
    return [System.IO.File]::ReadAllText($fullPath)
}

function Write-Text([string]$path, [string]$text) {
    $fullPath = Resolve-RepoPath $path
    [System.IO.File]::WriteAllText($fullPath, $text, [System.Text.Encoding]::UTF8)
}

function Backup-File([string]$path) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $fullPath = Resolve-RepoPath $path
    $backupPath = "$fullPath.bak_masm_$stamp"
    Copy-Item -LiteralPath $fullPath -Destination $backupPath -Force
    return $backupPath
}

if (-not (Test-Path -LiteralPath $RepoRoot)) {
    throw "Repo root does not exist: $RepoRoot"
}

$repoRootResolved = (Resolve-Path $RepoRoot).Path
Set-Location $repoRootResolved

Write-Info "Pure MASM conversion gate started"
Write-Info "Repository: $repoRootResolved"
Write-Info "Mode: $(if ($Apply) { 'APPLY' } else { 'REPORT' })"

$targetSpec = @(
    [PSCustomObject]@{
        Name = "Unified Overclock Governor"
        CppPath = "src/core/unified_overclock_governor.cpp"
        HeaderPaths = @("src/core/unified_overclock_governor.h")
        RequiredAsm = @("src/asm/RawrXD_UnifiedOverclock_Governor.asm")
    },
    [PSCustomObject]@{
        Name = "Dual Engine + Quantum Beacon"
        CppPath = "src/core/dual_engine_system.cpp"
        HeaderPaths = @("src/core/dual_engine_system.h", "src/core/quantum_beaconism_backend.h")
        RequiredAsm = @("src/asm/RawrXD_DualEngine_QuantumBeacon.asm", "src/asm/quantum_beaconism_backend.asm")
    },
    [PSCustomObject]@{
        Name = "Codebase Audit System"
        CppPath = "src/audit/codebase_audit_system_impl.cpp"
        HeaderPaths = @()
        RequiredAsm = @("src/asm/RawrXD_CodebaseAuditSystem.asm")
    }
)

$scaffoldPatterns = @(
    "in a production impl",
    "production implementation would",
    "placeholder",
    "scaffold",
    "TODO",
    "stub only",
    "minimal implementation"
)

$violations = New-Object System.Collections.Generic.List[string]
$missingAsm = New-Object System.Collections.Generic.List[string]

Write-Info "Checking requested module coverage"
foreach ($spec in $targetSpec) {
    if (-not (Test-Path -LiteralPath $spec.CppPath)) {
        Write-Warn "Missing C++ source for $($spec.Name): $($spec.CppPath)"
    }

    foreach ($hdr in $spec.HeaderPaths) {
        if (-not (Test-Path -LiteralPath $hdr)) {
            Write-Warn "Missing header for $($spec.Name): $hdr"
        }
    }

    foreach ($asm in $spec.RequiredAsm) {
        if (-not (Test-Path -LiteralPath $asm)) {
            $missingAsm.Add($asm)
            Write-Fail "Missing MASM implementation for $($spec.Name): $asm"
        } else {
            Write-Ok "MASM present for $($spec.Name): $asm"
        }
    }
}

Write-Info "Scanning scaffold markers in target files"
$targetFiles = @(
    "src/core/unified_overclock_governor.cpp",
    "src/audit/codebase_audit_system_impl.cpp",
    "src/core/quantum_beaconism_backend.h",
    "src/core/unified_overclock_governor.h",
    "src/core/dual_engine_system.h"
) | Where-Object { Test-Path -LiteralPath $_ }

foreach ($f in $targetFiles) {
    $content = Read-Text $f
    foreach ($pattern in $scaffoldPatterns) {
        if ($content -match [regex]::Escape($pattern)) {
            $violations.Add("$f :: $pattern")
        }
    }
}

if ($GlobalScan) {
    Write-Info "Global scan enabled (src/**/*.cpp, src/**/*.hpp, src/**/*.h, src/**/*.asm)"
    $scanGlobs = @("src/**/*.cpp", "src/**/*.hpp", "src/**/*.h", "src/**/*.asm")
    $allFiles = foreach ($glob in $scanGlobs) { Get-ChildItem -Path $glob -File -ErrorAction SilentlyContinue }
    foreach ($file in ($allFiles | Sort-Object -Property FullName -Unique)) {
        $content = Read-Text $file.FullName
        foreach ($pattern in $scaffoldPatterns) {
            if ($content -match [regex]::Escape($pattern)) {
                $relative = $file.FullName.Substring($repoRootResolved.Length + 1).Replace("\\", "/")
                $violations.Add("$relative :: $pattern")
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Fail "Scaffold markers detected:"
    $violations | Sort-Object -Unique | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
} else {
    Write-Ok "No scaffold markers detected in selected scope"
}

$cmakePath = "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $cmakePath)) {
    throw "CMakeLists.txt not found in repo root"
}

$cmake = Read-Text $cmakePath
$originalCmake = $cmake

$sourceReplacements = @(
    @{ From = "src/core/dual_engine_system.cpp"; To = "src/asm/RawrXD_DualEngine_QuantumBeacon.asm" },
    @{ From = "src/core/unified_overclock_governor.cpp"; To = "src/asm/RawrXD_UnifiedOverclock_Governor.asm" },
    @{ From = "src/core/quantum_beaconism_backend.cpp"; To = "src/asm/quantum_beaconism_backend.asm" }
)

Write-Info "Preparing CMake source migration"
foreach ($pair in $sourceReplacements) {
    if ($cmake.Contains($pair.From)) {
        $cmake = $cmake.Replace($pair.From, $pair.To)
        Write-Ok "Mapped $($pair.From) -> $($pair.To)"
    } else {
        Write-Warn "Source not present in CMake source list: $($pair.From)"
    }
}

$masmPropertyBlock = @"
    if(RAWR_HAS_MASM)
        set_source_files_properties(
            src/asm/RawrXD_DualEngine_QuantumBeacon.asm
            src/asm/RawrXD_UnifiedOverclock_Governor.asm
            src/asm/quantum_beaconism_backend.asm
            PROPERTIES LANGUAGE ASM_MASM
        )
    endif()
"@

if ($cmake -notmatch "set_source_files_properties\(\s*src/asm/RawrXD_DualEngine_QuantumBeacon\.asm") {
    $anchor = "src/asm/quantum_beaconism_backend.asm"
    if ($cmake.Contains($anchor)) {
        $cmake = $cmake.Replace("$anchor`r`n    )", "$anchor`r`n    )`r`n`r`n$masmPropertyBlock")
        Write-Ok "Inserted ASM_MASM source property block for migrated modules"
    } else {
        Write-Warn "Could not locate insertion anchor for ASM_MASM source property block"
    }
}

if ($cmake -notmatch "RawrXD_CodebaseAuditSystem\.asm") {
    Write-Warn "Audit MASM source is not wired automatically (no existing entry in current CMake source list)"
}

$uniqueViolations = @($violations | Sort-Object -Unique)
$summary = [PSCustomObject]@{
    MissingAsmCount = $missingAsm.Count
    ScaffoldViolationCount = $uniqueViolations.Count
    CMakeWouldChange = ($cmake -ne $originalCmake)
}

Write-Host ""
Write-Info "Summary"
Write-Host ("  Missing MASM modules: {0}" -f $summary.MissingAsmCount)
Write-Host ("  Scaffold violations:  {0}" -f $summary.ScaffoldViolationCount)
Write-Host ("  CMake changes:        {0}" -f $(if ($summary.CMakeWouldChange) { "YES" } else { "NO" }))

if (-not $Apply) {
    Write-Info "Report mode complete. Re-run with -Apply to write CMake migration and optional archiving."
    if ($summary.MissingAsmCount -gt 0) {
        Write-Fail "Cannot claim full MASM conversion: missing required MASM module(s)."
        exit 2
    }
    if ($summary.ScaffoldViolationCount -gt 0) {
        Write-Fail "Cannot claim zero-scaffold state: violations found."
        exit 3
    }
    exit 0
}

if ($summary.MissingAsmCount -gt 0) {
    Write-Fail "Apply aborted: required MASM file(s) missing"
    $missingAsm | Sort-Object -Unique | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 4
}

if ($summary.ScaffoldViolationCount -gt 0) {
    Write-Fail "Apply aborted: remove scaffold markers first"
    exit 5
}

if ($summary.CMakeWouldChange) {
    $bak = Backup-File $cmakePath
    Write-Text $cmakePath $cmake
    Write-Ok "CMake migrated (backup: $bak)"
} else {
    Write-Info "CMake already aligned for listed source migrations"
}

if ($ArchiveCpp) {
    $archiveRoot = "legacy/migrated_cpp"
    New-Item -Path $archiveRoot -ItemType Directory -Force | Out-Null

    foreach ($spec in $targetSpec) {
        if (Test-Path -LiteralPath $spec.CppPath) {
            $name = [System.IO.Path]::GetFileName($spec.CppPath)
            $dest = Join-Path $archiveRoot $name
            Move-Item -LiteralPath $spec.CppPath -Destination $dest -Force
            Write-Ok "Archived C++ module: $($spec.CppPath) -> $dest"
        }
    }
}

Write-Ok "Pure MASM conversion gate completed"
exit 0
