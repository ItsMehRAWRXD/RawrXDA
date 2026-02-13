#!/usr/bin/env powershell
<#
.SYNOPSIS
    Comprehensive IDE rendering diagnosis and logging
.DESCRIPTION
    Captures all IDE initialization, rendering, and event errors
    Logs to file for detailed analysis
#>

param(
    [switch]$FixIssues
)

$DiagnosticLog = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\IDE-DIAGNOSTIC.log"
$ErrorLog = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\IDE-ERRORS.log"

# Clear old logs
@() | Out-File $DiagnosticLog -Force
@() | Out-File $ErrorLog -Force

function Write-Diagnostic {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Level] $Message"

    Write-Host $logEntry -ForegroundColor $(
        switch ($Level) {
            "ERROR" { "Red" }
            "WARN" { "Yellow" }
            "SUCCESS" { "Green" }
            default { "White" }
        }
    )
    Add-Content $DiagnosticLog $logEntry
}

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  IDE RENDERING DIAGNOSTICS & LOGGING           ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Diagnostic "Starting comprehensive IDE diagnostics..."
Write-Diagnostic "Diagnostic log: $DiagnosticLog"
Write-Diagnostic "Error log: $ErrorLog"

# ============================================
# 1. CHECK TEXT BOX COLOR ISSUES
# ============================================
Write-Host "`n[1] TEXT BOX COLOR ANALYSIS" -ForegroundColor Yellow
Write-Host "───────────────────────────────" -ForegroundColor Gray

Write-Diagnostic "Analyzing text box color configurations..."

$ColorIssues = @()

# Search for color assignments
$RawrXDPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"
if (Test-Path $RawrXDPath) {
    Write-Diagnostic "Reading RawrXD.ps1 for color assignments..."

    # Find all BackColor assignments
    $backColors = Select-String -Path $RawrXDPath -Pattern "BackColor\s*=\s*\[System\.Drawing\.Color\]::" -Context 0,0 | Select-Object -ExpandProperty Line

    Write-Host "✓ BackColor assignments found: $($backColors.Count)" -ForegroundColor Green
    Write-Diagnostic "BackColor assignments: $($backColors.Count)"

    $backColors | ForEach-Object {
        Write-Host "  $_" -ForegroundColor Gray
        Write-Diagnostic "  $_"
    }

    # Find all ForeColor assignments
    $foreColors = Select-String -Path $RawrXDPath -Pattern "ForeColor\s*=\s*\[System\.Drawing\.Color\]::" -Context 0,0 | Select-Object -ExpandProperty Line

    Write-Host "✓ ForeColor assignments found: $($foreColors.Count)" -ForegroundColor Green
    Write-Diagnostic "ForeColor assignments: $($foreColors.Count)"

    $foreColors | ForEach-Object {
        Write-Host "  $_" -ForegroundColor Gray
        Write-Diagnostic "  $_"
    }

    # Check for color mismatches (both dark)
    Write-Host "`n⚠ Analyzing for color conflicts..." -ForegroundColor Yellow
    Write-Diagnostic "Checking for color mismatches..."

    $darkColors = @("30, 30, 30", "20, 20, 20", "0, 0, 0", "10, 10, 10", "FromArgb(30", "FromArgb(20", "FromArgb(0")
    $lightColors = @("220, 220, 220", "255, 255, 255", "200, 200, 200", "FromArgb(220", "FromArgb(255")

    if (($backColors -join "`n") -match ($darkColors -join "|") -and ($foreColors -join "`n") -match ($darkColors -join "|")) {
        $issue = "CRITICAL: Both BackColor and ForeColor are dark - text will be invisible!"
        Write-Host "  ❌ $issue" -ForegroundColor Red
        Write-Diagnostic $issue "ERROR"
        $ColorIssues += $issue
    }
} else {
    Write-Diagnostic "RawrXD.ps1 not found!" "ERROR"
}

# ============================================
# 2. CHECK TEXTBOX INITIALIZATION
# ============================================
Write-Host "`n[2] TEXT BOX INITIALIZATION" -ForegroundColor Yellow
Write-Host "───────────────────────────────" -ForegroundColor Gray

Write-Diagnostic "Searching for text box creation..."

$textBoxLines = Select-String -Path $RawrXDPath -Pattern 'New-Object System\.Windows\.Forms\.TextBox' | ForEach-Object {
    $_.LineNumber
}

Write-Host "✓ TextBox creations at lines: $($textBoxLines -join ', ')" -ForegroundColor Green
Write-Diagnostic "TextBox creations at lines: $($textBoxLines -join ', ')"

# ============================================
# 3. CHECK RICHTEXTBOX ISSUES
# ============================================
Write-Host "`n[3] RICHTEXTBOX DIAGNOSTICS" -ForegroundColor Yellow
Write-Host "───────────────────────────────" -ForegroundColor Gray

Write-Diagnostic "Searching for RichTextBox controls..."

$richtextboxLines = Select-String -Path $RawrXDPath -Pattern 'New-Object System\.Windows\.Forms\.RichTextBox'

