#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Comprehensive IDE Color Fix for RawrXD
.DESCRIPTION
    Fixes text visibility issues in all controls by:
    1. Adding GotFocus handlers to preserve colors
    2. Setting consistent foreground/background colors
    3. Adding logging to diagnose issues
#>

param(
    [switch]$ApplyToFile,
    [string]$TargetFile = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"
)

$ErrorActionPreference = 'Stop'

# Define the color fix code to inject
$colorFixCode = @'

# ============================================
# COMPREHENSIVE COLOR FIX - Auto-injected
# ============================================
$script:IDE_COLORS = @{
    EditorBG = [System.Drawing.Color]::FromArgb(30, 30, 30)
    EditorFG = [System.Drawing.Color]::FromArgb(220, 220, 220)
    ChatBG = [System.Drawing.Color]::FromArgb(30, 30, 30)
    ChatFG = [System.Drawing.Color]::FromArgb(220, 220, 220)
    InputBG = [System.Drawing.Color]::FromArgb(45, 45, 45)
    InputFG = [System.Drawing.Color]::FromArgb(220, 220, 220)
    ListBG = [System.Drawing.Color]::FromArgb(37, 37, 38)
    ListFG = [System.Drawing.Color]::FromArgb(220, 220, 220)
    FormBG = [System.Drawing.Color]::FromArgb(45, 45, 48)
    FormFG = [System.Drawing.Color]::FromArgb(241, 241, 241)
}

function Apply-ControlColors {
    param(
        [System.Windows.Forms.Control]$Control,
        [string]$ControlType = "Default"
    )
    
    if (-not $Control) { return }
    
    try {
        switch ($ControlType) {
            "Editor" {
                $Control.BackColor = $script:IDE_COLORS.EditorBG
                $Control.ForeColor = $script:IDE_COLORS.EditorFG
            }
            "Chat" {
                $Control.BackColor = $script:IDE_COLORS.ChatBG
                $Control.ForeColor = $script:IDE_COLORS.ChatFG
            }
            "Input" {
                $Control.BackColor = $script:IDE_COLORS.InputBG
                $Control.ForeColor = $script:IDE_COLORS.InputFG
            }
            "List" {
                $Control.BackColor = $script:IDE_COLORS.ListBG
                $Control.ForeColor = $script:IDE_COLORS.ListFG
            }
            "Form" {
                $Control.BackColor = $script:IDE_COLORS.FormBG
                $Control.ForeColor = $script:IDE_COLORS.FormFG
            }
            default {
                $Control.BackColor = $script:IDE_COLORS.EditorBG
                $Control.ForeColor = $script:IDE_COLORS.EditorFG
            }
        }
        
        # Force refresh
        $Control.Invalidate()
        $Control.Update()
    }
    catch {
        Write-StartupLog "Apply-ControlColors failed for $ControlType : $_" "WARNING"
    }
}

function Add-ColorPreservationHandler {
    param(
        [System.Windows.Forms.Control]$Control,
        [string]$ControlType = "Default"
    )
    
    if (-not $Control) { return }
    
    try {
        # Store the control type in Tag for reference
        if (-not $Control.Tag) {
            $Control.Tag = @{ ColorType = $ControlType }
        }
        elseif ($Control.Tag -is [hashtable]) {
            $Control.Tag.ColorType = $ControlType
        }
        
        # Add GotFocus handler to preserve colors
        $Control.Add_GotFocus({
            param($sender, $e)
            $colorType = "Default"
            if ($sender.Tag -is [hashtable] -and $sender.Tag.ColorType) {
                $colorType = $sender.Tag.ColorType
            }
            Apply-ControlColors -Control $sender -ControlType $colorType
        })
        
        # Add LostFocus handler as well
        $Control.Add_LostFocus({
            param($sender, $e)
            $colorType = "Default"
            if ($sender.Tag -is [hashtable] -and $sender.Tag.ColorType) {
                $colorType = $sender.Tag.ColorType
            }
            Apply-ControlColors -Control $sender -ControlType $colorType
        })
        
        # For RichTextBox, also handle TextChanged
        if ($Control -is [System.Windows.Forms.RichTextBox]) {
            $Control.Add_TextChanged({
                param($sender, $e)
                # Only reapply if selection is at end (new text added)
                if ($sender.SelectionStart -ge $sender.TextLength - 1) {
                    $colorType = "Default"
                    if ($sender.Tag -is [hashtable] -and $sender.Tag.ColorType) {
                        $colorType = $sender.Tag.ColorType
                    }
                    $sender.SelectionColor = $script:IDE_COLORS."$($colorType)FG"
                }
            })
        }
        
        # Initial color application
        Apply-ControlColors -Control $Control -ControlType $ControlType
        
    }
    catch {
        Write-StartupLog "Add-ColorPreservationHandler failed: $_" "WARNING"
    }
}

