# RawrXD Basic Editor Extension# ============================================

# Provides basic text editing capabilities# RawrXD Basic Editor Extension  

# Author: RawrXD# ============================================

# Version: 1.0.0# Lightweight text editor using TextBox control - fast and minimal



Add-Type -AssemblyName System.Windows.Forms# Extension Metadata

Add-Type -AssemblyName System.Drawing$global:RawrXDBasicEditorExtension = @{

    Id = "rawrxd-editor-basic"

# Extension metadata    Name = "RawrXD Basic Editor"

$global:RawrXDEditorBasicExtension = @{    Description = "Lightweight text editor using TextBox control for maximum speed and minimal resource usage"

    Name = "Basic Editor"    Author = "RawrXD Official"  

    Version = "1.0.0"    Language = 0  # LANG_CUSTOM

    Author = "RawrXD"    Capabilities = (1024 -bor 2048)  # CAP_TEXT_EDITOR | CAP_FILE_OPERATIONS

    Description = "Simple text editor with basic functionality"    Version = "2.0.0"

    Id = "rawrxd-editor-basic"    EditorType = "Basic"

    Capabilities = @("TextEditor", "Basic")}

    EditorType = "Basic"

    Dependencies = @()# Editor instance and state

    Enabled = $true$script:editorInstance = $null

}$script:editorSettings = @{

    FontFamily = "Consolas"

# Extension initialization    FontSize = 11

function Initialize-RawrXD-Editor-BasicExtension {    BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)

    Write-DevConsole "📄 Initializing Basic Editor Extension..." "INFO"    ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)

    WordWrap = $false

    # Register extension    TabSize = 4

    if ($global:extensionRegistry) {}

        $extension = Register-Extension -Id "rawrxd-editor-basic" -Name "Basic Editor" `

            -Description "Simple text editor with basic functionality" `function Initialize-RawrXDBasicEditorExtension {

            -Author "RawrXD" -Capabilities @("TextEditor", "Basic") `    <#

            -Version "1.0.0" -EditorType "Basic"    .SYNOPSIS

    }        Initializes the Basic Editor extension

    #>

    Write-DevConsole "✅ Basic Editor Extension loaded successfully" "SUCCESS"    

}    Write-Host "🎯 Initializing RawrXD Basic Editor Extension" -ForegroundColor Cyan

    

# Editor creation functions    try {

function New-BasicEditor {        # Import required assemblies

    <#        Add-Type -AssemblyName System.Windows.Forms

    .SYNOPSIS        Add-Type -AssemblyName System.Drawing

        Creates a new basic TextBox editor control        

    #>        Write-Host "✅ Basic Editor Extension loaded successfully" -ForegroundColor Green

    param(        return $true

        [Parameter(Mandatory = $false)]    }

        [string]$InitialText = "",    catch {

        Write-Host "❌ Failed to initialize Basic Editor: $($_.Exception.Message)" -ForegroundColor Red

        [Parameter(Mandatory = $false)]        return $false

        [System.Drawing.Font]$Font,    }

}

        [Parameter(Mandatory = $false)]

        [System.Drawing.Color]$BackColor = [System.Drawing.Color]::White,function New-BasicEditor {

    <#

        [Parameter(Mandatory = $false)]    .SYNOPSIS

        [System.Drawing.Color]$ForeColor = [System.Drawing.Color]::Black,        Creates a new TextBox editor instance (lightweight)

    .OUTPUTS

        [Parameter(Mandatory = $false)]        System.Windows.Forms.TextBox

        [bool]$WordWrap = $true    #>

    )    

    if ($script:editorInstance) {

    try {        return $script:editorInstance

        $editor = New-Object System.Windows.Forms.TextBox    }

        $editor.Dock = [System.Windows.Forms.DockStyle]::Fill    

        $editor.Multiline = $true    try {

        $editor.ScrollBars = [System.Windows.Forms.ScrollBars]::Both        # Create TextBox control (much lighter than RichTextBox)

        $editor.WordWrap = $WordWrap        $editor = New-Object System.Windows.Forms.TextBox

        $editor.AcceptsTab = $true        

        # Apply basic settings

        # Set font if provided        $editor.Multiline = $true

        if ($Font) {        $editor.Dock = [System.Windows.Forms.DockStyle]::Fill

            $editor.Font = $Font        $editor.Font = New-Object System.Drawing.Font($script:editorSettings.FontFamily, $script:editorSettings.FontSize)

        }        $editor.BackColor = $script:editorSettings.BackColor

        else {        $editor.ForeColor = $script:editorSettings.ForeColor

            $editor.Font = New-Object System.Drawing.Font("Consolas", 10)        $editor.WordWrap = $script:editorSettings.WordWrap

        }        $editor.AcceptsTab = $true

        $editor.AcceptsReturn = $true

        # Set colors        $editor.ScrollBars = [System.Windows.Forms.ScrollBars]::Both

        $editor.BackColor = $BackColor        

        $editor.ForeColor = $ForeColor        # Add minimal event handlers

        Register-BasicEditorEvents -Editor $editor

        # Set initial text        

        if ($InitialText) {        $script:editorInstance = $editor

            $editor.Text = $InitialText        Write-Host "✅ Basic editor instance created (TextBox)" -ForegroundColor Green

        }        

        return $editor

        # Event handlers    }

        $editor.add_TextChanged({    catch {

            # Basic text change handling        Write-Host "❌ Failed to create Basic editor: $($_.Exception.Message)" -ForegroundColor Red

            if ($script:syntaxHighlightingEnabled) {        return $null

                # Basic syntax highlighting could be added here    }

            }}

        })

function Register-BasicEditorEvents {

        $editor.add_KeyDown({    <#

            param($sender, $e)    .SYNOPSIS

            # Handle special key combinations        Registers minimal event handlers for the basic editor

            if ($e.Control -and $e.KeyCode -eq [System.Windows.Forms.Keys]::S) {    #>

                $e.Handled = $true    param(

                # Save current file        [Parameter(Mandatory = $true)]

                if ($script:currentFilePath) {        [System.Windows.Forms.TextBox]$Editor

                    Save-File -FilePath $script:currentFilePath -Content $editor.Text    )

                }    

            }    try {

        })        # Key down for Tab handling only (keep it minimal)

        $Editor.add_KeyDown({

        Write-DevConsole "✅ Basic Editor created successfully" "SUCCESS"            param($sender, $e)

        return $editor            

    }            # Handle Tab key for proper indentation

    catch {            if ($e.KeyCode -eq [System.Windows.Forms.Keys]::Tab -and -not $e.Control -and -not $e.Alt) {

        Write-DevConsole "❌ Failed to create Basic Editor: $($_.Exception.Message)" "ERROR"                $sender.SelectedText = "`t"

        return $null                $e.Handled = $true

    }            }

}        })

        

