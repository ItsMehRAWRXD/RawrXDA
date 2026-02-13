# Fix-EPipe-Error.ps1
# Diagnoses and fixes EPipe (broken pipe) errors

Write-Host "🔧 EPipe Error Diagnostic & Fix" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

Write-Host "`n📋 Common Causes of EPipe Errors:" -ForegroundColor Yellow
Write-Host "   1. VS Code/Cursor extension communication issues" -ForegroundColor Gray
Write-Host "   2. Process terminated while writing to pipe" -ForegroundColor Gray
Write-Host "   3. Node.js/Electron process crash" -ForegroundColor Gray
Write-Host "   4. Extension host communication failure" -ForegroundColor Gray
Write-Host "   5. Terminal/process communication error" -ForegroundColor Gray

# Check running processes
Write-Host "`n🔍 Checking Running Processes..." -ForegroundColor Yellow

$vscodeProcesses = Get-Process | Where-Object { $_.ProcessName -match "code|cursor|Code" } -ErrorAction SilentlyContinue
if ($vscodeProcesses) {
    Write-Host "   Found VS Code/Cursor processes:" -ForegroundColor Green
    $vscodeProcesses | ForEach-Object {
        Write-Host "      - $($_.ProcessName) (PID: $($_.Id))" -ForegroundColor Gray
    }
} else {
    Write-Host "   No VS Code/Cursor processes running" -ForegroundColor Gray
}

$nodeProcesses = Get-Process | Where-Object { $_.ProcessName -eq "node" } -ErrorAction SilentlyContinue
if ($nodeProcesses) {
    Write-Host "   Found Node.js processes: $($nodeProcesses.Count)" -ForegroundColor Yellow
    Write-Host "      (Multiple Node processes can cause pipe issues)" -ForegroundColor Gray
}

# Check VS Code extension host
Write-Host "`n🔍 Checking VS Code Extension Host..." -ForegroundColor Yellow

$extensionHost = Get-Process | Where-Object { $_.ProcessName -eq "Code" -and $_.MainWindowTitle -match "Extension" } -ErrorAction SilentlyContinue
if ($extensionHost) {
    Write-Host "   ⚠️  Extension host process detected" -ForegroundColor Yellow
    Write-Host "      This might be causing the pipe error" -ForegroundColor Gray
}

# Solutions
Write-Host "`n💡 Solutions:" -ForegroundColor Cyan

Write-Host "`n   1. Restart VS Code/Cursor" -ForegroundColor Yellow
Write-Host "      • Close all VS Code/Cursor windows" -ForegroundColor Gray
Write-Host "      • Wait a few seconds" -ForegroundColor Gray
Write-Host "      • Reopen VS Code/Cursor" -ForegroundColor Gray

Write-Host "`n   2. Clear Extension Host Cache" -ForegroundColor Yellow
$extensionHostCache = "$env:APPDATA\Code\User\workspaceStorage"
if (Test-Path $extensionHostCache) {
    Write-Host "      Cache location: $extensionHostCache" -ForegroundColor Gray
    Write-Host "      You can clear workspace storage if needed" -ForegroundColor Gray
}

Write-Host "`n   3. Disable Problematic Extensions" -ForegroundColor Yellow
Write-Host "      • Open Extensions view (Ctrl+Shift+X)" -ForegroundColor Gray
Write-Host "      • Disable recently installed extensions" -ForegroundColor Gray
Write-Host "      • Restart VS Code" -ForegroundColor Gray

Write-Host "`n   4. Reset Extension Host" -ForegroundColor Yellow
Write-Host "      • Command Palette (Ctrl+Shift+P)" -ForegroundColor Gray
Write-Host "      • Run: 'Developer: Reload Window'" -ForegroundColor Gray
Write-Host "      • Or: 'Developer: Restart Extension Host'" -ForegroundColor Gray

Write-Host "`n   5. Check for Extension Errors" -ForegroundColor Yellow
Write-Host "      • View → Output" -ForegroundColor Gray
Write-Host "      • Select 'Log (Extension Host)' from dropdown" -ForegroundColor Gray
Write-Host "      • Look for error messages" -ForegroundColor Gray

# Check specific extensions that might cause issues
Write-Host "`n🔍 Checking Extension Configuration..." -ForegroundColor Yellow

$cursorSettings = "$env:APPDATA\Cursor\User\settings.json"
if (Test-Path $cursorSettings) {
    $settings = Get-Content $cursorSettings -Raw | ConvertFrom-Json -ErrorAction SilentlyContinue
    if ($settings) {
        Write-Host "   Checking Cursor settings..." -ForegroundColor Gray
        
        # Check for Amazon Q (we just fixed this)
        if ($settings.'amazonQ.telemetry') {
            Write-Host "      ✅ Amazon Q configured" -ForegroundColor Green
        }
        
        # Check for proxy settings that might cause issues
        if ($settings.PSObject.Properties.Name -contains 'aiSuite.simple.provider') {
            if ($settings.'aiSuite.simple.provider' -eq "proxy") {
                Write-Host "      ⚠️  Proxy setting found (this was causing issues)" -ForegroundColor Yellow
            }
        }
    }
}

# Quick fix options
Write-Host "`n🚀 Quick Fix Options:" -ForegroundColor Cyan

Write-Host "`n   Option A: Restart Extension Host" -ForegroundColor Yellow
Write-Host "      Press Ctrl+Shift+P, then type: 'Developer: Restart Extension Host'" -ForegroundColor Gray

Write-Host "`n   Option B: Reload Window" -ForegroundColor Yellow
Write-Host "      Press Ctrl+Shift+P, then type: 'Developer: Reload Window'" -ForegroundColor Gray

Write-Host "`n   Option C: Kill All VS Code Processes" -ForegroundColor Yellow
$killChoice = Read-Host "      Kill all VS Code/Cursor processes? (y/N)"
if ($killChoice -eq "y" -or $killChoice -eq "Y") {
    Get-Process | Where-Object { $_.ProcessName -match "code|Code|Cursor" } | Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Host "      ✅ Processes killed. Restart VS Code/Cursor now." -ForegroundColor Green
}

Write-Host "`n   Option D: Clear Workspace Storage" -ForegroundColor Yellow
Write-Host "      This will clear extension host cache but keep your settings" -ForegroundColor Gray
$clearChoice = Read-Host "      Clear workspace storage? (y/N)"
if ($clearChoice -eq "y" -or $clearChoice -eq "Y") {
    $workspaceStorage = "$env:APPDATA\Code\User\workspaceStorage"
    if (Test-Path $workspaceStorage) {
        Get-ChildItem $workspaceStorage | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "      ✅ Workspace storage cleared" -ForegroundColor Green
    }
    
    $cursorStorage = "$env:APPDATA\Cursor\User\workspaceStorage"
    if (Test-Path $cursorStorage) {
        Get-ChildItem $cursorStorage | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "      ✅ Cursor workspace storage cleared" -ForegroundColor Green
    }
}

Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
Write-Host "💡 Most Common Fix:" -ForegroundColor Yellow
Write-Host "   Restart VS Code/Cursor completely (close all windows)" -ForegroundColor White
Write-Host "   This usually resolves EPipe errors" -ForegroundColor White

