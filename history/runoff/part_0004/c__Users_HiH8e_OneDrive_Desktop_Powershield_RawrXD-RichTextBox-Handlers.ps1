<#
.SYNOPSIS
    RichTextBox Editor Handlers for Menu System Integration
    
.DESCRIPTION
    This module provides PowerShell functions that bridge JavaScript menu commands
    to the Windows Forms RichTextBox editor. When menu calls Set-Theme, PowerShell
    changes RichTextBox colors directly.
    
    Time Estimate: 6-8 hours (Option 1: Keep Windows Forms, wire menu handlers)
#>

# ============================================================
# THEME DEFINITIONS
# ============================================================

$script:EditorThemes = @{
    "Dark+" = @{
        Background = [System.Drawing.Color]::FromArgb(30, 30, 30)
        Foreground = [System.Drawing.Color]::FromArgb(220, 220, 220)
        Keyword    = [System.Drawing.Color]::FromArgb(86, 156, 214)   # Blue
        String     = [System.Drawing.Color]::FromArgb(206, 145, 120)   # Orange
        Comment    = [System.Drawing.Color]::FromArgb(106, 153, 85)    # Green
        Function   = [System.Drawing.Color]::FromArgb(220, 220, 170)  # Yellow
        Variable   = [System.Drawing.Color]::FromArgb(156, 220, 254)   # Light blue
        Number     = [System.Drawing.Color]::FromArgb(181, 206, 168)  # Light green
        Operator   = [System.Drawing.Color]::FromArgb(180, 180, 180)   # Gray
    }
    "Light+" = @{
        Background = [System.Drawing.Color]::FromArgb(255, 255, 255)
        Foreground = [System.Drawing.Color]::FromArgb(30, 30, 30)
        Keyword    = [System.Drawing.Color]::FromArgb(0, 0, 255)       # Blue
        String     = [System.Drawing.Color]::FromArgb(163, 21, 21)     # Red
        Comment    = [System.Drawing.Color]::FromArgb(0, 128, 0)       # Green
        Function   = [System.Drawing.Color]::FromArgb(121, 94, 38)      # Brown
        Variable   = [System.Drawing.Color]::FromArgb(0, 0, 255)        # Blue
        Number     = [System.Drawing.Color]::FromArgb(9, 134, 88)       # Green
        Operator   = [System.Drawing.Color]::FromArgb(0, 0, 0)            # Black
    }
    "Monokai" = @{
        Background = [System.Drawing.Color]::FromArgb(39, 40, 34)
        Foreground = [System.Drawing.Color]::FromArgb(248, 248, 242)
        Keyword    = [System.Drawing.Color]::FromArgb(249, 38, 114)     # Pink
        String     = [System.Drawing.Color]::FromArgb(230, 219, 116)   # Yellow
        Comment    = [System.Drawing.Color]::FromArgb(117, 113, 94)    # Gray
        Function   = [System.Drawing.Color]::FromArgb(166, 226, 46)    # Green
        Variable   = [System.Drawing.Color]::FromArgb(253, 151, 31)   # Orange
        Number     = [System.Drawing.Color]::FromArgb(174, 129, 255)   # Purple
        Operator   = [System.Drawing.Color]::FromArgb(248, 248, 242)    # White
    }
    "Solarized Dark" = @{
        Background = [System.Drawing.Color]::FromArgb(0, 43, 54)
        Foreground = [System.Drawing.Color]::FromArgb(131, 148, 150)
        Keyword    = [System.Drawing.Color]::FromArgb(38, 139, 210)    # Blue
        String     = [System.Drawing.Color]::FromArgb(220, 50, 47)     # Red
        Comment    = [System.Drawing.Color]::FromArgb(88, 110, 117)    # Gray
        Function   = [System.Drawing.Color]::FromArgb(133, 153, 0)     # Green
        Variable   = [System.Drawing.Color]::FromArgb(181, 137, 0)     # Yellow
        Number     = [System.Drawing.Color]::FromArgb(211, 54, 130)   # Magenta
        Operator   = [System.Drawing.Color]::FromArgb(131, 148, 150)   # Base
    }
    "Dracula" = @{
        Background = [System.Drawing.Color]::FromArgb(40, 42, 54)
        Foreground = [System.Drawing.Color]::FromArgb(248, 248, 242)
        Keyword    = [System.Drawing.Color]::FromArgb(255, 121, 198)   # Pink
        String     = [System.Drawing.Color]::FromArgb(241, 250, 140)   # Yellow
        Comment    = [System.Drawing.Color]::FromArgb(98, 114, 164)   # Blue-gray
        Function   = [System.Drawing.Color]::FromArgb(80, 250, 123)    # Green
        Variable   = [System.Drawing.Color]::FromArgb(189, 147, 249)  # Purple
        Number     = [System.Drawing.Color]::FromArgb(255, 184, 108)  # Orange
        Operator   = [System.Drawing.Color]::FromArgb(255, 121, 198)  # Pink
    }
    "Nord" = @{
        Background = [System.Drawing.Color]::FromArgb(46, 52, 64)
        Foreground = [System.Drawing.Color]::FromArgb(216, 222, 233)
        Keyword    = [System.Drawing.Color]::FromArgb(136, 192, 208)   # Blue
        String     = [System.Drawing.Color]::FromArgb(163, 190, 140)  # Green
        Comment    = [System.Drawing.Color]::FromArgb(129, 161, 193)  # Light blue
        Function   = [System.Drawing.Color]::FromArgb(143, 188, 187)  # Teal
        Variable   = [System.Drawing.Color]::FromArgb(180, 142, 173)  # Purple
        Number     = [System.Drawing.Color]::FromArgb(136, 192, 208)  # Blue
        Operator   = [System.Drawing.Color]::FromArgb(216, 222, 233)  # White
    }
    "One Dark Pro" = @{
        Background = [System.Drawing.Color]::FromArgb(40, 44, 52)
        Foreground = [System.Drawing.Color]::FromArgb(171, 178, 191)
        Keyword    = [System.Drawing.Color]::FromArgb(198, 120, 221)   # Purple
        String     = [System.Drawing.Color]::FromArgb(152, 195, 121)   # Green
        Comment    = [System.Drawing.Color]::FromArgb(92, 99, 112)    # Gray
        Function   = [System.Drawing.Color]::FromArgb(97, 175, 239)     # Blue
        Variable   = [System.Drawing.Color]::FromArgb(224, 108, 117)    # Red
        Number     = [System.Drawing.Color]::FromArgb(209, 154, 102)   # Orange
        Operator   = [System.Drawing.Color]::FromArgb(171, 178, 191)   # Base
    }
}

