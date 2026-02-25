#!/usr/bin/env pwsh
# Quick Module Wiring Report - Scans only the scripts directory

param(
    [string]$ModuleDir = "d:\lazy init ide\scripts"
)

Write-Host "`n=== Quick Module Wiring Report ===" -ForegroundColor Cyan
Write-Host "Scanning: $ModuleDir`n"

$modules = @(Get-ChildItem -Path $ModuleDir -Filter *.psm1)
$scriptFiles = @(Get-ChildItem -Path $ModuleDir -Filter *.ps1)

Write-Host "Found $($modules.Count) modules and $($scriptFiles.Count) scripts"

$results = @()
foreach ($module in $modules) {
    $moduleName = $module.BaseName
    $references = 0
    
    foreach ($script in $scriptFiles) {
        $content = Get-Content $script.FullName -Raw -ErrorAction SilentlyContinue
        if ($content -match "Import-Module.*$moduleName|using\s+module.*$moduleName") {
            $references++
        }
    }
    
    $status = if ($references -gt 0) { "WIRED" } else { "UNWIRED" }
    $color = if ($references -gt 0) { "Green" } else { "Yellow" }
    
    Write-Host "  [$status] " -NoNewline -ForegroundColor $color
    Write-Host "$moduleName " -NoNewline
    Write-Host "($references references)" -ForegroundColor Gray
    
    $results += [PSCustomObject]@{
        Module = $moduleName
        References = $references
        Status = $status
    }
}

Write-Host "`n=== Summary ===" -ForegroundColor Cyan
$wired = @($results | Where-Object { $_.Status -eq "WIRED" })
$unwired = @($results | Where-Object { $_.Status -eq "UNWIRED" })

Write-Host "Wired modules: " -NoNewline
Write-Host "$($wired.Count)" -ForegroundColor Green
Write-Host "Unwired modules: " -NoNewline
Write-Host "$($unwired.Count)" -ForegroundColor Yellow

if ($unwired.Count -gt 0) {
    Write-Host "`nUnwired modules:" -ForegroundColor Yellow
    foreach ($m in $unwired) {
        Write-Host "  - $($m.Module)"
    }
}

Write-Host "`nDone!" -ForegroundColor Green
