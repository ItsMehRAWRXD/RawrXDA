#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Canonical Surface Audit — CI Gate
.DESCRIPTION
    Verifies that the production surface contains ONLY canonical files
    and that no banned symbols (Qt, spdlog, ggml, nlohmann) leak in.
    Exit 0 = clean. Exit 1 = entropy detected.
.NOTES
    Run from repo root: .\scripts\audit_entropy.ps1
#>

param(
    [string]$RepoRoot = (Split-Path $PSScriptRoot -Parent)
)

$ErrorActionPreference = "Stop"
Set-Location $RepoRoot

$Green = "`e[32m"
$Red   = "`e[31m"
$Cyan  = "`e[36m"
$Reset = "`e[0m"

Write-Host "${Cyan}══ RawrXD Canonical Surface Audit ══${Reset}"

# ── Canonical manifest ──────────────────────────────────────────────
$CanonicalASM = @(
    "main.asm",
    "inference.asm",
    "ui.asm",
    "beacon.asm",
    "lsp.asm",
    "agent.asm",
    "model_loader.asm",
    "dap.asm",
    "testing.asm",
    "tasks.asm"
)

$MonoDir = Join-Path $RepoRoot "src\asm\monolithic"
$violations = 0

# ── Check 1: All canonical files exist ──────────────────────────────
Write-Host "`n  [1] Canonical file presence"
foreach ($f in $CanonicalASM) {
    $path = Join-Path $MonoDir $f
    if (Test-Path $path) {
        Write-Host "    ${Green}✓${Reset} $f"
    } else {
        Write-Host "    ${Red}✗ MISSING: $f${Reset}"
        $violations++
    }
}

# ── Check 2: No extra files in monolithic dir ───────────────────────
Write-Host "`n  [2] No non-canonical files in monolithic/"
$extras = Get-ChildItem -Path $MonoDir -File -ErrorAction SilentlyContinue |
          Where-Object { $_.Name -notin $CanonicalASM -and $_.Extension -ne ".inc" }
if ($extras) {
    foreach ($e in $extras) {
        Write-Host "    ${Red}✗ EXTRA: $($e.Name)${Reset}"
        $violations++
    }
} else {
    Write-Host "    ${Green}✓${Reset} Clean — only canonical files present"
}

# ── Check 3: Banned symbols in canonical sources ────────────────────
Write-Host "`n  [3] Banned symbol scan"
$BannedPatterns = @(
    @{ Name = "Qt";        Pattern = "QApplication|QWidget|QMainWindow|Q_OBJECT|QtWidgets|QtCore" },
    @{ Name = "spdlog";    Pattern = "spdlog::|#include.*spdlog" },
    @{ Name = "ggml";      Pattern = "ggml_|ggml\.h|ggml-backend" },
    @{ Name = "nlohmann";  Pattern = "nlohmann::|nlohmann/json" },
    @{ Name = "std::cout"; Pattern = "std::cout|std::cerr|iostream" },
    @{ Name = "NASM";      Pattern = "(?m)^\s*section\s+\.(text|data|bss)|(?m)^\s*%macro|(?m)^\s*%define|(?m)^\s*DEFAULT\s+REL" }
)

foreach ($f in $CanonicalASM) {
    $path = Join-Path $MonoDir $f
    if (-not (Test-Path $path)) { continue }
    $content = Get-Content $path -Raw
    foreach ($ban in $BannedPatterns) {
        if ($content -match $ban.Pattern) {
            Write-Host "    ${Red}✗ BANNED [$($ban.Name)] in $f${Reset}"
            $violations++
        }
    }
}
if ($violations -eq 0) {
    Write-Host "    ${Green}✓${Reset} No banned symbols in canonical surface"
}

# ── Check 4: PROC/ENDP balance ──────────────────────────────────────
Write-Host "`n  [4] PROC/ENDP balance"
foreach ($f in $CanonicalASM) {
    $path = Join-Path $MonoDir $f
    if (-not (Test-Path $path)) { continue }
    $content = Get-Content $path -Raw
    $procs = [regex]::Matches($content, '(?im)^\s*\w+\s+PROC\b').Count
    $endps = [regex]::Matches($content, '(?im)^\s*\w+\s+ENDP\b').Count
    if ($procs -ne $endps) {
        Write-Host "    ${Red}✗ UNBALANCED $f (PROC=$procs, ENDP=$endps)${Reset}"
        $violations++
    } else {
        Write-Host "    ${Green}✓${Reset} $f (${procs} procs)"
    }
}

# ── Check 5: ML64 syntax compliance ─────────────────────────────────
Write-Host "`n  [5] ML64 syntax compliance"
$ml64Issues = @(
    @{ Name = "EXTERNEL typo";    Pattern = "\bEXTERNEL\b" },
    @{ Name = "C-style hex";      Pattern = "\b0x[0-9A-Fa-f]+" },
    @{ Name = "sizeof keyword";   Pattern = "\bsizeof\s+\w+" },
    @{ Name = "addr keyword";     Pattern = "\baddr\s+\w+" },
    @{ Name = "multi-reg push";   Pattern = "(?m)^\s*push\s+\w+[ \t,]+\w+" },
    @{ Name = "INVOKE-style call";Pattern = "(?m)^\s*call\s+\w+\s*,\s*\w+" },
    @{ Name = "align 4096 in BSS";Pattern = "(?s)\.data\?.*?align\s+4096" }
)

$syntaxClean = $true
foreach ($f in $CanonicalASM) {
    $path = Join-Path $MonoDir $f
    if (-not (Test-Path $path)) { continue }
    $content = Get-Content $path -Raw
    foreach ($check in $ml64Issues) {
        if ($content -match $check.Pattern) {
            Write-Host "    ${Red}✗ $($check.Name) in $f${Reset}"
            $violations++
            $syntaxClean = $false
        }
    }
}
if ($syntaxClean) {
    Write-Host "    ${Green}✓${Reset} All files ML64-compliant"
}

# ── Check 6: Assemble test ──────────────────────────────────────────
Write-Host "`n  [6] Assembly smoke test"
$ml64 = $null
$candidates = @(
    "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
)
foreach ($c in $candidates) { if (Test-Path $c) { $ml64 = $c; break } }
if (-not $ml64) {
    $found = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($found) { $ml64 = $found.Source }
}
if (-not $ml64) {
    $wild = Get-ChildItem "C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($wild) { $ml64 = $wild.FullName }
}

if ($ml64) {
    foreach ($f in $CanonicalASM) {
        $path = Join-Path $MonoDir $f
        if (-not (Test-Path $path)) { continue }
        $tempObj = Join-Path $env:TEMP ($f -replace '\.asm$', '.obj')
        & $ml64 /c /nologo /Fo"$tempObj" "$path" 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "    ${Green}✓${Reset} $f"
        } else {
            Write-Host "    ${Red}✗ ASSEMBLE FAILED: $f${Reset}"
            $violations++
        }
        if (Test-Path $tempObj) { Remove-Item $tempObj -Force -ErrorAction SilentlyContinue }
    }
} else {
    Write-Host "    ${Red}SKIP: ml64.exe not found${Reset}"
}

# ── Result ──────────────────────────────────────────────────────────
Write-Host ""
if ($violations -eq 0) {
    Write-Host "${Green}✓ CANONICAL SURFACE CLEAN — Zero entropy detected${Reset}"
    exit 0
} else {
    Write-Host "${Red}✗ $violations violation(s) detected${Reset}"
    exit 1
}
