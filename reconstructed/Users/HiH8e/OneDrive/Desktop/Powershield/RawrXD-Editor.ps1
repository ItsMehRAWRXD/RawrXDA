# ============================================
# RawrXD-Editor.ps1 - Editor Module
# ============================================
# Contains file operation handlers, editor command handlers,
# find & replace dialogs, cursor style hooking helpers,
# enhanced file browser functions, and code editing tools.
# ============================================

# ===============================
# FILE OPERATION HANDLERS
# ===============================

function New-EditorFile {
    Write-DevConsole "[File] Creating new file..." "INFO"

    # Clear current file reference
    $global:currentFile = $null

    # Send command to Monaco editor
    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.setValue('');
                window.editor.focus();
                console.log('✅ New file created');
            }
"@) | Out-Null
    }

    Write-DevConsole "[File] New file ready" "SUCCESS"
}

function Open-EditorFile {
    Write-DevConsole "[File] Opening file..." "INFO"

    Add-Type -AssemblyName System.Windows.Forms
    $openDialog = New-Object System.Windows.Forms.OpenFileDialog
    $openDialog.Filter = "All Files (*.*)|*.*|PowerShell (*.ps1)|*.ps1|JavaScript (*.js)|*.js|Python (*.py)|*.py|Text (*.txt)|*.txt"
    $openDialog.Title = "Open File"

    if ($openDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $filePath = $openDialog.FileName
        $content = Get-Content -Path $filePath -Raw -ErrorAction SilentlyContinue

        if ($content) {
            $global:currentFile = $filePath

            # Escape content for JavaScript
            $escapedContent = $content -replace '\\', '\\' -replace "`r`n", '\n' -replace "`n", '\n' -replace '"', '\"' -replace "'", "\'"

            # Send to Monaco editor
            if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
                $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
                    if (window.editor) {
                        window.editor.setValue("$escapedContent");
                        console.log('✅ File opened: $filePath');
                    }
"@) | Out-Null
            }

            Write-DevConsole "[File] Opened: $filePath" "SUCCESS"
        }
    }
}

function Save-EditorFile {
    Write-DevConsole "[File] Saving file..." "INFO"

    if (-not $global:currentFile) {
        Save-EditorFileAs
        return
    }

    # Get content from Monaco editor
    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            (function() {
                if (window.editor) {
                    const content = window.editor.getValue();
                    window.chrome.webview.postMessage({
                        command: 'saveContent',
                        params: { content: content, path: '$global:currentFile' }
                    });
                }
            })();
"@) | Out-Null
    }
}

function Save-EditorFileAs {
    Write-DevConsole "[File] Save As..." "INFO"

    Add-Type -AssemblyName System.Windows.Forms
    $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
    $saveDialog.Filter = "All Files (*.*)|*.*|PowerShell (*.ps1)|*.ps1|JavaScript (*.js)|*.js|Python (*.py)|*.py|Text (*.txt)|*.txt"
    $saveDialog.Title = "Save File As"

    if ($saveDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $global:currentFile = $saveDialog.FileName
        Save-EditorFile
    }
}

function Close-EditorFile {
    Write-DevConsole "[File] Closing file..." "INFO"
    New-EditorFile
}

function Close-AllEditorFiles {
    Write-DevConsole "[File] Closing all files..." "INFO"
    New-EditorFile
}

