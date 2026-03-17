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

if (-not (Test-Path -LiteralPath $RepoRoot)) {
    throw "Repo root does not exist: $RepoRoot"
}

$repoRootResolved = (Resolve-Path $RepoRoot).Path
Set-Location $repoRootResolved

function Resolve-RepoPath([string]$path) {
    if ([System.IO.Path]::IsPathRooted($path)) {
        return [System.IO.Path]::GetFullPath($path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRootResolved $path))
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

Write-Info "Pure MASM conversion gate started"
Write-Info "Repository: $repoRootResolved"
Write-Info "Mode: $(if ($Apply) { 'APPLY' } else { 'REPORT' })"

$targetFiles = @(
    "src/core/unified_overclock_governor.cpp",
    "src/audit/codebase_audit_system_impl.cpp",
    "src/core/quantum_beaconism_backend.h",
    "src/core/unified_overclock_governor.h",
    "src/core/dual_engine_system.h"
)

$requiredAsm = @(
    "src/asm/RawrXD_UnifiedOverclock_Governor.asm",
    "src/asm/RawrXD_CodebaseAuditSystem.asm",
    "src/asm/quantum_beaconism_backend.asm",
    "src/asm/RawrXD_DualEngine_QuantumBeacon.asm"
)

$scaffoldPatterns = @(
    "in a production impl",
    "production implementation would",
    "production would use",
    "full implementation would",
    "placeholder",
    "scaffold",
    "stub",
    "minimal implementation"
)

$missingTargets = New-Object System.Collections.Generic.List[string]
$missingAsm = New-Object System.Collections.Generic.List[string]
$violations = New-Object System.Collections.Generic.List[string]

Write-Info "Checking five requested files"
foreach ($f in $targetFiles) {
    if (-not (Test-Path -LiteralPath (Resolve-RepoPath $f))) {
        $missingTargets.Add($f)
        Write-Fail "Missing target file: $f"
    } else {
        Write-Ok "Found target file: $f"
    }
}

Write-Info "Checking required MASM modules"
foreach ($asm in $requiredAsm) {
    if (-not (Test-Path -LiteralPath (Resolve-RepoPath $asm))) {
        $missingAsm.Add($asm)
        Write-Fail "Missing MASM module: $asm"
    } else {
        Write-Ok "MASM present: $asm"
    }
}

function Add-ViolationsFromFile([string]$relativePath) {
    if (-not (Test-Path -LiteralPath (Resolve-RepoPath $relativePath))) { return }
    $content = Read-Text $relativePath
    foreach ($pattern in $scaffoldPatterns) {
        if ($content -match [regex]::Escape($pattern)) {
            $violations.Add("$relativePath :: $pattern")
        }
    }
}

Write-Info "Scanning scaffold language in requested files"
foreach ($f in $targetFiles) {
    Add-ViolationsFromFile $f
}

if ($GlobalScan) {
    Write-Info "Global scan enabled (src/**/*.cpp, src/**/*.hpp, src/**/*.h, src/**/*.asm)"
    $patterns = @("*.cpp", "*.hpp", "*.h", "*.asm")
    foreach ($pattern in $patterns) {
        Get-ChildItem -Path (Join-Path $repoRootResolved "src") -Recurse -File -Filter $pattern -ErrorAction SilentlyContinue |
            ForEach-Object {
                $full = $_.FullName
                $relative = $full.Substring($repoRootResolved.Length + 1).Replace("\", "/")
                Add-ViolationsFromFile $relative
            }
    }
}

$cmakePath = "CMakeLists.txt"
if (-not (Test-Path -LiteralPath (Resolve-RepoPath $cmakePath))) {
    throw "CMakeLists.txt not found in repo root"
}

$cmake = Read-Text $cmakePath
$originalCmake = $cmake

$forbiddenCppInCmake = @(
    "src/core/unified_overclock_governor.cpp",
    "src/audit/codebase_audit_system_impl.cpp",
    "src/core/quantum_beaconism_backend.cpp",
    "src/core/dual_engine_system.cpp"
)

$requiredAsmInCmake = @(
    "src/asm/RawrXD_UnifiedOverclock_Governor.asm",
    "src/asm/quantum_beaconism_backend.asm",
    "src/asm/RawrXD_DualEngine_QuantumBeacon.asm"
)

$cmakeViolations = New-Object System.Collections.Generic.List[string]

Write-Info "Checking CMake for MASM-only wiring"
foreach ($cpp in $forbiddenCppInCmake) {
    if ($cmake.Contains($cpp)) {
        $cmakeViolations.Add("CMake still references C++ module: $cpp")
        Write-Fail "CMake references forbidden C++ source: $cpp"
        $cmake = $cmake.Replace($cpp, "")
    }
}

foreach ($asm in $requiredAsmInCmake) {
    if (-not $cmake.Contains($asm)) {
        $cmakeViolations.Add("CMake missing MASM reference: $asm")
        Write-Warn "CMake missing required MASM reference: $asm"
    } else {
        Write-Ok "CMake contains MASM reference: $asm"
    }
}

if ($cmake -match "src/asm/RawrXD_DualEngine_QuantumBeacon\.asm" -and
    $cmake -match "src/asm/RawrXD_UnifiedOverclock_Governor\.asm" -and
    $cmake -match "src/asm/quantum_beaconism_backend\.asm") {
    Write-Ok "CMake already includes MASM trio for governor/dual/quantum"
}

$uniqueViolations = @($violations | Sort-Object -Unique)
$uniqueCmakeViolations = @($cmakeViolations | Sort-Object -Unique)

Write-Host ""
Write-Info "Summary"
Write-Host ("  Missing target files:  {0}" -f $missingTargets.Count)
Write-Host ("  Missing MASM files:    {0}" -f $missingAsm.Count)
Write-Host ("  Scaffold violations:   {0}" -f $uniqueViolations.Count)
Write-Host ("  CMake violations:      {0}" -f $uniqueCmakeViolations.Count)
Write-Host ("  CMake changes pending: {0}" -f $(if ($cmake -ne $originalCmake) { "YES" } else { "NO" }))

if ($uniqueViolations.Count -gt 0) {
    Write-Fail "Scaffold violations detected"
    $uniqueViolations | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
}

if ($uniqueCmakeViolations.Count -gt 0) {
    Write-Warn "CMake wiring issues detected"
    $uniqueCmakeViolations | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
}

if (-not $Apply) {
    if ($missingTargets.Count -gt 0) { exit 10 }
    if ($missingAsm.Count -gt 0) { exit 11 }
    if ($uniqueViolations.Count -gt 0) { exit 12 }
    if ($uniqueCmakeViolations.Count -gt 0) { exit 13 }
    Write-Ok "REPORT clean: pure MASM gate passed"
    exit 0
}

if ($missingTargets.Count -gt 0) {
    Write-Fail "Apply aborted: requested files missing"
    exit 20
}

if ($missingAsm.Count -gt 0) {
    Write-Fail "Apply aborted: required MASM modules missing"
    exit 21
}

if ($uniqueViolations.Count -gt 0) {
    Write-Fail "Apply aborted: scaffold/minimal language still present"
    exit 22
}

if ($cmake -ne $originalCmake) {
    $backup = Backup-File $cmakePath
    Write-Text $cmakePath $cmake
    Write-Ok "CMake updated for MASM-only module binding (backup: $backup)"
} else {
    Write-Info "No CMake edits needed"
}

if ($ArchiveCpp) {
    $archiveRoot = Resolve-RepoPath "legacy/migrated_cpp"
    New-Item -Path $archiveRoot -ItemType Directory -Force | Out-Null
    foreach ($cpp in @("src/core/unified_overclock_governor.cpp", "src/audit/codebase_audit_system_impl.cpp")) {
        $full = Resolve-RepoPath $cpp
        if (Test-Path -LiteralPath $full) {
            $dest = Join-Path $archiveRoot ([System.IO.Path]::GetFileName($cpp))
            Move-Item -LiteralPath $full -Destination $dest -Force
            Write-Ok "Archived C++ source: $cpp -> $dest"
        }
    }
}

Write-Ok "APPLY complete: pure MASM conversion gate enforced"
exit 0
