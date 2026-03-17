# ============================================
# RawrXD Chat Module
# ============================================
# This module contains all chat-related functionality including:
# - Chat tab management (New-ChatTab, Remove-ChatTab, Get-ActiveChatTab)
# - Agentic AI functions (Process-AgentCommand, Write-AgenticErrorLog)
# - Ollama server management
# ============================================

# ============================================
# Model Management Functions
# ============================================

function Get-AvailableModels {
    <#
    .SYNOPSIS
        Dynamically retrieves available Ollama models, with fallback to defaults
    #>
    param()

    # Default fallback models
    $defaultModels = @("bigdaddyg-fast:latest", "llama3.2", "llama3.2:1b", "llama3.1", "codellama", "mistral", "qwen2.5-coder")

    try {
        # Check if ollama command is available
        if (Get-Command ollama -ErrorAction SilentlyContinue) {
            Write-DevConsole "🔍 Querying Ollama for available models..." "INFO"

            # Use job with timeout to prevent hanging
            $job = Start-Job -ScriptBlock {
                try {
                    $result = ollama list 2>$null
                    if ($result) {
                        # Parse the output - skip header line and extract model names
                        $models = $result | Select-Object -Skip 1 | ForEach-Object {
                            if ($_ -match '^\s*(\S+)') {
                                $matches[1]
                            }
                        } | Where-Object { $_ } # Filter out empty entries

                        return $models
                    }
                }
                catch {
                    return $null
                }
            } -ErrorAction SilentlyContinue

            if ($job) {
                # Wait up to 3 seconds for the job to complete
                $result = Wait-Job -Job $job -Timeout 3 -ErrorAction SilentlyContinue
                if ($result -and $result.State -eq 'Completed') {
                    $ollamaModels = Receive-Job -Job $job -ErrorAction SilentlyContinue
                    if ($ollamaModels -and $ollamaModels.Count -gt 0) {
                        Write-DevConsole "✅ Loaded $($ollamaModels.Count) models from Ollama" "SUCCESS"
                        Stop-Job -Job $job -ErrorAction SilentlyContinue
                        Remove-Job -Job $job -ErrorAction SilentlyContinue
                        return $ollamaModels
                    }
                }

                # Clean up job if still running
                if ($job.State -eq 'Running') {
                    Stop-Job -Job $job -ErrorAction SilentlyContinue
                }
                Remove-Job -Job $job -ErrorAction SilentlyContinue
            }
        }
    }
    catch {
        Write-DevConsole "⚠️ Could not query Ollama models: $($_.Exception.Message)" "WARNING"
    }

    # Fallback to defaults
    Write-DevConsole "📋 Using default model list" "INFO"
    return $defaultModels
}

# ============================================
# Chat Tab Management
# ============================================