$script:CurrentTheme = "Dark+"

# ============================================================
# RICHTEXTBOX EDITOR FUNCTIONS
# ============================================================

function Set-RichTextBoxTheme {
    <#
    .SYNOPSIS
        Sets the theme for the RichTextBox editor
        
    .PARAMETER ThemeName
        Name of the theme to apply (Dark+, Light+, Monokai, etc.)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ThemeName
    )
    
    if (-not $script:editor) {
        Write-DevConsole "[Theme] Editor not available" "WARNING"
        return
    }
    
    if (-not $script:EditorThemes.ContainsKey($ThemeName)) {
        Write-DevConsole "[Theme] Unknown theme: $ThemeName" "WARNING"
        return
    }
    
    $theme = $script:EditorThemes[$ThemeName]
    $script:CurrentTheme = $ThemeName
    
    try {
        # Ensure we're on the UI thread
        if ($script:editor.InvokeRequired) {
            $script:editor.Invoke([System.Action] {
                param($t)
                $script:editor.BackColor = $t.Background
                $script:editor.ForeColor = $t.Foreground
                
                # Apply theme to all existing text
                if ($script:editor.Text.Length -gt 0) {
                    $selStart = $script:editor.SelectionStart
                    $selLength = $script:editor.SelectionLength
                    
                    $script:editor.SelectAll()
                    $script:editor.SelectionColor = $t.Foreground
                    $script:editor.DeselectAll()
                    
                    $script:editor.SelectionStart = $selStart
                    $script:editor.SelectionLength = $selLength
                }
                
                # Set default color for new text
                $script:editor.SelectionColor = $t.Foreground
            }, $theme)
        }
        else {
            $script:editor.BackColor = $theme.Background
            $script:editor.ForeColor = $theme.Foreground
            
            # Apply theme to all existing text
            if ($script:editor.Text.Length -gt 0) {
                $selStart = $script:editor.SelectionStart
                $selLength = $script:editor.SelectionLength
                
                $script:editor.SelectAll()
                $script:editor.SelectionColor = $theme.Foreground
                $script:editor.DeselectAll()
                
                $script:editor.SelectionStart = $selStart
                $script:editor.SelectionLength = $selLength
            }
            
            # Set default color for new text
            $script:editor.SelectionColor = $theme.Foreground
        }
        
        Write-DevConsole "[Theme] Applied theme: $ThemeName" "SUCCESS"
    }
    catch {
        Write-DevConsole "[Theme] Error applying theme: $_" "ERROR"
    }
}

function Set-RichTextBoxFontSize {
    <#
    .SYNOPSIS
        Sets the font size for the RichTextBox editor
        
    .PARAMETER Size
        Font size in points
    #>
    param(
        [Parameter(Mandatory = $true)]
        [int]$Size
    )
    
    if (-not $script:editor) {
        Write-DevConsole "[FontSize] Editor not available" "WARNING"
        return
    }
    
    if ($Size -lt 8 -or $Size -gt 72) {
        Write-DevConsole "[FontSize] Invalid size: $Size (must be 8-72)" "WARNING"
        return
    }
    
    try {
        $currentFont = $script:editor.Font
        $newFont = New-Object System.Drawing.Font($currentFont.FontFamily, $Size, $currentFont.Style)
        
        if ($script:editor.InvokeRequired) {
            $script:editor.Invoke([System.Action] {
                param($f)
                $script:editor.Font = $f
            }, $newFont)
        }
        else {
            $script:editor.Font = $newFont
        }
        
        Write-DevConsole "[FontSize] Set font size to $Size" "SUCCESS"
    }
    catch {
        Write-DevConsole "[FontSize] Error setting font size: $_" "ERROR"
    }
}

