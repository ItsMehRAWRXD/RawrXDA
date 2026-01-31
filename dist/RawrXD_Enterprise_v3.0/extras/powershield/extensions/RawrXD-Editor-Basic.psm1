# ============================================
# RawrXD Basic Editor Extension
# ============================================
# Category: Editor
# Purpose: Lightweight text editor using TextBox control
# Author: RawrXD Official
# Version: 2.0.0
# ============================================

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Extension Metadata
# Note: Capabilities must be integer (bitwise OR of capability flags)
# CAP_TEXT_EDITOR = 1024, CAP_FILE_OPERATIONS = 2048
$global:RawrXDBasicEditorExtension = @{
    Id = "rawrxd-editor-basic"
    Name = "RawrXD Basic Editor"
    Description = "Lightweight text editor using TextBox control for maximum speed and minimal resource usage"
    Author = "RawrXD Official"
    Version = "2.0.0"
    Language = 0  # LANG_CUSTOM
    Capabilities = 3072  # CAP_TEXT_EDITOR (1024) | CAP_FILE_OPERATIONS (2048)
    EditorType = "Basic"
    Dependencies = @()
    Enabled = $true
}

# Editor instance and state
$script:editorInstance = $null
$script:editorSettings = @{
    FontFamily = "Consolas"
    FontSize = 11
    BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
    WordWrap = $false
    TabSize = 4
}

