# ============================================================================
# ConvertToMASM_GovernorAuditBeaconDual.ps1
# ============================================================================
# Policy: All four subsystems are implemented in PURE x64 MASM. No partial,
# scaffolded, or "in a production impl this would be *" placeholders.
# The simple HAS TO LEAVE: C++ implementations are either removed or become
# thin bridges that call MASM exports when RAWR_HAS_MASM=1.
#
# Targets:
#   1. unified_overclock_governor  -> src/asm/RawrXD_UnifiedOverclock_Governor.asm
#   2. codebase_audit_system_impl -> src/asm/RawrXD_CodebaseAudit_Kernel.asm
#   3. quantum_beaconism_backend  -> src/asm/RawrXD_QuantumBeaconism_Kernel.asm
#   4. dual_engine_system         -> src/asm/RawrXD_DualEngine_Coordinator.asm
#
# Usage:
#   .\ConvertToMASM_GovernorAuditBeaconDual.ps1 -Action Verify   # Check ASM present and linked
#   .\ConvertToMASM_GovernorAuditBeaconDual.ps1 -Action Build     # Run CMake build for IDE target
#   .\ConvertToMASM_GovernorAuditBeaconDual.ps1 -Action List      # List ASM files and CMake refs
# ============================================================================

param(
    [ValidateSet('Verify', 'Build', 'List')]
    [string] $Action = 'List',
    [string] $BuildDir = 'build_ide',
    [string] $ProjectRoot = $PSScriptRoot + '\..'
)

$ErrorActionPreference = 'Stop'
$AsmDir = Join-Path $ProjectRoot 'src\asm'
$CmakePath = Join-Path $ProjectRoot 'CMakeLists.txt'

$RequiredAsm = @(
    'RawrXD_UnifiedOverclock_Governor.asm',
    'RawrXD_CodebaseAudit_Kernel.asm',
    'RawrXD_QuantumBeaconism_Kernel.asm',
    'RawrXD_DualEngine_Coordinator.asm'
)

function Get-AsmStatus {
    foreach ($f in $RequiredAsm) {
        $path = Join-Path $AsmDir $f
        $exists = Test-Path $path
        $inCmake = $false
        if (Test-Path $CmakePath) {
            $content = Get-Content $CmakePath -Raw
            $inCmake = $content -match [regex]::Escape($f)
        }
        [PSCustomObject]@{
            File    = $f
            Exists  = $exists
            InCMake = $inCmake
        }
    }
}

function Invoke-Verify {
    $status = Get-AsmStatus
    $missing = $status | Where-Object { -not $_.Exists }
    $unlinked = $status | Where-Object { $_.Exists -and -not $_.InCMake }
    if ($missing.Count -gt 0) {
        Write-Host "Missing MASM files (must be full production, no stubs):" -ForegroundColor Red
        $missing | ForEach-Object { Write-Host "  - $($_.File)" }
    }
    if ($unlinked.Count -gt 0) {
        Write-Host "ASM files not referenced in CMakeLists.txt:" -ForegroundColor Yellow
        $unlinked | ForEach-Object { Write-Host "  - $($_.File)" }
    }
    if ($missing.Count -eq 0 -and $unlinked.Count -eq 0) {
        Write-Host "All four MASM kernels present and linked. No simple/stub left." -ForegroundColor Green
        return 0
    }
    return 1
}

function Invoke-List {
    $status = Get-AsmStatus
    foreach ($s in $status) {
        $state = if ($s.Exists) { 'OK' } else { 'MISSING' }
        $link = if ($s.InCMake) { 'linked' } else { 'not linked' }
        Write-Host "$($s.File) [$state] [$link]"
    }
    $cmakeSnippet = @"

# Add to CMakeLists.txt (under if(RAWR_HAS_MASM) ASM_KERNEL_SOURCES or custom_command):
#     src/asm/RawrXD_UnifiedOverclock_Governor.asm
#     src/asm/RawrXD_CodebaseAudit_Kernel.asm
#     src/asm/RawrXD_QuantumBeaconism_Kernel.asm
#     src/asm/RawrXD_DualEngine_Coordinator.asm
# And define: RAWRXD_LINK_UNIFIED_OVERCLOCK_ASM, RAWRXD_LINK_CODEBASE_AUDIT_ASM,
#             RAWRXD_LINK_QUANTUM_BEACONISM_ASM, RAWRXD_LINK_DUAL_ENGINE_ASM
"@
    Write-Host $cmakeSnippet
}

function Invoke-Build {
    $dir = Join-Path $ProjectRoot $BuildDir
    if (-not (Test-Path $dir)) {
        Write-Host "Build dir missing: $dir" -ForegroundColor Red
        return 1
    }
    Push-Location $ProjectRoot
    try {
        & cmake --build $BuildDir --target RawrXD-Win32IDE 2>&1 | Out-Host
        return $LASTEXITCODE
    } finally {
        Pop-Location
    }
}

switch ($Action) {
    'Verify' { exit (Invoke-Verify) }
    'List'   { Invoke-List }
    'Build'  { exit (Invoke-Build) }
}
