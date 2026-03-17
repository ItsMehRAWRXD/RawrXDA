# RawrXD-Settings.ps1
# Comprehensive IDE Settings Module
# Contains all settings-related functions including IDE settings dialog, performance optimization, theming, and customization

# ============================================
# DEPENDENCIES
# ============================================

# Dot-source core module for shared functions
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$coreModulePath = Join-Path $scriptDir "RawrXD-Core.ps1"
if (Test-Path $coreModulePath) {
    . $coreModulePath
    Write-Host "✅ RawrXD-Core.ps1 loaded successfully" -ForegroundColor Green
} else {
    Write-Warning "⚠️ RawrXD-Core.ps1 not found at: $coreModulePath"
    Write-Warning "Some settings functions may not work correctly without the core module."
}

# ============================================
# IDE SETTINGS DIALOG
# ============================================

function Show-IDESettings {
    <#
    .SYNOPSIS
        Opens the comprehensive IDE Settings dialog with all customization options
    .DESCRIPTION
        Full settings panel with tabs for: General, Editor, Appearance, Keyboard Shortcuts,
        AI/Chat, Browser, Terminal, Performance, and Advanced settings.
    #>

    $settingsForm = New-Object System.Windows.Forms.Form
    $settingsForm.Text = "⚙️ RawrXD IDE Settings"
    $settingsForm.Size = New-Object System.Drawing.Size(800, 650)
    $settingsForm.StartPosition = "CenterScreen"
    $settingsForm.FormBorderStyle = "FixedDialog"
    $settingsForm.MaximizeBox = $false
    $settingsForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $settingsForm.ForeColor = [System.Drawing.Color]::White

    # Create tab control
    $tabControl = New-Object System.Windows.Forms.TabControl
    $tabControl.Location = New-Object System.Drawing.Point(10, 10)
    $tabControl.Size = New-Object System.Drawing.Size(765, 545)
    $tabControl.Font = New-Object System.Drawing.Font("Segoe UI", 9)
    $settingsForm.Controls.Add($tabControl)

    # Helper function to create styled label
    $createLabel = {
        param($text, $x, $y, $width = 180)
        $lbl = New-Object System.Windows.Forms.Label
        $lbl.Text = $text
        $lbl.Location = New-Object System.Drawing.Point($x, $y)
        $lbl.Size = New-Object System.Drawing.Size($width, 23)
        $lbl.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
        return $lbl
    }

    # Helper function to create styled checkbox
    $createCheckbox = {
        param($text, $x, $y, $checked = $false)
        $cb = New-Object System.Windows.Forms.CheckBox
        $cb.Text = $text
        $cb.Location = New-Object System.Drawing.Point($x, $y)
        $cb.Size = New-Object System.Drawing.Size(300, 23)
        $cb.Checked = $checked
        $cb.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
        return $cb
    }

    # Helper function to create styled numeric
    $createNumeric = {
        param($x, $y, $min, $max, $value, $width = 80)
        $num = New-Object System.Windows.Forms.NumericUpDown
        $num.Location = New-Object System.Drawing.Point($x, $y)
        $num.Size = New-Object System.Drawing.Size($width, 25)
        $num.Minimum = $min
        $num.Maximum = $max
        $num.Value = [Math]::Max($min, [Math]::Min($max, $value))
        $num.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
        $num.ForeColor = [System.Drawing.Color]::White
        return $num
    }

    # Helper function to create styled combobox
    $createCombo = {
        param($x, $y, $items, $selected, $width = 180)
        $cmb = New-Object System.Windows.Forms.ComboBox
        $cmb.Location = New-Object System.Drawing.Point($x, $y)
        $cmb.Size = New-Object System.Drawing.Size($width, 25)
        $cmb.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
        $cmb.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
        $cmb.ForeColor = [System.Drawing.Color]::White
        $cmb.FlatStyle = "Flat"
        foreach ($item in $items) { $cmb.Items.Add($item) | Out-Null }
        $cmb.SelectedItem = $selected
        if (-not $cmb.SelectedItem -and $cmb.Items.Count -gt 0) { $cmb.SelectedIndex = 0 }
        return $cmb
    }

    # Helper function to create styled textbox
    $createTextbox = {
        param($x, $y, $text, $width = 250)
        $tb = New-Object System.Windows.Forms.TextBox
        $tb.Location = New-Object System.Drawing.Point($x, $y)
        $tb.Size = New-Object System.Drawing.Size($width, 25)
        $tb.Text = $text
        $tb.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
        $tb.ForeColor = [System.Drawing.Color]::White
        return $tb
    }

    # Helper function to create group box
    $createGroup = {
        param($text, $x, $y, $width, $height)
        $grp = New-Object System.Windows.Forms.GroupBox
        $grp.Text = $text
        $grp.Location = New-Object System.Drawing.Point($x, $y)
        $grp.Size = New-Object System.Drawing.Size($width, $height)
        $grp.ForeColor = [System.Drawing.Color]::FromArgb(100, 180, 255)
        return $grp
    }

    # =====================
    # TAB 1: GENERAL
    # =====================
    $generalTab = New-Object System.Windows.Forms.TabPage
    $generalTab.Text = "🏠 General"
    $generalTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($generalTab)

    # Startup Group
    $startupGroup = & $createGroup "Startup" 15 15 350 130
    $generalTab.Controls.Add($startupGroup)

    $startupGroup.Controls.Add((& $createLabel "Startup Behavior:" 15 25))
    $script:startupBehaviorCombo = & $createCombo 200 22 @("LastSession", "NewFile", "Empty") $global:settings.StartupBehavior
    $startupGroup.Controls.Add($script:startupBehaviorCombo)

    $script:checkUpdatesCheck = & $createCheckbox "Check for updates on startup" 15 55 $global:settings.CheckForUpdates
    $startupGroup.Controls.Add($script:checkUpdatesCheck)

    $script:rememberWindowCheck = & $createCheckbox "Remember window position and size" 15 85 $global:settings.RememberWindowState
    $startupGroup.Controls.Add($script:rememberWindowCheck)

    # Exit Group
    $exitGroup = & $createGroup "Exit Behavior" 15 155 350 100
    $generalTab.Controls.Add($exitGroup)

    $script:confirmExitCheck = & $createCheckbox "Confirm before exit" 15 25 $global:settings.ConfirmOnExit
    $exitGroup.Controls.Add($script:confirmExitCheck)

    $exitGroup.Controls.Add((& $createLabel "Max Recent Files:" 15 60))
    $script:maxRecentNumeric = & $createNumeric 200 57 5 50 $global:settings.MaxRecentFiles
    $exitGroup.Controls.Add($script:maxRecentNumeric)

    # Auto-Save Group
    $autoSaveGroup = & $createGroup "Auto-Save" 380 15 355 130
    $generalTab.Controls.Add($autoSaveGroup)

    $script:autoSaveCheck = & $createCheckbox "Enable Auto-Save" 15 25 $global:settings.AutoSaveEnabled
    $autoSaveGroup.Controls.Add($script:autoSaveCheck)

    $autoSaveGroup.Controls.Add((& $createLabel "Interval (seconds):" 15 60))
    $script:autoSaveIntervalNumeric = & $createNumeric 180 57 5 300 $global:settings.AutoSaveInterval
    $autoSaveGroup.Controls.Add($script:autoSaveIntervalNumeric)

    $script:createBackupsCheck = & $createCheckbox "Create backup files" 15 95 $global:settings.CreateBackups
    $autoSaveGroup.Controls.Add($script:createBackupsCheck)

    # Tabs Group
    $tabsGroup = & $createGroup "Tabs" 380 155 355 100
    $generalTab.Controls.Add($tabsGroup)

    $tabsGroup.Controls.Add((& $createLabel "Max Editor Tabs:" 15 30))
    $script:maxTabsNumeric = & $createNumeric 180 27 1 100 $global:settings.MaxTabs
    $tabsGroup.Controls.Add($script:maxTabsNumeric)

    $tabsGroup.Controls.Add((& $createLabel "Max Chat Tabs:" 15 65))
    $script:maxChatTabsNumeric = & $createNumeric 180 62 1 50 $global:settings.MaxChatTabs
    $tabsGroup.Controls.Add($script:maxChatTabsNumeric)

    # =====================
    # TAB 2: EDITOR
    # =====================
    $editorTab = New-Object System.Windows.Forms.TabPage
    $editorTab.Text = "📝 Editor"
    $editorTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($editorTab)

    # Font Group
    $fontGroup = & $createGroup "Font Settings" 15 15 350 100
    $editorTab.Controls.Add($fontGroup)

    $fontGroup.Controls.Add((& $createLabel "Font Family:" 15 30))
    $script:editorFontCombo = & $createCombo 150 27 @("Consolas", "Cascadia Code", "Fira Code", "JetBrains Mono", "Source Code Pro", "Courier New", "Monaco", "Menlo") $global:settings.EditorFontFamily
    $fontGroup.Controls.Add($script:editorFontCombo)

    $fontGroup.Controls.Add((& $createLabel "Size:" 15 65))
    $script:editorFontSizeNumeric = & $createNumeric 150 62 8 72 $global:settings.EditorFontSize 60
    $fontGroup.Controls.Add($script:editorFontSizeNumeric)

    # Indentation Group
    $indentGroup = & $createGroup "Indentation" 380 15 355 100
    $editorTab.Controls.Add($indentGroup)

    $indentGroup.Controls.Add((& $createLabel "Tab Size:" 15 30))
    $script:tabSizeNumeric = & $createNumeric 150 27 1 8 $global:settings.TabSize 60
    $indentGroup.Controls.Add($script:tabSizeNumeric)

    $script:autoIndentCheck = & $createCheckbox "Auto Indent" 15 65 $global:settings.AutoIndent
    $indentGroup.Controls.Add($script:autoIndentCheck)

    # Display Group
    $displayGroup = & $createGroup "Display Options" 15 125 350 180
    $editorTab.Controls.Add($displayGroup)

    $script:lineNumbersCheck = & $createCheckbox "Show Line Numbers" 15 25 $global:settings.ShowLineNumbers
    $displayGroup.Controls.Add($script:lineNumbersCheck)

    $script:wordWrapCheck = & $createCheckbox "Word Wrap" 15 55 $global:settings.WrapText
    $displayGroup.Controls.Add($script:wordWrapCheck)

    $script:whitespaceCheck = & $createCheckbox "Show Whitespace Characters" 15 85 $global:settings.ShowWhitespace
    $displayGroup.Controls.Add($script:whitespaceCheck)

    $script:highlightLineCheck = & $createCheckbox "Highlight Current Line" 15 115 $global:settings.HighlightCurrentLine
    $displayGroup.Controls.Add($script:highlightLineCheck)

    $script:miniMapCheck = & $createCheckbox "Show Mini Map" 15 145 $global:settings.MiniMap
    $displayGroup.Controls.Add($script:miniMapCheck)

    # Features Group
    $featuresGroup = & $createGroup "Features" 380 125 355 180
    $editorTab.Controls.Add($featuresGroup)

    $script:syntaxHighlightCheck = & $createCheckbox "Syntax Highlighting" 15 25 $global:settings.CodeHighlighting
    $featuresGroup.Controls.Add($script:syntaxHighlightCheck)

    $script:autoCompleteCheck = & $createCheckbox "Auto Complete" 15 55 $global:settings.AutoComplete
    $featuresGroup.Controls.Add($script:autoCompleteCheck)

    $script:bracketMatchCheck = & $createCheckbox "Bracket Matching" 15 85 $global:settings.BracketMatching
    $featuresGroup.Controls.Add($script:bracketMatchCheck)

    $featuresGroup.Controls.Add((& $createLabel "Max Undo History:" 15 120))
    $script:maxUndoNumeric = & $createNumeric 180 117 10 1000 $global:settings.MaxUndoHistory
    $featuresGroup.Controls.Add($script:maxUndoNumeric)

    # =====================
    # TAB 3: APPEARANCE
    # =====================
    $appearanceTab = New-Object System.Windows.Forms.TabPage
    $appearanceTab.Text = "🎨 Appearance"
    $appearanceTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($appearanceTab)

    # Theme Group
    $themeGroup = & $createGroup "Theme" 15 15 350 140
    $appearanceTab.Controls.Add($themeGroup)

    $themeGroup.Controls.Add((& $createLabel "UI Theme:" 15 30))
    $script:themeCombo = & $createCombo 150 27 @("Dark", "Light", "High Contrast", "Monokai", "Solarized", "Nord") $global:settings.ThemeMode
    $themeGroup.Controls.Add($script:themeCombo)

    $themeGroup.Controls.Add((& $createLabel "Editor Theme:" 15 65))
    $script:editorThemeCombo = & $createCombo 150 62 @("Monokai", "One Dark", "Dracula", "GitHub", "Solarized", "Tomorrow Night", "Material") $global:settings.EditorTheme
    $themeGroup.Controls.Add($script:editorThemeCombo)

    $themeGroup.Controls.Add((& $createLabel "Accent Color:" 15 100))
    $script:accentColorBtn = New-Object System.Windows.Forms.Button
    $script:accentColorBtn.Location = New-Object System.Drawing.Point(150, 97)
    $script:accentColorBtn.Size = New-Object System.Drawing.Size(100, 25)
    $script:accentColorBtn.Text = $global:settings.AccentColor
    $script:accentColorBtn.BackColor = [System.Drawing.ColorTranslator]::FromHtml($global:settings.AccentColor)
    $script:accentColorBtn.FlatStyle = "Flat"
    $script:accentColorBtn.Add_Click({
        $colorDialog = New-Object System.Windows.Forms.ColorDialog
        $colorDialog.Color = $script:accentColorBtn.BackColor
        if ($colorDialog.ShowDialog() -eq "OK") {
            $script:accentColorBtn.BackColor = $colorDialog.Color
            $script:accentColorBtn.Text = "#" + $colorDialog.Color.R.ToString("X2") + $colorDialog.Color.G.ToString("X2") + $colorDialog.Color.B.ToString("X2")
        }
    })
    $themeGroup.Controls.Add($script:accentColorBtn)

    # UI Group
    $uiGroup = & $createGroup "User Interface" 380 15 355 140
    $appearanceTab.Controls.Add($uiGroup)

    $uiGroup.Controls.Add((& $createLabel "UI Scale (%):" 15 30))
    $script:uiScaleNumeric = & $createNumeric 150 27 75 200 $global:settings.UIScale 60
    $uiGroup.Controls.Add($script:uiScaleNumeric)

    $script:showToolbarCheck = & $createCheckbox "Show Toolbar" 15 60 $global:settings.ShowToolbar
    $uiGroup.Controls.Add($script:showToolbarCheck)

    $script:showStatusBarCheck = & $createCheckbox "Show Status Bar" 15 90 $global:settings.ShowStatusBar
    $uiGroup.Controls.Add($script:showStatusBarCheck)

    # Animation Group
    $animGroup = & $createGroup "Effects" 15 165 350 100
    $appearanceTab.Controls.Add($animGroup)

    $script:animationsCheck = & $createCheckbox "Enable Animations" 15 30 $global:settings.AnimationsEnabled
    $animGroup.Controls.Add($script:animationsCheck)

    $script:compactModeCheck = & $createCheckbox "Compact Mode" 15 60 $global:settings.CompactMode
    $animGroup.Controls.Add($script:compactModeCheck)

    # =====================
    # TAB 4: KEYBOARD SHORTCUTS
    # =====================
    $hotkeyTab = New-Object System.Windows.Forms.TabPage
    $hotkeyTab.Text = "⌨️ Shortcuts"
    $hotkeyTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($hotkeyTab)

    # Hotkey List
    $hotkeyLabel = & $createLabel "Configure keyboard shortcuts:" 15 15 300
    $hotkeyTab.Controls.Add($hotkeyLabel)

    $script:hotkeyListView = New-Object System.Windows.Forms.ListView
    $script:hotkeyListView.Location = New-Object System.Drawing.Point(15, 45)
    $script:hotkeyListView.Size = New-Object System.Drawing.Size(720, 380)
    $script:hotkeyListView.View = [System.Windows.Forms.View]::Details
    $script:hotkeyListView.FullRowSelect = $true
    $script:hotkeyListView.GridLines = $true
    $script:hotkeyListView.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $script:hotkeyListView.ForeColor = [System.Drawing.Color]::White
    $script:hotkeyListView.Font = New-Object System.Drawing.Font("Segoe UI", 9)

    $script:hotkeyListView.Columns.Add("Command", 200) | Out-Null
    $script:hotkeyListView.Columns.Add("Shortcut", 150) | Out-Null
    $script:hotkeyListView.Columns.Add("Category", 150) | Out-Null

    # Populate hotkeys
    $hotkeyCategories = @{
        "NewFile" = "File"; "OpenFile" = "File"; "SaveFile" = "File"; "SaveAs" = "File"; "CloseTab" = "File"
        "Undo" = "Edit"; "Redo" = "Edit"; "Cut" = "Edit"; "Copy" = "Edit"; "Paste" = "Edit"; "SelectAll" = "Edit"
        "Find" = "Search"; "Replace" = "Search"; "GoToLine" = "Navigation"
        "CommentLine" = "Editor"; "DuplicateLine" = "Editor"; "DeleteLine" = "Editor"; "MoveLineUp" = "Editor"; "MoveLineDown" = "Editor"
        "CommandPalette" = "View"; "ToggleTerminal" = "View"; "ToggleBrowser" = "View"; "ToggleChat" = "View"; "ToggleFullscreen" = "View"
        "SendMessage" = "Chat"; "NewChatTab" = "Chat"
        "Settings" = "General"; "ZoomIn" = "View"; "ZoomOut" = "View"; "ResetZoom" = "View"
    }

    foreach ($key in $global:settings.Hotkeys.Keys | Sort-Object) {
        $item = New-Object System.Windows.Forms.ListViewItem($key)
        $item.SubItems.Add($global:settings.Hotkeys[$key]) | Out-Null
        $category = if ($hotkeyCategories.ContainsKey($key)) { $hotkeyCategories[$key] } else { "Other" }
        $item.SubItems.Add($category) | Out-Null
        $script:hotkeyListView.Items.Add($item) | Out-Null
    }
    $hotkeyTab.Controls.Add($script:hotkeyListView)

    # Edit hotkey button
    $editHotkeyBtn = New-Object System.Windows.Forms.Button
    $editHotkeyBtn.Text = "Edit Shortcut..."
    $editHotkeyBtn.Location = New-Object System.Drawing.Point(15, 435)
    $editHotkeyBtn.Size = New-Object System.Drawing.Size(120, 30)
    $editHotkeyBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $editHotkeyBtn.ForeColor = [System.Drawing.Color]::White
    $editHotkeyBtn.FlatStyle = "Flat"
    $editHotkeyBtn.Add_Click({
        if ($script:hotkeyListView.SelectedItems.Count -eq 0) {
            [System.Windows.Forms.MessageBox]::Show("Please select a command to edit.", "No Selection", "OK", "Information")
            return
        }
        $selectedItem = $script:hotkeyListView.SelectedItems[0]
        $command = $selectedItem.Text
        $currentShortcut = $selectedItem.SubItems[1].Text

        # Show hotkey capture dialog
        $captureForm = New-Object System.Windows.Forms.Form
        $captureForm.Text = "Edit Shortcut: $command"
        $captureForm.Size = New-Object System.Drawing.Size(400, 180)
        $captureForm.StartPosition = "CenterParent"
        $captureForm.FormBorderStyle = "FixedDialog"
        $captureForm.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
        $captureForm.MaximizeBox = $false
        $captureForm.MinimizeBox = $false

        $instructLabel = New-Object System.Windows.Forms.Label
        $instructLabel.Text = "Press the key combination you want to use:"
        $instructLabel.Location = New-Object System.Drawing.Point(20, 20)
        $instructLabel.Size = New-Object System.Drawing.Size(350, 25)
        $instructLabel.ForeColor = [System.Drawing.Color]::White
        $captureForm.Controls.Add($instructLabel)

        $keyDisplay = New-Object System.Windows.Forms.TextBox
        $keyDisplay.Location = New-Object System.Drawing.Point(20, 55)
        $keyDisplay.Size = New-Object System.Drawing.Size(340, 30)
        $keyDisplay.Text = $currentShortcut
        $keyDisplay.ReadOnly = $true
        $keyDisplay.Font = New-Object System.Drawing.Font("Consolas", 14)
        $keyDisplay.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
        $keyDisplay.ForeColor = [System.Drawing.Color]::Cyan
        $keyDisplay.TextAlign = "Center"
        $captureForm.Controls.Add($keyDisplay)

        $capturedKey = $currentShortcut
        $keyDisplay.Add_KeyDown({
            param($sender, $e)
            $e.Handled = $true
            $e.SuppressKeyPress = $true

            $parts = @()
            if ($e.Control) { $parts += "Ctrl" }
            if ($e.Alt) { $parts += "Alt" }
            if ($e.Shift) { $parts += "Shift" }

            $key = $e.KeyCode.ToString()
            if ($key -notin @("ControlKey", "ShiftKey", "Menu", "Alt")) {
                $parts += $key
            }

            if ($parts.Count -gt 0) {
                $script:capturedKey = $parts -join "+"
                $sender.Text = $script:capturedKey
            }
        })

        $okCaptureBtn = New-Object System.Windows.Forms.Button
        $okCaptureBtn.Text = "OK"
        $okCaptureBtn.Location = New-Object System.Drawing.Point(190, 100)
        $okCaptureBtn.Size = New-Object System.Drawing.Size(80, 30)
        $okCaptureBtn.DialogResult = "OK"
        $okCaptureBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
        $okCaptureBtn.ForeColor = [System.Drawing.Color]::White
        $okCaptureBtn.FlatStyle = "Flat"
        $captureForm.Controls.Add($okCaptureBtn)

        $cancelCaptureBtn = New-Object System.Windows.Forms.Button
        $cancelCaptureBtn.Text = "Cancel"
        $cancelCaptureBtn.Location = New-Object System.Drawing.Point(280, 100)
        $cancelCaptureBtn.Size = New-Object System.Drawing.Size(80, 30)
        $cancelCaptureBtn.DialogResult = "Cancel"
        $cancelCaptureBtn.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
        $cancelCaptureBtn.ForeColor = [System.Drawing.Color]::White
        $cancelCaptureBtn.FlatStyle = "Flat"
        $captureForm.Controls.Add($cancelCaptureBtn)

        $keyDisplay.Focus()

        if ($captureForm.ShowDialog() -eq "OK") {
            $selectedItem.SubItems[1].Text = $keyDisplay.Text
        }
        $captureForm.Dispose()
    })
    $hotkeyTab.Controls.Add($editHotkeyBtn)

    # Reset to defaults button
    $resetHotkeysBtn = New-Object System.Windows.Forms.Button
    $resetHotkeysBtn.Text = "Reset to Defaults"
    $resetHotkeysBtn.Location = New-Object System.Drawing.Point(145, 435)
    $resetHotkeysBtn.Size = New-Object System.Drawing.Size(120, 30)
    $resetHotkeysBtn.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $resetHotkeysBtn.ForeColor = [System.Drawing.Color]::White
    $resetHotkeysBtn.FlatStyle = "Flat"
    $resetHotkeysBtn.Add_Click({
        $result = [System.Windows.Forms.MessageBox]::Show("Reset all keyboard shortcuts to defaults?", "Confirm Reset", "YesNo", "Question")
        if ($result -eq "Yes") {
            # Default hotkeys
            $defaults = @{
                "NewFile" = "Ctrl+N"; "OpenFile" = "Ctrl+O"; "SaveFile" = "Ctrl+S"; "SaveAs" = "Ctrl+Shift+S"; "CloseTab" = "Ctrl+W"
                "Undo" = "Ctrl+Z"; "Redo" = "Ctrl+Y"; "Cut" = "Ctrl+X"; "Copy" = "Ctrl+C"; "Paste" = "Ctrl+V"; "SelectAll" = "Ctrl+A"
                "Find" = "Ctrl+F"; "Replace" = "Ctrl+H"; "GoToLine" = "Ctrl+G"
                "CommentLine" = "Ctrl+/"; "DuplicateLine" = "Ctrl+D"; "DeleteLine" = "Ctrl+Shift+K"; "MoveLineUp" = "Alt+Up"; "MoveLineDown" = "Alt+Down"
                "CommandPalette" = "Ctrl+Shift+P"; "ToggleTerminal" = 'Ctrl+`'; "ToggleBrowser" = "Ctrl+B"; "ToggleChat" = "Ctrl+Shift+C"; "ToggleFullscreen" = "F11"
                "SendMessage" = "Ctrl+Enter"; "NewChatTab" = "Ctrl+T"; "Settings" = "Ctrl+,"; "ZoomIn" = "Ctrl+Plus"; "ZoomOut" = "Ctrl+Minus"; "ResetZoom" = "Ctrl+0"
            }
            foreach ($item in $script:hotkeyListView.Items) {
                if ($defaults.ContainsKey($item.Text)) {
                    $item.SubItems[1].Text = $defaults[$item.Text]
                }
            }
        }
    })
    $hotkeyTab.Controls.Add($resetHotkeysBtn)

    # =====================
    # TAB 5: AI/CHAT
    # =====================
    $aiTab = New-Object System.Windows.Forms.TabPage
    $aiTab.Text = "🤖 AI/Chat"
    $aiTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($aiTab)

    # Model Group
    $modelGroup = & $createGroup "AI Model Settings" 15 15 350 160
    $aiTab.Controls.Add($modelGroup)

    $modelGroup.Controls.Add((& $createLabel "Default Model:" 15 30))
    $script:aiModelCombo = & $createCombo 150 27 @("bigdaddyg-fast:latest", "llama3.2", "llama3.2:1b", "llama3.1", "codellama", "mistral", "qwen2.5-coder", "deepseek-coder") $global:settings.OllamaModel 180
    $modelGroup.Controls.Add($script:aiModelCombo)

    $modelGroup.Controls.Add((& $createLabel "Endpoint URL:" 15 65))
    $script:endpointTextbox = & $createTextbox 150 62 $global:settings.OllamaEndpoint 180
    $modelGroup.Controls.Add($script:endpointTextbox)

    $modelGroup.Controls.Add((& $createLabel "Max Tokens:" 15 100))
    $script:maxTokensNumeric = & $createNumeric 150 97 256 32768 $global:settings.MaxTokens 100
    $modelGroup.Controls.Add($script:maxTokensNumeric)

    $script:streamResponsesCheck = & $createCheckbox "Stream Responses" 15 130 $global:settings.StreamResponses
    $modelGroup.Controls.Add($script:streamResponsesCheck)

    # Parameters Group
    $paramsGroup = & $createGroup "Generation Parameters" 380 15 355 160
    $aiTab.Controls.Add($paramsGroup)

    $paramsGroup.Controls.Add((& $createLabel "Temperature:" 15 30))
    $script:temperatureTrack = New-Object System.Windows.Forms.TrackBar
    $script:temperatureTrack.Location = New-Object System.Drawing.Point(130, 25)
    $script:temperatureTrack.Size = New-Object System.Drawing.Size(150, 30)
    $script:temperatureTrack.Minimum = 0
    $script:temperatureTrack.Maximum = 20
    $script:temperatureTrack.Value = [int]($global:settings.Temperature * 10)
    $script:temperatureTrack.TickFrequency = 2
    $paramsGroup.Controls.Add($script:temperatureTrack)

    $script:tempValueLabel = & $createLabel $global:settings.Temperature.ToString("0.0") 290 30 50
    $paramsGroup.Controls.Add($script:tempValueLabel)
    $script:temperatureTrack.Add_ValueChanged({ $script:tempValueLabel.Text = ($script:temperatureTrack.Value / 10).ToString("0.0") })

    $paramsGroup.Controls.Add((& $createLabel "Top P:" 15 70))
    $script:topPTrack = New-Object System.Windows.Forms.TrackBar
    $script:topPTrack.Location = New-Object System.Drawing.Point(130, 65)
    $script:topPTrack.Size = New-Object System.Drawing.Size(150, 30)
    $script:topPTrack.Minimum = 0
    $script:topPTrack.Maximum = 10
    $script:topPTrack.Value = [int]($global:settings.TopP * 10)
    $script:topPTrack.TickFrequency = 1
    $paramsGroup.Controls.Add($script:topPTrack)

    $script:topPValueLabel = & $createLabel $global:settings.TopP.ToString("0.0") 290 70 50
    $paramsGroup.Controls.Add($script:topPValueLabel)
    $script:topPTrack.Add_ValueChanged({ $script:topPValueLabel.Text = ($script:topPTrack.Value / 10).ToString("0.0") })

    # Chat Group
    $chatGroup = & $createGroup "Chat Display" 15 185 350 120
    $aiTab.Controls.Add($chatGroup)

    $chatGroup.Controls.Add((& $createLabel "Chat Font Size:" 15 30))
    $script:chatFontSizeNumeric = & $createNumeric 150 27 8 24 $global:settings.ChatFontSize 60
    $chatGroup.Controls.Add($script:chatFontSizeNumeric)

    $script:showTimestampsCheck = & $createCheckbox "Show Timestamps" 15 60 $global:settings.ShowTimestamps
    $chatGroup.Controls.Add($script:showTimestampsCheck)

    $script:autoScrollChatCheck = & $createCheckbox "Auto-scroll to New Messages" 15 90 $global:settings.AutoScrollChat
    $chatGroup.Controls.Add($script:autoScrollChatCheck)

    # =====================
    # TAB 6: BROWSER
    # =====================
    $browserTab = New-Object System.Windows.Forms.TabPage
    $browserTab.Text = "🌐 Browser"
    $browserTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($browserTab)

    # Engine Group
    $engineGroup = & $createGroup "Browser Engine" 15 15 350 120
    $browserTab.Controls.Add($engineGroup)

    $engineGroup.Controls.Add((& $createLabel "Rendering Engine:" 15 30))
    $script:browserEngineCombo = & $createCombo 150 27 @("WebView2", "PS51-Bridge", "Legacy") $global:settings.BrowserEngine
    $engineGroup.Controls.Add($script:browserEngineCombo)

    $engineGroup.Controls.Add((& $createLabel "Home Page:" 15 70))
    $script:homePageTextbox = & $createTextbox 150 67 $global:settings.BrowserHomePage 180
    $engineGroup.Controls.Add($script:homePageTextbox)

    # Features Group
    $browserFeaturesGroup = & $createGroup "Browser Features" 380 15 355 120
    $browserTab.Controls.Add($browserFeaturesGroup)

    $script:enableJSCheck = & $createCheckbox "Enable JavaScript" 15 30 $global:settings.EnableJavaScript
    $browserFeaturesGroup.Controls.Add($script:enableJSCheck)

    $script:enableCookiesCheck = & $createCheckbox "Enable Cookies" 15 60 $global:settings.EnableCookies
    $browserFeaturesGroup.Controls.Add($script:enableCookiesCheck)

    $script:clearCacheCheck = & $createCheckbox "Clear Cache on Exit" 15 90 $global:settings.ClearCacheOnExit
    $browserFeaturesGroup.Controls.Add($script:clearCacheCheck)

    # =====================
    # TAB 7: TERMINAL
    # =====================
    $terminalTab = New-Object System.Windows.Forms.TabPage
    $terminalTab.Text = "💻 Terminal"
    $terminalTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($terminalTab)

    # Terminal Font Group
    $termFontGroup = & $createGroup "Terminal Font" 15 15 350 100
    $terminalTab.Controls.Add($termFontGroup)

    $termFontGroup.Controls.Add((& $createLabel "Font Family:" 15 30))
    $script:termFontCombo = & $createCombo 150 27 @("Cascadia Mono", "Consolas", "Courier New", "Lucida Console", "Source Code Pro") $global:settings.TerminalFontFamily
    $termFontGroup.Controls.Add($script:termFontCombo)

    $termFontGroup.Controls.Add((& $createLabel "Font Size:" 15 65))
    $script:termFontSizeNumeric = & $createNumeric 150 62 8 24 $global:settings.TerminalFontSize 60
    $termFontGroup.Controls.Add($script:termFontSizeNumeric)

    # Terminal Buffer Group
    $termBufferGroup = & $createGroup "Buffer Settings" 380 15 355 100
    $terminalTab.Controls.Add($termBufferGroup)

    $termBufferGroup.Controls.Add((& $createLabel "Scrollback Lines:" 15 30))
    $script:scrollbackNumeric = & $createNumeric 150 27 100 10000 $global:settings.TerminalScrollback 100
    $termBufferGroup.Controls.Add($script:scrollbackNumeric)

    # =====================
    # TAB 8: ADVANCED
    # =====================
    $advancedTab = New-Object System.Windows.Forms.TabPage
    $advancedTab.Text = "⚡ Advanced"
    $advancedTab.BackColor = [System.Drawing.Color]::FromArgb(35, 35, 35)
    $tabControl.TabPages.Add($advancedTab)

    # Debug Group
    $debugGroup = & $createGroup "Debugging" 15 15 350 130
    $advancedTab.Controls.Add($debugGroup)

    $script:debugModeCheck = & $createCheckbox "Enable Debug Mode" 15 30 $global:settings.DebugMode
    $debugGroup.Controls.Add($script:debugModeCheck)

    $debugGroup.Controls.Add((& $createLabel "Log Level:" 15 65))
    $script:logLevelCombo = & $createCombo 150 62 @("Debug", "Info", "Warning", "Error") $global:settings.LogLevel
    $debugGroup.Controls.Add($script:logLevelCombo)

    $script:telemetryCheck = & $createCheckbox "Enable Telemetry" 15 100 $global:settings.EnableTelemetry
    $debugGroup.Controls.Add($script:telemetryCheck)

    # Performance Group
    $perfGroup = & $createGroup "Performance" 380 15 355 130
    $advancedTab.Controls.Add($perfGroup)

    $script:lazyLoadCheck = & $createCheckbox "Lazy Load Tabs" 15 30 $global:settings.LazyLoadTabs
    $perfGroup.Controls.Add($script:lazyLoadCheck)

    $perfGroup.Controls.Add((& $createLabel "Syntax Highlight Delay (ms):" 15 65))
    $script:highlightDelayNumeric = & $createNumeric 220 62 0 500 $global:settings.SyntaxHighlightDelay 80
    $perfGroup.Controls.Add($script:highlightDelayNumeric)

    # Data Management Group
    $dataGroup = & $createGroup "Data Management" 15 155 720 100
    $advancedTab.Controls.Add($dataGroup)

    $clearCacheBtn = New-Object System.Windows.Forms.Button
    $clearCacheBtn.Text = "Clear All Cache"
    $clearCacheBtn.Location = New-Object System.Drawing.Point(15, 35)
    $clearCacheBtn.Size = New-Object System.Drawing.Size(130, 30)
    $clearCacheBtn.BackColor = [System.Drawing.Color]::FromArgb(180, 80, 80)
    $clearCacheBtn.ForeColor = [System.Drawing.Color]::White
    $clearCacheBtn.FlatStyle = "Flat"
    $clearCacheBtn.Add_Click({
        $result = [System.Windows.Forms.MessageBox]::Show("This will clear all cached data. Continue?", "Clear Cache", "YesNo", "Warning")
        if ($result -eq "Yes") {
            # Clear WebView2 cache
            $cachePath = "$env:TEMP\RawrXD-WebView2-*"
            Get-ChildItem $cachePath -Directory -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
            [System.Windows.Forms.MessageBox]::Show("Cache cleared successfully.", "Done", "OK", "Information")
        }
    })
    $dataGroup.Controls.Add($clearCacheBtn)

    $resetSettingsBtn = New-Object System.Windows.Forms.Button
    $resetSettingsBtn.Text = "Reset All Settings"
    $resetSettingsBtn.Location = New-Object System.Drawing.Point(160, 35)
    $resetSettingsBtn.Size = New-Object System.Drawing.Size(130, 30)
    $resetSettingsBtn.BackColor = [System.Drawing.Color]::FromArgb(180, 80, 80)
    $resetSettingsBtn.ForeColor = [System.Drawing.Color]::White
    $resetSettingsBtn.FlatStyle = "Flat"
    $resetSettingsBtn.Add_Click({
        $result = [System.Windows.Forms.MessageBox]::Show("This will reset ALL settings to defaults. This cannot be undone. Continue?", "Reset Settings", "YesNo", "Warning")
        if ($result -eq "Yes") {
            $settingsPath = Join-Path $env:APPDATA "RawrXD\settings.json"
            if (Test-Path $settingsPath) {
                Remove-Item $settingsPath -Force
            }
            [System.Windows.Forms.MessageBox]::Show("Settings reset. Please restart the application.", "Reset Complete", "OK", "Information")
        }
    })
    $dataGroup.Controls.Add($resetSettingsBtn)

    $exportSettingsBtn = New-Object System.Windows.Forms.Button
    $exportSettingsBtn.Text = "Export Settings..."
    $exportSettingsBtn.Location = New-Object System.Drawing.Point(305, 35)
    $exportSettingsBtn.Size = New-Object System.Drawing.Size(130, 30)
    $exportSettingsBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $exportSettingsBtn.ForeColor = [System.Drawing.Color]::White
    $exportSettingsBtn.FlatStyle = "Flat"
    $exportSettingsBtn.Add_Click({
        $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
        $saveDialog.Filter = "JSON Files (*.json)|*.json"
        $saveDialog.FileName = "RawrXD-Settings-Backup.json"
        if ($saveDialog.ShowDialog() -eq "OK") {
            $global:settings | ConvertTo-Json -Depth 5 | Out-File $saveDialog.FileName -Encoding UTF8
            [System.Windows.Forms.MessageBox]::Show("Settings exported successfully.", "Export Complete", "OK", "Information")
        }
    })
    $dataGroup.Controls.Add($exportSettingsBtn)

    $importSettingsBtn = New-Object System.Windows.Forms.Button
    $importSettingsBtn.Text = "Import Settings..."
    $importSettingsBtn.Location = New-Object System.Drawing.Point(450, 35)
    $importSettingsBtn.Size = New-Object System.Drawing.Size(130, 30)
    $importSettingsBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $importSettingsBtn.ForeColor = [System.Drawing.Color]::White
    $importSettingsBtn.FlatStyle = "Flat"
    $importSettingsBtn.Add_Click({
        $openDialog = New-Object System.Windows.Forms.OpenFileDialog
        $openDialog.Filter = "JSON Files (*.json)|*.json"
        if ($openDialog.ShowDialog() -eq "OK") {
            try {
                $imported = Get-Content $openDialog.FileName | ConvertFrom-Json
                foreach ($prop in $imported.PSObject.Properties) {
                    if ($global:settings.ContainsKey($prop.Name)) {
                        $global:settings[$prop.Name] = $prop.Value
                    }
                }
                Save-Settings
                [System.Windows.Forms.MessageBox]::Show("Settings imported. Please restart for full effect.", "Import Complete", "OK", "Information")
            } catch {
                [System.Windows.Forms.MessageBox]::Show("Error importing settings: $_", "Import Error", "OK", "Error")
            }
        }
    })
    $dataGroup.Controls.Add($importSettingsBtn)

    # =====================
    # BOTTOM BUTTONS
    # =====================
    $buttonPanel = New-Object System.Windows.Forms.Panel
    $buttonPanel.Location = New-Object System.Drawing.Point(0, 560)
    $buttonPanel.Size = New-Object System.Drawing.Size(800, 60)
    $buttonPanel.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
    $settingsForm.Controls.Add($buttonPanel)

    $okButton = New-Object System.Windows.Forms.Button
    $okButton.Text = "OK"
    $okButton.Location = New-Object System.Drawing.Point(580, 15)
    $okButton.Size = New-Object System.Drawing.Size(90, 35)
    $okButton.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $okButton.ForeColor = [System.Drawing.Color]::White
    $okButton.FlatStyle = "Flat"
    $okButton.Font = New-Object System.Drawing.Font("Segoe UI", 10)
    $buttonPanel.Controls.Add($okButton)

    $cancelButton = New-Object System.Windows.Forms.Button
    $cancelButton.Text = "Cancel"
    $cancelButton.Location = New-Object System.Drawing.Point(685, 15)
    $cancelButton.Size = New-Object System.Drawing.Size(90, 35)
    $cancelButton.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $cancelButton.ForeColor = [System.Drawing.Color]::White
    $cancelButton.FlatStyle = "Flat"
    $cancelButton.Font = New-Object System.Drawing.Font("Segoe UI", 10)
    $cancelButton.DialogResult = "Cancel"
    $buttonPanel.Controls.Add($cancelButton)

    $applyButton = New-Object System.Windows.Forms.Button
    $applyButton.Text = "Apply"
    $applyButton.Location = New-Object System.Drawing.Point(475, 15)
    $applyButton.Size = New-Object System.Drawing.Size(90, 35)
    $applyButton.BackColor = [System.Drawing.Color]::FromArgb(80, 80, 80)
    $applyButton.ForeColor = [System.Drawing.Color]::White
    $applyButton.FlatStyle = "Flat"
    $applyButton.Font = New-Object System.Drawing.Font("Segoe UI", 10)
    $buttonPanel.Controls.Add($applyButton)

    # Apply settings function
    $applySettings = {
        # General
        $global:settings.StartupBehavior = $script:startupBehaviorCombo.SelectedItem
        $global:settings.CheckForUpdates = $script:checkUpdatesCheck.Checked
        $global:settings.RememberWindowState = $script:rememberWindowCheck.Checked
        $global:settings.ConfirmOnExit = $script:confirmExitCheck.Checked
        $global:settings.MaxRecentFiles = $script:maxRecentNumeric.Value
        $global:settings.AutoSaveEnabled = $script:autoSaveCheck.Checked
        $global:settings.AutoSaveInterval = $script:autoSaveIntervalNumeric.Value
        $global:settings.CreateBackups = $script:createBackupsCheck.Checked
        $global:settings.MaxTabs = $script:maxTabsNumeric.Value
        $global:settings.MaxChatTabs = $script:maxChatTabsNumeric.Value

        # Editor
        $global:settings.EditorFontFamily = $script:editorFontCombo.SelectedItem
        $global:settings.EditorFontSize = $script:editorFontSizeNumeric.Value
        $global:settings.TabSize = $script:tabSizeNumeric.Value
        $global:settings.AutoIndent = $script:autoIndentCheck.Checked
        $global:settings.ShowLineNumbers = $script:lineNumbersCheck.Checked
        $global:settings.WrapText = $script:wordWrapCheck.Checked
        $global:settings.ShowWhitespace = $script:whitespaceCheck.Checked
        $global:settings.HighlightCurrentLine = $script:highlightLineCheck.Checked
        $global:settings.MiniMap = $script:miniMapCheck.Checked
        $global:settings.CodeHighlighting = $script:syntaxHighlightCheck.Checked
        $global:settings.AutoComplete = $script:autoCompleteCheck.Checked
        $global:settings.BracketMatching = $script:bracketMatchCheck.Checked
        $global:settings.MaxUndoHistory = $script:maxUndoNumeric.Value

        # Appearance
        $global:settings.ThemeMode = $script:themeCombo.SelectedItem
        $global:settings.EditorTheme = $script:editorThemeCombo.SelectedItem
        $global:settings.AccentColor = $script:accentColorBtn.Text
        $global:settings.UIScale = $script:uiScaleNumeric.Value
        $global:settings.ShowToolbar = $script:showToolbarCheck.Checked
        $global:settings.ShowStatusBar = $script:showStatusBarCheck.Checked
        $global:settings.AnimationsEnabled = $script:animationsCheck.Checked
        $global:settings.CompactMode = $script:compactModeCheck.Checked

        # Hotkeys
        $global:settings.Hotkeys = @{}
        foreach ($item in $script:hotkeyListView.Items) {
            $global:settings.Hotkeys[$item.Text] = $item.SubItems[1].Text
        }

        # AI/Chat
        $global:settings.OllamaModel = $script:aiModelCombo.SelectedItem
        $global:settings.OllamaEndpoint = $script:endpointTextbox.Text
        $global:settings.MaxTokens = $script:maxTokensNumeric.Value
        $global:settings.StreamResponses = $script:streamResponsesCheck.Checked
        $global:settings.Temperature = $script:temperatureTrack.Value / 10
        $global:settings.TopP = $script:topPTrack.Value / 10
        $global:settings.ChatFontSize = $script:chatFontSizeNumeric.Value
        $global:settings.ShowTimestamps = $script:showTimestampsCheck.Checked
        $global:settings.AutoScrollChat = $script:autoScrollChatCheck.Checked

        # Browser
        $global:settings.BrowserEngine = $script:browserEngineCombo.SelectedItem
        $global:settings.BrowserHomePage = $script:homePageTextbox.Text
        $global:settings.EnableJavaScript = $script:enableJSCheck.Checked
        $global:settings.EnableCookies = $script:enableCookiesCheck.Checked
        $global:settings.ClearCacheOnExit = $script:clearCacheCheck.Checked

        # Terminal
        $global:settings.TerminalFontFamily = $script:termFontCombo.SelectedItem
        $global:settings.TerminalFontSize = $script:termFontSizeNumeric.Value
        $global:settings.TerminalScrollback = $script:scrollbackNumeric.Value

        # Advanced
        $global:settings.DebugMode = $script:debugModeCheck.Checked
        $global:settings.LogLevel = $script:logLevelCombo.SelectedItem
        $global:settings.EnableTelemetry = $script:telemetryCheck.Checked
        $global:settings.LazyLoadTabs = $script:lazyLoadCheck.Checked
        $global:settings.SyntaxHighlightDelay = $script:highlightDelayNumeric.Value

        # Save and apply
        Save-Settings
        Apply-EditorSettings
        Write-DevConsole "✅ All settings saved and applied" "SUCCESS"
    }

    $applyButton.Add_Click($applySettings)

    $okButton.Add_Click({
        & $applySettings
        $settingsForm.DialogResult = "OK"
        $settingsForm.Close()
    })

    # Show dialog
    $settingsForm.ShowDialog() | Out-Null
    $settingsForm.Dispose()
}

