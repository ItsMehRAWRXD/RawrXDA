# Extension Graveyard Purge — Consolidate 9 Electron parasites into pure MASM64
# Uninstalls bloat from both VS Code and Cursor; backs up configs to RawrXD for native migration.
# Run from repo root: .\ExtensionGraveyard_Purge.ps1

$ErrorActionPreference = "Continue"
$exts = @(
    "cursor-simple-ai",
    "cursor-multi-ai",
    "cursor-ollama-proxy",
    "bigdaddyg.bigdaddyg-copilot",
    "bigdaddyg.bigdaddyg-asm-ide",
    "undefined_publisher.bigdaddyg-cursor-chat",
    "undefined_publisher.bigdaddyg-asm-extension",
    "your-name.cursor-simple-ai",
    "your-name.cursor-multi-ai",
    "undefined_publisher.cursor-ollama-proxy"
)

$configDir = "D:\rawrxd\config"
$legacyImport = "D:\rawrxd\config\legacy_import.json"
$cursorSettings = "$env:USERPROFILE\.cursor\User\settings.json"
$vscodeSettings = "$env:USERPROFILE\.vscode\User\settings.json"

Write-Host "`nEXTENSION GRAVEYARD PURGE — 9 extensions -> 1 executable" -ForegroundColor Magenta
Write-Host "=========================================================`n" -ForegroundColor DarkGray

# Dedupe (same extension can appear under different publisher names)
$exts = $exts | Sort-Object -Unique

foreach ($e in $exts) {
    $code = $null
    $cursor = $null
    try { & code --uninstall-extension $e 2>$null; $code = $true } catch {}
    try { & cursor --uninstall-extension $e 2>$null; $cursor = $true } catch {}
    if ($code -or $cursor) {
        Write-Host "  PURGED: $e" -ForegroundColor Red
    } else {
        Write-Host "  Skip (not installed): $e" -ForegroundColor DarkGray
    }
}

# Ensure RawrXD config dir exists
if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Force -Path $configDir | Out-Null
    Write-Host "`n  Created: $configDir" -ForegroundColor DarkGreen
}

# Import Cursor settings for native migration (ollama.baseUrl, etc.)
if (Test-Path $cursorSettings) {
    Copy-Item $cursorSettings $legacyImport -Force
    Write-Host "`n  Config migrated: $legacyImport" -ForegroundColor Green
} else {
    Write-Host "`n  No Cursor settings at $cursorSettings" -ForegroundColor DarkYellow
}

# Optional: backup VS Code settings too
$vscodeImport = "D:\rawrxd\config\legacy_vscode_import.json"
if (Test-Path $vscodeSettings) {
    Copy-Item $vscodeSettings $vscodeImport -Force
    Write-Host "  VS Code backup: $vscodeImport" -ForegroundColor Green
}

Write-Host "`nElectron residue eliminated. Next: Wire native host." -ForegroundColor Green
Write-Host "  .\extension-host\Wire_Extensions.ps1" -ForegroundColor Cyan
Write-Host "  Merge cursor-ollama-proxy logic -> RawrXD_HttpClient_Real.asm (see docs/EXTENSION_GRAVEYARD_CONSOLIDATION.md)" -ForegroundColor Cyan
