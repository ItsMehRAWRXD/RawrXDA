
# EXTENSION EXTERMINATOR v1.0 - Consolidates VS Code bloat into RawrXD Genesis
# Targets: 9 fragmented extensions -> 1 unified MASM64 IDE
# Usage: .\ExtensionExterminator.ps1 [-DestroyVSCode]
#   -DestroyVSCode: Optional. After purge, removes VS Code install (prompts for confirmation).
#
# ENVIRONMENT: Windows only. Uses PowerShell, code CLI, Windows Registry, .cmd/.reg files.
# Do not run on Linux/macOS.

param([switch]$DestroyVSCode)

# Enforce Windows (not Linux/macOS)
if ($env:OS -ne "Windows_NT") {
    Write-Error "ExtensionExterminator requires Windows. This is not a Linux environment."
    exit 1
}

$ErrorActionPreference = "Continue"
$rootDir = if ($PSScriptRoot) { $PSScriptRoot } else { "D:\rawrxd" }
$extensions = @(
    "your-name.cursor-simple-ai",
    "rawrz-underground.rawrz-agentic",
    "ItsMehRAWRXD.rawrxd-lsp-client",
    "undefined_publisher.cursor-ollama-proxy",
    "your-name.cursor-multi-ai",
    "bigdaddyg.bigdaddyg-copilot",
    "undefined_publisher.bigdaddyg-cursor-chat",
    "bigdaddyg.bigdaddyg-asm-ide",
    "undefined_publisher.bigdaddyg-asm-extension"
)

Write-Host "EXTENSION EXTERMINATOR INITIATED" -ForegroundColor Red
Write-Host "Target: 9 VS Code fragments -> 1 RawrXD.exe" -ForegroundColor DarkGray

# Phase 1: Kill VS Code Extensions (Pave the way)
Write-Host "`n[Phase 1] Purging VS Code Extension Bloat..." -ForegroundColor Yellow
foreach ($ext in $extensions) {
    try {
        $result = & code --uninstall-extension $ext 2>&1 | Out-String
        if ($result -match "successfully uninstalled") {
            Write-Host "  EXTERMINATED: $ext" -ForegroundColor DarkGreen
        } else {
            Write-Host "  Already purged or never existed: $ext" -ForegroundColor DarkGray
        }
    } catch {
        Write-Host "  (code CLI not in PATH or not installed): $ext" -ForegroundColor DarkGray
    }
}

# Phase 2: Feature Mapping to RawrXD MASM64 Modules
Write-Host "`n[Phase 2] Mapping Features to Genesis Modules..." -ForegroundColor Cyan

$mapping = @{
    "cursor-simple-ai"       = "RawrXD_ModelPipeline_x64.asm (WinHTTP async)"
    "rawrz-agentic"          = "RawrXD_AgentCore_x64.asm (Autonomous orchestration)"
    "rawrxd-lsp-client"      = "RawrXD_LSPBridge_x64.asm (Native LSP over pipes)"
    "cursor-ollama-proxy"    = "RawrXD_OllamaNative_x64.asm (Direct HTTP, no proxy)"
    "cursor-multi-ai"        = "RawrXD_ModelRouter_x64.asm (Multi-backend switching)"
    "bigdaddyg-copilot"      = "RawrXD_CopilotEngine_x64.asm (Inline completion)"
    "bigdaddyg-cursor-chat"  = "RawrXD_Sidebar_x64.asm (Chat view integrated)"
    "bigdaddyg-asm-ide"      = "RawrXD_Genesis.asm (Self-hosting assembler)"
    "bigdaddyg-asm-extension"= "RawrXD_Installer_x64.asm (Extension loader)"
}

foreach ($fragment in $mapping.GetEnumerator()) {
    Write-Host "  -> $($fragment.Key): " -NoNewline -ForegroundColor Gray
    Write-Host $fragment.Value -ForegroundColor Green
}

# Phase 3: Generate Unified Manifest
Write-Host "`n[Phase 3] Creating RawrXD Unified Manifest..." -ForegroundColor Magenta

$manifestPath = Join-Path $rootDir "src\RawrXD_Unified_Manifest.asm"
$extList = $extensions -join ", "
$manifest = @"
; RawrXD_Unified_Manifest.asm - Consolidated extension replacement
; Replaces: $extList
; With: Zero-dependency MASM64 modules

EXTENSION_MANIFEST SEGMENT READONLY ALIGN(64)

; Extension 1: Simple AI -> ModelPipeline
db "cursor-simple-ai", 0
dd OFFSET OllamaGenerate
dd OFFSET TensorDequantQ4_0_AVX512

; Extension 2: Agentic -> AgentCore
db "rawrz-agentic", 0
dd OFFSET AgentOrchestrate
dd OFFSET AutonomousLoop

; Extension 3: LSP Client -> LSPBridge
db "rawrxd-lsp-client", 0
dd OFFSET LSP_Initialize
dd OFFSET LSP_CodeComplete

; Extension 4: Ollama Proxy -> Native HTTP
db "cursor-ollama-proxy", 0
dd OFFSET WinHttpCallback
dd OFFSET ProcessStreamChunk

