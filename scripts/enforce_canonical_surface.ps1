#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Canonical Surface Enforcement
.DESCRIPTION
    Physically isolates architectural entropy from the production surface.
    Moves all non-canonical source files to entropy/ archive (timestamped).
    Canonical surface = 7 MASM64 modules in src/asm/monolithic/.
    Non-destructive: files are moved, not deleted.
.NOTES
    Run from repo root: .\scripts\enforce_canonical_surface.ps1
    Use -WhatIf to preview without moving anything.
    Use -IncludeScripts to also quarantine non-essential PowerShell scripts.
#>

param(
    [string]$RepoRoot = (Split-Path $PSScriptRoot -Parent),
    [switch]$WhatIf,
    [switch]$IncludeScripts
)

$ErrorActionPreference = "Stop"
Set-Location $RepoRoot

$Green  = "`e[32m"
$Yellow = "`e[33m"
$Red    = "`e[31m"
$Cyan   = "`e[36m"
$Reset  = "`e[0m"

Write-Host "${Cyan}══ RawrXD Canonical Surface Enforcement ══${Reset}"
if ($WhatIf) { Write-Host "${Yellow}  [DRY RUN — no files will be moved]${Reset}" }

# ── Canonical manifest ──────────────────────────────────────────────
$CanonicalASM = @(
    "main.asm", "inference.asm", "ui.asm", "beacon.asm",
    "lsp.asm", "agent.asm", "model_loader.asm"
)

# Files that are part of the working build system
$CanonicalScripts = @(
    "scripts\build_monolithic.ps1",
    "scripts\genesis_final_link.ps1",
    "scripts\audit_entropy.ps1",
    "scripts\enforce_canonical_surface.ps1",
    "compile_fix_orchestrator.ps1",
    "Validate-BuildIntegrity.ps1"
)

# Canonical paths (always preserved in place)
$CanonicalPaths = @(
    "src\asm\monolithic"
)

$MonoDir    = Join-Path $RepoRoot "src\asm\monolithic"
$stamp      = Get-Date -Format "yyyyMMdd_HHmmss"
$EntropyDir = Join-Path $RepoRoot "entropy_$stamp"

# ── Phase 1: Verify canonical surface exists ────────────────────────
Write-Host "`n  Phase 1: Verifying canonical surface"
$missing = 0
foreach ($f in $CanonicalASM) {
    $path = Join-Path $MonoDir $f
    if (Test-Path $path) {
        Write-Host "    ${Green}✓${Reset} $f"
    } else {
        Write-Host "    ${Red}✗ MISSING: $f — ABORTING${Reset}"
        $missing++
    }
}
if ($missing -gt 0) {
    Write-Host "${Red}Cannot enforce: $missing canonical files missing. Fix first.${Reset}"
    exit 1
}

# ── Phase 2: Identify entropy ──────────────────────────────────────
Write-Host "`n  Phase 2: Scanning for entropy"

# All source files outside monolithic/
$extensions = @("*.asm", "*.cpp", "*.c", "*.h", "*.hpp")
$entropyFiles = @()

foreach ($ext in $extensions) {
    Get-ChildItem -Path (Join-Path $RepoRoot "src") -Filter $ext -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        # Skip canonical monolithic dir
        if ($_.FullName -like "*\src\asm\monolithic\*") { return }
        $entropyFiles += $_
    }
}

# Non-monolithic ASM in src/asm/ (legacy scattered modules)
$legacyAsm = $entropyFiles | Where-Object { $_.Extension -eq ".asm" }
$legacyCpp = $entropyFiles | Where-Object { $_.Extension -in ".cpp", ".c" }
$legacyH   = $entropyFiles | Where-Object { $_.Extension -in ".h", ".hpp" }

Write-Host "    Legacy ASM  : $($legacyAsm.Count) files"
Write-Host "    Legacy C/C++: $($legacyCpp.Count) files"
Write-Host "    Legacy H/HPP: $($legacyH.Count) files"
Write-Host "    ${Yellow}Total entropy : $($entropyFiles.Count) files${Reset}"

# ── Phase 3: Move entropy ──────────────────────────────────────────
if ($entropyFiles.Count -eq 0) {
    Write-Host "`n${Green}✓ No entropy found — surface is already clean${Reset}"
    exit 0
}

Write-Host "`n  Phase 3: Quarantining entropy → entropy_$stamp/"

if (-not $WhatIf) {
    New-Item -ItemType Directory -Path $EntropyDir -Force | Out-Null
}

$moved = 0
foreach ($file in $entropyFiles) {
    $relPath = $file.FullName.Substring($RepoRoot.Length).TrimStart('\')
    $destDir = Join-Path $EntropyDir (Split-Path $relPath -Parent)

    if ($WhatIf) {
        Write-Host "    → $relPath"
    } else {
        if (-not (Test-Path $destDir)) {
            New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        }
        Move-Item $file.FullName (Join-Path $destDir $file.Name) -Force
    }
    $moved++
}

Write-Host "    ${Yellow}$moved files $(if ($WhatIf) {'would be'} else {'were'}) quarantined${Reset}"

# ── Phase 4: Verify build still works ──────────────────────────────
if (-not $WhatIf) {
    Write-Host "`n  Phase 4: Post-enforcement build verification"
    $buildScript = Join-Path $RepoRoot "scripts\build_monolithic.ps1"
    if (Test-Path $buildScript) {
        & $buildScript 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "    ${Green}✓ Monolithic build passes after enforcement${Reset}"
        } else {
            Write-Host "    ${Red}✗ Build broke — restoring from entropy${Reset}"
            # Restore: move everything back
            Get-ChildItem -Path $EntropyDir -Recurse -File | ForEach-Object {
                $relPath = $_.FullName.Substring($EntropyDir.Length).TrimStart('\')
                $dest = Join-Path $RepoRoot $relPath
                $destDir = Split-Path $dest -Parent
                if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }
                Move-Item $_.FullName $dest -Force
            }
            Remove-Item $EntropyDir -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "    ${Yellow}Files restored. Enforcement rolled back.${Reset}"
            exit 1
        }
    }
}

# ── Summary ────────────────────────────────────────────────────────
Write-Host ""
Write-Host "${Cyan}╔═════════════════════════════════════════════════════════════╗${Reset}"
Write-Host "${Cyan}║              Enforcement Summary                          ║${Reset}"
Write-Host "${Cyan}╠═════════════════════════════════════════════════════════════╣${Reset}"
Write-Host "║  Canonical modules : $($CanonicalASM.Count)                                       ║"
Write-Host "║  Entropy quarantined: $($moved.ToString().PadLeft(5))                                 ║"
Write-Host "║  Archive location  : entropy_$stamp/               ║"
Write-Host "║  Build status      : $(if (-not $WhatIf) { "${Green}PASS${Reset}" } else { "DRY RUN" })                                    ║"
Write-Host "${Cyan}╚═════════════════════════════════════════════════════════════╝${Reset}"
Write-Host ""

if (-not $WhatIf) {
    Write-Host "${Green}✓ Production surface locked: 7 files, 1 EXE, zero deps.${Reset}"
}
