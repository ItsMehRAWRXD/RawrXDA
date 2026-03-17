# RawrXD-UI-ChatPanel.psm1 - AI chat interface component

function Initialize-ChatPanel {
    try {
        Write-RawrXDLog "Initializing AI chat panel..." -Level INFO -Component "ChatPanel"
        
        $chatTab = $global:RawrXD.Components.ChatTab
        if (-not $chatTab) {
            throw "Chat tab not found"
        }
        
        # Create main chat container
        $chatContainer = New-Object System.Windows.Forms.Panel
        $chatContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
        
        # Create chat layout - splitter for chat history and input
        $chatSplitter = New-Object System.Windows.Forms.SplitContainer
        $chatSplitter.Dock = [System.Windows.Forms.DockStyle]::Fill
        $chatSplitter.Orientation = [System.Windows.Forms.Orientation]::Horizontal
        $chatSplitter.SplitterDistance = 400
        $chatSplitter.Panel2MinSize = 100
        
        # Create chat toolbar
        $chatToolbar = Create-ChatToolbar
        $chatContainer.Controls.Add($chatToolbar)
        
        # Create chat history panel (top)
        $chatHistory = Create-ChatHistory
        $chatSplitter.Panel1.Controls.Add($chatHistory)
        
        # Create input panel (bottom)
        $inputPanel = Create-ChatInputPanel
        $chatSplitter.Panel2.Controls.Add($inputPanel)
        
        $chatContainer.Controls.Add($chatSplitter)
        $chatTab.Controls.Add($chatContainer)
        
        # Apply theme
        Apply-ChatTheme -Container $chatContainer
        
        # Store references
        $global:RawrXD.Components.ChatContainer = $chatContainer
        $global:RawrXD.Components.ChatSplitter = $chatSplitter
        $global:RawrXD.Components.ChatToolbar = $chatToolbar
        
        # Initialize chat state
        $global:RawrXD.Chat = @{
            Messages = New-Object System.Collections.ArrayList
            CurrentModel = $global:RawrXD.Settings.AI.DefaultModel
            IsProcessing = $false
            SystemPrompt = "You are a helpful AI assistant integrated into a PowerShell-based text editor. Help the user with coding tasks, code analysis, and general questions."
        }
        
        # Add welcome message
        Add-ChatMessage -Sender "System" -Message "Welcome to RawrXD AI Chat! I'm here to help you with coding tasks and questions. Type your message below and press Enter or click Send." -IsSystem $true
        
        Write-RawrXDLog "AI chat panel initialized successfully" -Level SUCCESS -Component "ChatPanel"
    }
    catch {
        Write-RawrXDLog "Failed to initialize chat panel: $($_.Exception.Message)" -Level ERROR -Component "ChatPanel"
        throw
    }
}