function Revert-EditorFile {
    Write-DevConsole "[File] Reverting file..." "INFO"

    if ($global:currentFile -and (Test-Path $global:currentFile)) {
        $content = Get-Content -Path $global:currentFile -Raw -ErrorAction SilentlyContinue
        if ($content -and $script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
            $escapedContent = $content -replace '\\', '\\' -replace "`r`n", '\n' -replace "`n", '\n' -replace '"', '\"'
            $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync("if (window.editor) { window.editor.setValue(`"$escapedContent`"); }") | Out-Null
        }
    }
}

# ===============================
# EDITOR COMMAND HANDLERS
# ===============================

function Invoke-EditorCommand {
    param([string]$Command)

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.trigger('keyboard', 'editor.action.$Command');
            }
"@) | Out-Null
    }
}

# ===============================
# EDITOR SETTINGS HANDLERS
# ===============================

function Set-EditorTheme {
    param([string]$Theme)

    Write-DevConsole "[Settings] Setting theme: $Theme" "INFO"

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.monaco) {
                monaco.editor.setTheme('$Theme');
                console.log('✅ Theme set: $Theme');
            }
"@) | Out-Null
    }
}

function Set-EditorFontSize {
    param([int]$Size)

    Write-DevConsole "[Settings] Setting font size: $Size" "INFO"

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.updateOptions({ fontSize: $Size });
                console.log('✅ Font size set: $Size');
            }
"@) | Out-Null
    }
}

function Set-EditorTabSize {
    param([int]$Size)

    Write-DevConsole "[Settings] Setting tab size: $Size" "INFO"

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.updateOptions({ tabSize: $Size });
                console.log('✅ Tab size set: $Size');
            }
"@) | Out-Null
    }
}

function Set-EditorWordWrap {
    param([bool]$Enabled)

    $value = if ($Enabled) { "'on'" } else { "'off'" }

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.updateOptions({ wordWrap: $value });
            }
"@) | Out-Null
    }
}

function Set-EditorLineNumbers {
    param([bool]$Enabled)

    $value = if ($Enabled) { "'on'" } else { "'off'" }

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.updateOptions({ lineNumbers: $value });
            }
"@) | Out-Null
    }
}

function Set-EditorMinimap {
    param([bool]$Enabled)

    $value = if ($Enabled) { "true" } else { "false" }

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.updateOptions({ minimap: { enabled: $value } });
            }
"@) | Out-Null
    }
}

function Set-EditorAutoSave {
    param([bool]$Enabled)

    Write-DevConsole "[Settings] Auto-save: $Enabled" "INFO"
    $global:EditorAutoSave = $Enabled
}

function Set-EditorFormatOnSave {
    param([bool]$Enabled)

    Write-DevConsole "[Settings] Format on save: $Enabled" "INFO"
    $global:EditorFormatOnSave = $Enabled
}

function Set-EditorBracketPairs {
    param([bool]$Enabled)

    $value = if ($Enabled) { "true" } else { "false" }

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            if (window.editor) {
                window.editor.updateOptions({ bracketPairColorization: { enabled: $value } });
            }
"@) | Out-Null
    }
}

# ===============================
# VIEW/PANEL HANDLERS
# ===============================

function Toggle-Sidebar {
    Write-DevConsole "[View] Toggling sidebar..." "INFO"

    if ($script:wpfFileTree) {
        $script:wpfFileTree.Visibility = if ($script:wpfFileTree.Visibility -eq "Visible") { "Collapsed" } else { "Visible" }
    }
}

function Toggle-TerminalPanel {
    Write-DevConsole "[View] Toggling terminal..." "INFO"

    # Find terminal panel in your UI structure
    # This is a placeholder - adjust based on your actual terminal control name
    if ($script:wpfTerminalPanel) {
        $script:wpfTerminalPanel.Visibility = if ($script:wpfTerminalPanel.Visibility -eq "Visible") { "Collapsed" } else { "Visible" }
    }
}

function Toggle-OutputPanel {
    Write-DevConsole "[View] Toggling output panel..." "INFO"

    if ($script:wpfOutputPanel) {
        $script:wpfOutputPanel.Visibility = if ($script:wpfOutputPanel.Visibility -eq "Visible") { "Collapsed" } else { "Visible" }
    }
}

function Toggle-ExplorerPanel {
    Write-DevConsole "[View] Toggling explorer..." "INFO"
    Toggle-Sidebar
}

function Toggle-Fullscreen {
    Write-DevConsole "[View] Toggling fullscreen..." "INFO"

    if ($script:wpfWindow) {
        $script:wpfWindow.WindowState = if ($script:wpfWindow.WindowState -eq "Maximized") { "Normal" } else { "Maximized" }
    }
}

function Adjust-EditorZoom {
    param([string]$Direction)

    if (-not $global:EditorZoomLevel) { $global:EditorZoomLevel = 100 }

    switch ($Direction) {
        "in" { $global:EditorZoomLevel += 10 }
        "out" { $global:EditorZoomLevel -= 10 }
        "reset" { $global:EditorZoomLevel = 100 }
    }

    # Clamp zoom level
    $global:EditorZoomLevel = [Math]::Max(50, [Math]::Min(200, $global:EditorZoomLevel))

    if ($script:wpfWebBrowser -and $script:wpfWebBrowser.CoreWebView2) {
        $zoomFactor = $global:EditorZoomLevel / 100.0
        $script:wpfWebBrowser.CoreWebView2.ExecuteScriptAsync(@"
            document.body.style.zoom = '$zoomFactor';
"@) | Out-Null
    }
}

# ===============================
# FIND & REPLACE DIALOGS
# ===============================

function Show-FindDialog {
    $findForm = New-Object System.Windows.Forms.Form
    $findForm.Text = "Find"
    $findForm.Size = New-Object System.Drawing.Size(450, 150)
    $findForm.StartPosition = "CenterScreen"
    $findForm.FormBorderStyle = "FixedDialog"
    $findForm.MaximizeBox = $false
    $findForm.MinimizeBox = $false
    $findForm.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)

    # Find label
    $findLabel = New-Object System.Windows.Forms.Label
    $findLabel.Text = "Find what:"
    $findLabel.Location = New-Object System.Drawing.Point(10, 20)
    $findLabel.Size = New-Object System.Drawing.Size(70, 20)
    $findLabel.ForeColor = [System.Drawing.Color]::White
    $findForm.Controls.Add($findLabel)

    # Find textbox
    $findTextBox = New-Object System.Windows.Forms.TextBox
    $findTextBox.Location = New-Object System.Drawing.Point(85, 17)
    $findTextBox.Size = New-Object System.Drawing.Size(250, 20)
    $findTextBox.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $findTextBox.ForeColor = [System.Drawing.Color]::White
    $findForm.Controls.Add($findTextBox)

    # Case sensitive checkbox
    $caseSensitiveCheckbox = New-Object System.Windows.Forms.CheckBox
    $caseSensitiveCheckbox.Text = "Match case"
    $caseSensitiveCheckbox.Location = New-Object System.Drawing.Point(85, 45)
    $caseSensitiveCheckbox.Size = New-Object System.Drawing.Size(100, 20)
    $caseSensitiveCheckbox.ForeColor = [System.Drawing.Color]::White
    $findForm.Controls.Add($caseSensitiveCheckbox)

    # Find Next button
    $findNextBtn = New-Object System.Windows.Forms.Button
    $findNextBtn.Text = "Find Next"
    $findNextBtn.Location = New-Object System.Drawing.Point(350, 15)
    $findNextBtn.Size = New-Object System.Drawing.Size(80, 25)
    $findNextBtn.Add_Click({
            if ([string]::IsNullOrEmpty($findTextBox.Text)) { return }

            $searchText = $findTextBox.Text
            $editorText = $script:editor.Text
            $startIndex = $script:editor.SelectionStart + $script:editor.SelectionLength

            $comparison = if ($caseSensitiveCheckbox.Checked) {
                [System.StringComparison]::Ordinal
            }
            else {
                [System.StringComparison]::OrdinalIgnoreCase
            }

            $foundIndex = $editorText.IndexOf($searchText, $startIndex, $comparison)

            if ($foundIndex -eq -1) {
                # Wrap around to beginning
                $foundIndex = $editorText.IndexOf($searchText, 0, $comparison)
            }

            if ($foundIndex -ge 0) {
                $script:editor.Select($foundIndex, $searchText.Length)
                $script:editor.ScrollToCaret()
                $script:editor.Focus()
            }
            else {
                Write-DevConsole "Text not found: '$searchText'" "INFO"
            }
        })
    $findForm.Controls.Add($findNextBtn)

    # Close button
    $closeBtn = New-Object System.Windows.Forms.Button
    $closeBtn.Text = "Close"
    $closeBtn.Location = New-Object System.Drawing.Point(350, 50)
    $closeBtn.Size = New-Object System.Drawing.Size(80, 25)
    $closeBtn.Add_Click({ $findForm.Close() })
    $findForm.Controls.Add($closeBtn)

    $findForm.ShowDialog()
}

function Show-ReplaceDialog {
    $replaceForm = New-Object System.Windows.Forms.Form
    $replaceForm.Text = "Find and Replace"
    $replaceForm.Size = New-Object System.Drawing.Size(450, 200)
    $replaceForm.StartPosition = "CenterScreen"
    $replaceForm.FormBorderStyle = "FixedDialog"
    $replaceForm.MaximizeBox = $false
    $replaceForm.MinimizeBox = $false
    $replaceForm.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)

    # Find label
    $findLabel = New-Object System.Windows.Forms.Label
    $findLabel.Text = "Find what:"
    $findLabel.Location = New-Object System.Drawing.Point(10, 20)
    $findLabel.Size = New-Object System.Drawing.Size(80, 20)
    $findLabel.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($findLabel)

    # Find textbox
    $findTextBox = New-Object System.Windows.Forms.TextBox
    $findTextBox.Location = New-Object System.Drawing.Point(95, 17)
    $findTextBox.Size = New-Object System.Drawing.Size(240, 20)
    $findTextBox.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $findTextBox.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($findTextBox)

    # Replace label
    $replaceLabel = New-Object System.Windows.Forms.Label
    $replaceLabel.Text = "Replace with:"
    $replaceLabel.Location = New-Object System.Drawing.Point(10, 50)
    $replaceLabel.Size = New-Object System.Drawing.Size(80, 20)
    $replaceLabel.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($replaceLabel)

    # Replace textbox
    $replaceTextBox = New-Object System.Windows.Forms.TextBox
    $replaceTextBox.Location = New-Object System.Drawing.Point(95, 47)
    $replaceTextBox.Size = New-Object System.Drawing.Size(240, 20)
    $replaceTextBox.BackColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $replaceTextBox.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($replaceTextBox)

    # Case sensitive checkbox
    $caseSensitiveCheckbox = New-Object System.Windows.Forms.CheckBox
    $caseSensitiveCheckbox.Text = "Match case"
    $caseSensitiveCheckbox.Location = New-Object System.Drawing.Point(95, 75)
    $caseSensitiveCheckbox.Size = New-Object System.Drawing.Size(100, 20)
    $caseSensitiveCheckbox.ForeColor = [System.Drawing.Color]::White
    $replaceForm.Controls.Add($caseSensitiveCheckbox)

    # Status label
    $statusLabel = New-Object System.Windows.Forms.Label
    $statusLabel.Text = ""
    $statusLabel.Location = New-Object System.Drawing.Point(10, 130)
    $statusLabel.Size = New-Object System.Drawing.Size(325, 20)
    $statusLabel.ForeColor = [System.Drawing.Color]::LightGreen
    $replaceForm.Controls.Add($statusLabel)

    # Find Next button
    $findNextBtn = New-Object System.Windows.Forms.Button
    $findNextBtn.Text = "Find Next"
    $findNextBtn.Location = New-Object System.Drawing.Point(350, 15)
    $findNextBtn.Size = New-Object System.Drawing.Size(80, 25)
    $findNextBtn.Add_Click({
            if ([string]::IsNullOrEmpty($findTextBox.Text)) { return }

            $searchText = $findTextBox.Text
            $editorText = $script:editor.Text
            $startIndex = $script:editor.SelectionStart + $script:editor.SelectionLength

            $comparison = if ($caseSensitiveCheckbox.Checked) {
                [System.StringComparison]::Ordinal
            }
            else {
                [System.StringComparison]::OrdinalIgnoreCase
            }

            $foundIndex = $editorText.IndexOf($searchText, $startIndex, $comparison)

            if ($foundIndex -eq -1) {
                $foundIndex = $editorText.IndexOf($searchText, 0, $comparison)
            }

            if ($foundIndex -ge 0) {
                $script:editor.Select($foundIndex, $searchText.Length)
                $script:editor.ScrollToCaret()
                $script:editor.Focus()
                $statusLabel.Text = "Found at position $foundIndex"
                $statusLabel.ForeColor = [System.Drawing.Color]::LightGreen
            }
            else {
                $statusLabel.Text = "Text not found"
                $statusLabel.ForeColor = [System.Drawing.Color]::Orange
            }
        })
    $replaceForm.Controls.Add($findNextBtn)

    # Replace button
    $replaceBtn = New-Object System.Windows.Forms.Button
    $replaceBtn.Text = "Replace"
    $replaceBtn.Location = New-Object System.Drawing.Point(350, 45)
    $replaceBtn.Size = New-Object System.Drawing.Size(80, 25)
    $replaceBtn.Add_Click({
            if ($script:editor.SelectionLength -gt 0) {
                $script:editor.SelectedText = $replaceTextBox.Text
                $statusLabel.Text = "Replaced"
                $statusLabel.ForeColor = [System.Drawing.Color]::LightGreen
            }
        })
    $replaceForm.Controls.Add($replaceBtn)

    # Replace All button
    $replaceAllBtn = New-Object System.Windows.Forms.Button
    $replaceAllBtn.Text = "Replace All"
    $replaceAllBtn.Location = New-Object System.Drawing.Point(350, 75)
    $replaceAllBtn.Size = New-Object System.Drawing.Size(80, 25)
    $replaceAllBtn.Add_Click({
            if ([string]::IsNullOrEmpty($findTextBox.Text)) { return }

            $searchText = $findTextBox.Text
            $replaceWith = $replaceTextBox.Text

            if ($caseSensitiveCheckbox.Checked) {
                $count = ($script:editor.Text | Select-String -Pattern [regex]::Escape($searchText) -AllMatches -CaseSensitive).Matches.Count
                $script:editor.Text = $script:editor.Text.Replace($searchText, $replaceWith)
            }
            else {
                $count = ($script:editor.Text | Select-String -Pattern [regex]::Escape($searchText) -AllMatches).Matches.Count
                $script:editor.Text = $script:editor.Text -ireplace [regex]::Escape($searchText), $replaceWith
            }

            $statusLabel.Text = "Replaced $count occurrence(s)"
            $statusLabel.ForeColor = [System.Drawing.Color]::LightGreen
        })
    $replaceForm.Controls.Add($replaceAllBtn)

    # Close button
    $closeBtn = New-Object System.Windows.Forms.Button
    $closeBtn.Text = "Close"
    $closeBtn.Location = New-Object System.Drawing.Point(350, 105)
    $closeBtn.Size = New-Object System.Drawing.Size(80, 25)
    $closeBtn.Add_Click({ $replaceForm.Close() })
    $replaceForm.Controls.Add($closeBtn)

    $replaceForm.ShowDialog()
}

# ===============================
# CURSOR STYLE HOOKING HELPERS
# ===============================

function Initialize-CursorHooks {
    if ($script:CursorHookState) { return }
    $script:CursorHookState = @{
        ActiveStack  = New-Object "System.Collections.Generic.List[System.Collections.Hashtable]"
        CurrentStyle = "Default"
        LastReason   = "Idle"
        LastUpdated  = Get-Date
    }
}

function Get-CursorHookObject {
    param([string]$Style)
    switch ($Style) {
        "AppStarting" { return [System.Windows.Forms.Cursors]::AppStarting }
        "Cross" { return [System.Windows.Forms.Cursors]::Cross }
        "Hand" { return [System.Windows.Forms.Cursors]::Hand }
        "IBeam" { return [System.Windows.Forms.Cursors]::IBeam }
        "No" { return [System.Windows.Forms.Cursors]::No }
        "Wait" { return [System.Windows.Forms.Cursors]::WaitCursor }
        "Busy" { return [System.Windows.Forms.Cursors]::WaitCursor }
        default { return [System.Windows.Forms.Cursors]::Default }
    }
}

function Update-CursorHookStyle {
    param(
        [string]$Style,
        [string]$Reason = "Operation"
    )
    Initialize-CursorHooks
    if (-not $script:RuntimeInfo.WinFormsAvailable) { return }

    $cursor = Get-CursorHookObject -Style $Style
    $useWait = $Style -in @("Wait", "Busy")

    try { [System.Windows.Forms.Cursor]::Current = $cursor } catch { }
    try { [System.Windows.Forms.Application]::UseWaitCursor = $useWait } catch { }

    try {
        $openForms = [System.Windows.Forms.Application]::OpenForms
        if ($openForms) {
            foreach ($openForm in $openForms) {
                if ($openForm -and -not $openForm.IsDisposed) {
                    $openForm.UseWaitCursor = $useWait
                    $openForm.Cursor = $cursor
                }
            }
        }
    }
    catch {
        # Non-fatal - cursor updates are best effort
    }

    $script:CursorHookState.CurrentStyle = $Style
    $script:CursorHookState.LastReason = $Reason
    $script:CursorHookState.LastUpdated = Get-Date
}

function Enter-CursorWaitState {
    param(
        [string]$Reason = "Operation",
        [ValidateSet("Default", "Wait", "Busy", "AppStarting", "Cross", "Hand", "IBeam", "No")]
        [string]$Style = "Wait"
    )
    Initialize-CursorHooks
    if (-not $script:RuntimeInfo.WinFormsAvailable) { return $null }

    $token = [guid]::NewGuid().ToString()
    $entry = @{
        Token     = $token
        Reason    = $Reason
        Style     = $Style
        Timestamp = Get-Date
    }
    $script:CursorHookState.ActiveStack.Add($entry)
    Update-CursorHookStyle -Style $Style -Reason $Reason
    return $token
}

function Exit-CursorWaitState {
    param([string]$Token)
    Initialize-CursorHooks
    if (-not $Token) { return }

    for ($i = $script:CursorHookState.ActiveStack.Count - 1; $i -ge 0; $i--) {
        $entry = $script:CursorHookState.ActiveStack[$i]
        if ($entry.Token -eq $Token) {
            $script:CursorHookState.ActiveStack.RemoveAt($i)
            break
        }
    }

    if ($script:CursorHookState.ActiveStack.Count -gt 0) {
        $next = $script:CursorHookState.ActiveStack[$script:CursorHookState.ActiveStack.Count - 1]
        Update-CursorHookStyle -Style $next.Style -Reason $next.Reason
    }
    else {
        Update-CursorHookStyle -Style "Default" -Reason "Idle"
    }
}

function Get-CursorHookStatus {
    Initialize-CursorHooks
    return @{
        ActiveRequests = if ($script:CursorHookState.ActiveStack) { $script:CursorHookState.ActiveStack.ToArray() } else { @() }
        CurrentStyle   = $script:CursorHookState.CurrentStyle
        LastReason     = $script:CursorHookState.LastReason
        LastUpdated    = $script:CursorHookState.LastUpdated
    }
}

Initialize-CursorHooks

# ===============================
# ENHANCED FILE BROWSER FUNCTIONS
# ===============================

function Update-Explorer {
    Write-StartupLog "Updating file explorer..." "INFO"
    $explorer.Nodes.Clear()
    $currentPath = $global:currentWorkingDir

    if (-not $currentPath) {
        $currentPath = Get-Location
        $global:currentWorkingDir = $currentPath
    }

    $explorerPathLabel.Text = "Path: $currentPath"
    Write-StartupLog "Current working directory: $currentPath" "INFO"

    try {
        # Add drives with better error handling
        $drives = @(Get-PSDrive -PSProvider FileSystem)
        Write-StartupLog "Found $($drives.Count) drives to load" "INFO"

        foreach ($drive in $drives) {
            try {
                $driveNode = New-Object System.Windows.Forms.TreeNode("� $($drive.Name):\ ($([math]::Round($drive.Used/1GB,1))GB used)")
                $driveNode.Tag = "$($drive.Root)"
                $driveNode.Name = $drive.Root
                $driveNode.ToolTipText = "Drive $($drive.Name): - Free: $([math]::Round($drive.Free/1GB,1))GB"
                $explorer.Nodes.Add($driveNode) | Out-Null

                # Add immediate children with lazy loading (don't load subdirectories)
                try {
                    Add-TreeNodeChildren -parentNode $driveNode -path $driveNode.Tag -showFiles $true -maxItems 500
                    Write-StartupLog "Successfully loaded drive $($drive.Name):" "SUCCESS"
                }
                catch {
                    Write-StartupLog "Warning: Could not fully load drive $($drive.Name): $_" "WARNING"
                    # Add an error indicator but don't fail the whole operation
                    $errorNode = New-Object System.Windows.Forms.TreeNode("⚠ Access restricted - some items may not be visible")
                    $errorNode.ForeColor = [System.Drawing.Color]::Orange
                    $driveNode.Nodes.Add($errorNode) | Out-Null
                }
            }
            catch {
                Write-StartupLog "Error processing drive $($drive.Name): $_" "ERROR"
            }
        }

        # Expand the drive containing current working directory and navigate to it
        if ($global:currentWorkingDir) {
            $currentDrive = [System.IO.Path]::GetPathRoot($global:currentWorkingDir)
            $matchingNode = $explorer.Nodes | Where-Object { $_.Tag -eq $currentDrive }
            if ($matchingNode) {
                $matchingNode.Expand()
                Write-StartupLog "Expanded current drive: $currentDrive" "INFO"

                # Try to expand path to current directory
                try {
                    Expand-PathInTree -treeView $explorer -targetPath $global:currentWorkingDir
                }
                catch {
                    Write-StartupLog "Could not navigate to current directory in tree: $_" "WARNING"
                }
            }
        }

        Write-StartupLog "File explorer update completed successfully" "SUCCESS"
    }
    catch {
        Write-StartupLog "Critical error in Update-Explorer: $_" "ERROR"
        Write-DevConsole "Error in Update-Explorer: $_" "ERROR"

        # Fallback to desktop
        try {
            $desktopPath = [Environment]::GetFolderPath("Desktop")
            $rootNode = New-Object System.Windows.Forms.TreeNode("�️ Desktop (Fallback)")
            $rootNode.Tag = $desktopPath
            $explorer.Nodes.Add($rootNode) | Out-Null
            Add-TreeNodeChildren -parentNode $rootNode -path $desktopPath -showFiles $true -maxItems 100
            $rootNode.Expand()
            Write-StartupLog "Using desktop fallback: $desktopPath" "WARNING"
        }
        catch {
            Write-StartupLog "Even desktop fallback failed: $_" "ERROR"
        }
    }
}

function Add-TreeNodeChildren {
    param(
        [System.Windows.Forms.TreeNode]$parentNode,
        [string]$path,
        [bool]$showFiles = $true,
        [int]$maxItems = 500,
        [int]$maxDepth = 0
    )

    if (-not (Test-Path $path -ErrorAction SilentlyContinue)) {
        Write-StartupLog "Path does not exist: $path" "WARNING"
        return
    }

    try {
        # Only clear nodes if they are dummy nodes or if explicitly refreshing
        $hasDummyNodes = $parentNode.Nodes | Where-Object { $_.Tag -eq "DUMMY" }
        $nodeCount = $parentNode.Nodes.Count
        if ($hasDummyNodes -or $nodeCount -eq 0) {
            $parentNode.Nodes.Clear()
            Write-StartupLog "Cleared existing nodes for: $path" "DEBUG"
        }
        else {
            Write-StartupLog "Nodes already populated for: $path, skipping" "DEBUG"
            return
        }

        # Get directories first, then files with better error handling
        $directories = @()
        $files = @()

        try {
            # Use more robust directory enumeration
            $directories = @(Get-ChildItem -Path $path -Directory -Force -ErrorAction SilentlyContinue |
                Where-Object { -not ($_.Attributes -band [System.IO.FileAttributes]::System) -or $_.Name -notmatch '^(\$|System Volume Information|pagefile\.sys)' } |
                Sort-Object Name |
                Select-Object -First $maxItems)

            Write-StartupLog "Found $($directories.Count) directories in: $path" "DEBUG"
        }
        catch {
            Write-StartupLog "Error reading directories in $path : $_" "WARNING"
        }

        if ($showFiles) {
            try {
                $files = @(Get-ChildItem -Path $path -File -Force -ErrorAction SilentlyContinue |
                    Where-Object { $_.Length -lt 100MB } |  # Skip very large files for performance
                    Sort-Object Name |
                    Select-Object -First $maxItems)

                Write-StartupLog "Found $($files.Count) files in: $path" "DEBUG"
            }
            catch {
                Write-StartupLog "Error reading files in $path : $_" "WARNING"
            }
        }

        # Add directories with improved icons and tooltips
        foreach ($dir in $directories) {
            try {
                $isHidden = $dir.Attributes -band [System.IO.FileAttributes]::Hidden
                $isSystem = $dir.Attributes -band [System.IO.FileAttributes]::System

                $dirIcon = if ($isHidden) { "👁️‍🗨️" } elseif ($isSystem) { "⚙️" } else { "📁" }
                $dirName = if ($isHidden) { "$($dir.Name) (Hidden)" } else { $dir.Name }

                $dirNode = New-Object System.Windows.Forms.TreeNode("$dirIcon $dirName")
                $dirNode.Tag = $dir.FullName
                $dirNode.Name = $dir.FullName
                $dirNode.ToolTipText = "Folder: $($dir.FullName)`nCreated: $($dir.CreationTime)`nAttributes: $($dir.Attributes)"

                # Dim hidden/system folders
                if ($isHidden) {
                    $dirNode.ForeColor = [System.Drawing.Color]::Gray
                }

                # Add dummy node to show expand arrow for lazy loading
                # Check if directory has subdirectories to decide whether to show expand arrow
                try {
                    $hasSubdirs = Get-ChildItem -Path $dir.FullName -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
                    if ($hasSubdirs) {
                        $dummy = New-Object System.Windows.Forms.TreeNode("Loading...")
                        $dummy.Tag = "DUMMY"
                        $dummy.ForeColor = [System.Drawing.Color]::Gray
                        $dirNode.Nodes.Add($dummy) | Out-Null
                    }
                }
                catch {
                    # If we can't check subdirectories, assume there might be some and add dummy
                    $dummy = New-Object System.Windows.Forms.TreeNode("Loading...")
                    $dummy.Tag = "DUMMY"
                    $dummy.ForeColor = [System.Drawing.Color]::Gray
                    $dirNode.Nodes.Add($dummy) | Out-Null
                }

                $parentNode.Nodes.Add($dirNode) | Out-Null
            }
            catch {
                Write-StartupLog "Error processing directory $($dir.Name): $_" "WARNING"
            }
        }

        # Add files with better categorization and icons
        foreach ($file in $files) {
            try {
                $fileIcon = Get-FileIcon $file.Extension
                $isHidden = $file.Attributes -band [System.IO.FileAttributes]::Hidden
                $fileName = if ($isHidden) { "$($file.Name) (Hidden)" } else { $file.Name }

                $fileNode = New-Object System.Windows.Forms.TreeNode("$fileIcon $fileName")
                $fileNode.Tag = $file.FullName
                $fileNode.Name = $file.FullName

                $fileSizeStr = if ($file.Length -gt 1MB) {
                    "$([math]::Round($file.Length / 1MB, 2)) MB"
                }
                elseif ($file.Length -gt 1KB) {
                    "$([math]::Round($file.Length / 1KB, 2)) KB"
                }
                else {
                    "$($file.Length) bytes"
                }

                $fileNode.ToolTipText = "File: $($file.FullName)`nSize: $fileSizeStr`nModified: $($file.LastWriteTime)`nAttributes: $($file.Attributes)"

                # Enhanced color coding for file types
                if ($file.Extension -match '\.(ps1|bat|cmd|sh)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Blue
                }
                elseif ($file.Extension -match '\.(txt|md|log|readme)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Green
                }
                elseif ($file.Extension -match '\.(json|xml|yml|yaml|config|ini)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Purple
                }
                elseif ($file.Extension -match '\.(js|html|css|py|cs|cpp|c|h|java|php|rb)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::DarkOrange
                }
                elseif ($file.Extension -match '\.(exe|dll|msi|app)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Red
                }
                elseif ($file.Extension -match '\.(jpg|jpeg|png|gif|bmp|ico|svg)$') {
                    $fileNode.ForeColor = [System.Drawing.Color]::Magenta
                }
                elseif ($isHidden) {
                    $fileNode.ForeColor = [System.Drawing.Color]::Gray
                }

                $parentNode.Nodes.Add($fileNode) | Out-Null
            }
            catch {
                Write-StartupLog "Error processing file $($file.Name): $_" "WARNING"
            }
        }

        # Add summary if items were truncated
        $totalDirs = (Get-ChildItem -Path $path -Directory -Force -ErrorAction SilentlyContinue | Measure-Object).Count
        $totalFiles = if ($showFiles) { (Get-ChildItem -Path $path -File -Force -ErrorAction SilentlyContinue | Measure-Object).Count } else { 0 }

        $dirCount = @($directories).Count
        $fileCount = @($files).Count
        if (($dirCount + $fileCount) -lt ($totalDirs + $totalFiles)) {
            $truncatedNode = New-Object System.Windows.Forms.TreeNode("⋯ ... and $($totalDirs + $totalFiles - $dirCount - $fileCount) more items")
            $truncatedNode.ForeColor = [System.Drawing.Color]::DarkGray
            $parentNode.Nodes.Add($truncatedNode) | Out-Null
        }

        Write-StartupLog "Loaded $dirCount folders and $fileCount files for: $path" "DEBUG"

    }
    catch {
        Write-StartupLog "Critical error loading children for $path : $_" "ERROR"
        Write-DevConsole "Error loading children for $path : $_" "WARNING"
        $errorNode = New-Object System.Windows.Forms.TreeNode("⚠ Error loading contents: $($_.Exception.Message)")
        $errorNode.ForeColor = [System.Drawing.Color]::Red
        $errorNode.ToolTipText = "Error details: $_"
        $parentNode.Nodes.Add($errorNode) | Out-Null
    }
}