function New-ChatTab {
    param(
        [string]$TabName,
        [string]$Model = "bigdaddyg-fast:latest"
    )

    if ($script:chatTabs.Count -ge $script:maxChatTabs) {
        Write-DevConsole "❌ Maximum chat tabs reached ($($script:maxChatTabs))" "ERROR"
        return $null
    }

    if ([string]::IsNullOrEmpty($TabName)) {
        $TabName = "Chat $($script:chatTabs.Count + 1)"
    }

    $tabId = [Guid]::NewGuid().ToString()
    
    # Create TabPage
    $tabPage = New-Object System.Windows.Forms.TabPage
    $tabPage.Text = $TabName
    $tabPage.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)

    # Create Chat Box (RichTextBox)
    $chatBox = New-Object System.Windows.Forms.RichTextBox
    $chatBox.Location = New-Object System.Drawing.Point(5, 5)
    $chatBox.Size = New-Object System.Drawing.Size(580, 350)
    $chatBox.BackColor = [System.Drawing.Color]::FromArgb(20, 20, 20)
    $chatBox.ForeColor = [System.Drawing.Color]::White
    $chatBox.ReadOnly = $true
    $chatBox.Font = New-Object System.Drawing.Font("Consolas", 10)
    $chatBox.ScrollBars = "Vertical"
    $tabPage.Controls.Add($chatBox)

    # Create Input Box
    $inputBox = New-Object System.Windows.Forms.TextBox
    $inputBox.Location = New-Object System.Drawing.Point(5, 365)
    $inputBox.Size = New-Object System.Drawing.Size(480, 25)
    $inputBox.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $inputBox.ForeColor = [System.Drawing.Color]::White
    $inputBox.Font = New-Object System.Drawing.Font("Consolas", 10)
    $inputBox.Multiline = $true
    $inputBox.AcceptsReturn = $true
    $tabPage.Controls.Add($inputBox)

    # Create Model Selector
    $modelCombo = New-Object System.Windows.Forms.ComboBox
    $modelCombo.Location = New-Object System.Drawing.Point(490, 365)
    $modelCombo.Size = New-Object System.Drawing.Size(95, 25)
    $modelCombo.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
    $modelCombo.ForeColor = [System.Drawing.Color]::White
    $modelCombo.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
    
    $availModels = Get-AvailableModels
    foreach ($m in $availModels) {
        $modelCombo.Items.Add($m) | Out-Null
    }
    if ($modelCombo.Items.Contains($Model)) {
        $modelCombo.SelectedItem = $Model
    } elseif ($modelCombo.Items.Count -gt 0) {
        $modelCombo.SelectedIndex = 0
    }
    $tabPage.Controls.Add($modelCombo)

    # Create Send Button
    $sendButton = New-Object System.Windows.Forms.Button
    $sendButton.Text = "Send"
    $sendButton.Location = New-Object System.Drawing.Point(490, 395)
    $sendButton.Size = New-Object System.Drawing.Size(95, 25)
    $sendButton.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
    $sendButton.ForeColor = [System.Drawing.Color]::White
    $sendButton.FlatStyle = "Flat"
    $tabPage.Controls.Add($sendButton)

    # Wire up events
    $inputBox.add_KeyDown({
        param($sender, $e)
        if ($e.KeyCode -eq "Enter" -and -not $e.Shift) {
            $e.SuppressKeyPress = $true
            $sendButton.PerformClick()
        }
    })

    $sendButton.add_Click({
        param($sender, $e)
        Send-ChatMessage -TabId $tabId
    })

    # Create Session Object
    $chatSession = @{
        TabId          = $tabId
        TabPage        = $tabPage
        ChatBox        = $chatBox
        InputBox       = $inputBox
        ModelCombo     = $modelCombo
        SendButton     = $sendButton
        Messages       = @()
        LastActivity   = Get-Date
    }

    # Store in collection
    $script:chatTabs[$tabId] = $chatSession

    # Add to tab control
    $script:chatTabControl.TabPages.Add($tabPage)
    $script:chatTabControl.SelectedTab = $tabPage
    $script:activeChatTabId = $tabId

    # Welcome message
    $welcomeText = "🤖 Welcome to RawrXD AI Chat!`n💡 Type your message and press Enter or click Send.`n🔧 Model: $($modelCombo.SelectedItem)`n`n"
    $chatBox.AppendText($welcomeText)
    $chatBox.ScrollToCaret()

    Write-DevConsole "✅ Created new chat tab: $TabName ($tabId)" "SUCCESS"
    return $tabId
}

function Remove-ChatTab {
    param([string]$TabId)

    if (-not $script:chatTabs.ContainsKey($TabId)) {
        Write-DevConsole "❌ Chat tab $TabId not found" "ERROR"
        return
    }

    $chatSession = $script:chatTabs[$TabId]

    # Remove from tab control
    $script:chatTabControl.TabPages.Remove($chatSession.TabPage)

    # Clean up resources
    $chatSession.TabPage.Dispose()

    # Remove from collection
    $script:chatTabs.Remove($TabId)

    # Update active tab if this was active
    if ($script:activeChatTabId -eq $TabId) {
        $tabPageCount = $script:chatTabControl.TabPages.Count
        if ($tabPageCount -gt 0) {
             $script:activeChatTabId = $null 
        }
        else {
            $script:activeChatTabId = $null
        }
    }

    Write-DevConsole "✅ Closed chat tab" "SUCCESS"
}

function Get-ActiveChatTab {
    if ($script:activeChatTabId -and $script:chatTabs.ContainsKey($script:activeChatTabId)) {
        return $script:chatTabs[$script:activeChatTabId]
    }
    return $null
}