function Create-ChatToolbar {
    $toolbar = New-Object System.Windows.Forms.ToolStrip
    $toolbar.Dock = [System.Windows.Forms.DockStyle]::Top
    $toolbar.Font = New-Object System.Drawing.Font("Segoe UI", 8)
    
    # Model selection
    $modelLabel = New-Object System.Windows.Forms.ToolStripLabel
    $modelLabel.Text = "Model:"
    
    $modelCombo = New-Object System.Windows.Forms.ToolStripComboBox
    $modelCombo.Size = New-Object System.Drawing.Size(120, 25)
    $modelCombo.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
    
    # Populate with available models
    if ($global:RawrXD.AvailableModels) {
        foreach ($model in $global:RawrXD.AvailableModels) {
            $modelCombo.Items.Add($model.name) | Out-Null
        }
    }
    else {
        $modelCombo.Items.AddRange(@("llama3.2", "llama2", "codellama", "mistral"))
    }
    
    # Set default model
    $defaultModel = $global:RawrXD.Settings.AI.DefaultModel
    if ($modelCombo.Items -contains $defaultModel) {
        $modelCombo.SelectedItem = $defaultModel
    }
    elseif ($modelCombo.Items.Count -gt 0) {
        $modelCombo.SelectedIndex = 0
    }
    
    $modelCombo.add_SelectedIndexChanged({
        $global:RawrXD.Chat.CurrentModel = $modelCombo.SelectedItem
        Write-RawrXDLog "Model changed to: $($modelCombo.SelectedItem)" -Level INFO -Component "ChatPanel"
    })
    
    # Separator
    $separator1 = New-Object System.Windows.Forms.ToolStripSeparator
    
    # Clear chat button
    $clearBtn = New-Object System.Windows.Forms.ToolStripButton
    $clearBtn.Text = "Clear"
    $clearBtn.ToolTipText = "Clear chat history"
    $clearBtn.add_Click({ Clear-ChatHistory })
    
    # Export chat button
    $exportBtn = New-Object System.Windows.Forms.ToolStripButton
    $exportBtn.Text = "Export"
    $exportBtn.ToolTipText = "Export chat to file"
    $exportBtn.add_Click({ Export-ChatHistory })
    
    # Separator
    $separator2 = New-Object System.Windows.Forms.ToolStripSeparator
    
    # Connection status
    $statusLabel = New-Object System.Windows.Forms.ToolStripLabel
    $statusLabel.Text = if ($global:RawrXD.OllamaAvailable) { "✅ Connected" } else { "❌ Disconnected" }
    $statusLabel.ForeColor = if ($global:RawrXD.OllamaAvailable) { [System.Drawing.Color]::Green } else { [System.Drawing.Color]::Red }
    
    # Test connection button
    $testBtn = New-Object System.Windows.Forms.ToolStripButton
    $testBtn.Text = "Test"
    $testBtn.ToolTipText = "Test AI connection"
    $testBtn.add_Click({ Test-ChatConnection })
    
    $toolbar.Items.AddRange(@($modelLabel, $modelCombo, $separator1, $clearBtn, $exportBtn, $separator2, $statusLabel, $testBtn))
    
    # Store toolbar references
    $global:RawrXD.Components.ChatButtons = @{
        ModelCombo = $modelCombo
        Clear = $clearBtn
        Export = $exportBtn
        Status = $statusLabel
        Test = $testBtn
    }
    
    return $toolbar
}

function Create-ChatHistory {
    # Use RichTextBox for better formatting support
    $chatHistory = New-Object System.Windows.Forms.RichTextBox
    $chatHistory.Dock = [System.Windows.Forms.DockStyle]::Fill
    $chatHistory.ReadOnly = $true
    $chatHistory.Font = New-Object System.Drawing.Font($global:RawrXD.Settings.UI.FontFamily, 9)
    $chatHistory.ScrollBars = [System.Windows.Forms.RichTextBoxScrollBars]::Vertical
    $chatHistory.DetectUrls = $true
    $chatHistory.WordWrap = $true
    
    # Store reference
    $global:RawrXD.Components.ChatHistory = $chatHistory
    
    return $chatHistory
}