# Override for forms to apply dark theme
function Apply-DarkThemeToForm {
    param([System.Windows.Forms.Form]$Form)
    
    if (-not $Form) { return }
    
    $Form.BackColor = $script:IDE_COLORS.FormBG
    $Form.ForeColor = $script:IDE_COLORS.FormFG
    
    # Apply to all child controls recursively
    function Apply-ToChildren {
        param([System.Windows.Forms.Control]$Parent)
        
        foreach ($child in $Parent.Controls) {
            if ($child -is [System.Windows.Forms.TextBox] -or 
                $child -is [System.Windows.Forms.RichTextBox]) {
                Add-ColorPreservationHandler -Control $child -ControlType "Input"
            }
            elseif ($child -is [System.Windows.Forms.ListView] -or
                    $child -is [System.Windows.Forms.ListBox] -or
                    $child -is [System.Windows.Forms.TreeView]) {
                Add-ColorPreservationHandler -Control $child -ControlType "List"
            }
            elseif ($child -is [System.Windows.Forms.Panel] -or
                    $child -is [System.Windows.Forms.SplitContainer]) {
                $child.BackColor = $script:IDE_COLORS.FormBG
                Apply-ToChildren -Parent $child
            }
            elseif ($child -is [System.Windows.Forms.Label]) {
                $child.ForeColor = $script:IDE_COLORS.FormFG
            }
            elseif ($child.Controls.Count -gt 0) {
                Apply-ToChildren -Parent $child
            }
        }
    }
    
    Apply-ToChildren -Parent $Form
}
'@

Write-Host "=== RawrXD IDE Color Fix ===" -ForegroundColor Cyan
Write-Host ""

if (-not $ApplyToFile) {
    Write-Host "This script will fix text visibility issues in RawrXD IDE." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "The fix includes:" -ForegroundColor White
    Write-Host "  1. GotFocus handlers to preserve colors when clicking" -ForegroundColor Gray
    Write-Host "  2. Consistent dark theme colors across all controls" -ForegroundColor Gray
    Write-Host "  3. TextChanged handlers for RichTextBox controls" -ForegroundColor Gray
    Write-Host ""
    Write-Host "To apply the fix, run:" -ForegroundColor Yellow
    Write-Host "  .\IDE-Color-Fix.ps1 -ApplyToFile" -ForegroundColor Cyan
    Write-Host ""
    
    # Show the code that would be injected
    Write-Host "Code to inject:" -ForegroundColor Yellow
    Write-Host $colorFixCode -ForegroundColor DarkGray
    
    exit 0
}

# Read the file
Write-Host "Reading $TargetFile..." -ForegroundColor Yellow
$content = Get-Content $TargetFile -Raw

# Check if fix already applied
if ($content -match "COMPREHENSIVE COLOR FIX - Auto-injected") {
    Write-Host "Color fix already applied!" -ForegroundColor Green
    exit 0
}

# Find where to inject - after the function definitions section
$injectionPoint = $content.IndexOf("# ============================================`n# Extension Marketplace Functions")

if ($injectionPoint -lt 0) {
    # Alternative injection point
    $injectionPoint = $content.IndexOf("# Extension Marketplace System")
}

if ($injectionPoint -lt 0) {
    Write-Host "Could not find injection point. Manual injection required." -ForegroundColor Red
    exit 1
}

# Inject the code
$newContent = $content.Substring(0, $injectionPoint) + $colorFixCode + "`n`n" + $content.Substring($injectionPoint)

# Backup original
$backupPath = "$TargetFile.color-backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
Copy-Item $TargetFile $backupPath -Force
Write-Host "Backup created: $backupPath" -ForegroundColor Gray

# Write new content
Set-Content $TargetFile -Value $newContent -Encoding UTF8
Write-Host "Color fix injected successfully!" -ForegroundColor Green

# Now we need to update the control creation code to use the new functions
Write-Host ""
Write-Host "Now updating control creation code..." -ForegroundColor Yellow

# Read the updated file
$content = Get-Content $TargetFile -Raw

# Update editor creation to use color preservation
$editorPattern = '\$script:editor\.BackColor = \[System\.Drawing\.Color\]::FromArgb\(30, 30, 30\)'
$editorReplacement = @'
$script:editor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    Add-ColorPreservationHandler -Control $script:editor -ControlType "Editor"
'@

if ($content -match $editorPattern) {
    $content = $content -replace [regex]::Escape($editorPattern), $editorReplacement
    Write-Host "  Updated editor color preservation" -ForegroundColor Green
}

# Update chatBox creation
$chatPattern = '\$chatBox\.BackColor = \[System\.Drawing\.Color\]::FromArgb\(30, 30, 30\)'
$chatReplacement = @'
$chatBox.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    Add-ColorPreservationHandler -Control $chatBox -ControlType "Chat"
'@

if ($content -match $chatPattern) {
    $content = $content -replace [regex]::Escape($chatPattern), $chatReplacement
    Write-Host "  Updated chatBox color preservation" -ForegroundColor Green
}

# Update inputBox creation
$inputPattern = '\$inputBox\.BackColor = \[System\.Drawing\.Color\]::FromArgb\(45, 45, 45\)'
$inputReplacement = @'
$inputBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    Add-ColorPreservationHandler -Control $inputBox -ControlType "Input"
'@

if ($content -match $inputPattern) {
    $content = $content -replace [regex]::Escape($inputPattern), $inputReplacement
    Write-Host "  Updated inputBox color preservation" -ForegroundColor Green
}

# Write final content
Set-Content $TargetFile -Value $content -Encoding UTF8

Write-Host ""
Write-Host "=== Fix Complete ===" -ForegroundColor Cyan
Write-Host "Please test the IDE by running:" -ForegroundColor Yellow
Write-Host "  .\RawrXD.ps1" -ForegroundColor Cyan
