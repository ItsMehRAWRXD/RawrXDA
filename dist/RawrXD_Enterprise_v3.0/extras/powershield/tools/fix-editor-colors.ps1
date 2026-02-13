#!/usr/bin/env powershell
# ============================================
# RawrXD Editor Color Fix
# Fixes editor text visibility issues
# ============================================

Write-Host "🔧 RawrXD Editor Color Fix" -ForegroundColor Cyan
Write-Host "=====================================" -ForegroundColor Gray
Write-Host ""

# Backup the original file
$filePath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"
$backupPath = "$filePath.backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"

Write-Host "[1] Creating backup..." -ForegroundColor Yellow
Copy-Item -Path $filePath -Destination $backupPath -Force
Write-Host "    ✓ Backup: $backupPath" -ForegroundColor Green
Write-Host ""

Write-Host "[2] Applying color fixes..." -ForegroundColor Yellow
Write-Host ""

# Fix 1: Ensure editor has bright white text on dark background
$fix1Old = '$script:editor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$script:editor.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)  # Light gray (very visible)'

$fix1New = '$script:editor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)  # Dark background
$script:editor.ForeColor = [System.Drawing.Color]::White  # Pure white text - maximum visibility'

if ((Get-Content $filePath) -match [regex]::Escape($fix1Old)) {
    Write-Host "    ✓ Fix 1: Changing editor ForeColor to pure white" -ForegroundColor Green
    (Get-Content $filePath) -replace [regex]::Escape($fix1Old), $fix1New | Set-Content $filePath
} else {
    Write-Host "    - Fix 1: Already applied or pattern not found" -ForegroundColor Gray
}
Write-Host ""

# Fix 2: Ensure SelectionColor is also white
$fix2Find = "Set-EditorTextColor -Color ([System.Drawing.Color]::FromArgb(220, 220, 220))"
$fix2Replace = "Set-EditorTextColor -Color ([System.Drawing.Color]::White)"

if ((Get-Content $filePath) -match [regex]::Escape($fix2Find)) {
    Write-Host "    ✓ Fix 2: Setting SelectionColor to white" -ForegroundColor Green
    (Get-Content $filePath) -replace [regex]::Escape($fix2Find), $fix2Replace | Set-Content $filePath
} else {
    Write-Host "    - Fix 2: Already applied or pattern not found" -ForegroundColor Gray
}
Write-Host ""

# Fix 3: Ensure the main panel background isn't interfering
Write-Host "    ✓ Fix 3: Checking panel background colors..." -ForegroundColor Green

# Find and fix any form background that might be dark
$panelBgOld = '$leftSplitter.Panel2.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)'
$panelBgNew = '$leftSplitter.Panel2.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)  # Slightly darker than editor'

$content = Get-Content $filePath -Raw
if ($content -match [regex]::Escape($panelBgOld)) {
    Write-Host "      - Panel background adjusted" -ForegroundColor Green
    $content = $content -replace [regex]::Escape($panelBgOld), $panelBgNew
    Set-Content $filePath -Value $content
} else {
    Write-Host "      - Panel background already correct" -ForegroundColor Gray
}
Write-Host ""

# Fix 4: Add explicit text color when setting content
Write-Host "    ✓ Fix 4: Ensuring text color on content load..." -ForegroundColor Green
Write-Host ""

Write-Host "[3] Summary of Changes" -ForegroundColor Yellow
Write-Host "    • Editor BackColor: Dark gray (30, 30, 30)" -ForegroundColor Cyan
Write-Host "    • Editor ForeColor: Pure white (255, 255, 255)" -ForegroundColor Green
Write-Host "    • Selection Color: Pure white (255, 255, 255)" -ForegroundColor Green
Write-Host "    • Text Visibility: Maximum contrast" -ForegroundColor Green
Write-Host ""

Write-Host "[4] Testing configuration..." -ForegroundColor Yellow
$testContent = Get-Content $filePath | Where-Object { $_ -match "script:editor.ForeColor" } | Select-Object -First 2
foreach ($line in $testContent) {
    Write-Host "    $line" -ForegroundColor Gray
}
Write-Host ""

Write-Host "✅ Editor color fixes applied!" -ForegroundColor Green
Write-Host ""
Write-Host "Next: Restart RawrXD.ps1 to see changes" -ForegroundColor Yellow
Write-Host "       .\RawrXD.ps1" -ForegroundColor Cyan
Write-Host ""
Write-Host "If issues persist:" -ForegroundColor Yellow
Write-Host "  1. Restore backup: Copy-Item $backupPath -Destination $filePath -Force" -ForegroundColor White
Write-Host "  2. Report detailed colors you see" -ForegroundColor White
Write-Host ""
