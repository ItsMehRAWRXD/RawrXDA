#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Diagnostic: Verify VS Code patch-storm freeze is resolved
  
.DESCRIPTION
  Checks:
  1. Data plane directories have HIDDEN+SYSTEM attributes
  2. VS Code settings.json has watcherExclude rules
  3. .gitignore has data plane exclusions
  4. Checks for remaining patch-related errors in logs
  
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Continue"

Write-Host ""
Write-Host "════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "PATCH FIREWALL DIAGNOSTIC (Phase 1)" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

Write-Host "CHECK 1: Data plane directory attributes" -ForegroundColor Yellow
Write-Host "─────────────────────────────────────────" -ForegroundColor Gray

$dirs = @("D:\rawrxd\OllamaModels", "D:\rawrxd\blobs", "F:\OllamaModels")

foreach ($dir in $dirs) {
    if (Test-Path -LiteralPath $dir -PathType Container) {
        $item = Get-Item -LiteralPath $dir -Force
        $hasHidden = $item.Attributes -band [System.IO.FileAttributes]::Hidden
        $hasSystem = $item.Attributes -band [System.IO.FileAttributes]::System
        
        if ($hasHidden -and $hasSystem) {
            Write-Host "  ✓ $dir (HIDDEN+SYSTEM)" -ForegroundColor Green
        }
        else {
            Write-Host "  ✗ $dir (Attributes: $($item.Attributes))" -ForegroundColor Red
        }
    }
    else {
        Write-Host "  − $dir (not found)" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "CHECK 2: VS Code settings (watcherExclude)" -ForegroundColor Yellow
Write-Host "─────────────────────────────────────────" -ForegroundColor Gray

$settingsFile = "D:\rawrxd\.vscode\settings.json"
if (Test-Path -LiteralPath $settingsFile) {
    $json = Get-Content -LiteralPath $settingsFile -Raw
    
    $checks = @(
        ('files.watcherExclude', '"files.watcherExclude"'),
        ('OllamaModels exclusion', '"**/OllamaModels/**": true'),
        ('blobs exclusion', '"**/blobs/**": true')
    )
    
    foreach ($check in $checks) {
        if ($json -match [regex]::Escape($check[1])) {
            Write-Host "  ✓ $($check[0])" -ForegroundColor Green
        }
        else {
            Write-Host "  ✗ $($check[0]) (NOT FOUND)" -ForegroundColor Red
        }
    }
}
else {
    Write-Host "  ✗ settings.json not found" -ForegroundColor Red
}

Write-Host ""
Write-Host "CHECK 3: .gitignore data plane rules" -ForegroundColor Yellow
Write-Host "─────────────────────────────────────" -ForegroundColor Gray

$gitignore = "D:\rawrxd\.gitignore"
if (Test-Path -LiteralPath $gitignore) {
    $content = Get-Content -LiteralPath $gitignore -Raw
    
    $rules = @("OllamaModels/", "blobs/", "*.gguf", "*.sqlite")
    
    foreach ($rule in $rules) {
        if ($content -like "*$rule*") {
            Write-Host "  ✓ $rule excluded" -ForegroundColor Green
        }
        else {
            Write-Host "  ✗ $rule NOT IN .gitignore" -ForegroundColor Red
        }
    }
}
else {
    Write-Host "  ✗ .gitignore not found" -ForegroundColor Red
}

Write-Host ""
Write-Host "CHECK 4: Patch-related errors in VS Code logs" -ForegroundColor Yellow
Write-Host "──────────────────────────────────────────────" -ForegroundColor Gray

$logPath = "$env:APPDATA\Code\logs\*\window*\renderer*.log"
$patchErrors = @()

try {
    $logs = Get-Item -LiteralPath ("$env:APPDATA\Code\logs") -ErrorAction SilentlyContinue | `
            Get-ChildItem -Recurse -Filter "*.log" -ErrorAction SilentlyContinue | `
            Where-Object { $_.Name -like "*renderer*" } | `
            Select-Object -First 10
    
    foreach ($log in $logs) {
        $content = @(Get-Content -LiteralPath $log.FullName -ErrorAction SilentlyContinue) -join "`n"
        
        if ($content -match "patch|diff|render.*freeze|out of memory") {
            $patchErrors += $log.Name
        }
    }
    
    if ($patchErrors.Count -eq 0) {
        Write-Host "  ✓ No patch/render errors in recent logs" -ForegroundColor Green
    }
    else {
        Write-Host "  ⚠ Found errors in: $($patchErrors -join ', ')" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "  − Could not check logs (may need elevated privileges)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "DIAGNOSIS COMPLETE" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""
Write-Host "RECOMMENDED NEXT STEPS:" -ForegroundColor Cyan
Write-Host "  1. If diagnostics show ✓ everywhere: freeze should be RESOLVED" -ForegroundColor White
Write-Host "  2. If any ✗ checks: run apply-patch-firewall-phase1.ps1 again" -ForegroundColor White
Write-Host "  3. RESTART VS Code completely (not just reload folder)" -ForegroundColor White
Write-Host "  4. Edit a file to trigger file watcher, verify no freeze" -ForegroundColor White
Write-Host ""