function Create-ChatInputPanel {
    $inputPanel = New-Object System.Windows.Forms.Panel
    $inputPanel.Dock = [System.Windows.Forms.DockStyle]::Fill
    $inputPanel.Height = 100
    
    # Create input text box
    $inputText = New-Object System.Windows.Forms.TextBox
    $inputText.Multiline = $true
    $inputText.Font = New-Object System.Drawing.Font($global:RawrXD.Settings.UI.FontFamily, 9)
    $inputText.ScrollBars = [System.Windows.Forms.ScrollBars]::Vertical
    $inputText.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Left -bor [System.Windows.Forms.AnchorStyles]::Right -bor [System.Windows.Forms.AnchorStyles]::Bottom
    $inputText.Location = New-Object System.Drawing.Point(5, 5)
    $inputText.Size = New-Object System.Drawing.Size(($inputPanel.Width - 90), ($inputPanel.Height - 10))
    
    # Handle Enter key for sending messages
    $inputText.add_KeyDown({
        param($sender, $e)
        if ($e.KeyCode -eq "Enter" -and $e.Control) {
            $e.Handled = $true
            Send-ChatMessage -Message $sender.Text
        }
    })
    
    # Create send button
    $sendBtn = New-Object System.Windows.Forms.Button
    $sendBtn.Text = "Send"
    $sendBtn.Size = New-Object System.Drawing.Size(75, 30)
    $sendBtn.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Right
    $sendBtn.Location = New-Object System.Drawing.Point(($inputPanel.Width - 80), 5)
    $sendBtn.add_Click({ 
        Send-ChatMessage -Message $global:RawrXD.Components.ChatInput.Text
    })
    
    # Create context button for code analysis
    $contextBtn = New-Object System.Windows.Forms.Button
    $contextBtn.Text = "Add Code"
    $contextBtn.Size = New-Object System.Drawing.Size(75, 25)
    $contextBtn.Anchor = [System.Windows.Forms.AnchorStyles]::Bottom -bor [System.Windows.Forms.AnchorStyles]::Right
    $contextBtn.Location = New-Object System.Drawing.Point(($inputPanel.Width - 80), ($inputPanel.Height - 30))
    $contextBtn.add_Click({ Add-CodeToInput })
    
    $inputPanel.Controls.AddRange(@($inputText, $sendBtn, $contextBtn))
    
    # Handle panel resize
    $inputPanel.add_Resize({
        $inputText.Size = New-Object System.Drawing.Size(($inputPanel.Width - 90), ($inputPanel.Height - 10))
        $sendBtn.Location = New-Object System.Drawing.Point(($inputPanel.Width - 80), 5)
        $contextBtn.Location = New-Object System.Drawing.Point(($inputPanel.Width - 80), ($inputPanel.Height - 30))
    })
    
    # Store references
    $global:RawrXD.Components.ChatInput = $inputText
    $global:RawrXD.Components.ChatSendButton = $sendBtn
    $global:RawrXD.Components.ChatContextButton = $contextBtn
    
    return $inputPanel
}

function Apply-ChatTheme {
    param($Container)
    
    $theme = $global:RawrXD.Settings.UI.Theme
    
    if ($theme -eq "Dark") {
        $Container.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 48)
        $global:RawrXD.Components.ChatHistory.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
        $global:RawrXD.Components.ChatHistory.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
        $global:RawrXD.Components.ChatInput.BackColor = [System.Drawing.Color]::FromArgb(37, 37, 38)
        $global:RawrXD.Components.ChatInput.ForeColor = [System.Drawing.Color]::FromArgb(220, 220, 220)
    }
    else {
        $Container.BackColor = [System.Drawing.SystemColors]::Control
        $global:RawrXD.Components.ChatHistory.BackColor = [System.Drawing.Color]::White
        $global:RawrXD.Components.ChatHistory.ForeColor = [System.Drawing.Color]::Black
        $global:RawrXD.Components.ChatInput.BackColor = [System.Drawing.Color]::White
        $global:RawrXD.Components.ChatInput.ForeColor = [System.Drawing.Color]::Black
    }
}

function Add-ChatMessage {
    param(
        [string]$Sender,
        [string]$Message,
        [bool]$IsSystem = $false,
        [bool]$IsError = $false
    )
    
    $chatHistory = $global:RawrXD.Components.ChatHistory
    if (-not $chatHistory) { return }
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    $prefix = if ($IsSystem) { "🤖 System" } elseif ($Sender -eq "User") { "👤 You" } else { "🧠 AI" }
    
    # Add message to chat history
    $chatHistory.SelectionStart = $chatHistory.TextLength
    $chatHistory.SelectionLength = 0
    
    # Set color for different message types
    if ($IsError) {
        $chatHistory.SelectionColor = [System.Drawing.Color]::Red
    }
    elseif ($IsSystem) {
        $chatHistory.SelectionColor = [System.Drawing.Color]::Gray
    }
    elseif ($Sender -eq "User") {
        $chatHistory.SelectionColor = [System.Drawing.Color]::Blue
    }
    else {
        $chatHistory.SelectionColor = $chatHistory.ForeColor
    }
    
    $chatHistory.AppendText("[$timestamp] $prefix`n")
    
    # Reset color for message content
    $chatHistory.SelectionColor = $chatHistory.ForeColor
    $chatHistory.AppendText("$Message`n`n")
    
    # Scroll to bottom
    $chatHistory.SelectionStart = $chatHistory.TextLength
    $chatHistory.ScrollToCaret()
    
    # Store in message history
    $global:RawrXD.Chat.Messages.Add(@{
        Timestamp = Get-Date
        Sender = $Sender
        Message = $Message
        IsSystem = $IsSystem
        IsError = $IsError
    }) | Out-Null
}

