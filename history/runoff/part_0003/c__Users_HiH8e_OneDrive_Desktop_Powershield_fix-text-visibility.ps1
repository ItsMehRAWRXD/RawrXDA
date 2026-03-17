#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Fix text visibility issues in RawrXD IDE - addresses grey/black text problem on focus
.DESCRIPTION
    The issue: When controls get focus, text becomes invisible (grey on grey, black on black)
    This happens because the default focus behavior overrides color settings
    Solution: Add event handlers to preserve colors during focus events
#>

param(
    [switch]$ApplyFix,
    [switch]$TestOnly
)

Write-Host "RawrXD Text Visibility Fixer" -ForegroundColor Cyan -BackgroundColor Black
Write-Host "=" * 60

# The key issue: Focus events in WinForms override text color
# We need to add GotFocus and LostFocus handlers that preserve colors

$fixCode = @'
# Add this handler function to preserve colors during focus events
function Preserve-ControlColors {
    param(
        [System.Windows.Forms.Control]$control,
        [System.Drawing.Color]$foreColor,
        [System.Drawing.Color]$backColor
    )
    
    # GotFocus event - preserve colors
    $control.Add_GotFocus({
        $this.ForeColor = $foreColor
        $this.BackColor = $backColor
        $this.Refresh()
    })
    
    # LostFocus event - restore colors
    $control.Add_LostFocus({
        $this.ForeColor = $foreColor
        $this.BackColor = $backColor
        $this.Refresh()
    })
    
    # KeyDown event - ensure colors persist during typing
    $control.Add_KeyDown({
        $this.ForeColor = $foreColor
        $this.BackColor = $backColor
    })
}

# For RichTextBox specifically (the code editor)
function Preserve-RichTextBoxColors {
    param(
        [System.Windows.Forms.RichTextBox]$rtb,
        [System.Drawing.Color]$foreColor,
        [System.Drawing.Color]$backColor
    )
    
    # Disable auto-formatting that might override colors
    $rtb.AutoWordSelection = $false
    
    # Set initial colors
    $rtb.ForeColor = $foreColor
    $rtb.BackColor = $backColor
    $rtb.SelectionColor = $foreColor
    
    # GotFocus - reset colors
    $rtb.Add_GotFocus({
        $this.ForeColor = $foreColor
        $this.BackColor = $backColor
        $this.SelectionColor = $foreColor
        $this.Refresh()
    })
    
    # TextChanged - ensure colors persist
    $rtb.Add_TextChanged({
        $this.ForeColor = $foreColor
        $this.SelectionColor = $foreColor
        $this.BackColor = $backColor
    })
    
    # KeyPress - maintain colors during typing
    $rtb.Add_KeyPress({
        $this.ForeColor = $foreColor
        $this.SelectionColor = $foreColor
    })
    
    # SelectionChanged - keep selection visible
    $rtb.Add_SelectionChanged({
        if ($this.SelectionLength -gt 0) {
            $this.SelectionColor = $foreColor
        }
    })
}

# For TextBox
function Preserve-TextBoxColors {
    param(
        [System.Windows.Forms.TextBox]$tb,
        [System.Drawing.Color]$foreColor,
        [System.Drawing.Color]$backColor
    )
    
    $tb.ForeColor = $foreColor
    $tb.BackColor = $backColor
    
    $tb.Add_GotFocus({
        $this.ForeColor = $foreColor
        $this.BackColor = $backColor
        $this.Refresh()
    })
    
    $tb.Add_KeyDown({
        $this.ForeColor = $foreColor
        $this.BackColor = $backColor
    })
}
'@

Write-Host "`n[*] The Problem:" -ForegroundColor Yellow
Write-Host "    - Text color matches background when control gets focus"
Write-Host "    - WinForms focus behavior overrides color settings"
Write-Host "    - Need to force color persistence on GotFocus/LostFocus events"

Write-Host "`n[*] Solution Pattern:" -ForegroundColor Green
Write-Host "    1. Create Preserve-RichTextBoxColors function"
Write-Host "    2. Add GotFocus handlers to maintain colors"
Write-Host "    3. Add TextChanged handlers to restore selection colors"
Write-Host "    4. Add KeyPress handlers to preserve colors during typing"

Write-Host "`n[*] Where to add this in RawrXD.ps1:" -ForegroundColor Cyan
Write-Host "    1. Add the helper functions after the imports"
Write-Host "    2. Call Preserve-RichTextBoxColors on each RichTextBox creation"
Write-Host "    3. Call Preserve-TextBoxColors on each TextBox creation"

if ($ApplyFix) {
    Write-Host "`n[!] Applying fix to RawrXD.ps1..." -ForegroundColor Yellow
    
    $rawrPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"
    
    if (-not (Test-Path $rawrPath)) {
        Write-Error "RawrXD.ps1 not found at $rawrPath"
        exit 1
    }
    
    # Read the file
    $content = Get-Content $rawrPath -Raw
    
    # Check if fix is already applied
    if ($content -match "Preserve-RichTextBoxColors") {
        Write-Host "[✓] Fix already applied!" -ForegroundColor Green
        exit 0
    }
    
    # Find the location after imports to add the functions
    $importEnd = $content.IndexOf("`n`n# Global") 
    if ($importEnd -eq -1) {
        $importEnd = $content.IndexOf("function")
        if ($importEnd -eq -1) {
            Write-Error "Could not find insertion point"
            exit 1
        }
    }
    
    # Insert the fix code
    $newContent = $content.Substring(0, $importEnd) + "`n`n# TEXT VISIBILITY FIX - Preserve colors during focus events`n" + $fixCode + "`n" + $content.Substring($importEnd)
    
    # Backup
    Copy-Item $rawrPath "$rawrPath.backup.$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    
    # Write back
    Set-Content $rawrPath $newContent -NoNewline
    
    Write-Host "[✓] Fix applied successfully!" -ForegroundColor Green
    Write-Host "[*] Backup created at: $rawrPath.backup*" -ForegroundColor Green
}

if ($TestOnly) {
    Write-Host "`n[*] Test Mode - showing code that needs to be added:" -ForegroundColor Yellow
    Write-Host $fixCode
}

Write-Host "`n[*] QUICK APPLICATION GUIDE:" -ForegroundColor Cyan
Write-Host @"
If you're applying this manually:

1. In RawrXD.ps1, find where the code editor RichTextBox is created (line ~7756)

2. BEFORE that line, add the Preserve-RichTextBoxColors function

3. RIGHT AFTER the RichTextBox is created, add:
   
   Preserve-RichTextBoxColors -rtb `$codeEditor -foreColor ([System.Drawing.Color]::White) -backColor ([System.Drawing.Color]::FromArgb(30,30,30))

4. Do the same for ALL RichTextBox and TextBox controls

5. Run the IDE and verify text is now visible on focus
"@
