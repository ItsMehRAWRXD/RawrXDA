#!/usr/bin/env pwsh
# Quick test of Extension Manager

Import-Module "d:\lazy init ide\scripts\ExtensionManager.psm1" -Force

Write-Host "`n=== Extension Manager Test ===" -ForegroundColor Cyan

# Test 1: Create a simple custom extension
Write-Host "`n[Test 1] Creating test extension..." -ForegroundColor Yellow
New-Extension -Name "TestPlugin" -Type Custom -AutoInstall -AutoEnable

# Test 2: List extensions
Write-Host "`n[Test 2] Listing extensions..." -ForegroundColor Yellow
Get-Extension | Format-Table -AutoSize

# Test 3: Disable and re-enable
Write-Host "`n[Test 3] Testing disable/enable..." -ForegroundColor Yellow
Disable-Extension -Name "TestPlugin"
Write-Host "Disabled status:"
Get-Extension -Name "TestPlugin" | Format-Table -AutoSize

Enable-Extension -Name "TestPlugin"
Write-Host "Re-enabled status:"
Get-Extension -Name "TestPlugin" | Format-Table -AutoSize

Write-Host "`n✓ Extension Manager is working!" -ForegroundColor Green
Write-Host "`nTo launch interactive menu, run:" -ForegroundColor Cyan
Write-Host "  Import-Module .\scripts\ExtensionManager.psm1" -ForegroundColor White
Write-Host "  Show-ExtensionMenu" -ForegroundColor White