function Get-BasicEditorContent {        Write-Host "✅ Basic editor events registered" -ForegroundColor Green

    <#    }

    .SYNOPSIS    catch {

        Gets content from a basic TextBox editor        Write-Host "⚠️ Warning: Could not register all basic editor events: $($_.Exception.Message)" -ForegroundColor Yellow

    #>    }

    param(}

        [Parameter(Mandatory = $true)]

        $Editorfunction Set-BasicEditorContent {

    )    <#

    .SYNOPSIS

    try {        Sets content in the Basic editor (TextBox only supports plain text)

        if (-not $Editor -or $Editor.GetType().Name -ne "TextBox") {    #>

            return ""    param(

        }        [Parameter(Mandatory = $true)]

        [AllowEmptyString()]

        return $Editor.Text        [string]$Content,

    }        

    catch {        [Parameter(Mandatory = $true)]

        Write-Host "❌ Error getting Basic Editor content: $($_.Exception.Message)" -ForegroundColor Red        [System.Windows.Forms.TextBox]$Editor

        return ""    )

    }    

}    try {

        if (-not $Editor -or $Editor.IsDisposed) { 

function Set-BasicEditorContent {            Write-Host "⚠️ Editor is null or disposed" -ForegroundColor Yellow

    <#            return $false

    .SYNOPSIS        }

        Sets content in a basic TextBox editor        

    #>        # TextBox only supports plain text - always use Text property

    param(        # PowerShell 5.1 compatible null-coalescing

        [Parameter(Mandatory = $true)]        $Editor.Text = if ($null -ne $Content) { $Content } else { "" }

        [AllowEmptyString()]        

        [string]$Content,        return $true

    }

        [Parameter(Mandatory = $true)]    catch {

        $Editor        Write-Host "❌ Failed to set basic editor content: $($_.Exception.Message)" -ForegroundColor Red

    )        return $false

    }

    try {}

        if (-not $Editor -or $Editor.GetType().Name -ne "TextBox") {

            return $falsefunction Get-BasicEditorContent {

        }    <#

    .SYNOPSIS

        $Editor.Text = $Content        Gets content from the Basic editor

        return $true    #>

    }    param(

    catch {        [Parameter(Mandatory = $true)]

        Write-Host "❌ Error setting Basic Editor content: $($_.Exception.Message)" -ForegroundColor Red        [System.Windows.Forms.TextBox]$Editor

        return $false    )

    }    

}    try {

        if (-not $Editor -or $Editor.IsDisposed) { 

function Set-BasicEditorSettings {            return ""

    <#        }

    .SYNOPSIS        

        Updates settings for a basic TextBox editor        # PowerShell 5.1 compatible null-coalescing

    #>        $text = $Editor.Text

    param(        return if ($null -ne $text) { $text } else { "" }

        [Parameter(Mandatory = $true)]    }

        $Editor,    catch {

        Write-Host "❌ Failed to get basic editor content: $($_.Exception.Message)" -ForegroundColor Red

        [string]$FontFamily,        return ""

        [int]$FontSize,    }

        [System.Drawing.Color]$BackColor,}

        [System.Drawing.Color]$ForeColor,

        [bool]$WordWrapfunction Set-BasicEditorSettings {

    )    <#

    .SYNOPSIS

    try {        Updates basic editor settings and applies them

        if (-not $Editor -or $Editor.GetType().Name -ne "TextBox") {    #>

            return $false    param(

        }        [string]$FontFamily,

        [int]$FontSize,

        # Update font        [System.Drawing.Color]$BackColor,

        if ($FontFamily -or $FontSize) {        [System.Drawing.Color]$ForeColor,

            $currentFont = $Editor.Font        [bool]$WordWrap,

            $newFamily = if ($FontFamily) { $FontFamily } else { $currentFont.FontFamily.Name }        [int]$TabSize

            $newSize = if ($FontSize -gt 0) { $FontSize } else { $currentFont.Size }    )

            $Editor.Font = New-Object System.Drawing.Font($newFamily, $newSize)    

        }    try {

        # Update settings

        # Update colors        if ($FontFamily) { $script:editorSettings.FontFamily = $FontFamily }

        if ($BackColor) { $Editor.BackColor = $BackColor }        if ($FontSize -gt 0) { $script:editorSettings.FontSize = $FontSize }

        if ($ForeColor) { $Editor.ForeColor = $ForeColor }        if ($BackColor) { $script:editorSettings.BackColor = $BackColor }

        if ($ForeColor) { $script:editorSettings.ForeColor = $ForeColor }

        # Update word wrap        if ($PSBoundParameters.ContainsKey('WordWrap')) { $script:editorSettings.WordWrap = $WordWrap }

        if ($PSBoundParameters.ContainsKey('WordWrap')) {        if ($TabSize -gt 0) { $script:editorSettings.TabSize = $TabSize }

            $Editor.WordWrap = $WordWrap        

        }        # Apply to existing editor instance

        if ($script:editorInstance) {

        return $true            Apply-SettingsToBasicEditor -Editor $script:editorInstance

    }        }

    catch {        

        Write-Host "❌ Error updating Basic Editor settings: $($_.Exception.Message)" -ForegroundColor Red        Write-Host "✅ Basic editor settings updated" -ForegroundColor Green

        return $false        return $true

    }    }

}    catch {

        Write-Host "❌ Failed to update basic editor settings: $($_.Exception.Message)" -ForegroundColor Red

function Remove-BasicEditor {        return $false

    <#    }

    .SYNOPSIS}

        Properly disposes of a basic TextBox editor

    #>function Apply-SettingsToBasicEditor {

    <#

    try {    .SYNOPSIS

        if ($script:activeEditor -and $script:activeEditor.GetType().Name -eq "TextBox") {        Applies current settings to a basic editor instance

            $script:activeEditor.Dispose()    #>

            $script:activeEditor = $null    param(

        }        [Parameter(Mandatory = $true)]

        Write-Host "✅ Basic Editor disposed" -ForegroundColor Green        [System.Windows.Forms.TextBox]$Editor

    }    )

    catch {    

        Write-Host "⚠️ Warning during Basic Editor cleanup: $($_.Exception.Message)" -ForegroundColor Yellow    try {

    }        $Editor.Font = New-Object System.Drawing.Font($script:editorSettings.FontFamily, $script:editorSettings.FontSize)

}        $Editor.BackColor = $script:editorSettings.BackColor

        $Editor.ForeColor = $script:editorSettings.ForeColor

# Export functions        $Editor.WordWrap = $script:editorSettings.WordWrap

Export-ModuleMember -Function @(        

    "Initialize-RawrXD-Editor-BasicExtension",        Write-Host "✅ Settings applied to basic editor" -ForegroundColor Green

    "New-BasicEditor",    }

    "Get-BasicEditorContent",    catch {

    "Set-BasicEditorContent",        Write-Host "⚠️ Warning: Could not apply all settings to basic editor: $($_.Exception.Message)" -ForegroundColor Yellow

    "Set-BasicEditorSettings",    }

    "Remove-BasicEditor"}

) -Variable @("RawrXDEditorBasicExtension")
# Cleanup function
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
        Write-Host "✅ Basic editor disposed" -ForegroundColor Green
    }
    catch {
        Write-Host "⚠️ Warning during basic editor cleanup: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

# Export functions for use by main application
Export-ModuleMember -Function Initialize-RawrXDBasicEditorExtension, New-BasicEditor, Set-BasicEditorContent, Get-BasicEditorContent, Set-BasicEditorSettings, Remove-BasicEditor -Variable RawrXDBasicEditorExtension