# Helper function to expand a specific path in the tree view
function Expand-PathInTree {
    param(
        [System.Windows.Forms.TreeView]$treeView,
        [string]$targetPath
    )

    try {
        $pathParts = $targetPath.Split([IO.Path]::DirectorySeparatorChar, [StringSplitOptions]::RemoveEmptyEntries)
        $currentPath = ""
        $currentNodes = $treeView.Nodes

        foreach ($part in $pathParts) {
            $currentPath = Join-Path $currentPath $part
            $matchingNode = $currentNodes | Where-Object { $_.Tag -eq $currentPath -or $_.Tag -eq "$currentPath\" }

            if ($matchingNode) {
                $matchingNode.Expand()
                $currentNodes = $matchingNode.Nodes
            }
            else {
                break
            }
        }
    }
    catch {
        Write-StartupLog "Error expanding path in tree: $_" "WARNING"
    }
}

function Get-FileIcon {
    param([string]$extension)

    switch -Regex ($extension.ToLower()) {
        '\.(txt|md|log)$' { return "📄" }
        '\.(ps1|bat|cmd|sh)$' { return "⚡" }
        '\.(json|xml|yml|yaml)$' { return "⚙️" }
        '\.(js|html|css)$' { return "🌐" }
        '\.(py|cs|cpp|c|h|java)$' { return "💻" }
        '\.(jpg|jpeg|png|gif|bmp)$' { return "🖼️" }
        '\.(mp3|wav|flac|mp4|avi)$' { return "🎵" }
        '\.(zip|rar|7z|gz)$' { return "📦" }
        '\.(pdf|doc|docx|xls|xlsx)$' { return "📋" }
        '\.(exe|msi|dll)$' { return "⚙️" }
        default { return "📄" }
    }
}

function Expand-ExplorerNode {
    param($node)

    # Don't expand dummy nodes or nodes without tags
    if (-not $node.Tag -or $node.Tag -eq "DUMMY") {
        Write-DevConsole "Skipping expansion of dummy or invalid node" "DEBUG"
        return
    }

    # Check if this node has dummy children (needs population)
    $hasDummyChild = $node.Nodes | Where-Object { $_.Tag -eq "DUMMY" }

    if ($hasDummyChild) {
        # This directory node needs to be populated - remove dummy and add real content
        Write-DevConsole "Expanding directory with dummy child: $($node.Tag)" "DEBUG"

        if (Test-Path $node.Tag -PathType Container) {
            try {
                # Clear all existing nodes (including dummy)
                $node.Nodes.Clear()

                # Populate with actual directories and files
                Add-TreeNodeChildren -parentNode $node -path $node.Tag -showFiles $true -maxItems 500

                Write-DevConsole "Successfully populated node: $($node.Tag) with $($node.Nodes.Count) items" "DEBUG"
            }
            catch {
                Write-DevConsole "Error expanding node $($node.Tag): $_" "ERROR"

                # Re-add dummy node if expansion failed
                $dummy = New-Object System.Windows.Forms.TreeNode("Error loading...")
                $dummy.Tag = "DUMMY"
                $dummy.ForeColor = [System.Drawing.Color]::Red
                $node.Nodes.Add($dummy) | Out-Null
            }
        }
        else {
            Write-DevConsole "Path no longer exists: $($node.Tag)" "WARNING"
            $node.Nodes.Clear()
        }
    }
    else {
        Write-DevConsole "Node already expanded or has no dummy children: $($node.Tag)" "DEBUG"
    }
}

# ===============================
# CODE EDITING TOOLS
# ===============================

function Apply-EditorSettings {
    param(
        [hashtable]$Settings
    )

    Write-DevConsole "[Settings] Applying editor settings..." "INFO"

    foreach ($setting in $Settings.GetEnumerator()) {
        switch ($setting.Key) {
            "Theme" { Set-EditorTheme $setting.Value }
            "FontSize" { Set-EditorFontSize $setting.Value }
            "TabSize" { Set-EditorTabSize $setting.Value }
            "WordWrap" { Set-EditorWordWrap $setting.Value }
            "LineNumbers" { Set-EditorLineNumbers $setting.Value }
            "Minimap" { Set-EditorMinimap $setting.Value }
            "AutoSave" { Set-EditorAutoSave $setting.Value }
            "FormatOnSave" { Set-EditorFormatOnSave $setting.Value }
            "BracketPairs" { Set-EditorBracketPairs $setting.Value }
        }
    }

function Apply-EditorSettings {
    param(
        [hashtable]$Settings
    )

    Write-DevConsole "[Settings] Applying editor settings..." "INFO"

    foreach ($setting in $Settings.GetEnumerator()) {
        switch ($setting.Key) {
            "Theme" { Set-EditorTheme $setting.Value }
            "FontSize" { Set-EditorFontSize $setting.Value }
            "TabSize" { Set-EditorTabSize $setting.Value }
            "WordWrap" { Set-EditorWordWrap $setting.Value }
            "LineNumbers" { Set-EditorLineNumbers $setting.Value }
            "Minimap" { Set-EditorMinimap $setting.Value }
            "AutoSave" { Set-EditorAutoSave $setting.Value }
            "FormatOnSave" { Set-EditorFormatOnSave $setting.Value }
            "BracketPairs" { Set-EditorBracketPairs $setting.Value }
        }
    }

    Write-DevConsole "[Settings] Editor settings applied" "SUCCESS"
}
}

function Show-EditorPopOut {
    if (-not $script:editor) {
        Write-DevConsole "Editor control not initialized; cannot pop out" "WARNING"
        return
    }

    if ($script:editorPopOutForm -and -not $script:editorPopOutForm.IsDisposed) {
        $script:editorPopOutForm.BringToFront()
        $script:editorPopOutForm.Activate()
        return
    }

    $editorForm = New-Object System.Windows.Forms.Form
    $editorForm.Text = "Editor Pop-Out"
    $editorForm.Size = New-Object System.Drawing.Size(900, 700)
    $editorForm.StartPosition = "CenterParent"

    $editorBox = New-Object System.Windows.Forms.RichTextBox
    $editorBox.Dock = [System.Windows.Forms.DockStyle]::Fill
    $editorBox.Font = $script:editor.Font
    $editorBox.WordWrap = $script:editor.WordWrap
    $editorBox.Multiline = $true

    $editorForm.Controls.Add($editorBox) | Out-Null

    $editorBox.Text = $script:editor.Text
    $editorBox.SelectionStart = $editorBox.TextLength
    $editorBox.ScrollToCaret()

    $syncFromMain = {
        if ($script:editorPopOutTextBox -and -not $script:editorPopOutTextBox.IsDisposed -and $script:editorPopOutFromPopHandler) {
            try {
                $script:editorPopOutTextBox.remove_TextChanged($script:editorPopOutFromPopHandler)
            } catch { }
            $script:editorPopOutTextBox.Text = $script:editor.Text
            $script:editorPopOutTextBox.SelectionStart = $script:editorPopOutTextBox.TextLength
            $script:editorPopOutTextBox.ScrollToCaret()
            try {
                $script:editorPopOutTextBox.add_TextChanged($script:editorPopOutFromPopHandler)
            } catch { }
        }
    }

    $syncFromPop = {
        if ($script:editor -and -not $script:editor.IsDisposed -and $script:editorPopOutFromMainHandler) {
            try {
                $script:editor.remove_TextChanged($script:editorPopOutFromMainHandler)
            } catch { }
            $script:editor.Text = $script:editorPopOutTextBox.Text
            $script:editor.SelectionStart = $script:editor.TextLength
            $script:editor.ScrollToCaret()
            try {
                $script:editor.add_TextChanged($script:editorPopOutFromMainHandler)
            } catch { }
        }
    }

    $script:editorPopOutTextBox = $editorBox
    $script:editorPopOutFromMainHandler = [System.EventHandler]$syncFromMain
    $script:editorPopOutFromPopHandler = [System.EventHandler]$syncFromPop

    $script:editor.Add_TextChanged($script:editorPopOutFromMainHandler)
    $editorBox.Add_TextChanged($script:editorPopOutFromPopHandler)

    $editorForm.Add_FormClosed({
            if ($script:editor -and -not $script:editor.IsDisposed -and $script:editorPopOutFromMainHandler) {
                try { $script:editor.remove_TextChanged($script:editorPopOutFromMainHandler) } catch { }
            }
            if ($script:editorPopOutTextBox -and $script:editorPopOutFromPopHandler) {
                try { $script:editorPopOutTextBox.remove_TextChanged($script:editorPopOutFromPopHandler) } catch { }
            }
            $script:editorPopOutForm = $null
            $script:editorPopOutTextBox = $null
            $script:editorPopOutFromMainHandler = $null
            $script:editorPopOutFromPopHandler = $null
        })

    $script:editorPopOutForm = $editorForm
    $editorForm.Show($form)
}