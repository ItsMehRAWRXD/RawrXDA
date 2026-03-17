# verify_build_exports.ps1 — Diagnostic export verifier
# Run after build to confirm critical ASM/MASM symbols are present.
# Usage: .\scripts\verify_build_exports.ps1 [-Config Release]

param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$root = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { $PWD.Path }
$exe = Join-Path $root "build\$Config\RawrXD-AgenticIDE.exe"

if (-not (Test-Path $exe)) {
    Write-Host "❌ Executable not found: $exe"
    exit 1
}

Write-Host "🔍 Verifying build exports..."

# Locate dumpbin (same dir as link.exe from VS)
$vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022"
$editions = @("Community", "Professional", "Enterprise", "BuildTools")
$dumpbin = $null
foreach ($ed in $editions) {
    $linkPath = Get-Command "link.exe" -ErrorAction SilentlyContinue
    if ($linkPath) {
        $binDir = Split-Path $linkPath.Source
        $dumpbin = Join-Path $binDir "dumpbin.exe"
        if (Test-Path $dumpbin) { break }
    }
    $base = Join-Path $vsPath "$ed\VC\Tools\MSVC"
    if (Test-Path $base) {
        $latest = Get-ChildItem -Path $base -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($latest) {
            $dumpbin = Join-Path $latest.FullName "bin\Hostx64\x64\dumpbin.exe"
            if (Test-Path $dumpbin) { break }
        }
    }
}

if (-not $dumpbin -or -not (Test-Path $dumpbin)) {
    Write-Host "⚠️  dumpbin not found; run from 'x64 Native Tools Command Prompt for VS 2022' or install VS C++ tools."
    exit 0
}

$exports = & $dumpbin /EXPORTS $exe 2>$null | Select-String "RawrXD_|RawrCodex_"

$criticalExports = @(
    "RawrXD_MemPatch",
    "RawrCodex_"
)

$optionalExports = @(
    "RawrXD_SGEMM_AVX2",
    "RawrXD_SGEMM_AVX512",
    "RawrXD_DMAStream_Init",
    "RawrXD_Dequant_Q4_0"
)

$issues = 0

Write-Host "`nCritical components (must exist or IDE may fail to start):"
foreach ($pat in $criticalExports) {
    $found = $exports | Where-Object { $_ -match [regex]::Escape($pat) }
    if ($found) {
        Write-Host "  ✅ $pat"
    } else {
        Write-Host "  ❌ $pat MISSING"
        $issues++
    }
}

Write-Host "`nOptional components (stubs acceptable):"
foreach ($pat in $optionalExports) {
    $found = $exports | Where-Object { $_ -match [regex]::Escape($pat) }
    if ($found) {
        Write-Host "  ✅ $pat"
    } else {
        Write-Host "  ⚠️  $pat missing (will use C++ fallback)"
    }
}

if ($issues -eq 0) {
    Write-Host "`n✅ Build verified — all critical exports present."
    exit 0
} else {
    Write-Host "`n❌ Build incomplete — $issues critical component(s) missing."
    Write-Host "   Run rebuild with missing ASM files or check build_arch.ps1."
    exit 1
}