# Helper function for logging
function Write-ExtensionLog {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) {
        Write-DevConsole $Message $Level
    } else {
        $color = switch ($Level) {
            "ERROR" { "Red" }
            "WARNING" { "Yellow" }
            "SUCCESS" { "Green" }
            "DEBUG" { "Cyan" }
            default { "White" }
        }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

# ============================================
# EXTENSION INITIALIZATION
# ============================================

function Initialize-RawrXDBasicEditorExtension {
    <#
    .SYNOPSIS
        Initializes the Basic Editor extension
    #>

    Write-ExtensionLog "🎯 Initializing RawrXD Basic Editor Extension" "INFO"

    try {
        # Import required assemblies
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing

        Write-ExtensionLog "✅ Basic Editor Extension loaded successfully" "SUCCESS"
        return $true
    }
    catch {
        Write-ExtensionLog "❌ Failed to initialize Basic Editor: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

# ============================================
# EDITOR CREATION
# ============================================

function New-BasicEditor {
    <#
    .SYNOPSIS
        Creates a new TextBox editor instance (lightweight)
    .OUTPUTS
        System.Windows.Forms.TextBox
    #>

    if ($script:editorInstance) {
        return $script:editorInstance
    }

    try {
        # Create TextBox control (much lighter than RichTextBox)
        $editor = New-Object System.Windows.Forms.TextBox
        $editor.Multiline = $true
        $editor.Dock = [System.Windows.Forms.DockStyle]::Fill
        $editor.Font = New-Object System.Drawing.Font($script:editorSettings.FontFamily, $script:editorSettings.FontSize)
        $editor.BackColor = $script:editorSettings.BackColor
        $editor.ForeColor = $script:editorSettings.ForeColor
        $editor.WordWrap = $script:editorSettings.WordWrap
        $editor.AcceptsTab = $true
        $editor.AcceptsReturn = $true
        $editor.ScrollBars = [System.Windows.Forms.ScrollBars]::Both

        # Add minimal event handlers
        Register-BasicEditorEvents -Editor $editor

        $script:editorInstance = $editor
        Write-ExtensionLog "✅ Basic editor instance created (TextBox)" "SUCCESS"

        return $editor
    }
    catch {
        Write-ExtensionLog "❌ Failed to create Basic editor: $($_.Exception.Message)" "ERROR"
        return $null
    }
}

function Register-BasicEditorEvents {
    <#
    .SYNOPSIS
        Registers minimal event handlers for the basic editor
    #>
    param(
        [Parameter(Mandatory = $true)]
        [System.Windows.Forms.TextBox]$Editor
    )

    try {
        # Key down for Tab handling only (keep it minimal)
        $Editor.add_KeyDown({
            param($sender, $e)

            # Handle Tab key for proper indentation
            if ($e.KeyCode -eq [System.Windows.Forms.Keys]::Tab -and -not $e.Control -and -not $e.Alt) {
                $sender.SelectedText = "`t"
                $e.Handled = $true
            }
        })

        Write-ExtensionLog "✅ Basic editor events registered" "SUCCESS"
    }
    catch {
        Write-ExtensionLog "⚠️ Warning: Could not register all basic editor events: $($_.Exception.Message)" "WARNING"
    }
}

# ============================================
# CONTENT MANAGEMENT
# ============================================

function Set-BasicEditorContent {
    <#
    .SYNOPSIS
        Sets content in the Basic editor (TextBox only supports plain text)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Content,

        [Parameter(Mandatory = $true)]
        [System.Windows.Forms.TextBox]$Editor
    )

    try {
        if (-not $Editor -or $Editor.IsDisposed) {
            Write-ExtensionLog "⚠️ Editor is null or disposed" "WARNING"
            return $false
        }

        # TextBox only supports plain text - always use Text property
        # PowerShell 5.1 compatible null-coalescing
        $Editor.Text = if ($null -ne $Content) { $Content } else { "" }

        return $true
    }
    catch {
        Write-ExtensionLog "❌ Failed to set basic editor content: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Get-BasicEditorContent {
    <#
    .SYNOPSIS
        Gets content from the Basic editor
    #>
    param(
        [Parameter(Mandatory = $true)]
        [System.Windows.Forms.TextBox]$Editor
    )

    try {
        if (-not $Editor -or $Editor.IsDisposed) {
            return ""
        }

        # PowerShell 5.1 compatible null-coalescing
        $text = $Editor.Text
        return if ($null -ne $text) { $text } else { "" }
    }
    catch {
        Write-ExtensionLog "❌ Failed to get basic editor content: $($_.Exception.Message)" "ERROR"
        return ""
    }
}

# ============================================
# SETTINGS MANAGEMENT
# ============================================

function Set-BasicEditorSettings {
    <#
    .SYNOPSIS
        Updates basic editor settings and applies them
    #>
    param(
        [string]$FontFamily,
        [int]$FontSize,
        [System.Drawing.Color]$BackColor,
        [System.Drawing.Color]$ForeColor,
        [bool]$WordWrap,
        [int]$TabSize
    )

    try {
        if ($FontFamily) { $script:editorSettings.FontFamily = $FontFamily }
        if ($FontSize -gt 0) { $script:editorSettings.FontSize = $FontSize }
        if ($BackColor) { $script:editorSettings.BackColor = $BackColor }
        if ($ForeColor) { $script:editorSettings.ForeColor = $ForeColor }
        if ($PSBoundParameters.ContainsKey('WordWrap')) { $script:editorSettings.WordWrap = $WordWrap }
        if ($TabSize -gt 0) { $script:editorSettings.TabSize = $TabSize }

        # Apply to existing editor instance
        if ($script:editorInstance) {
            Apply-SettingsToBasicEditor -Editor $script:editorInstance
        }

        Write-ExtensionLog "✅ Basic editor settings updated" "SUCCESS"
        return $true
    }
    catch {
        Write-ExtensionLog "❌ Failed to update basic editor settings: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Apply-SettingsToBasicEditor {
    <#
    .SYNOPSIS
        Applies current settings to a basic editor instance
    #>
    param(
        [Parameter(Mandatory = $true)]
        [System.Windows.Forms.TextBox]$Editor
    )

    try {
        $Editor.Font = New-Object System.Drawing.Font($script:editorSettings.FontFamily, $script:editorSettings.FontSize)
        $Editor.BackColor = $script:editorSettings.BackColor
        $Editor.ForeColor = $script:editorSettings.ForeColor
        $Editor.WordWrap = $script:editorSettings.WordWrap

        Write-ExtensionLog "✅ Settings applied to basic editor" "SUCCESS"
    }
    catch {
        Write-ExtensionLog "⚠️ Warning: Could not apply all settings to basic editor: $($_.Exception.Message)" "WARNING"
    }
}

# ============================================
# CLEANUP
# ============================================

function Remove-BasicEditor {
    <#
    .SYNOPSIS
        Properly disposes of the basic editor instance
    #>

    try {
        if ($script:editorInstance) {
            $script:editorInstance.Dispose()
            $script:editorInstance = $null
        }
        Write-ExtensionLog "✅ Basic editor disposed" "SUCCESS"
    }
    catch {
        Write-ExtensionLog "⚠️ Warning during basic editor cleanup: $($_.Exception.Message)" "WARNING"
    }
}

# ============================================
# EXPORT FUNCTIONS
# ============================================

Export-ModuleMember -Function @(
    "Initialize-RawrXDBasicEditorExtension",
    "New-BasicEditor",
    "Set-BasicEditorContent",
    "Get-BasicEditorContent",
    "Set-BasicEditorSettings",
    "Remove-BasicEditor"
) -Variable @("RawrXDBasicEditorExtension")