function Show-BulkActionsMenu {
    <#
    .SYNOPSIS
        Shows a menu for bulk operations to demonstrate multithreading capabilities
    #>

    $bulkForm = New-Object System.Windows.Forms.Form
    $bulkForm.Text = "Bulk Actions - Multithreading Demo"
    $bulkForm.Size = New-Object System.Drawing.Size(500, 400)
    $bulkForm.StartPosition = "CenterScreen"
    $bulkForm.FormBorderStyle = "FixedDialog"
    $bulkForm.MaximizeBox = $false

    # Status display
    $statusGroup = New-Object System.Windows.Forms.GroupBox
    $statusGroup.Text = "Threading Status"
    $statusGroup.Location = New-Object System.Drawing.Point(20, 20)
    $statusGroup.Size = New-Object System.Drawing.Size(440, 100)
    $bulkForm.Controls.Add($statusGroup)

    $statusText = New-Object System.Windows.Forms.TextBox
    $statusText.Multiline = $true
    $statusText.ReadOnly = $true
    $statusText.Location = New-Object System.Drawing.Point(10, 20)
    $statusText.Size = New-Object System.Drawing.Size(420, 70)
    $statusText.ScrollBars = [System.Windows.Forms.ScrollBars]::Vertical

    $threadingStatus = Get-ThreadingStatus
    $statusText.Text = @"
Multithreading: $($threadingStatus.IsInitialized)
Active Jobs: $($threadingStatus.ActiveJobs)
Queued Tasks: $($threadingStatus.QueuedTasks)
Worker Count: $($threadingStatus.WorkerCount)
Max Concurrent: $($threadingStatus.MaxConcurrentTasks)
"@
    $statusGroup.Controls.Add($statusText)

    # Actions group
    $actionsGroup = New-Object System.Windows.Forms.GroupBox
    $actionsGroup.Text = "Bulk Operations"
    $actionsGroup.Location = New-Object System.Drawing.Point(20, 130)
    $actionsGroup.Size = New-Object System.Drawing.Size(440, 180)
    $bulkForm.Controls.Add($actionsGroup)

    # Create 3 new chats button
    $createChatsBtn = New-Object System.Windows.Forms.Button
    $createChatsBtn.Text = "Create 3 New Chat Tabs"
    $createChatsBtn.Location = New-Object System.Drawing.Point(20, 30)
    $createChatsBtn.Size = New-Object System.Drawing.Size(180, 30)
    $createChatsBtn.add_Click({
            $cursorToken = Enter-CursorWaitState -Reason "Bulk:CreateChats" -Style "AppStarting"
            try {
                Write-DevConsole "🚀 Creating multiple chat tabs..." "INFO"
                for ($i = 1; $i -le 3; $i++) {
                    $tabId = New-ChatTab -TabName "Bulk Chat $i"
                    if ($tabId) {
                        Write-DevConsole "✅ Created bulk chat tab $i : $tabId" "SUCCESS"
                    }
                }
                $bulkForm.Close()
            }
            finally {
                if ($cursorToken) {
                    Exit-CursorWaitState -Token $cursorToken
                }
            }
        })
    $actionsGroup.Controls.Add($createChatsBtn)

    # Send parallel messages button
    $parallelMsgBtn = New-Object System.Windows.Forms.Button
    $parallelMsgBtn.Text = "Send Test Messages to All Chats"
    $parallelMsgBtn.Location = New-Object System.Drawing.Point(220, 30)
    $parallelMsgBtn.Size = New-Object System.Drawing.Size(200, 30)
    $parallelMsgBtn.add_Click({
            $cursorToken = Enter-CursorWaitState -Reason "Bulk:ParallelChats" -Style "Wait"
            try {
                $activeChatIds = @($script:chatTabs.Keys | Select-Object -First 3)
                if ($activeChatIds.Count -eq 0) {
                    Write-DevConsole "⚠ No active chats to test" "WARNING"
                    return
                }

                Write-DevConsole "🔄 Sending parallel test messages to $($activeChatIds.Count) chats..." "INFO"

                $chatRequests = @()
                $testMessages = @(
                    "What is the capital of France?",
                    "Explain quantum computing in simple terms",
                    "Write a hello world program in Python"
                )

                $testMsgCount = $testMessages.Count
                for ($i = 0; $i -lt [Math]::Min($activeChatIds.Count, $testMsgCount); $i++) {
                    $chatId = $activeChatIds[$i]
                    $chatSession = $script:chatTabs[$chatId]

                    # Add test message to input
                    $chatSession.InputBox.Text = $testMessages[$i]

                    $chatRequests += @{
                        TabId       = $chatId
                        Message     = $testMessages[$i]
                        Model       = $chatSession.ModelCombo.SelectedItem
                        ChatHistory = $chatSession.Messages
                    }
                }

                $requestCount = @($chatRequests).Count
                if ($requestCount -gt 0) {
                    Start-ParallelChatProcessing -ChatRequests $chatRequests
                    Write-DevConsole "🚀 Parallel processing started for $requestCount chats" "SUCCESS"
                }

                $bulkForm.Close()
            }
            finally {
                if ($cursorToken) {
                    Exit-CursorWaitState -Token $cursorToken
                }
            }
        })
    $actionsGroup.Controls.Add($parallelMsgBtn)

    # Create agent tasks button
    $agentTasksBtn = New-Object System.Windows.Forms.Button
    $agentTasksBtn.Text = "Create 3 Demo Agent Tasks"
    $agentTasksBtn.Location = New-Object System.Drawing.Point(20, 80)
    $agentTasksBtn.Size = New-Object System.Drawing.Size(180, 30)
    $agentTasksBtn.add_Click({
            $cursorToken = Enter-CursorWaitState -Reason "Bulk:AgentTasks" -Style "Wait"
            try {
                Write-DevConsole "🤖 Creating demo agent tasks..." "INFO"

                # Task 1: File analysis
                $task1Id = New-AgentTask -Name "File Analysis" -Description "Analyze current file structure"
                $task1 = $global:agentContext.Tasks | Where-Object { $_.Id -eq $task1Id } | Select-Object -First 1
                $task1.Steps = @(
                    @{ Type = "tool"; Description = "List directory"; Tool = "list_directory"; Arguments = @{} }
                    @{ Type = "ai_query"; Description = "Analyze structure"; Query = "Analyze the file structure" }
                )
                Start-AgentTaskAsync -TaskId $task1Id -Priority "Normal"

                # Task 2: Environment check
                $task2Id = New-AgentTask -Name "Environment Check" -Description "Check system environment"
                $task2 = $global:agentContext.Tasks | Where-Object { $_.Id -eq $task2Id } | Select-Object -First 1
                $task2.Steps = @(
                    @{ Type = "command"; Description = "Get PowerShell version"; Command = '$PSVersionTable.PSVersion' }
                    @{ Type = "tool"; Description = "Get environment"; Tool = "get_environment"; Arguments = @{} }
                )
                Start-AgentTaskAsync -TaskId $task2Id -Priority "High"

                # Task 3: Code generation
                $task3Id = New-AgentTask -Name "Code Generation" -Description "Generate sample code"
                $task3 = $global:agentContext.Tasks | Where-Object { $_.Id -eq $task3Id } | Select-Object -First 1
                $task3.Steps = @(
                    @{ Type = "ai_query"; Description = "Generate PowerShell function"; Query = "Create a simple PowerShell function" }
                    @{ Type = "edit"; Description = "Save generated code"; File = "generated_code.ps1"; Content = "# Generated code" }
                )
                Start-AgentTaskAsync -TaskId $task3Id -Priority "Low"

                Write-DevConsole "✅ Created 3 demo agent tasks with different priorities" "SUCCESS"
                $bulkForm.Close()
            }
            finally {
                if ($cursorToken) {
                    Exit-CursorWaitState -Token $cursorToken
                }
            }
        })
    $actionsGroup.Controls.Add($agentTasksBtn)

    # Threading stats button
    $statsBtn = New-Object System.Windows.Forms.Button
    $statsBtn.Text = "Refresh Stats"
    $statsBtn.Location = New-Object System.Drawing.Point(220, 80)
    $statsBtn.Size = New-Object System.Drawing.Size(120, 30)
    $statsBtn.add_Click({
            $threadingStatus = Get-ThreadingStatus
            $statusText.Text = @"
Multithreading: $($threadingStatus.IsInitialized)
Active Jobs: $($threadingStatus.ActiveJobs)
Queued Tasks: $($threadingStatus.QueuedTasks)
Worker Count: $($threadingStatus.WorkerCount)
Max Concurrent: $($threadingStatus.MaxConcurrentTasks)

Worker States:
$($threadingStatus.WorkerStates.Keys | ForEach-Object {
    "  $_ : $($threadingStatus.WorkerStates[$_].Status)"
} | Out-String)
"@
        })
    $actionsGroup.Controls.Add($statsBtn)

    # Close button
    $closeBtn = New-Object System.Windows.Forms.Button
    $closeBtn.Text = "Close"
    $closeBtn.Location = New-Object System.Drawing.Point(380, 320)
    $closeBtn.Size = New-Object System.Drawing.Size(80, 30)
    $closeBtn.DialogResult = [System.Windows.Forms.DialogResult]::OK
    $bulkForm.Controls.Add($closeBtn)

    $bulkForm.ShowDialog() | Out-Null
    $bulkForm.Dispose()
}