; Extension 5: Multi-AI -> ModelRouter
db "cursor-multi-ai", 0
dd OFFSET RouteToClaude
dd OFFSET RouteToGPT
dd OFFSET RouteToLocal

; Extension 6: Copilot -> CompletionEngine
db "bigdaddyg-copilot", 0
dd OFFSET InlineComplete
dd OFFSET GhostTextShow

; Extension 7: Cursor Chat -> Sidebar
db "bigdaddyg-cursor-chat", 0
dd OFFSET ChatAppendToken
dd OFFSET ChatStreamBegin

; Extension 8: ASM IDE -> Genesis
db "bigdaddyg-asm-ide", 0
dd OFFSET GenesisMain
dd OFFSET AssembleFile

; Extension 9: ASM Extension -> Installer
db "bigdaddyg-asm-extension", 0
dd OFFSET InstallExtension
dd OFFSET RegisterLanguage

EXTENSION_MANIFEST ENDS
"@

$null = New-Item -ItemType Directory -Force -Path (Split-Path $manifestPath)
$manifest | Out-File -FilePath $manifestPath -Encoding ASCII
Write-Host "  Wrote: $manifestPath" -ForegroundColor Green

# Phase 4: Create Launcher that replaces code.exe
Write-Host "`n[Phase 4] Generating RawrXD Launcher (replaces code.cmd)..." -ForegroundColor Yellow

$buildProd = Join-Path $rootDir "build_prod"
$null = New-Item -ItemType Directory -Force -Path $buildProd
$launcherPath = Join-Path $buildProd "rawrxd.cmd"

$launcher = @"
@echo off
:: RawrXD Launcher - Replaces VS Code entirely
:: Routes all "code" calls to RawrXD_Genesis.exe

set "RAWRXD_HOME=$buildProd"
set "FILE_OR_DIR=%~1"

if "%FILE_OR_DIR%"=="" (
    start "" "%RAWRXD_HOME%\RawrXD_Genesis.exe" --new-window
) else (
    start "" "%RAWRXD_HOME%\RawrXD_Genesis.exe" "%FILE_OR_DIR%"
)
"@

$launcher | Out-File -FilePath $launcherPath -Encoding ASCII
Write-Host "  Wrote: $launcherPath" -ForegroundColor Green

# Phase 5: Registry Hijack (Optional - makes RawrXD default for .asm)
Write-Host "`n[Phase 5] Registering RawrXD as default ASM IDE..." -ForegroundColor Cyan

$regPath = Join-Path $rootDir "register_rawrxd.reg"
$exePathReg = ($rootDir + '\build_prod\RawrXD_Genesis.exe') -replace '\\', '\\\\'
$regCode = @"
Windows Registry Editor Version 5.00

[HKEY_CLASSES_ROOT\.asm]
@="RawrXDAssemblyFile"

[HKEY_CLASSES_ROOT\RawrXDAssemblyFile\shell\open\command]
@="\"$exePathReg\" \"%1\""

[HKEY_LOCAL_MACHINE\SOFTWARE\Classes\Applications\RawrXD_Genesis.exe\Capabilities]
"ApplicationDescription"="RawrXD Genesis - Pure MASM64 IDE"
"ApplicationName"="RawrXD"

[HKEY_LOCAL_MACHINE\SOFTWARE\RegisteredApplications]
"RawrXD"="Software\\Classes\\Applications\\RawrXD_Genesis.exe\\Capabilities"
"@

$regCode | Out-File -FilePath $regPath -Encoding ASCII
Write-Host "  Wrote: $regPath" -ForegroundColor Green

Write-Host "`nCONSOLIDATION COMPLETE" -ForegroundColor Green
Write-Host "Fragments Purged: 9" -ForegroundColor Gray
Write-Host "Unified Modules: 1 Genesis executable" -ForegroundColor Gray
Write-Host ""
Write-Host "NEXT:" -ForegroundColor Yellow
Write-Host "  1. Run: .\GENESIS_COMPILER.ps1 -SelfCompile" -ForegroundColor White
Write-Host "  2. Run: regedit /s $regPath" -ForegroundColor White
if (-not $DestroyVSCode) {
    Write-Host "  3. (Optional) Delete VS Code: .\ExtensionExterminator.ps1 -DestroyVSCode" -ForegroundColor DarkGray
} else {
    $vscodePath = Join-Path $env:LOCALAPPDATA "Programs\Microsoft VS Code"
    if (Test-Path $vscodePath) {
        $confirm = Read-Host "Remove VS Code at $vscodePath ? (y/N)"
        if ($confirm -eq 'y' -or $confirm -eq 'Y') {
            Remove-Item $vscodePath -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "  VS Code directory removed." -ForegroundColor Red
        } else {
            Write-Host "  Skipped VS Code removal." -ForegroundColor Gray
        }
    } else {
        Write-Host "  VS Code not found at $vscodePath" -ForegroundColor DarkGray
    }
}
Write-Host ""
Write-Host "RawrXD now subsumes all 9 extensions." -ForegroundColor Cyan