function Send-ChatMessage {
    param(
        [string]$Message,
        [bool]$IsSystemGenerated = $false
    )
    
    if ([string]::IsNullOrWhiteSpace($Message)) {
        return
    }
    
    $inputText = $global:RawrXD.Components.ChatInput
    $sendButton = $global:RawrXD.Components.ChatSendButton
    
    # Disable input while processing
    if ($inputText) { $inputText.Enabled = $false }
    if ($sendButton) { $sendButton.Text = "Sending..." }
    $global:RawrXD.Chat.IsProcessing = $true
    
    try {
        # Add user message to history
        if (-not $IsSystemGenerated) {
            Add-ChatMessage -Sender "User" -Message $Message
            $inputText.Clear()
        }
        else {
            Add-ChatMessage -Sender "System" -Message "Analyzing: $($Message.Substring(0, [Math]::Min(50, $Message.Length)))..." -IsSystem $true
        }
        
        # Check AI availability
        if (-not $global:RawrXD.OllamaAvailable) {
            Add-ChatMessage -Sender "System" -Message "AI service is not available. Please check the Ollama connection." -IsError $true
            return
        }
        
        # Get AI response
        $model = $global:RawrXD.Chat.CurrentModel
        $systemPrompt = $global:RawrXD.Chat.SystemPrompt
        
        # Add context about current file if available
        if ($global:RawrXD.CurrentFile) {
            $fileName = [System.IO.Path]::GetFileName($global:RawrXD.CurrentFile)
            $language = Get-FileExtensionLanguage -FilePath $global:RawrXD.CurrentFile
            $systemPrompt += "`n`nContext: User is currently working on a $language file named '$fileName'."
        }
        
        Write-RawrXDLog "Sending message to AI model: $model" -Level INFO -Component "ChatPanel"
        
        # Create a background job for AI response to prevent UI freezing
        $response = Invoke-OllamaChat -Model $model -Prompt $Message -System $systemPrompt
        
        # Add AI response
        Add-ChatMessage -Sender "AI" -Message $response
        
        Write-RawrXDLog "AI response received successfully" -Level SUCCESS -Component "ChatPanel"
    }
    catch {
        $errorMsg = "Error communicating with AI: $($_.Exception.Message)"
        Add-ChatMessage -Sender "System" -Message $errorMsg -IsError $true
        Write-RawrXDLog $errorMsg -Level ERROR -Component "ChatPanel"
    }
    finally {
        # Re-enable input
        if ($inputText) { $inputText.Enabled = $true; $inputText.Focus() }
        if ($sendButton) { $sendButton.Text = "Send" }
        $global:RawrXD.Chat.IsProcessing = $false
    }
}

function Add-CodeToInput {
    $textEditor = $global:RawrXD.Components.TextEditor
    $chatInput = $global:RawrXD.Components.ChatInput
    
    if ($textEditor -and $chatInput) {
        $selectedText = $textEditor.SelectedText
        $codeToAdd = if ($selectedText) { $selectedText } else { $textEditor.Text }
        
        if ($codeToAdd) {
            $language = if ($global:RawrXD.CurrentFile) { 
                Get-FileExtensionLanguage -FilePath $global:RawrXD.CurrentFile 
            } else { 
                "Text" 
            }
            
            $codeBlock = "``````$language`n$codeToAdd`n``````"
            
            if (-not [string]::IsNullOrEmpty($chatInput.Text)) {
                $chatInput.Text += "`n`n"
            }
            $chatInput.Text += "Please analyze this code:`n$codeBlock"
            $chatInput.Focus()
            $chatInput.SelectionStart = $chatInput.TextLength
        }
    }
}