function Set-RichTextBoxFontFamily {
    <#
    .SYNOPSIS
        Sets the font family for the RichTextBox editor
        
    .PARAMETER FontFamily
        Font family name (e.g., "Consolas", "Fira Code", "JetBrains Mono")
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$FontFamily
    )
    
    if (-not $script:editor) {
        Write-DevConsole "[FontFamily] Editor not available" "WARNING"
        return
    }
    
    try {
        $currentFont = $script:editor.Font
        $newFont = New-Object System.Drawing.Font($FontFamily, $currentFont.Size, $currentFont.Style)
        
        if ($script:editor.InvokeRequired) {
            $script:editor.Invoke([System.Action] {
                param($f)
                $script:editor.Font = $f
            }, $newFont)
        }
        else {
            $script:editor.Font = $newFont
        }
        
        Write-DevConsole "[FontFamily] Set font family to $FontFamily" "SUCCESS"
    }
    catch {
        Write-DevConsole "[FontFamily] Error setting font family: $_" "ERROR"
    }
}

function Set-RichTextBoxTabSize {
    <#
    .SYNOPSIS
        Sets the tab size (indentation) for the RichTextBox editor
        
    .PARAMETER Size
        Number of spaces per tab (2, 4, or 8)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet(2, 4, 8)]
        [int]$Size
    )
    
    if (-not $script:editor) {
        Write-DevConsole "[TabSize] Editor not available" "WARNING"
        return
    }
    
    try {
        # RichTextBox doesn't have a direct TabSize property
        # We'll store it in settings and use it for indentation
        if (-not $global:settings) {
            $global:settings = @{}
        }
        $global:settings.TabSize = $Size
        
        Write-DevConsole "[TabSize] Set tab size to $Size spaces" "SUCCESS"
    }
    catch {
        Write-DevConsole "[TabSize] Error setting tab size: $_" "ERROR"
    }
}

function Set-RichTextBoxWordWrap {
    <#
    .SYNOPSIS
        Enables or disables word wrap in the RichTextBox editor
        
    .PARAMETER Enabled
        True to enable word wrap, false to disable
    #>
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Enabled
    )
    
    if (-not $script:editor) {
        Write-DevConsole "[WordWrap] Editor not available" "WARNING"
        return
    }
    
    try {
        if ($script:editor.InvokeRequired) {
            $script:editor.Invoke([System.Action] {
                param($enabled)
                $script:editor.WordWrap = $enabled
            }, $Enabled)
        }
        else {
            $script:editor.WordWrap = $Enabled
        }
        
        Write-DevConsole "[WordWrap] Word wrap $(if ($Enabled) { 'enabled' } else { 'disabled' })" "SUCCESS"
    }
    catch {
        Write-DevConsole "[WordWrap] Error setting word wrap: $_" "ERROR"
    }
}

function Set-RichTextBoxLineNumbers {
    <#
    .SYNOPSIS
        Enables or disables line numbers display (placeholder for future implementation)
        
    .PARAMETER Enabled
        True to enable line numbers, false to disable
    #>
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Enabled
    )
    
    # RichTextBox doesn't have built-in line numbers
    # This is a placeholder for future implementation with a custom control
    Write-DevConsole "[LineNumbers] Line numbers $(if ($Enabled) { 'enabled' } else { 'disabled' }) (feature coming soon)" "INFO"
}

function Set-RichTextBoxZoom {
    <#
    .SYNOPSIS
        Sets the zoom level for the RichTextBox editor
        
    .PARAMETER ZoomPercent
        Zoom percentage (75-200)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [ValidateRange(75, 200)]
        [int]$ZoomPercent
    )
    
    if (-not $script:editor) {
        Write-DevConsole "[Zoom] Editor not available" "WARNING"
        return
    }
    
    try {
        # Calculate new font size based on zoom
        $baseSize = 10  # Default base size
        $newSize = [math]::Round($baseSize * ($ZoomPercent / 100))
        
        Set-RichTextBoxFontSize -Size $newSize
        Write-DevConsole "[Zoom] Set zoom to $ZoomPercent%" "SUCCESS"
    }
    catch {
        Write-DevConsole "[Zoom] Error setting zoom: $_" "ERROR"
    }
}

function Get-RichTextBoxTheme {
    <#
    .SYNOPSIS
        Gets the current theme name
    #>
    return $script:CurrentTheme
}

function Get-AvailableThemes {
    <#
    .SYNOPSIS
        Gets list of available theme names
    #>
    return $script:EditorThemes.Keys | Sort-Object
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
# Functions are automatically available in the parent scope when dot-sourced
# Export-ModuleMember -Function @(
#     'Set-RichTextBoxTheme',
#     'Set-RichTextBoxFontSize',
#     'Set-RichTextBoxFontFamily',
#     'Set-RichTextBoxTabSize',
#     'Set-RichTextBoxWordWrap',
#     'Set-RichTextBoxLineNumbers',
#     'Set-RichTextBoxZoom',
#     'Get-RichTextBoxTheme',
#     'Get-AvailableThemes'
# )

