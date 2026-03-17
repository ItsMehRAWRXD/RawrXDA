# RawrXD Advanced Editor Extension
# Provides advanced text editing with toolbar and enhanced features
# Author: RawrXD
# Version: 1.0.0

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Extension metadata
$global:RawrXDEditorAdvancedExtension = @{
    Name = "Advanced Editor"
    Version = "1.0.0"
    Author = "RawrXD"
    Description = "Advanced text editor with toolbar and enhanced editing features"
    Id = "rawrxd-editor-advanced"
    Capabilities = @("TextEditor", "Advanced", "Toolbar")
    EditorType = "Advanced"
    Dependencies = @()
    Enabled = $true
}

# Extension initialization
function Initialize-RawrXD-Editor-AdvancedExtension {
    Write-DevConsole "🔧 Initializing Advanced Editor Extension..." "INFO"

    # Register extension
    if ($global:extensionRegistry) {
        $extension = Register-Extension -Id "rawrxd-editor-advanced" -Name "Advanced Editor" `
            -Description "Advanced text editor with toolbar and enhanced editing features" `
            -Author "RawrXD" -Capabilities @("TextEditor", "Advanced", "Toolbar") `
            -Version "1.0.0" -EditorType "Advanced"
    }

    Write-DevConsole "✅ Advanced Editor Extension loaded successfully" "SUCCESS"
}

# Editor creation functions
function New-AdvancedEditor {
    <#
    .SYNOPSIS
        Creates a new advanced editor with toolbar and RichTextBox
    #>
    param(
        [Parameter(Mandatory = $false)]
        [string]$InitialText = "",

        [Parameter(Mandatory = $false)]
        [System.Drawing.Font]$Font,

        [Parameter(Mandatory = $false)]
        [System.Drawing.Color]$BackColor = [System.Drawing.Color]::White,

        [Parameter(Mandatory = $false)]
        [System.Drawing.Color]$ForeColor = [System.Drawing.Color]::Black,

        [Parameter(Mandatory = $false)]
        [bool]$WordWrap = $true
    )

    try {
        # Create main container panel
        $container = New-Object System.Windows.Forms.Panel
        $container.Dock = [System.Windows.Forms.DockStyle]::Fill
        $container.BackColor = [System.Drawing.Color]::FromArgb(245, 245, 245)

        # Create toolbar
        $toolbar = New-Object System.Windows.Forms.ToolStrip
        $toolbar.Dock = [System.Windows.Forms.DockStyle]::Top
        $toolbar.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 240)

        # Add toolbar buttons
        $boldButton = New-Object System.Windows.Forms.ToolStripButton
        $boldButton.Text = "B"
        $boldButton.ToolTipText = "Bold"
        $boldButton.Font = New-Object System.Drawing.Font("Arial", 10, [System.Drawing.FontStyle]::Bold)
        $boldButton.add_Click({
            $richTextBox = $container.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] } | Select-Object -First 1
            if ($richTextBox) {
                Toggle-Bold -Editor $richTextBox
            }
        })
        $toolbar.Items.Add($boldButton) | Out-Null

        $italicButton = New-Object System.Windows.Forms.ToolStripButton
        $italicButton.Text = "I"
        $italicButton.ToolTipText = "Italic"
        $italicButton.Font = New-Object System.Drawing.Font("Arial", 10, [System.Drawing.FontStyle]::Italic)
        $italicButton.add_Click({
            $richTextBox = $container.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] } | Select-Object -First 1
            if ($richTextBox) {
                Toggle-Italic -Editor $richTextBox
            }
        })
        $toolbar.Items.Add($italicButton) | Out-Null

        $underlineButton = New-Object System.Windows.Forms.ToolStripButton
        $underlineButton.Text = "U"
        $underlineButton.ToolTipText = "Underline"
        $underlineButton.Font = New-Object System.Drawing.Font("Arial", 10, [System.Drawing.FontStyle]::Underline)
        $underlineButton.add_Click({
            $richTextBox = $container.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] } | Select-Object -First 1
            if ($richTextBox) {
                Toggle-Underline -Editor $richTextBox
            }
        })
        $toolbar.Items.Add($underlineButton) | Out-Null

        # Separator
        $toolbar.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator)) | Out-Null

        # Font size combo
        $fontSizeCombo = New-Object System.Windows.Forms.ToolStripComboBox
        $fontSizeCombo.ToolTipText = "Font Size"
        $fontSizeCombo.Items.AddRange(@("8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "28", "32"))
        $fontSizeCombo.SelectedItem = "10"
        $fontSizeCombo.add_SelectedIndexChanged({
            $richTextBox = $container.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] } | Select-Object -First 1
            if ($richTextBox -and $fontSizeCombo.SelectedItem) {
                $size = [int]::Parse($fontSizeCombo.SelectedItem)
                if ($richTextBox.SelectionLength -gt 0) {
                    $richTextBox.SelectionFont = New-Object System.Drawing.Font($richTextBox.SelectionFont.FontFamily, $size, $richTextBox.SelectionFont.Style)
                }
            }
        })
        $toolbar.Items.Add($fontSizeCombo) | Out-Null

        $container.Controls.Add($toolbar)

        # Create RichTextBox
        $editor = New-Object System.Windows.Forms.RichTextBox
        $editor.Dock = [System.Windows.Forms.DockStyle]::Fill
        $editor.Multiline = $true
        $editor.ScrollBars = [System.Windows.Forms.RichTextBoxScrollBars]::Both
        $editor.WordWrap = $WordWrap
        $editor.AcceptsTab = $true
        $editor.DetectUrls = $true

        # Set font if provided
        if ($Font) {
            $editor.Font = $Font
        }
        else {
            $editor.Font = New-Object System.Drawing.Font("Consolas", 10)
        }

        # Set colors
        $editor.BackColor = $BackColor
        $editor.ForeColor = $ForeColor

        # Set initial text
        if ($InitialText) {
            $editor.Text = $InitialText
        }

        # Event handlers
        $editor.add_TextChanged({
            # Update syntax highlighting if enabled
            if ($script:syntaxHighlightingEnabled) {
                Update-AdvancedSyntaxHighlighting -Editor $editor
            }
        })

        $editor.add_KeyDown({
            param($sender, $e)
            # Handle special key combinations
            if ($e.Control -and $e.KeyCode -eq [System.Windows.Forms.Keys]::S) {
                $e.Handled = $true
                # Save current file
                if ($script:currentFilePath) {
                    Save-File -FilePath $script:currentFilePath -Content $editor.Text
                }
            }
            elseif ($e.Control -and $e.KeyCode -eq [System.Windows.Forms.Keys]::B) {
                $e.Handled = $true
                Toggle-Bold -Editor $editor
            }
            elseif ($e.Control -and $e.KeyCode -eq [System.Windows.Forms.Keys]::I) {
                $e.Handled = $true
                Toggle-Italic -Editor $editor
            }
            elseif ($e.Control -and $e.KeyCode -eq [System.Windows.Forms.Keys]::U) {
                $e.Handled = $true
                Toggle-Underline -Editor $editor
            }
        })

        $container.Controls.Add($editor)
        $container.Tag = @{ Editor = $editor; Toolbar = $toolbar }

        Write-DevConsole "✅ Advanced Editor created successfully" "SUCCESS"
        return $container
    }
    catch {
        Write-DevConsole "❌ Failed to create Advanced Editor: $($_.Exception.Message)" "ERROR"
        return $null
    }
}

function Get-AdvancedEditorContent {
    <#
    .SYNOPSIS
        Gets content from an advanced editor
    #>
    param(
        [Parameter(Mandatory = $true)]
        $Editor,

        [switch]$AsRTF
    )

    try {
        if (-not $Editor) {
            return ""
        }

        # If it's a Panel container, get the RichTextBox inside
        if ($Editor -is [System.Windows.Forms.Panel]) {
            $richTextBox = $Editor.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] } | Select-Object -First 1
            if ($richTextBox) {
                $Editor = $richTextBox
            }
            else {
                return ""
            }
        }

        if ($Editor.GetType().Name -ne "RichTextBox") {
            return ""
        }

        if ($AsRTF) {
            return $Editor.Rtf
        }
        else {
            return $Editor.Text
        }
    }
    catch {
        Write-Host "❌ Error getting Advanced Editor content: $($_.Exception.Message)" -ForegroundColor Red
        return ""
    }
}

function Set-AdvancedEditorContent {
    <#
    .SYNOPSIS
        Sets content in an advanced editor
    #>
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Content,

        [Parameter(Mandatory = $true)]
        $Editor,

        [switch]$AsRTF
    )

    try {
        if (-not $Editor) {
            return $false
        }

        # If it's a Panel container, get the RichTextBox inside
        if ($Editor -is [System.Windows.Forms.Panel]) {
            $richTextBox = $Editor.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] } | Select-Object -First 1
            if ($richTextBox) {
                $Editor = $richTextBox
            }
            else {
                return $false
            }
        }

        if ($Editor.GetType().Name -ne "RichTextBox") {
            return $false
        }

        if ($AsRTF) {
            $Editor.Rtf = $Content
        }
        else {
            $Editor.Text = $Content
        }

        return $true
    }
    catch {
        Write-Host "❌ Error setting Advanced Editor content: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

function Set-AdvancedEditorSettings {
    <#
    .SYNOPSIS
        Updates settings for an advanced editor
    #>
    param(
        [Parameter(Mandatory = $true)]
        $Editor,

        [string]$FontFamily,
        [int]$FontSize,
        [System.Drawing.Color]$BackColor,
        [System.Drawing.Color]$ForeColor,
        [bool]$WordWrap
    )

    try {
        if (-not $Editor) {
            return $false
        }

        # If it's a Panel container, get the RichTextBox inside
        if ($Editor -is [System.Windows.Forms.Panel]) {
            $richTextBox = $Editor.Controls | Where-Object { $_ -is [System.Windows.Forms.RichTextBox] } | Select-Object -First 1
            if ($richTextBox) {
                $Editor = $richTextBox
            }
            else {
                return $false
            }
        }

        if ($Editor.GetType().Name -ne "RichTextBox") {
            return $false
        }

        # Update font
        if ($FontFamily -or $FontSize) {
            $currentFont = $Editor.Font
            $newFamily = if ($FontFamily) { $FontFamily } else { $currentFont.FontFamily.Name }
            $newSize = if ($FontSize -gt 0) { $FontSize } else { $currentFont.Size }
            $Editor.Font = New-Object System.Drawing.Font($newFamily, $newSize)
        }

        # Update colors
        if ($BackColor) { $Editor.BackColor = $BackColor }
        if ($ForeColor) { $Editor.ForeColor = $ForeColor }

        # Update word wrap
        if ($PSBoundParameters.ContainsKey('WordWrap')) {
            $Editor.WordWrap = $WordWrap
        }

        return $true
    }
    catch {
        Write-Host "❌ Error updating Advanced Editor settings: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

function Remove-AdvancedEditor {
    <#
    .SYNOPSIS
        Properly disposes of an advanced editor
    #>

    try {
        if ($script:activeEditor -and $script:activeEditor -is [System.Windows.Forms.Panel]) {
            $script:activeEditor.Dispose()
            $script:activeEditor = $null
        }
        Write-Host "✅ Advanced Editor disposed" -ForegroundColor Green
    }
    catch {
        Write-Host "⚠️ Warning during Advanced Editor cleanup: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

function Toggle-Bold {
    <#
    .SYNOPSIS
        Toggles bold formatting for selected text
    #>
    param([Parameter(Mandatory = $true)]$Editor)

    try {
        if ($Editor.SelectionLength -gt 0) {
            $currentFont = $Editor.SelectionFont
            $newStyle = if ($currentFont.Bold) {
                $currentFont.Style -band -bnot [System.Drawing.FontStyle]::Bold
            } else {
                $currentFont.Style -bor [System.Drawing.FontStyle]::Bold
            }
            $Editor.SelectionFont = New-Object System.Drawing.Font($currentFont.FontFamily, $currentFont.Size, $newStyle)
        }
    }
    catch {
        # Silently ignore formatting errors
    }
}

function Toggle-Italic {
    <#
    .SYNOPSIS
        Toggles italic formatting for selected text
    #>
    param([Parameter(Mandatory = $true)]$Editor)

    try {
        if ($Editor.SelectionLength -gt 0) {
            $currentFont = $Editor.SelectionFont
            $newStyle = if ($currentFont.Italic) {
                $currentFont.Style -band -bnot [System.Drawing.FontStyle]::Italic
            } else {
                $currentFont.Style -bor [System.Drawing.FontStyle]::Italic
            }
            $Editor.SelectionFont = New-Object System.Drawing.Font($currentFont.FontFamily, $currentFont.Size, $newStyle)
        }
    }
    catch {
        # Silently ignore formatting errors
    }
}

function Toggle-Underline {
    <#
    .SYNOPSIS
        Toggles underline formatting for selected text
    #>
    param([Parameter(Mandatory = $true)]$Editor)

    try {
        if ($Editor.SelectionLength -gt 0) {
            $currentFont = $Editor.SelectionFont
            $newStyle = if ($currentFont.Underline) {
                $currentFont.Style -band -bnot [System.Drawing.FontStyle]::Underline
            } else {
                $currentFont.Style -bor [System.Drawing.FontStyle]::Underline
            }
            $Editor.SelectionFont = New-Object System.Drawing.Font($currentFont.FontFamily, $currentFont.Size, $newStyle)
        }
    }
    catch {
        # Silently ignore formatting errors
    }
}

function Update-AdvancedSyntaxHighlighting {
    <#
    .SYNOPSIS
        Updates advanced syntax highlighting for the editor content
    #>
    param(
        [Parameter(Mandatory = $true)]
        $Editor
    )

    try {
        if (-not $Editor -or $Editor.GetType().Name -ne "RichTextBox") {
            return
        }

        # Enhanced syntax highlighting
        $content = $Editor.Text

        # Reset formatting
        $Editor.SelectAll()
        $Editor.SelectionColor = $Editor.ForeColor
        $Editor.SelectionFont = New-Object System.Drawing.Font($Editor.Font.FontFamily, $Editor.Font.Size, [System.Drawing.FontStyle]::Regular)

        # Highlight keywords with different colors
        $keywords = @{
            "function" = [System.Drawing.Color]::Blue
            "if" = [System.Drawing.Color]::DarkGreen
            "else" = [System.Drawing.Color]::DarkGreen
            "elseif" = [System.Drawing.Color]::DarkGreen
            "foreach" = [System.Drawing.Color]::Purple
            "while" = [System.Drawing.Color]::Purple
            "try" = [System.Drawing.Color]::DarkRed
            "catch" = [System.Drawing.Color]::DarkRed
            "param" = [System.Drawing.Color]::DarkOrange
            "return" = [System.Drawing.Color]::DarkOrange
        }

        foreach ($keyword in $keywords.Keys) {
            $index = 0
            while (($index = $content.IndexOf($keyword, $index, [System.StringComparison]::OrdinalIgnoreCase)) -ne -1) {
                $Editor.Select($index, $keyword.Length)
                $Editor.SelectionColor = $keywords[$keyword]
                $index += $keyword.Length
            }
        }

        # Highlight strings
        $stringPattern = '"[^"]*"'
        $matches = [regex]::Matches($content, $stringPattern)
        foreach ($match in $matches) {
            $Editor.Select($match.Index, $match.Length)
            $Editor.SelectionColor = [System.Drawing.Color]::DarkRed
        }

        # Highlight comments
        $commentPattern = '#.*$'
        $matches = [regex]::Matches($content, $commentPattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
        foreach ($match in $matches) {
            $Editor.Select($match.Index, $match.Length)
            $Editor.SelectionColor = [System.Drawing.Color]::Green
        }

        # Reset selection
        $Editor.Select($Editor.Text.Length, 0)
    }
    catch {
        # Silently ignore syntax highlighting errors
    }
}

# Export functions
Export-ModuleMember -Function @(
    "Initialize-RawrXD-Editor-AdvancedExtension",
    "New-AdvancedEditor",
    "Get-AdvancedEditorContent",
    "Set-AdvancedEditorContent",
    "Set-AdvancedEditorSettings",
    "Remove-AdvancedEditor",
    "Toggle-Bold",
    "Toggle-Italic",
    "Toggle-Underline",
    "Update-AdvancedSyntaxHighlighting"
) -Variable @("RawrXDEditorAdvancedExtension")