function Clear-ChatHistory {
    $result = [System.Windows.Forms.MessageBox]::Show("Are you sure you want to clear the chat history?", "Clear Chat", [System.Windows.Forms.MessageBoxButtons]::YesNo, [System.Windows.Forms.MessageBoxIcon]::Question)
    
    if ($result -eq "Yes") {
        $global:RawrXD.Components.ChatHistory.Clear()
        $global:RawrXD.Chat.Messages.Clear()
        Add-ChatMessage -Sender "System" -Message "Chat history cleared." -IsSystem $true
        Write-RawrXDLog "Chat history cleared" -Level INFO -Component "ChatPanel"
    }
}

function Export-ChatHistory {
    $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
    $saveDialog.Title = "Export Chat History"
    $saveDialog.Filter = "Text Files (*.txt)|*.txt|Markdown Files (*.md)|*.md"
    $saveDialog.FileName = "RawrXD-Chat-$(Get-Date -Format 'yyyy-MM-dd-HHmm').txt"
    
    if ($saveDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        try {
            $content = $global:RawrXD.Components.ChatHistory.Text
            [System.IO.File]::WriteAllText($saveDialog.FileName, $content, [System.Text.Encoding]::UTF8)
            Add-ChatMessage -Sender "System" -Message "Chat history exported to: $($saveDialog.FileName)" -IsSystem $true
            Write-RawrXDLog "Chat history exported: $($saveDialog.FileName)" -Level SUCCESS -Component "ChatPanel"
        }
        catch {
            Add-ChatMessage -Sender "System" -Message "Failed to export chat history: $($_.Exception.Message)" -IsError $true
        }
    }
}

function Test-ChatConnection {
    $statusLabel = $global:RawrXD.Components.ChatButtons.Status
    
    try {
        $connected = Test-OllamaConnection
        $global:RawrXD.OllamaAvailable = $connected
        
        if ($connected) {
            $statusLabel.Text = "✅ Connected"
            $statusLabel.ForeColor = [System.Drawing.Color]::Green
            Add-ChatMessage -Sender "System" -Message "AI connection test successful!" -IsSystem $true
            
            # Refresh model list
            $models = Get-OllamaModels
            if ($models.Count -gt 0) {
                $modelCombo = $global:RawrXD.Components.ChatButtons.ModelCombo
                $currentSelection = $modelCombo.SelectedItem
                $modelCombo.Items.Clear()
                foreach ($model in $models) {
                    $modelCombo.Items.Add($model.name) | Out-Null
                }
                if ($modelCombo.Items -contains $currentSelection) {
                    $modelCombo.SelectedItem = $currentSelection
                }
                elseif ($modelCombo.Items.Count -gt 0) {
                    $modelCombo.SelectedIndex = 0
                }
            }
        }
        else {
            $statusLabel.Text = "❌ Disconnected"
            $statusLabel.ForeColor = [System.Drawing.Color]::Red
            Add-ChatMessage -Sender "System" -Message "AI connection test failed. Please check if Ollama is running." -IsError $true
        }
    }
    catch {
        $statusLabel.Text = "❌ Error"
        $statusLabel.ForeColor = [System.Drawing.Color]::Red
        Add-ChatMessage -Sender "System" -Message "Connection test error: $($_.Exception.Message)" -IsError $true
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-ChatPanel',
    'Add-ChatMessage',
    'Send-ChatMessage',
    'Add-CodeToInput',
    'Clear-ChatHistory',
    'Export-ChatHistory',
    'Test-ChatConnection'
)