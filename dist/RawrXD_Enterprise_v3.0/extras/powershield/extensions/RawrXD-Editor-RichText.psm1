# RawrXD Rich Text Editor Extension
# Provides rich text editing capabilities with formatting support
# Author: RawrXD
# Version: 1.0.0

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Extension metadata
# Note: Capabilities must be integer (bitwise OR of capability flags)
# CAP_TEXT_EDITOR = 1024, CAP_FILE_OPERATIONS = 2048, CAP_SYNTAX_HIGHLIGHT = 1
$global:RawrXDEditorRichTextExtension = @{
    Name = "Rich Text Editor"
    Version = "1.0.0"
    Author = "RawrXD"
    Description = "Full-featured rich text editor with formatting and styling"
    Id = "rawrxd-editor-richtext"
    Language = 0  # LANG_CUSTOM
    Capabilities = 3073  # CAP_TEXT_EDITOR (1024) | CAP_FILE_OPERATIONS (2048) | CAP_SYNTAX_HIGHLIGHT (1)
    EditorType = "RichText"
    Dependencies = @()
    Enabled = $true
}

# Extension initialization
function Initialize-RawrXD-Editor-RichTextExtension {
    # Helper for logging
    if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) {
        Write-DevConsole "📝 Initializing Rich Text Editor Extension..." "INFO"
    } else {
        Write-Host "[INFO] 📝 Initializing Rich Text Editor Extension..." -ForegroundColor Cyan
    }

    # Register extension
    if ($global:extensionRegistry) {
        $extension = Register-Extension -Id "rawrxd-editor-richtext" -Name "Rich Text Editor" `
            -Description "Full-featured rich text editor with formatting and styling" `
            -Author "RawrXD" -Language 0 -Capabilities 3073 `
            -Version "1.0.0"
    }

    if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) {
        Write-DevConsole "✅ Rich Text Editor Extension loaded successfully" "SUCCESS"
    } else {
        Write-Host "[SUCCESS] ✅ Rich Text Editor Extension loaded successfully" -ForegroundColor Green
    }
}

# Editor creation functions
function New-RichTextEditor {
    <#
    .SYNOPSIS
        Creates a new RichTextBox editor control
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
                Update-SyntaxHighlighting -Editor $editor
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
        })

        if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) {
            Write-DevConsole "✅ Rich Text Editor created successfully" "SUCCESS"
        }
        return $editor
    }
    catch {
        if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) {
            Write-DevConsole "❌ Failed to create Rich Text Editor: $($_.Exception.Message)" "ERROR"
        }
        return $null
    }
}

function Get-RichTextEditorContent {
    <#
    .SYNOPSIS
        Gets content from a RichTextBox editor
    #>
    param(
        [Parameter(Mandatory = $true)]
        $Editor,

        [switch]$AsRTF
    )

    try {
        if (-not $Editor -or $Editor.GetType().Name -ne "RichTextBox") {
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
        Write-Host "❌ Error getting Rich Text Editor content: $($_.Exception.Message)" -ForegroundColor Red
        return ""
    }
}

function Set-RichTextEditorContent {
    <#
    .SYNOPSIS
        Sets content in a RichTextBox editor
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
        if (-not $Editor -or $Editor.GetType().Name -ne "RichTextBox") {
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
        Write-Host "❌ Error setting Rich Text Editor content: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

function Set-RichTextEditorSettings {
    <#
    .SYNOPSIS
        Updates settings for a RichTextBox editor
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
        if (-not $Editor -or $Editor.GetType().Name -ne "RichTextBox") {
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
        Write-Host "❌ Error updating Rich Text Editor settings: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

function Remove-RichTextEditor {
    <#
    .SYNOPSIS
        Properly disposes of a RichTextBox editor
    #>

    try {
        if ($script:activeEditor -and $script:activeEditor.GetType().Name -eq "RichTextBox") {
            $script:activeEditor.Dispose()
            $script:activeEditor = $null
        }
        Write-Host "✅ Rich Text Editor disposed" -ForegroundColor Green
    }
    catch {
        Write-Host "⚠️ Warning during Rich Text Editor cleanup: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

function Update-SyntaxHighlighting {
    <#
    .SYNOPSIS
        Updates syntax highlighting for the editor content
    #>
    param(
        [Parameter(Mandatory = $true)]
        $Editor
    )

    try {
        if (-not $Editor -or $Editor.GetType().Name -ne "RichTextBox") {
            return
        }

        # Basic syntax highlighting for common patterns
        $content = $Editor.Text

        # Reset formatting
        $Editor.SelectAll()
        $Editor.SelectionColor = $Editor.ForeColor
        $Editor.SelectionFont = $Editor.Font

        # Highlight keywords (basic example)
        $keywords = @("function", "if", "else", "foreach", "while", "try", "catch", "param")
        foreach ($keyword in $keywords) {
            $index = 0
            while (($index = $content.IndexOf($keyword, $index)) -ne -1) {
                $Editor.Select($index, $keyword.Length)
                $Editor.SelectionColor = [System.Drawing.Color]::Blue
                $index += $keyword.Length
            }
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
    "Initialize-RawrXD-Editor-RichTextExtension",
    "New-RichTextEditor",
    "Get-RichTextEditorContent",
    "Set-RichTextEditorContent",
    "Set-RichTextEditorSettings",
    "Remove-RichTextEditor",
    "Update-SyntaxHighlighting"
) -Variable @("RawrXDEditorRichTextExtension")
