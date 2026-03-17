# verify_build_completeness.ps1 — Ensure IDE build has required MASM symbols
# Run after build to catch "white screen" cause: missing ASM components that init waits on.
# Usage: .\scripts\verify_build_completeness.ps1 [-path .\build\Release\RawrXD-AgenticIDE.exe]

param(
    [string]$ExePath = "",
    [string]$Config = "Release",
    [string]$Target = "RawrXD-AgenticIDE"
)

$ErrorActionPreference = "Stop"

$Red = "`e[31m"
$Green = "`e[32m"
$Yellow = "`e[33m"
$Cyan = "`e[36m"
$Reset = "`e[0m"

if (-not $ExePath) {
    $root = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { $PWD.Path }
    $ExePath = Join-Path $root "build" $Config "$Target.exe"
}

if (-not (Test-Path $ExePath)) {
    Write-Host "${Red}❌ EXE not found: $ExePath${Reset}"
    Write-Host "Build first with: .\scripts\build_arch.ps1 -Config $Config"
    exit 1
}

# Symbols required for main_win32 MASM init (no spin/hang). C linkage so they appear in SYMBOLS, not EXPORTS.
$requiredSymbols = @(
    "asm_spengine_init",
    "asm_spengine_shutdown",
    "asm_gguf_loader_init",
    "asm_gguf_loader_close",
    "asm_lsp_bridge_init",
    "asm_lsp_bridge_shutdown",
    "asm_orchestrator_init",
    "asm_orchestrator_shutdown",
    "asm_quadbuf_init",
    "asm_quadbuf_shutdown"
)

# Find dumpbin (same dir as link.exe from VS)
$linkDir = $null
$searchPaths = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64"
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64"
)
foreach ($pattern in $searchPaths) {
    $dirs = Get-Item -Path $pattern -ErrorAction SilentlyContinue
    if ($dirs) {
        $linkDir = $dirs[0].FullName
        break
    }
}
if (-not $linkDir) {
    $linkExe = (Get-Command link.exe -ErrorAction SilentlyContinue).Source
    if ($linkExe) { $linkDir = Split-Path $linkExe -Parent }
}

$dumpbin = Join-Path $linkDir "dumpbin.exe"
if (-not (Test-Path $dumpbin)) {
    Write-Host "${Yellow}⚠️  dumpbin.exe not found; skipping symbol check.${Reset}"
    Write-Host "  Run from 'x64 Native Tools Command Prompt for VS 2022' for full verification."
    Write-Host "${Green}✓${Reset} EXE exists: $ExePath"
    exit 0
}

Write-Host "${Cyan}Verifying build: $ExePath${Reset}"
$symOutput = & $dumpbin /SYMBOLS $ExePath 2>&1 | Out-String
$missing = @()
foreach ($sym in $requiredSymbols) {
    if ($symOutput -match [regex]::Escape($sym)) {
        Write-Host "  ${Green}✓${Reset} $sym"
    }
    else {
        Write-Host "  ${Red}✗${Reset} $sym"
        $missing += $sym
    }
}

if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "${Red}❌ MISSING SYMBOLS: $($missing -join ', ')${Reset}"
    Write-Host "IDE may hang on init (white screen) waiting for these. Either:"
    Write-Host "  1. Build with cathedral ASM (rawrxd_link.asm / rawrxd_scc.asm), or"
    Write-Host "  2. Ensure arch_stub.cpp is linked (bridge build)."
    exit 1
}

Write-Host ""
Write-Host "${Green}✅ All required MASM symbols present — init will not hang on missing ASM.${Reset}"
exit 0
