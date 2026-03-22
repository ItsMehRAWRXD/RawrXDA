#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Patch Firewall Phase 1: Set "Ghost" attributes on data plane directories.
  
  This script marks OllamaModels/ and blobs/ directories as HIDDEN + SYSTEM,
  which prevents generic file crawlers (like VS Code's watcher) from scanning
  them during recursive traversals. This blocks the patch-storm freeze.

.DESCRIPTION
  Executes Win32 SetFileAttributes(HIDDEN | SYSTEM) on defined directories.
  This is filesystem-level, not Git.
  
.EXAMPLE
  .\apply-patch-firewall-phase1.ps1
  
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

Write-Host "[PATCH-FIREWALL] Phase 1: Sovereign Data Plane Isolation" -ForegroundColor Cyan
Write-Host ""

# Define target directories (data plane)
$dataPlaneDirs = @(
    "D:\rawrxd\OllamaModels",
    "D:\rawrxd\blobs",
    "F:\OllamaModels"
)

# Define attributes
# FILE_ATTRIBUTE_HIDDEN = 0x2 (2)
# FILE_ATTRIBUTE_SYSTEM = 0x4 (4)
# Combined: 0x6 (6)
$HIDDEN_SYSTEM = 0x2 -bor 0x4

Write-Host "Step 1: Marking directories as HIDDEN + SYSTEM (filesystem-level)" -ForegroundColor Yellow

foreach ($dir in $dataPlaneDirs) {
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
        Write-Host "  ⚠ $dir does not exist (skip)" -ForegroundColor Gray
        continue
    }
    
    try {
        # Get file info object
        $item = Get-Item -LiteralPath $dir -Force
        
        # Apply attributes
        $item.Attributes = [System.IO.FileAttributes]::Hidden -bor [System.IO.FileAttributes]::System
        
        Write-Host "  ✓ $dir" -ForegroundColor Green
        Write-Host "    Attributes: $($item.Attributes)" -ForegroundColor Gray
    }
    catch {
        Write-Host "  ✗ $dir - Error: $_" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Step 2: Verify VS Code settings.json has watcherExclude" -ForegroundColor Yellow

$settingsPath = "D:\rawrxd\.vscode\settings.json"
if (Test-Path -LiteralPath $settingsPath) {
    $content = Get-Content -LiteralPath $settingsPath -Raw
    if ($content -match '"files.watcherExclude"') {
        Write-Host "  ✓ Patch firewall settings present in .vscode/settings.json" -ForegroundColor Green
    }
    else {
        Write-Host "  ⚠ watcherExclude not found in settings.json" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Step 3: Verify .gitignore has data plane exclusions" -ForegroundColor Yellow

$gitignorePath = "D:\rawrxd\.gitignore"
if (Test-Path -LiteralPath $gitignorePath) {
    $content = Get-Content -LiteralPath $gitignorePath -Raw
    $hasOllama = $content -match "OllamaModels/"
    $hasBlobs = $content -match "blobs/"
    
    if ($hasOllama -or $hasBlobs) {
        Write-Host "  ✓ .gitignore has data plane exclusions" -ForegroundColor Green
    }
    else {
        Write-Host "  ⚠ .gitignore may not have complete exclusions" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "[PATCH-FIREWALL] Phase 1 Complete!" -ForegroundColor Green
Write-Host ""
Write-Host "NEXT STEPS:" -ForegroundColor Cyan
Write-Host "  1. RESTART VS Code (close completely, then reopen)"
Write-Host "  2. Open new terminal in D:\rawrxd"
Write-Host "  3. Run: git add .gitignore"
Write-Host "  4. Run: git commit -m 'Patch Firewall Phase 1: Data plane isolation'"
Write-Host "  5. Verify: VS Code stays responsive during file edits"
Write-Host ""
