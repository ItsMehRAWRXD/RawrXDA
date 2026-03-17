#!/usr/bin/env powershell
#=============================================================================
# Build_Amphibious_ML64_Complete.ps1
# Complete Amphibious Build — Core2 + HTTP Inference API + GUI/CLI
# Produces: RawrXD-Amphibious-CLI.exe, RawrXD-Amphibious-GUI.exe
#=============================================================================

param(
    [switch]$Clean,
    [switch]$GUI,
    [switch]$CLI,
    [switch]$Inference,
    [switch]$Both = $true  # Default: build both
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ML64_PATH = "C:\masm64\bin64\ml64.exe"
$LINK_PATH = "link.exe"

$ROOT = "D:\rawrxd"
$OUT = "$ROOT\bin\Amphibious"
$SRC = "$ROOT"

if ($Clean) {
    Write-Host "[CLEAN] Removing $OUT" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $OUT -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force -Path $OUT | Out-Null

Write-Host "
╔══════════════════════════════════════════════════════════════════════╗
║         RawrXD Amphibious ML64 — Complete Build Pipeline            ║
╚══════════════════════════════════════════════════════════════════════╝
" -ForegroundColor Cyan

# ============================================================================
# STAGE 1: Assemble MASM modules
# ============================================================================

Write-Host "[1/5] ASSEMBLE: Core2 (Real ML Inference Pipeline)..." -ForegroundColor Cyan
& $ML64_PATH /c /Fo"$OUT\Core2.obj" /W3 /Zi "$SRC\RawrXD_Amphibious_Core2_ml64.asm" 2>&1 | grep -v "^$" | Write-Host
if ($LASTEXITCODE -ne 0) { throw "[FATAL] Core2 assembly failed" }
Write-Host "         ✓ Core2.obj created" -ForegroundColor Green

Write-Host "[2/5] ASSEMBLE: Inference API (WinHTTP Bridge to RawrEngine)..." -ForegroundColor Cyan
& $ML64_PATH /c /Fo"$OUT\InferenceAPI.obj" /W3 /Zi "$SRC\RawrXD_InferenceAPI.asm" 2>&1 | grep -v "^$" | Write-Host
if ($LASTEXITCODE -ne 0) { throw "[FATAL] Inference API assembly failed" }
Write-Host "         ✓ InferenceAPI.obj created" -ForegroundColor Green

Write-Host "[3/5] ASSEMBLE: CLI Wrapper..." -ForegroundColor Cyan
& $ML64_PATH /c /Fo"$OUT\CLI.obj" /W3 /Zi "$SRC\RawrXD_Amphibious_CLI_ml64.asm" 2>&1 | grep -v "^$" | Write-Host
if ($LASTEXITCODE -ne 0) { throw "[FATAL] CLI assembly failed" }
Write-Host "         ✓ CLI.obj created" -ForegroundColor Green

Write-Host "[4/5] ASSEMBLE: GUI Wrapper (Win32)..." -ForegroundColor Cyan
& $ML64_PATH /c /Fo"$OUT\GUI.obj" /W3 /Zi "$SRC\RawrXD_Amphibious_GUI_ml64.asm" 2>&1 | grep -v "^$" | Write-Host
if ($LASTEXITCODE -ne 0) { throw "[FATAL] GUI assembly failed" }
Write-Host "         ✓ GUI.obj created" -ForegroundColor Green

# ============================================================================
# STAGE 2: Link Executables
# ============================================================================

Write-Host "[5/5] LINK: Building Executables..." -ForegroundColor Cyan

# CLI Executable
Write-Host "         Building CLI executable..." -ForegroundColor Gray
& $LINK_PATH /NOLOGO `
    /OUT:"$OUT\RawrXD-Amphibious-CLI.exe" `
    /SUBSYSTEM:CONSOLE `
    /ENTRY:main `
    /MACHINE:X64 `
    /LARGEADDRESSAWARE `
    /INCREMENTAL:NO `
    /OPT:REF /OPT:ICF `
    "$OUT\Core2.obj" `
    "$OUT\InferenceAPI.obj" `
    "$OUT\CLI.obj" `
    winhttp.lib kernel32.lib user32.lib `
    /NODEFAULTLIB:libcmt.lib `
    /DEFAULTLIB:msvcrt.lib `
    2>&1 | Select-String -NotMatch "^$" | Write-Host

if ($LASTEXITCODE -ne 0) { throw "[FATAL] CLI link failed" }

$cliSize = (Get-Item "$OUT\RawrXD-Amphibious-CLI.exe").Length
Write-Host "         ✓ RawrXD-Amphibious-CLI.exe ($([math]::Round($cliSize/1024, 1)) KB)" -ForegroundColor Green

# GUI Executable
Write-Host "         Building GUI executable..." -ForegroundColor Gray
& $LINK_PATH /NOLOGO `
    /OUT:"$OUT\RawrXD-Amphibious-GUI.exe" `
    /SUBSYSTEM:WINDOWS `
    /ENTRY:WinMain `
    /MACHINE:X64 `
    /LARGEADDRESSAWARE `
    /INCREMENTAL:NO `
    /OPT:REF /OPT:ICF `
    "$OUT\Core2.obj" `
    "$OUT\InferenceAPI.obj" `
    "$OUT\GUI.obj" `
    winhttp.lib kernel32.lib user32.lib gdi32.lib comctl32.lib `
    /NODEFAULTLIB:libcmt.lib `
    /DEFAULTLIB:msvcrt.lib `
    2>&1 | Select-String -NotMatch "^$" | Write-Host

if ($LASTEXITCODE -ne 0) { throw "[FATAL] GUI link failed" }

$guiSize = (Get-Item "$OUT\RawrXD-Amphibious-GUI.exe").Length
Write-Host "         ✓ RawrXD-Amphibious-GUI.exe ($([math]::Round($guiSize/1024, 1)) KB)" -ForegroundColor Green

# ============================================================================
# BUILD COMPLETE
# ============================================================================

Write-Host "
╔══════════════════════════════════════════════════════════════════════╗
║                      ✅ BUILD COMPLETE                              ║
╚══════════════════════════════════════════════════════════════════════╝
" -ForegroundColor Magenta

Write-Host "Location: $OUT" -ForegroundColor Gray
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Cyan
Write-Host "  1. Start RawrEngine or Ollama on port 11434:"
Write-Host "     ollama serve" -ForegroundColor DarkGray
Write-Host ""
Write-Host "  2. Run CLI (JSON telemetry output):"
Write-Host "     & '$OUT\RawrXD-Amphibious-CLI.exe'" -ForegroundColor DarkGray
Write-Host ""
Write-Host "  3. Run GUI (live token streaming):"
Write-Host "     & '$OUT\RawrXD-Amphibious-GUI.exe'" -ForegroundColor DarkGray
Write-Host ""

Write-Host "Artifacts:" -ForegroundColor Cyan
Write-Host "  CLI telemetry:  D:\rawrxd\build\amphibious-ml64\rawrxd_telemetry_cli.json" -ForegroundColor Gray
Write-Host "  GUI telemetry:  D:\rawrxd\build\amphibious-ml64\rawrxd_telemetry_gui.json" -ForegroundColor Gray