function Update-ThreadingStatusLabel {
    <#
    .SYNOPSIS
        Updates the threading status label in the chat toolbar
    #>

    if ($threadingStatusLabel) {
        $status = Get-ThreadingStatus
        if ($status.IsInitialized) {
            $threadingStatusLabel.Text = "MT: $($status.ActiveJobs)/$($status.MaxConcurrentTasks) active"
            $threadingStatusLabel.ForeColor = [System.Drawing.Color]::Green
        }
        else {
            $threadingStatusLabel.Text = "MT: Disabled"
            $threadingStatusLabel.ForeColor = [System.Drawing.Color]::Red
        }
    }
}

# ============================================
# Command Palette Functions
# ============================================
function Show-CommandPalette {
    $commandPalette.Show()
    $paletteInput.Focus()
    $paletteInput.SelectAll()
    Update-CommandPalette
}

function Hide-CommandPalette {
    $commandPalette.Hide()
}

function Update-CommandPalette {
    $query = $paletteInput.Text.ToLower()
    $paletteResults.Items.Clear()

    if ([string]::IsNullOrWhiteSpace($query)) {
        $paletteLabel.Text = "Type a command or search extensions..."
        return
    }

    # Command list
    $commands = @(
        @{Name = "> Git: Status"; Action = { Update-GitStatus; $rightTabControl.SelectedTab = $gitTab } }
        @{Name = "> Git: Add All"; Action = { Invoke-GitCommand "add" @("."); Update-GitStatus } }
        @{Name = "> Git: Commit"; Action = { $msg = [Microsoft.VisualBasic.Interaction]::InputBox("Commit message:", "Git Commit"); if ($msg) { Invoke-GitCommand "commit" @("-m", $msg); Update-GitStatus } } }
        @{Name = "> Git: Push"; Action = { Invoke-GitCommand "push" @(); Update-GitStatus } }
        @{Name = "> Git: Pull"; Action = { Invoke-GitCommand "pull" @(); Update-GitStatus } }
        @{Name = "> File: Open"; Action = { $openItem.PerformClick() } }
        @{Name = "> File: Save"; Action = { $saveItem.PerformClick() } }
        @{Name = "> File: Save As"; Action = { $saveAsItem.PerformClick() } }
        @{Name = "> Terminal: Focus"; Action = { $rightTabControl.SelectedTab = $terminalTab } }
        @{Name = "> Browser: Focus"; Action = { $rightTabControl.SelectedTab = $browserTab } }
        @{Name = "> Chat: Focus"; Action = { $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Extensions: Marketplace"; Action = { Show-Marketplace } }
        @{Name = "> Extensions: Installed"; Action = { Show-InstalledExtensions } }
        @{Name = "> Agent: Toggle Mode"; Action = { $toggle.PerformClick() } }
        @{Name = "> Code: Generate"; Action = { $chatBox.AppendText("Use /code <description> in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Code: Review"; Action = { $chatBox.AppendText("Use /review in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: Start Workflow"; Action = { $chatBox.AppendText("Use /workflow <goal> in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: List Tools"; Action = { $chatBox.AppendText("Use /tools in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: Environment Info"; Action = { $chatBox.AppendText("Use /env in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        @{Name = "> Agent: Tasks Panel"; Action = { $rightTabControl.SelectedTab = $agentTasksTab } }
        @{Name = "> Settings"; Action = { Show-IDESettings } }
    )

    # Extension search
    $extensions = Search-Marketplace -Query $query
    foreach ($ext in $extensions) {
        $commands += @{Name = "Extension: $($ext.Name)"; Action = { $chatBox.AppendText("Extension: $($ext.Name) - $($ext.Description)`r`n"); $rightTabControl.SelectedTab = $chatTab } }
    }

    # Filter and add matching commands
    $matching = $commands | Where-Object { $_.Name.ToLower() -like "*$query*" }
    foreach ($cmd in $matching) {
        $paletteResults.Items.Add($cmd.Name) | Out-Null
    }

    $resultCount = $paletteResults.Items.Count
    if ($resultCount -gt 0) {
        $paletteResults.SelectedIndex = 0
        $paletteLabel.Text = "$($paletteResults.Items.Count) result(s) found"
    }
    else {
        $paletteLabel.Text = "No commands found"
    }
}

function Invoke-CommandPaletteSelection {
    $selected = $paletteResults.SelectedItem
    if ($selected) {
        $query = $paletteInput.Text.ToLower()
        $commands = @(
            @{Name = "> Git: Status"; Action = { Update-GitStatus; $rightTabControl.SelectedTab = $gitTab } }
            @{Name = "> Git: Add All"; Action = { Invoke-GitCommand "add" @("."); Update-GitStatus } }
            @{Name = "> Git: Commit"; Action = { $msg = [Microsoft.VisualBasic.Interaction]::InputBox("Commit message:", "Git Commit"); if ($msg) { Invoke-GitCommand "commit" @("-m", $msg); Update-GitStatus } } }
            @{Name = "> Git: Push"; Action = { Invoke-GitCommand "push" @(); Update-GitStatus } }
            @{Name = "> Git: Pull"; Action = { Invoke-GitCommand "pull" @(); Update-GitStatus } }
            @{Name = "> File: Open"; Action = { $openItem.PerformClick() } }
            @{Name = "> File: Save"; Action = { $saveItem.PerformClick() } }
            @{Name = "> File: Save As"; Action = { $saveAsItem.PerformClick() } }
            @{Name = "> Terminal: Focus"; Action = { $rightTabControl.SelectedTab = $terminalTab } }
            @{Name = "> Browser: Focus"; Action = { $rightTabControl.SelectedTab = $browserTab } }
            @{Name = "> Chat: Focus"; Action = { $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Extensions: Marketplace"; Action = { Show-Marketplace } }
            @{Name = "> Extensions: Installed"; Action = { Show-InstalledExtensions } }
            @{Name = "> Agent: Toggle Mode"; Action = { $toggle.PerformClick() } }
            @{Name = "> Code: Generate"; Action = { $chatBox.AppendText("Use /code <description> in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Code: Review"; Action = { $chatBox.AppendText("Use /review in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Agent: Start Workflow"; Action = { $chatBox.AppendText("Use /workflow <goal> in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Agent: List Tools"; Action = { $chatBox.AppendText("Use /tools in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Agent: Environment Info"; Action = { $chatBox.AppendText("Use /env in chat`r`n"); $rightTabControl.SelectedTab = $chatTab } }
            @{Name = "> Agent: Tasks Panel"; Action = { $rightTabControl.SelectedTab = $agentTasksTab } }
            @{Name = "> Settings"; Action = { Show-IDESettings } }
        )

        $extensions = Search-Marketplace -Query $query
        foreach ($ext in $extensions) {
            $commands += @{Name = "Extension: $($ext.Name)"; Action = { $chatBox.AppendText("Extension: $($ext.Name) - $($ext.Description)`r`n"); $rightTabControl.SelectedTab = $chatTab } }
        }

        $cmd = $commands | Where-Object { $_.Name -eq $selected } | Select-Object -First 1
        if ($cmd) {
            Hide-CommandPalette
            $cmd.Action.Invoke()
        }
    }
}

# ============================================
# SECURITY DIALOG FUNCTIONS
# ============================================

function Show-SessionInfo {
    $infoForm = New-Object System.Windows.Forms.Form
    $infoForm.Text = "Session Information"
    $infoForm.Size = New-Object System.Drawing.Size(500, 400)
    $infoForm.StartPosition = "CenterScreen"
    $infoForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $infoForm.ForeColor = [System.Drawing.Color]::White

    $infoText = New-Object System.Windows.Forms.TextBox
    $infoText.Multiline = $true
    $infoText.ReadOnly = $true
    $infoText.ScrollBars = "Vertical"
    $infoText.Dock = [System.Windows.Forms.DockStyle]::Fill
    $infoText.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $infoText.ForeColor = [System.Drawing.Color]::White
    $infoText.Font = New-Object System.Drawing.Font("Consolas", 10)

    $sessionDuration = ((Get-Date) - $script:CurrentSession.StartTime)
    $lastActivityAgo = ((Get-Date) - $script:CurrentSession.LastActivity)

    $sessionInfo = @"
SESSION INFORMATION
═══════════════════════════════════════
Session ID: $($script:CurrentSession.SessionId)
User ID: $(if ($script:CurrentSession.UserId) { $script:CurrentSession.UserId } else { "Anonymous" })
Authenticated: $($script:CurrentSession.IsAuthenticated)
Security Level: $($script:CurrentSession.SecurityLevel)
Login Attempts: $($script:CurrentSession.LoginAttempts)

TIMING
═══════════════════════════════════════
Start Time: $($script:CurrentSession.StartTime.ToString("yyyy-MM-dd HH:mm:ss"))
Duration: $($sessionDuration.Hours)h $($sessionDuration.Minutes)m $($sessionDuration.Seconds)s
Last Activity: $([math]::Round($lastActivityAgo.TotalMinutes, 2)) minutes ago

SECURITY CONFIGURATION
═══════════════════════════════════════
Stealth Mode: $($script:SecurityConfig.StealthMode)
Encrypt Sensitive Data: $($script:SecurityConfig.EncryptSensitiveData)
Validate Inputs: $($script:SecurityConfig.ValidateAllInputs)
Secure Connections: $($script:UseHTTPS)
Session Timeout: $($script:SecurityConfig.SessionTimeout)s
Max Login Attempts: $($script:SecurityConfig.MaxLoginAttempts)
Log Security Events: $($script:SecurityConfig.LogSecurityEvents)
Anti-Forensics: $($script:SecurityConfig.AntiForensics)
Process Hiding: $($script:SecurityConfig.ProcessHiding)

SYSTEM INFORMATION
═══════════════════════════════════════
Process ID: $PID
User Context: $([Environment]::UserName)
Machine Name: $([Environment]::MachineName)
OS Version: $([Environment]::OSVersion.VersionString)
PowerShell Version: $($PSVersionTable.PSVersion)
Security Events Logged: $(@($script:SecurityLog).Count)

OLLAMA CONNECTION
═══════════════════════════════════════
Endpoint: $OllamaAPIEndpoint
HTTPS Enabled: $script:UseHTTPS
API Key Configured: $($null -ne $script:OllamaAPIKey)
Model: $OllamaModel
"@

    $infoText.Text = $sessionInfo
    $infoForm.Controls.Add($infoText)
    $infoForm.ShowDialog()
}

function Show-SecurityLog {
    $secLogCount = @($script:SecurityLog).Count
    $logForm = New-Object System.Windows.Forms.Form
    $logForm.Text = "Security Event Log ($secLogCount events)"
    $logForm.Size = New-Object System.Drawing.Size(800, 600)
    $logForm.StartPosition = "CenterScreen"
    $logForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)

    $logGrid = New-Object System.Windows.Forms.DataGridView
    $logGrid.Dock = [System.Windows.Forms.DockStyle]::Fill
    $logGrid.BackgroundColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $logGrid.DefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $logGrid.DefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $logGrid.ColumnHeadersDefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $logGrid.ColumnHeadersDefaultCellStyle.ForeColor = [System.Drawing.Color]::White
    $logGrid.ReadOnly = $true
    $logGrid.AutoSizeColumnsMode = "AllCells"
    $logGrid.AllowUserToAddRows = $false

    # Add columns
    $logGrid.Columns.Add("Timestamp", "Timestamp") | Out-Null
    $logGrid.Columns.Add("Level", "Level") | Out-Null
    $logGrid.Columns.Add("Event", "Event") | Out-Null
    $logGrid.Columns.Add("Details", "Details") | Out-Null

    # Add data
    foreach ($entry in $script:SecurityLog) {
        $row = @($entry.Timestamp, $entry.Level, $entry.Event, $entry.Details)
        $logGrid.Rows.Add($row) | Out-Null

        # Color code by level
        $lastRow = $logGrid.Rows[$logGrid.Rows.Count - 1]
        switch ($entry.Level) {
            "ERROR" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Red }
            "WARNING" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Yellow }
            "SUCCESS" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Green }
            "DEBUG" { $lastRow.DefaultCellStyle.ForeColor = [System.Drawing.Color]::Gray }
        }
    }

    $logForm.Controls.Add($logGrid)
    $logForm.ShowDialog()
}

function Show-EncryptionTest {
    $testForm = New-Object System.Windows.Forms.Form
    $testForm.Text = "Encryption Test"
    $testForm.Size = New-Object System.Drawing.Size(600, 500)
    $testForm.StartPosition = "CenterScreen"
    $testForm.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $testForm.ForeColor = [System.Drawing.Color]::White

    # Input section
    $inputLabel = New-Object System.Windows.Forms.Label
    $inputLabel.Text = "Plain Text:"
    $inputLabel.Location = New-Object System.Drawing.Point(20, 20)
    $inputLabel.Size = New-Object System.Drawing.Size(100, 20)
    $testForm.Controls.Add($inputLabel)

    $inputBox = New-Object System.Windows.Forms.TextBox
    $inputBox.Location = New-Object System.Drawing.Point(20, 45)
    $inputBox.Size = New-Object System.Drawing.Size(540, 25)
    $inputBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $inputBox.ForeColor = [System.Drawing.Color]::White
    $inputBox.Text = "This is a test message for encryption"
    $testForm.Controls.Add($inputBox)

    # Encrypted section
    $encryptedLabel = New-Object System.Windows.Forms.Label
    $encryptedLabel.Text = "Encrypted:"
    $encryptedLabel.Location = New-Object System.Drawing.Point(20, 90)
    $encryptedLabel.Size = New-Object System.Drawing.Size(100, 20)
    $testForm.Controls.Add($encryptedLabel)

    $encryptedBox = New-Object System.Windows.Forms.TextBox
    $encryptedBox.Location = New-Object System.Drawing.Point(20, 115)
    $encryptedBox.Size = New-Object System.Drawing.Size(540, 100)
    $encryptedBox.Multiline = $true
    $encryptedBox.ReadOnly = $true
    $encryptedBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $encryptedBox.ForeColor = [System.Drawing.Color]::Cyan
    $encryptedBox.ScrollBars = "Vertical"
    $testForm.Controls.Add($encryptedBox)

    # Decrypted section
    $decryptedLabel = New-Object System.Windows.Forms.Label
    $decryptedLabel.Text = "Decrypted:"
    $decryptedLabel.Location = New-Object System.Drawing.Point(20, 235)
    $decryptedLabel.Size = New-Object System.Drawing.Size(100, 20)
    $testForm.Controls.Add($decryptedLabel)

    $decryptedBox = New-Object System.Windows.Forms.TextBox
    $decryptedBox.Location = New-Object System.Drawing.Point(20, 260)
    $decryptedBox.Size = New-Object System.Drawing.Size(540, 25)
    $decryptedBox.ReadOnly = $true
    $decryptedBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $decryptedBox.ForeColor = [System.Drawing.Color]::LightGreen
    $testForm.Controls.Add($decryptedBox)

    # Test info section
    $infoBox = New-Object System.Windows.Forms.TextBox
    $infoBox.Location = New-Object System.Drawing.Point(20, 300)
    $infoBox.Size = New-Object System.Drawing.Size(540, 100)
    $infoBox.Multiline = $true
    $infoBox.ReadOnly = $true
    $infoBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $infoBox.ForeColor = [System.Drawing.Color]::Yellow
    $infoBox.ScrollBars = "Vertical"
    $testForm.Controls.Add($infoBox)

    # Buttons
    $encryptBtn = New-Object System.Windows.Forms.Button
    $encryptBtn.Text = "Encrypt"
    $encryptBtn.Location = New-Object System.Drawing.Point(20, 410)
    $encryptBtn.Size = New-Object System.Drawing.Size(100, 30)
    $encryptBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $encryptBtn.ForeColor = [System.Drawing.Color]::White
    $encryptBtn.FlatStyle = "Flat"
    $testForm.Controls.Add($encryptBtn)

    $decryptBtn = New-Object System.Windows.Forms.Button
    $decryptBtn.Text = "Decrypt"
    $decryptBtn.Location = New-Object System.Drawing.Point(130, 410)
    $decryptBtn.Size = New-Object System.Drawing.Size(100, 30)
    $decryptBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $decryptBtn.ForeColor = [System.Drawing.Color]::White
    $decryptBtn.FlatStyle = "Flat"
    $testForm.Controls.Add($decryptBtn)

    $testBtn = New-Object System.Windows.Forms.Button
    $testBtn.Text = "Full Test"
    $testBtn.Location = New-Object System.Drawing.Point(240, 410)
    $testBtn.Size = New-Object System.Drawing.Size(100, 30)
    $testBtn.BackColor = [System.Drawing.Color]::FromArgb(0, 150, 0)
    $testBtn.ForeColor = [System.Drawing.Color]::White
    $testBtn.FlatStyle = "Flat"
    $testForm.Controls.Add($testBtn)

    # Event handlers
    $encryptBtn.Add_Click({
            try {
                $plainText = $inputBox.Text
                $encrypted = [StealthCrypto]::Encrypt($plainText, $script:CurrentSession.EncryptionKey)
                $encryptedBox.Text = $encrypted
                $hash = [StealthCrypto]::Hash($plainText)
                $infoBox.Text = "Encryption successful!`r`nOriginal length: $($plainText.Length) chars`r`nEncrypted length: $($encrypted.Length) chars`r`nSHA256 Hash: $hash"
            }
            catch {
                $infoBox.Text = "Encryption failed: $($_.Exception.Message)"
            }
        })

    $decryptBtn.Add_Click({
            try {
                if ($encryptedBox.Text) {
                    $decrypted = [StealthCrypto]::Decrypt($encryptedBox.Text, $script:CurrentSession.EncryptionKey)
                    $decryptedBox.Text = $decrypted
                    $match = $decrypted -eq $inputBox.Text
                    $infoBox.AppendText("`r`nDecryption successful!`r`nMatches original: $match")
                }
                else {
                    $infoBox.Text = "No encrypted data to decrypt"
                }
            }
            catch {
                $infoBox.Text = "Decryption failed: $($_.Exception.Message)"
            }
        })

    $testBtn.Add_Click({
            try {
                $testData = "Test encryption with special chars: !@#$%^&*()_+-=[]{}|;:',.<>?/`"~``"
                $startTime = Get-Date

                # Test encryption
                $encrypted = [StealthCrypto]::Encrypt($testData)
                $encryptTime = ((Get-Date) - $startTime).TotalMilliseconds

                # Test decryption
                $startTime = Get-Date
                $decrypted = [StealthCrypto]::Decrypt($encrypted)
                $decryptTime = ((Get-Date) - $startTime).TotalMilliseconds

                # Test hash
                $hash1 = [StealthCrypto]::Hash($testData)
                $hash2 = [StealthCrypto]::Hash($testData)

                $success = $decrypted -eq $testData
                $hashConsistent = $hash1 -eq $hash2

                $infoBox.Text = @"
FULL ENCRYPTION TEST RESULTS:
═══════════════════════════════════
Test Data: Special characters test
Original Length: $($testData.Length) chars
Encrypted Length: $($encrypted.Length) chars
Encryption Time: $([math]::Round($encryptTime, 2))ms
Decryption Time: $([math]::Round($decryptTime, 2))ms

VALIDATION:
Decryption Success: $success
Hash Consistency: $hashConsistent
Hash Value: $($hash1.Substring(0, 16))...

SECURITY STATUS:
Algorithm: AES-256-CBC
Key Size: $($script:CurrentSession.EncryptionKey.Length * 8) bits
Session Key: Yes (unique per session)
"@

                Write-SecurityLog "Encryption test completed" "SUCCESS" "Duration: $([math]::Round($encryptTime + $decryptTime, 2))ms"
            }
            catch {
                $infoBox.Text = "Full test failed: $($_.Exception.Message)"
            }
        })

    $testForm.ShowDialog()
}

# ===============================
# PERFORMANCE OPTIMIZATION FUNCTIONS
# ===============================

function Start-PerformanceOptimization {
    try {
        Write-DevConsole "🚀 Starting performance optimization..." "INFO"

        # Memory optimization
        Optimize-Memory

        # Process priority optimization
        Optimize-ProcessPriority

        # Network optimization
        Optimize-NetworkSettings

        # UI optimization
        Optimize-UIPerformance

        Write-DevConsole "✅ Performance optimization completed" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error during performance optimization: $_" "ERROR"
    }
}

function Optimize-Memory {
    try {
        # Force garbage collection
        [System.GC]::Collect()
        [System.GC]::WaitForPendingFinalizers()
        [System.GC]::Collect()

        # Set memory management options
        [System.GC]::TryStartNoGCRegion(50MB)

        Write-DevConsole "🧹 Memory optimization completed" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ Memory optimization partial: $_" "WARNING"
    }
}

function Optimize-ProcessPriority {
    try {
        $process = Get-Process -Id $PID
        $process.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High

        Write-DevConsole "⚡ Process priority set to High" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ Could not set process priority: $_" "WARNING"
    }
}

function Optimize-NetworkSettings {
    try {
        # Set HTTP connection limits
        [System.Net.ServicePointManager]::DefaultConnectionLimit = 20
        [System.Net.ServicePointManager]::Expect100Continue = $false
        [System.Net.ServicePointManager]::UseNagleAlgorithm = $false

        # Enable concurrent connections
        [System.Net.ServicePointManager]::EnableDnsRoundRobin = $true

        Write-DevConsole "🌐 Network settings optimized" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ Network optimization partial: $_" "WARNING"
    }
}

function Optimize-UIPerformance {
    try {
        # Enable double buffering for smoother UI
        Enable-ControlDoubleBuffering -Control $form

        # Optimize text rendering
        if ($script:editor) {
            Enable-ControlDoubleBuffering -Control $script:editor
        }

        Write-DevConsole "🎨 UI performance optimized" "SUCCESS"
    }
    catch {
        Write-DevConsole "⚠️ UI optimization partial: $_" "WARNING"
    }
}

function Enable-ControlDoubleBuffering {
    param(
        [System.Windows.Forms.Control]$Control
    )

    if (-not $Control) {
        return
    }

    $bindingFlags = [System.Reflection.BindingFlags] "Instance, NonPublic"
    $setStyleMethod = [System.Windows.Forms.Control].GetMethod("SetStyle", $bindingFlags)
    if ($setStyleMethod) {
        $styles = @(
            [System.Windows.Forms.ControlStyles]::AllPaintingInWmPaint,
            [System.Windows.Forms.ControlStyles]::DoubleBuffer,
            [System.Windows.Forms.ControlStyles]::ResizeRedraw,
            [System.Windows.Forms.ControlStyles]::UserPaint
        )

        foreach ($style in $styles) {
            try {
                $setStyleMethod.Invoke($Control, @($style, $true))
            }
            catch {
                # best-effort, ignore failures when reflection isn't allowed
            }
        }
    }

    $doubleBufferedProp = $Control.GetType().GetProperty("DoubleBuffered", $bindingFlags)
    if ($doubleBufferedProp) {
        try {
            $doubleBufferedProp.SetValue($Control, $true)
        }
        catch {
            # ignore failures
        }
    }
}

function Show-PerformanceMonitor {
    $perfForm = New-Object System.Windows.Forms.Form
    $perfForm.Text = "Performance Monitor"
    $perfForm.Size = New-Object System.Drawing.Size(600, 500)
    $perfForm.StartPosition = "CenterScreen"
    $perfForm.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::Sizable

    # Performance display
    $perfTextBox = New-Object System.Windows.Forms.TextBox
    $perfTextBox.Multiline = $true
    $perfTextBox.ReadOnly = $true
    $perfTextBox.ScrollBars = "Vertical"
    $perfTextBox.Font = New-Object System.Drawing.Font("Consolas", 10)
    $perfTextBox.Location = New-Object System.Drawing.Point(10, 10)
    $perfTextBox.Size = New-Object System.Drawing.Size(565, 400)
    $perfForm.Controls.Add($perfTextBox)

    # Optimize button
    $optimizeBtn = New-Object System.Windows.Forms.Button
    $optimizeBtn.Text = "Optimize Performance"
    $optimizeBtn.Location = New-Object System.Drawing.Point(10, 420)
    $optimizeBtn.Size = New-Object System.Drawing.Size(150, 30)
    $perfForm.Controls.Add($optimizeBtn)

    $optimizeBtn.Add_Click({
            Start-PerformanceOptimization
            Update-PerformanceDisplay $perfTextBox
        })

    # Refresh button
    $refreshBtn = New-Object System.Windows.Forms.Button
    $refreshBtn.Text = "Refresh"
    $refreshBtn.Location = New-Object System.Drawing.Point(170, 420)
    $refreshBtn.Size = New-Object System.Drawing.Size(100, 30)
    $perfForm.Controls.Add($refreshBtn)

    $refreshBtn.Add_Click({
            Update-PerformanceDisplay $perfTextBox
        })

    # Auto-refresh timer
    $perfTimer = New-Object System.Windows.Forms.Timer
    $perfTimer.Interval = 3000  # 3 seconds
    $perfTimer.Add_Tick({
            Update-PerformanceDisplay $perfTextBox
        })
    $perfTimer.Start()

    $perfForm.Add_FormClosed({
            $perfTimer.Stop()
            $perfTimer.Dispose()
        })

    # Initial display
    Update-PerformanceDisplay $perfTextBox

    $perfForm.ShowDialog()
}

function Update-PerformanceDisplay {
    param($TextBox)

    try {
        $process = Get-Process -Id $PID
        $timestamp = Get-Date -Format "HH:mm:ss"

        $perfInfo = @"
PERFORMANCE MONITOR - $timestamp
═══════════════════════════════════════════════════════

PROCESS INFORMATION:
  Process Name: $($process.ProcessName)
  Process ID: $($process.Id)
  Priority Class: $($process.PriorityClass)

MEMORY USAGE:
  Working Set: $([math]::Round($process.WorkingSet64/1MB, 2)) MB
  Private Memory: $([math]::Round($process.PrivateMemorySize64/1MB, 2)) MB
  Virtual Memory: $([math]::Round($process.VirtualMemorySize64/1MB, 2)) MB
  Peak Working Set: $([math]::Round($process.PeakWorkingSet64/1MB, 2)) MB

CPU USAGE:
  Total Processor Time: $($process.TotalProcessorTime)
  User Processor Time: $($process.UserProcessorTime)

THREAD INFORMATION:
  Thread Count: $($process.Threads.Count)

HANDLE COUNT:
  Handle Count: $($process.HandleCount)

OLLAMA STATUS:
  Connection Status: $(if (Test-OllamaConnection) { "✅ Connected" } else { "❌ Disconnected" })
  Active Servers: $(@($script:OllamaServers).Count)

REAL-TIME MONITORING:
  Status Updates: $(if ($script:RealTimeMonitoring.StatusTimer.Enabled) { "✅ Active" } else { "❌ Inactive" })
  Performance Tracking: $(if ($script:RealTimeMonitoring.PerformanceTimer.Enabled) { "✅ Active" } else { "❌ Inactive" })
  Network Monitoring: $(if ($script:RealTimeMonitoring.NetworkTimer.Enabled) { "✅ Active" } else { "❌ Inactive" })

ERROR HANDLING:
  Total Errors Handled: $($script:ErrorStats.TotalErrors)
  Critical Errors: $($script:ErrorStats.CriticalErrors)
  Security Events: $($script:ErrorStats.SecurityErrors)
  Auto-Recovery Actions: $($script:ErrorStats.AutoRecoveryCount)

SECURITY STATUS:
  Authentication: $(if ($script:CurrentSession) { "✅ Authenticated" } else { "❌ Not Authenticated" })
  Session Active: $(if ($script:CurrentSession) { "✅ Active (ID: $($script:CurrentSession.SessionId.Substring(0,8))...)" } else { "❌ No Session" })
  Encryption: $(if ($script:CurrentSession -and $script:CurrentSession.EncryptionKey) { "✅ AES-256-CBC" } else { "❌ Not Available" })
  Stealth Mode: $(if ($script:StealthModeActive) { "✅ Active" } else { "❌ Inactive" })

CUSTOMIZATION:
  Current Theme: $($script:CurrentTheme)
  Font Size: $($script:CurrentFontSize)pt
  UI Scale: $($script:CurrentUIScale * 100)%

RECOMMENDATIONS:
$(if ($process.WorkingSet64 -gt 500MB) { "⚠️ High memory usage detected - consider restarting`n" })$(if (@($process.Threads).Count -gt 50) { "⚠️ High thread count - check for resource leaks`n" })$(if (-not (Test-OllamaConnection)) { "⚠️ Ollama connection lost - check server status`n" })$(if ($script:ErrorStats.CriticalErrors -gt 0) { "🚨 Critical errors detected - review error logs`n" })
"@

        $TextBox.Text = $perfInfo
        $TextBox.SelectionStart = $TextBox.Text.Length
        $TextBox.ScrollToCaret()
    }
    catch {
        $TextBox.Text = "Error updating performance display: $($_.Exception.Message)"
    }
}

function Start-PerformanceProfiler {
    param(
        [int]$DurationSeconds = 60,
        [int]$SampleIntervalMs = 1000
    )

    Write-DevConsole "🔍 Starting performance profiler for $DurationSeconds seconds..." "INFO"

    $script:ProfilerData = @{
        StartTime = Get-Date
        Samples   = @()
        IsRunning = $true
    }

    $profilerTimer = New-Object System.Windows.Forms.Timer
    $profilerTimer.Interval = $SampleIntervalMs
    $sampleCount = 0
    $maxSamples = $DurationSeconds * (1000 / $SampleIntervalMs)

    $profilerTimer.Add_Tick({
            if ($sampleCount -ge $maxSamples) {
                $profilerTimer.Stop()
                $script:ProfilerData.IsRunning = $false
                Show-ProfilerResults
                return
            }

            try {
                $process = Get-Process -Id $PID
                $sample = @{
                    Timestamp     = Get-Date
                    WorkingSet    = $process.WorkingSet64
                    PrivateMemory = $process.PrivateMemorySize64
                    ThreadCount   = $process.Threads.Count
                    HandleCount   = $process.HandleCount
                }

                $script:ProfilerData.Samples += $sample
                $sampleCount++

                Write-DevConsole "📊 Profiler sample $sampleCount/$maxSamples collected" "INFO"
            }
            catch {
                Write-DevConsole "⚠️ Profiler sample error: $_" "WARNING"
            }
        })

    $profilerTimer.Start()
}

function Show-ProfilerResults {
    if (-not $script:ProfilerData -or $script:ProfilerData.Samples.Count -eq 0) {
        Write-DevConsole "⚠️ No profiler data available" "WARNING"
        return
    }

    $resultsForm = New-Object System.Windows.Forms.Form
    $resultsForm.Text = "Performance Profiler Results"
    $resultsForm.Size = New-Object System.Drawing.Size(800, 600)
    $resultsForm.StartPosition = "CenterScreen"

    $resultsTextBox = New-Object System.Windows.Forms.TextBox
    $resultsTextBox.Multiline = $true
    $resultsTextBox.ReadOnly = $true
    $resultsTextBox.ScrollBars = "Vertical"
    $resultsTextBox.Font = New-Object System.Drawing.Font("Consolas", 9)
    $resultsTextBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $resultsForm.Controls.Add($resultsTextBox)

    # Calculate statistics
    $samples = @($script:ProfilerData.Samples)
    $sampleCount = $samples.Count
    $duration = ($samples[-1].Timestamp - $samples[0].Timestamp).TotalSeconds

    $avgWorkingSet = ($samples | Measure-Object -Property WorkingSet -Average).Average
    $maxWorkingSet = ($samples | Measure-Object -Property WorkingSet -Maximum).Maximum
    $minWorkingSet = ($samples | Measure-Object -Property WorkingSet -Minimum).Minimum

    $avgPrivateMemory = ($samples | Measure-Object -Property PrivateMemory -Average).Average
    $avgThreadCount = ($samples | Measure-Object -Property ThreadCount -Average).Average
    $avgHandleCount = ($samples | Measure-Object -Property HandleCount -Average).Average

    $results = @"
PERFORMANCE PROFILER RESULTS
═══════════════════════════════════════════════════════

PROFILING SESSION:
  Start Time: $($script:ProfilerData.StartTime)
  Duration: $([math]::Round($duration, 2)) seconds
  Sample Count: $sampleCount
  Sample Rate: $([math]::Round($sampleCount / $duration, 2)) samples/second

MEMORY STATISTICS:
  Working Set:
    Average: $([math]::Round($avgWorkingSet/1MB, 2)) MB
    Maximum: $([math]::Round($maxWorkingSet/1MB, 2)) MB
    Minimum: $([math]::Round($minWorkingSet/1MB, 2)) MB
    Variation: $([math]::Round(($maxWorkingSet - $minWorkingSet)/1MB, 2)) MB

  Private Memory:
    Average: $([math]::Round($avgPrivateMemory/1MB, 2)) MB

RESOURCE STATISTICS:
  Thread Count Average: $([math]::Round($avgThreadCount, 1))
  Handle Count Average: $([math]::Round($avgHandleCount, 1))

PERFORMANCE ANALYSIS:
$(if (($maxWorkingSet - $minWorkingSet) -gt 100MB) { "⚠️ High memory variation detected - potential memory leaks`n" })$(if ($avgThreadCount -gt 30) { "⚠️ High average thread count - check for thread leaks`n" })$(if ($avgWorkingSet -gt 300MB) { "⚠️ High average memory usage`n" })✅ Profiling completed successfully

DETAILED SAMPLES:
════════════════
"@

    foreach ($sample in $samples) {
        $results += "`n$($sample.Timestamp.ToString('HH:mm:ss.fff')) | WS: $([math]::Round($sample.WorkingSet/1MB, 1))MB | PM: $([math]::Round($sample.PrivateMemory/1MB, 1))MB | T: $($sample.ThreadCount) | H: $($sample.HandleCount)"
    }

    $resultsTextBox.Text = $results
    $resultsForm.ShowDialog()
}

# ===============================
# CUSTOMIZATION FUNCTIONS
# ===============================

function Apply-Theme {
    param(
        [string]$ThemeName
    )

    try {
        Write-DevConsole "Applying $ThemeName theme..." "INFO"

        switch ($ThemeName) {
            "Stealth-Cheetah" {
                # Stealth-Cheetah: Professional dark theme with amber accents for stealth operations
                $bgColor = [System.Drawing.Color]::FromArgb(18, 18, 18)          # Deep black background
                $fgColor = [System.Drawing.Color]::FromArgb(220, 220, 220)       # Light gray text
                $panelColor = [System.Drawing.Color]::FromArgb(25, 25, 25)       # Slightly lighter panels
                $textColor = [System.Drawing.Color]::FromArgb(255, 191, 0)       # Amber/cheetah accent color
                Write-DevConsole "🐆 Stealth-Cheetah theme activated - Maximum stealth mode" "SUCCESS"
            }
            "Dark" {
                $bgColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
                $fgColor = [System.Drawing.Color]::White
                $panelColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
                $textColor = [System.Drawing.Color]::White
            }
            "Light" {
                $bgColor = [System.Drawing.Color]::White
                $fgColor = [System.Drawing.Color]::Black
                $panelColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
                $textColor = [System.Drawing.Color]::Black
            }
            default {
                # Default to Stealth-Cheetah
                $bgColor = [System.Drawing.Color]::FromArgb(18, 18, 18)
                $fgColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
                $panelColor = [System.Drawing.Color]::FromArgb(25, 25, 25)
                $textColor = [System.Drawing.Color]::FromArgb(255, 191, 0)
                Write-DevConsole "🐆 Defaulting to Stealth-Cheetah theme" "INFO"
            }
        }

        # Apply to main form
        $form.BackColor = $bgColor
        $form.ForeColor = $fgColor

        # Apply to panels - use correct splitter panel references
        try {
            if ($mainSplitter.Panel1) { $mainSplitter.Panel1.BackColor = $panelColor }
            if ($mainSplitter.Panel2) { $mainSplitter.Panel2.BackColor = $panelColor }
            if ($leftSplitter.Panel1) { $leftSplitter.Panel1.BackColor = $panelColor }
            if ($leftSplitter.Panel2) { $leftSplitter.Panel2.BackColor = $panelColor }
            if ($leftPanel) { $leftPanel.BackColor = $panelColor }
            if ($explorerContainer) { $explorerContainer.BackColor = $panelColor }
            if ($explorerToolbar) { $explorerToolbar.BackColor = $panelColor }
        }
        catch {
            Write-DevConsole "Panel theming partial: $_" "WARNING"
        }

        # Apply to chat boxes
        try {
            if ($script:chatTabs) {
                foreach ($session in $script:chatTabs.Values) {
                    if ($session.ChatBox) {
                        $session.ChatBox.BackColor = $bgColor
                        $session.ChatBox.ForeColor = $textColor
                    }
                    if ($session.InputBox) {
                        $session.InputBox.BackColor = $bgColor
                        $session.InputBox.ForeColor = $textColor
                    }
                }
            }
        }
        catch {
            Write-DevConsole "Chat theming partial: $_" "WARNING"
        }

        # Apply to text editor
        try {
            if ($script:editor) {
                $script:editor.BackColor = $bgColor
                $script:editor.ForeColor = $textColor
            }
        }
        catch {
            Write-DevConsole "Editor theming partial: $_" "WARNING"
        }

        # Save theme preference
        $script:CurrentTheme = $ThemeName
        Save-CustomizationSettings

        Write-DevConsole "✅ $ThemeName theme applied successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying theme: $_" "ERROR"
    }
}

function Apply-FontSize {
    param(
        [int]$Size
    )

    try {
        Write-DevConsole "Applying font size: ${Size}pt..." "INFO"

        $newFont = New-Object System.Drawing.Font("Segoe UI", $Size)

        # Apply to main form elements
        $form.Font = $newFont

        # Apply to chat boxes
        try {
            if ($script:chatTabs) {
                foreach ($session in $script:chatTabs.Values) {
                    if ($session.ChatBox) {
                        $session.ChatBox.Font = New-Object System.Drawing.Font("Consolas", $Size)
                    }
                    if ($session.InputBox) {
                        $session.InputBox.Font = New-Object System.Drawing.Font("Consolas", $Size)
                    }
                }
            }
        }
        catch {
            Write-DevConsole "Chat font update partial: $_" "WARNING"
        }

        # Apply to text editor
        try {
            if ($script:editor) {
                $script:editor.Font = New-Object System.Drawing.Font("Consolas", $Size)
            }
        }
        catch {
            Write-DevConsole "Editor font update partial: $_" "WARNING"
        }

        # Save font preference
        $script:CurrentFontSize = $Size
        Save-CustomizationSettings

        Write-DevConsole "✅ Font size set to ${Size}pt successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying font size: $_" "ERROR"
    }
}

function Apply-UIScaling {
    param(
        [double]$Scale
    )

    try {
        Write-DevConsole "Applying UI scaling: $($Scale * 100)%..." "INFO"

        # Calculate scaled dimensions
        $baseWidth = 1200
        $baseHeight = 800
        $scaledWidth = [int]($baseWidth * $Scale)
        $scaledHeight = [int]($baseHeight * $Scale)

        # Apply to main form
        $form.Size = New-Object System.Drawing.Size($scaledWidth, $scaledHeight)

        # Scale panels proportionally
        try {
            if ($mainSplitter) { $mainSplitter.SplitterDistance = [int](300 * $Scale) }
            # Note: StatusPanel doesn't exist, might be referring to a toolbar - skipping for now
        }
        catch {
            Write-DevConsole "Panel scaling partial: $_" "WARNING"
        }

        # Save scaling preference
        $script:CurrentUIScale = $Scale
        Save-CustomizationSettings

        Write-DevConsole "✅ UI scaling set to $($Scale * 100)% successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying UI scaling: $_" "ERROR"
    }
}

function Update-FontMenuChecks {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$SelectedItem,
        [System.Windows.Forms.ToolStripMenuItem[]]$AllItems
    )

    foreach ($item in $AllItems) {
        $item.Checked = ($item -eq $SelectedItem)
    }
}

function Update-ScaleMenuChecks {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$SelectedItem,
        [System.Windows.Forms.ToolStripMenuItem[]]$AllItems
    )

    foreach ($item in $AllItems) {
        $item.Checked = ($item -eq $SelectedItem)
    }
}

function Show-CustomThemeBuilder {
    $themeForm = New-Object System.Windows.Forms.Form
    $themeForm.Text = "Custom Theme Builder"
    $themeForm.Size = New-Object System.Drawing.Size(500, 400)
    $themeForm.StartPosition = "CenterScreen"
    $themeForm.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::FixedDialog
    $themeForm.MaximizeBox = $false

    # Background Color
    $bgLabel = New-Object System.Windows.Forms.Label
    $bgLabel.Text = "Background Color:"
    $bgLabel.Location = New-Object System.Drawing.Point(20, 30)
    $bgLabel.Size = New-Object System.Drawing.Size(120, 20)
    $themeForm.Controls.Add($bgLabel)

    $bgButton = New-Object System.Windows.Forms.Button
    $bgButton.Text = "Select Color"
    $bgButton.Location = New-Object System.Drawing.Point(150, 25)
    $bgButton.Size = New-Object System.Drawing.Size(100, 30)
    $bgButton.BackColor = [System.Drawing.Color]::White
    $themeForm.Controls.Add($bgButton)

    $bgButton.Add_Click({
            $colorDialog = New-Object System.Windows.Forms.ColorDialog
            if ($colorDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                $bgButton.BackColor = $colorDialog.Color
            }
        })

    # Text Color
    $textLabel = New-Object System.Windows.Forms.Label
    $textLabel.Text = "Text Color:"
    $textLabel.Location = New-Object System.Drawing.Point(20, 80)
    $textLabel.Size = New-Object System.Drawing.Size(120, 20)
    $themeForm.Controls.Add($textLabel)

    $textButton = New-Object System.Windows.Forms.Button
    $textButton.Text = "Select Color"
    $textButton.Location = New-Object System.Drawing.Point(150, 75)
    $textButton.Size = New-Object System.Drawing.Size(100, 30)
    $textButton.BackColor = [System.Drawing.Color]::Black
    $textButton.ForeColor = [System.Drawing.Color]::White
    $themeForm.Controls.Add($textButton)

    $textButton.Add_Click({
            $colorDialog = New-Object System.Windows.Forms.ColorDialog
            if ($colorDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                $textButton.BackColor = $colorDialog.Color
                $textButton.ForeColor = if ($colorDialog.Color.GetBrightness() -gt 0.5) { [System.Drawing.Color]::Black } else { [System.Drawing.Color]::White }
            }
        })

    # Panel Color
    $panelLabel = New-Object System.Windows.Forms.Label
    $panelLabel.Text = "Panel Color:"
    $panelLabel.Location = New-Object System.Drawing.Point(20, 130)
    $panelLabel.Size = New-Object System.Drawing.Size(120, 20)
    $themeForm.Controls.Add($panelLabel)

    $panelButton = New-Object System.Windows.Forms.Button
    $panelButton.Text = "Select Color"
    $panelButton.Location = New-Object System.Drawing.Point(150, 125)
    $panelButton.Size = New-Object System.Drawing.Size(100, 30)
    $panelButton.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
    $themeForm.Controls.Add($panelButton)

    $panelButton.Add_Click({
            $colorDialog = New-Object System.Windows.Forms.ColorDialog
            if ($colorDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                $panelButton.BackColor = $colorDialog.Color
            }
        })

    # Preview Panel
    $previewPanel = New-Object System.Windows.Forms.Panel
    $previewPanel.Location = New-Object System.Drawing.Point(300, 25)
    $previewPanel.Size = New-Object System.Drawing.Size(150, 200)
    $previewPanel.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
    $previewPanel.BackColor = $bgButton.BackColor
    $themeForm.Controls.Add($previewPanel)

    $previewLabel = New-Object System.Windows.Forms.Label
    $previewLabel.Text = "Preview Text"
    $previewLabel.Location = New-Object System.Drawing.Point(10, 10)
    $previewLabel.Size = New-Object System.Drawing.Size(130, 20)
    $previewLabel.BackColor = $textButton.BackColor
    $previewLabel.ForeColor = $textButton.ForeColor
    $previewPanel.Controls.Add($previewLabel)

    # Apply Button
    $applyButton = New-Object System.Windows.Forms.Button
    $applyButton.Text = "Apply Theme"
    $applyButton.Location = New-Object System.Drawing.Point(200, 300)
    $applyButton.Size = New-Object System.Drawing.Size(100, 35)
    $themeForm.Controls.Add($applyButton)

    $applyButton.Add_Click({
            Apply-CustomTheme -BackColor $bgButton.BackColor -TextColor $textButton.BackColor -PanelColor $panelButton.BackColor
            $themeForm.Close()
        })

    $themeForm.ShowDialog()
}

function Apply-CustomTheme {
    param(
        [System.Drawing.Color]$BackColor,
        [System.Drawing.Color]$TextColor,
        [System.Drawing.Color]$PanelColor
    )

    try {
        Write-DevConsole "Applying custom theme..." "INFO"

        # Apply to main form
        $form.BackColor = $BackColor
        $form.ForeColor = $TextColor

        # Apply to panels - use correct splitter panel references
        try {
            if ($mainSplitter.Panel1) { $mainSplitter.Panel1.BackColor = $PanelColor }
            if ($mainSplitter.Panel2) { $mainSplitter.Panel2.BackColor = $PanelColor }
            if ($leftSplitter.Panel1) { $leftSplitter.Panel1.BackColor = $PanelColor }
            if ($leftSplitter.Panel2) { $leftSplitter.Panel2.BackColor = $PanelColor }
            if ($leftPanel) { $leftPanel.BackColor = $PanelColor }
            if ($explorerContainer) { $explorerContainer.BackColor = $PanelColor }
            if ($explorerToolbar) { $explorerToolbar.BackColor = $PanelColor }
        }
        catch {
            Write-DevConsole "Panel theming partial: $_" "WARNING"
        }

        # Apply to chat boxes
        try {
            if ($script:chatTabs) {
                foreach ($session in $script:chatTabs.Values) {
                    if ($session.ChatBox) {
                        $session.ChatBox.BackColor = $BackColor
                        $session.ChatBox.ForeColor = $TextColor
                    }
                    if ($session.InputBox) {
                        $session.InputBox.BackColor = $BackColor
                        $session.InputBox.ForeColor = $TextColor
                    }
                }
            }
        }
        catch {
            Write-DevConsole "Chat custom theming partial: $_" "WARNING"
        }

        # Apply to text editor
        try {
            if ($script:editor) {
                $script:editor.BackColor = $BackColor
                $script:editor.ForeColor = $TextColor
            }
        }
        catch {
            Write-DevConsole "Editor custom theming partial: $_" "WARNING"
        }

        # Save custom theme
        $script:CustomTheme = @{
            BackColor  = $BackColor
            TextColor  = $TextColor
            PanelColor = $PanelColor
        }
        $script:CurrentTheme = "Custom"
        Save-CustomizationSettings

        Write-DevConsole "✅ Custom theme applied successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error applying custom theme: $_" "ERROR"
    }
}

function Reset-UILayout {
    try {
        Write-DevConsole "Resetting UI layout to defaults..." "INFO"

        # Reset form size
        $form.Size = New-Object System.Drawing.Size(1200, 800)
        $form.StartPosition = "CenterScreen"

        # Reset panel sizes - use splitter distance instead of non-existent panels
        try {
            if ($mainSplitter) { $mainSplitter.SplitterDistance = 300 }
            # Note: StatusPanel doesn't exist in current structure
        }
        catch {
            Write-DevConsole "Panel reset partial: $_" "WARNING"
        }

        # Reset splitter position
        if ($mainSplitter) { $mainSplitter.SplitterDistance = 300 }

        # Reset theme to light
        Apply-Theme "Light"

        # Reset font size to 14pt
        Apply-FontSize 14

        # Reset UI scaling to 100%
        Apply-UIScaling 1.0

        Write-DevConsole "✅ UI layout reset to defaults successfully" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error resetting UI layout: $_" "ERROR"
    }
}

function Save-UILayout {
    try {
        $layoutData = @{
            FormSize          = @{
                Width  = $form.Width
                Height = $form.Height
            }
            FormPosition      = @{
                X = $form.Location.X
                Y = $form.Location.Y
            }
            LeftPanelWidth    = if ($mainSplitter) { $mainSplitter.SplitterDistance } else { 300 }
            StatusPanelHeight = 30  # Default value as status panel doesn't exist
            SplitterDistance  = if ($mainSplitter) { $mainSplitter.SplitterDistance } else { 300 }
            Theme             = $script:CurrentTheme
            FontSize          = $script:CurrentFontSize
            UIScale           = $script:CurrentUIScale
        }

        $layoutPath = Join-Path $env:USERPROFILE "RawrXD_Layout.json"
        $layoutData | ConvertTo-Json -Depth 3 | Set-Content -Path $layoutPath

        Write-DevConsole "✅ UI layout saved to: $layoutPath" "SUCCESS"
    }
    catch {
        Write-DevConsole "❌ Error saving UI layout: $_" "ERROR"
    }
}

function Load-UILayout {
    try {
        $layoutPath = Join-Path $env:USERPROFILE "RawrXD_Layout.json"

        if (Test-Path $layoutPath) {
            $layoutData = Get-Content -Path $layoutPath | ConvertFrom-Json

            # Apply saved layout
            $form.Size = New-Object System.Drawing.Size($layoutData.FormSize.Width, $layoutData.FormSize.Height)
            $form.Location = New-Object System.Drawing.Point($layoutData.FormPosition.X, $layoutData.FormPosition.Y)

            # Apply splitter distance instead of non-existent panel properties
            try {
                if ($mainSplitter -and $layoutData.SplitterDistance) {
                    $mainSplitter.SplitterDistance = $layoutData.SplitterDistance
                }
            }
            catch {
                Write-DevConsole "Splitter restore partial: $_" "WARNING"
            }

            # Apply saved customization settings
            if ($layoutData.Theme) { Apply-Theme $layoutData.Theme }
            if ($layoutData.FontSize) { Apply-FontSize $layoutData.FontSize }
            if ($layoutData.UIScale) { Apply-UIScaling $layoutData.UIScale }

            Write-DevConsole "✅ UI layout loaded successfully" "SUCCESS"
        }
        else {
            Write-DevConsole "⚠️ No saved layout found, using defaults" "WARNING"
        }
    }
    catch {
        Write-DevConsole "❌ Error loading UI layout: $_" "ERROR"
    }
}

function Save-CustomizationSettings {
    try {
        $settings = @{
            Theme       = $script:CurrentTheme
            FontSize    = $script:CurrentFontSize
            UIScale     = $script:CurrentUIScale
            CustomTheme = $script:CustomTheme
        }

        $settingsPath = Join-Path $env:USERPROFILE "RawrXD_Customization.json"
        $settings | ConvertTo-Json -Depth 3 | Set-Content -Path $settingsPath
    }
    catch {
        Write-DevConsole "❌ Error saving customization settings: $_" "ERROR"
    }
}

function Load-CustomizationSettings {
    try {
        $settingsPath = Join-Path $env:USERPROFILE "RawrXD_Customization.json"

        if (Test-Path $settingsPath) {
            $settings = Get-Content -Path $settingsPath | ConvertFrom-Json

            $script:CurrentTheme = if ($settings.Theme) { $settings.Theme } else { "Stealth-Cheetah" }
            $script:CurrentFontSize = if ($settings.FontSize) { $settings.FontSize } else { 14 }
            $script:CurrentUIScale = if ($settings.UIScale) { $settings.UIScale } else { 1.0 }
            $script:CustomTheme = $settings.CustomTheme

            # Apply loaded settings
            if ($script:CurrentTheme -ne "Stealth-Cheetah") {
                Apply-Theme $script:CurrentTheme
            }
            else {
                # Apply default Stealth-Cheetah theme
                Apply-Theme "Stealth-Cheetah"
            }
            if ($script:CurrentFontSize -ne 14) {
                Apply-FontSize $script:CurrentFontSize
            }
            if ($script:CurrentUIScale -ne 1.0) {
                Apply-UIScaling $script:CurrentUIScale
            }
        }
        else {
            # No settings file exists, apply default Stealth-Cheetah theme
            Apply-Theme "Stealth-Cheetah"
            Write-DevConsole "🐆 Applied default Stealth-Cheetah theme" "INFO"
        }
    }
    catch {
        Write-DevConsole "❌ Error loading customization settings: $_" "ERROR"
        # Fallback to Stealth-Cheetah on error
        Apply-Theme "Stealth-Cheetah"
    }
}

# ============================================
# AGENTIC AI ERROR DASHBOARD
# ============================================

function Get-AIErrorDashboard {
    param(
        [int]$DaysBack = 7,
        [switch]$IncludeSuccessMetrics
    )

    try {
        # Load AI error statistics
        $statsFile = Join-Path $script:EmergencyLogPath "ai_error_stats.json"
        $aiLogPath = Join-Path $script:EmergencyLogPath "AI_Errors"

        $dashboard = @"
═══════════════════════════════════════════════════════════
🤖 AI AGENT ERROR DASHBOARD - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
═══════════════════════════════════════════════════════════

"@

        # Check if stats file exists
        if (Test-Path $statsFile) {
            $stats = Get-Content $statsFile -Raw | ConvertFrom-Json

            $dashboard += @"
📊 OVERALL STATISTICS (Since Start):
   Total AI Errors: $($stats.TotalErrors)
   Last Updated: $($stats.LastUpdated)

📈 ERRORS BY CATEGORY:
"@
            if ($stats.ErrorsByCategory) {
                foreach ($category in $stats.ErrorsByCategory.PSObject.Properties) {
                    $dashboard += "`n   $($category.Name): $($category.Value)"
                }
            }
            else {
                $dashboard += "`n   No category data available"
            }

            $dashboard += @"

🔥 ERRORS BY SEVERITY:
"@
            if ($stats.ErrorsBySeverity) {
                foreach ($severity in $stats.ErrorsBySeverity.PSObject.Properties) {
                    $severity_icon = switch ($severity.Name) {
                        "CRITICAL" { "🔴" }
                        "HIGH" { "🟡" }
                        "MEDIUM" { "🟠" }
                        "LOW" { "🟢" }
                        default { "⚪" }
                    }
                    $dashboard += "`n   $severity_icon $($severity.Name): $($severity.Value)"
                }
            }
            else {
                $dashboard += "`n   No severity data available"
            }

            $dashboard += @"

🤖 ERRORS BY MODEL:
"@
            if ($stats.ErrorsByModel) {
                foreach ($model in $stats.ErrorsByModel.PSObject.Properties) {
                    $dashboard += "`n   🧠 $($model.Name): $($model.Value)"
                }
            }
            else {
                $dashboard += "`n   No model data available"
            }
        }
        else {
            $dashboard += @"
📊 OVERALL STATISTICS:
   No error statistics available yet
   Stats file: $statsFile
"@
        }

        # Recent error files analysis
        $dashboard += @"

📁 RECENT ERROR LOGS (Last $DaysBack days):
"@

        if (Test-Path $aiLogPath) {
            $cutoffDate = (Get-Date).AddDays(-$DaysBack)
            $recentLogs = Get-ChildItem "$aiLogPath\ai_errors_*.log" | Where-Object { $_.LastWriteTime -ge $cutoffDate } | Sort-Object LastWriteTime -Descending

            if ($recentLogs) {
                foreach ($log in $recentLogs) {
                    $fileDate = $log.LastWriteTime.ToString("yyyy-MM-dd HH:mm")
                    $fileSize = [Math]::Round($log.Length / 1KB, 1)
                    $dashboard += "`n   📄 $($log.Name) - $fileDate - ${fileSize}KB"
                }
            }
            else {
                $dashboard += "`n   ✅ No recent error logs found"
            }
        }
        else {
            $dashboard += "`n   📁 AI error log directory not created yet"
        }

        # System health indicators
        $dashboard += @"

🏥 SYSTEM HEALTH:
   Current Session: $($script:CurrentSession.SessionId)
   Session Start: $($script:CurrentSession.StartTime.ToString("yyyy-MM-dd HH:mm:ss"))
   Last Activity: $($script:CurrentSession.LastActivity.ToString("yyyy-MM-dd HH:mm:ss"))
   Agent Mode: $(if ($global:AgentMode) { "🟢 ACTIVE" } else { "🔴 INACTIVE" })
   Ollama Connection: $(if (Test-NetConnection -ComputerName localhost -Port 11434 -InformationLevel Quiet) { "🟢 ONLINE" } else { "🔴 OFFLINE" })

💡 QUICK ACTIONS:
   /ai-errors          - Show this dashboard
   /clear-ai-errors    - Clear error statistics
   /ai-logs           - View recent error details
   /agent-status      - Check agent system status

═══════════════════════════════════════════════════════════
"@

        return $dashboard
    }
    catch {
        return @"
❌ Error generating AI Error Dashboard: $($_.Exception.Message)

Basic Info:
- Emergency Log Path: $script:EmergencyLogPath
- Current Session: $($script:CurrentSession.SessionId)
- Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
"@
    }
}

function Clear-AIErrorStatistics {
    try {
        $statsFile = Join-Path $script:EmergencyLogPath "ai_error_stats.json"
        $aiLogPath = Join-Path $script:EmergencyLogPath "AI_Errors"

        # Reset statistics file
        if (Test-Path $statsFile) {
            Remove-Item $statsFile -Force
        }

        # Archive old error logs (don't delete, just move to archive)
        if (Test-Path $aiLogPath) {
            $archivePath = Join-Path $aiLogPath "archive_$(Get-Date -Format 'yyyy-MM-dd_HH-mm-ss')"
            New-Item -ItemType Directory -Path $archivePath -Force | Out-Null

            Get-ChildItem "$aiLogPath\ai_errors_*.log" | ForEach-Object {
                Move-Item $_.FullName -Destination $archivePath
            }
        }

        Write-StartupLog "AI error statistics cleared and logs archived" "INFO"
        return "✅ AI error statistics cleared and logs archived to $archivePath"
    }
    catch {
        Write-StartupLog "Failed to clear AI error statistics: $($_.Exception.Message)" "ERROR"
        return "❌ Failed to clear AI error statistics: $($_.Exception.Message)"
    }
}

# ============================================

# Initialize customization variables
$script:CurrentTheme = "Stealth-Cheetah"  # Default to stealth-cheetah theme
$script:CurrentFontSize = 14
$script:CurrentUIScale = 1.0
$script:CustomTheme = $null

# Initialize error statistics
$script:ErrorStats = @{
    TotalErrors       = 0
    CriticalErrors    = 0
    SecurityErrors    = 0
    NetworkErrors     = 0
    FilesystemErrors  = 0
    UIErrors          = 0
    OllamaErrors      = 0
    AuthErrors        = 0
    PerformanceErrors = 0
    AutoRecoveryCount = 0
}