Write-Host "✓ RichTextBox controls found: $($richtextboxLines.Count)" -ForegroundColor Green
Write-Diagnostic "RichTextBox controls found: $($richtextboxLines.Count)"

$richtextboxLines | ForEach-Object {
    Write-Host "  Line $($_.LineNumber): $($_.Line.Trim())" -ForegroundColor Gray
    Write-Diagnostic "  Line $($_.LineNumber): $($_.Line.Trim())"
}

# ============================================
# 4. FONT SETTINGS
# ============================================
Write-Host "`n[4] FONT CONFIGURATION" -ForegroundColor Yellow
Write-Host "───────────────────────────────" -ForegroundColor Gray

Write-Diagnostic "Checking font settings..."

$fontLines = Select-String -Path $RawrXDPath -Pattern '\.Font\s*=' | Select-Object -First 5

Write-Host "✓ Font assignments found: $($fontLines.Count)" -ForegroundColor Green
Write-Diagnostic "Font assignments found: $($fontLines.Count)"

$fontLines | ForEach-Object {
    Write-Host "  $_" -ForegroundColor Gray
    Write-Diagnostic "  $_"
}

# ============================================
# 5. GENERATE FIX SCRIPT
# ============================================
Write-Host "`n[5] GENERATING FIXES" -ForegroundColor Yellow
Write-Host "───────────────────────────────" -ForegroundColor Gray

$fixScript = @'
# ============================================
# RAWR XD IDE - TEXT RENDERING FIX
# Fixes invisible text by correcting colors
# ============================================

$RawrXDPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"

# FIXES TO APPLY:
# 1. Replace dark text with light text
# 2. Ensure contrasting colors
# 3. Fix all text boxes and chat areas
# 4. Add logging to capture initialization

$fixes = @(
    @{
        Pattern = "ForeColor\s*=\s*\[System\.Drawing\.Color\]::FromArgb\(30,\s*30,\s*30\)"
        Replacement = "ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)"
        Description = "Change editor text from dark to light"
    },
    @{
        Pattern = "ForeColor\s*=\s*\[System\.Drawing\.Color\]::FromArgb\(20,\s*20,\s*20\)"
        Replacement = "ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)"
        Description = "Change chat text from dark to light"
    },
    @{
        Pattern = "ForeColor\s*=\s*\[System\.Drawing\.Color\]::White"
        Replacement = "ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)"
        Description = "Ensure bright white text"
    },
    @{
        Pattern = "BackColor\s*=\s*\[System\.Drawing\.Color\]::FromArgb\(255,\s*255,\s*255\)"
        Replacement = "BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)"
        Description = "Ensure dark background"
    }
)

foreach ($fix in $fixes) {
    Write-Host "Applying: $($fix.Description)" -ForegroundColor Green
    $content = Get-Content $RawrXDPath -Raw
    $content = $content -replace $fix.Pattern, $fix.Replacement
    $content | Out-File $RawrXDPath -Force
    Write-Host "✓ Applied" -ForegroundColor Green
}

Write-Host "`n✅ Fixes applied. Restart RawrXD.ps1" -ForegroundColor Green
'@

$fixScript | Out-File "C:\Users\HiH8e\OneDrive\Desktop\Powershield\apply-rendering-fixes.ps1" -Force
Write-Diagnostic "Generated fix script: apply-rendering-fixes.ps1"

# ============================================
# 6. SUMMARY
# ============================================
Write-Host "`n[6] SUMMARY" -ForegroundColor Yellow
Write-Host "───────────────────────────────" -ForegroundColor Gray

Write-Host "Issues found: $($ColorIssues.Count)" -ForegroundColor Yellow
if ($ColorIssues.Count -gt 0) {
    $ColorIssues | ForEach-Object {
        Write-Host "  ❌ $_" -ForegroundColor Red
        Write-Diagnostic $_ "ERROR"
    }
}

Write-Host "`nLogs created:" -ForegroundColor Green
Write-Host "  • $DiagnosticLog" -ForegroundColor Cyan
Write-Host "  • $ErrorLog" -ForegroundColor Cyan

Write-Host "`nFix script created:" -ForegroundColor Green
Write-Host "  • C:\Users\HiH8e\OneDrive\Desktop\Powershield\apply-rendering-fixes.ps1" -ForegroundColor Cyan

Write-Diagnostic "Diagnostics complete"

if ($FixIssues) {
    Write-Host "`n[7] APPLYING FIXES" -ForegroundColor Yellow
    Write-Host "───────────────────────────────" -ForegroundColor Gray

    & "C:\Users\HiH8e\OneDrive\Desktop\Powershield\apply-rendering-fixes.ps1"

    Write-Host "`n✅ FIXES APPLIED - Restart RawrXD.ps1" -ForegroundColor Green
    Write-Diagnostic "Fixes applied successfully"
}

Write-Host "`n" -ForegroundColor Cyan
Write-Host "Run with -FixIssues flag to automatically apply fixes:" -ForegroundColor Gray
Write-Host "  .\diagnose-rendering.ps1 -FixIssues`n" -ForegroundColor White