function Send-ChatMessage {
    param(
        [string]$TabId
    )

    if (-not $script:chatTabs.ContainsKey($TabId)) { return }

    $chatSession = $script:chatTabs[$TabId]
    $message = $chatSession.InputBox.Text.Trim()

    if ([string]::IsNullOrWhiteSpace($message)) { return }

    # Add user message to chat
    $userText = "You: $message`n"
    $chatSession.ChatBox.SelectionColor = [System.Drawing.Color]::LightGreen
    $chatSession.ChatBox.AppendText($userText)
    $chatSession.ChatBox.ScrollToCaret()

    # Store message in session
    $chatSession.Messages += @{
        Role      = "user"
        Content   = $message
        Timestamp = Get-Date
    }

    # Clear input
    $chatSession.InputBox.Text = ""
    $chatSession.LastActivity = Get-Date

    # Show processing indicator
    $processingText = "AI (processing...): "
    $chatSession.ChatBox.SelectionColor = [System.Drawing.Color]::Yellow
    $chatSession.ChatBox.AppendText($processingText)
    $chatSession.ChatBox.ScrollToCaret()

    # Process Response
    $model = $chatSession.ModelCombo.SelectedItem
    
    # Check for agent commands
    $agentCommand = Detect-AgentIntent -Message $message
    if ($agentCommand) {
        $responseMessage = Process-AgentCommand -Command $agentCommand.Command -Parameters $agentCommand.Parameters -SourceContext "Chat:$TabId"
        if ($responseMessage -eq $true) { $responseMessage = "Command executed successfully." }
        if ($responseMessage -eq $false) { $responseMessage = "Command failed." }
    }
    else {
        # Use new multi-backend AI system (prefers llama.cpp over Ollama)
        try {
            $backend = Get-SelectiveAIBackend
            Write-DevConsole "🤖 Using backend: $backend" "INFO"
            
            switch ($backend) {
                "LlamaCPP" {
                    # Use direct GGUF model loading (no Ollama)
                    $responseMessage = Invoke-LlamaCPPQuery -Prompt $message
                }
                "Ollama" {
                    $body = @{
                        model  = $model
                        prompt = $message
                        stream = $false
                    } | ConvertTo-Json
                    
                    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $body -ContentType "application/json" -TimeoutSec 60 -ErrorAction Stop
                    $responseMessage = $response.response
                }
                "LlamaFile" {
                    $responseMessage = Invoke-LlamaFileQuery -Prompt $message
                }
                "ReverseHttp" {
                    $responseMessage = Invoke-ReverseHttpQuery -Prompt $message
                }
                default {
                    $responseMessage = "❌ No AI backend available. Check that llama.cpp is configured in D:\OllamaModels\llama.cpp\"
                }
            }
        }
        catch {
            $responseMessage = "Error: $($_.Exception.Message)"
            Write-DevConsole "AI query failed: $_" "ERROR"
        }
    }

    # Append response
    $chatSession.ChatBox.AppendText("`n")
    
    $aiText = "AI: $responseMessage`n`n"
    $chatSession.ChatBox.SelectionColor = [System.Drawing.Color]::White
    $chatSession.ChatBox.AppendText($aiText)
    $chatSession.ChatBox.ScrollToCaret()

    $chatSession.Messages += @{
        Role      = "assistant"
        Content   = $responseMessage
        Timestamp = Get-Date
    }
}

function Detect-AgentIntent {
    param([string]$Message)
    
    if ($Message -match "^/(run|exec)\s+(.+)") {
        return @{ Command = "run_command"; Parameters = @{ Command = $matches[2] } }
    }
    return $null
}

function Process-AgentCommand {
    param(
        [string]$Command,
        [hashtable]$Parameters,
        [string]$SourceContext
    )
    
    Write-DevConsole "🤖 Processing Agent Command: $Command" "INFO"
    
    switch ($Command) {
        "run_command" {
            if ($Parameters.Command) {
                Invoke-Expression $Parameters.Command
                return "Executed: $($Parameters.Command)"
            }
        }
    }
    return "Unknown command"
}

function Write-AgenticErrorLog {
    param(
        [string]$ErrorMessage,
        [string]$ErrorCategory = "AI",
        [string]$Severity = "MEDIUM",
        [string]$AgentContext = "",
        [string]$AIModel = "",
        [hashtable]$AIMetrics = @{}
    )
    Write-DevConsole "[AGENTIC ERROR] $ErrorMessage" "ERROR"
}

function Update-AIErrorStatistics {
    # Stub
}

function Process-ThreadSafeLogs {
    # Stub
}
