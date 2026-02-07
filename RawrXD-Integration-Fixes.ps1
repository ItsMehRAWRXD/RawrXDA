# 🔧 RawrXD Integration Fixes
# Auto-generated code to fix Multi-Component Integration failures

# ═══════════════════════════════════════════════════════════════
# FIX 1: UI ↔ CHAT INTEGRATION
# ═══════════════════════════════════════════════════════════════

# Add to your RawrXD.ps1 file:

# Fix: Connect Send Button to Chat Function
$sendButton.Add_Click({
    if ($textBox.Text.Trim() -ne "") {
        Send-ChatMessage -Message $textBox.Text
        $textBox.Text = ""
        $textBox.Focus()
    }
})

# Fix: Connect Enter Key to Chat Sending
$textBox.Add_KeyDown({
    if ($_.KeyCode -eq [System.Windows.Forms.Keys]::Enter) {
        $sendButton.PerformClick()
    }
})

# Fix: Update Chat Display
function Update-ChatDisplay {
    param([string]$Message, [string]$Sender = "User")
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    $formattedMessage = "[$	imestamp] $Sender: $Message"
    
    # Add to chat history
    $chatHistory.AppendText("$formattedMessage
")
    $chatHistory.ScrollToCaret()
}

# Fix: Chat History Integration
$chatHistory.Text = ""  # Initialize empty
$global:ChatMessages = @()  # Store messages

function Add-ChatMessage {
    param([string]$Message, [string]$Sender = "User")
    
    $global:ChatMessages += @{
        Timestamp = Get-Date
        Sender = $Sender
        Message = $Message
    }
    
    Update-ChatDisplay -Message $Message -Sender $Sender
}

# ═══════════════════════════════════════════════════════════════
# FIX 2: FILE ↔ UI INTEGRATION  
# ═══════════════════════════════════════════════════════════════

# Fix: File Open Integration
$openMenuItem.Add_Click({
    $openFileDialog = New-Object System.Windows.Forms.OpenFileDialog
    $openFileDialog.Filter = "Text Files (*.txt)|*.txt|PowerShell Files (*.ps1)|*.ps1|All Files (*.*)|*.*"
    
    if ($openFileDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        try {
            $content = Get-Content $openFileDialog.FileName -Raw
            $textEditor.Text = $content
            $statusLabel.Text = "Opened: $($openFileDialog.FileName)"
            $global:CurrentFilePath = $openFileDialog.FileName
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error opening file: $($_.Exception.Message)", "File Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
        }
    }
})

# Fix: File Save Integration  
$saveMenuItem.Add_Click({
    if ($global:CurrentFilePath) {
        try {
            Set-Content -Path $global:CurrentFilePath -Value $textEditor.Text -Encoding UTF8
            $statusLabel.Text = "Saved: $$global:CurrentFilePath"
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error saving file: $($_.Exception.Message)", "Save Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
        }
    } else {
        # Trigger Save As
        $saveAsMenuItem.PerformClick()
    }
})

# Fix: Save As Integration
$saveAsMenuItem.Add_Click({
    $saveFileDialog = New-Object System.Windows.Forms.SaveFileDialog
    $saveFileDialog.Filter = "Text Files (*.txt)|*.txt|PowerShell Files (*.ps1)|*.ps1|All Files (*.*)|*.*"
    
    if ($saveFileDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        try {
            Set-Content -Path $saveFileDialog.FileName -Value $textEditor.Text -Encoding UTF8
            $statusLabel.Text = "Saved: $($saveFileDialog.FileName)"
            $global:CurrentFilePath = $saveFileDialog.FileName
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error saving file: $($_.Exception.Message)", "Save Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
        }
    }
})

# ═══════════════════════════════════════════════════════════════
# FIX 3: ERROR HANDLING ↔ UI INTEGRATION
# ═══════════════════════════════════════════════════════════════

# Fix: Global Error Handler
function Show-ErrorToUser {
    param(
        [string]$ErrorMessage,
        [string]$Title = "Error",
        [System.Windows.Forms.MessageBoxIcon]$Icon = [System.Windows.Forms.MessageBoxIcon]::Error
    )
    
    # Show in MessageBox
    [System.Windows.Forms.MessageBox]::Show($ErrorMessage, $Title, [System.Windows.Forms.MessageBoxButtons]::OK, $Icon)
    
    # Also update status bar
    if ($statusLabel) {
        $statusLabel.Text = "Error: $ErrorMessage"
        $statusLabel.ForeColor = [System.Drawing.Color]::Red
    }
    
    # Log error
    Write-ErrorLog -Message $ErrorMessage
}

# Fix: Wrap risky operations in try-catch
function Invoke-SafeOperation {
    param([scriptblock]$Operation, [string]$OperationName = "Operation")
    
    try {
        & $Operation
    }
    catch {
        Show-ErrorToUser -ErrorMessage "$OperationName failed: $($_.Exception.Message)" -Title "$OperationName Error"
    }
}

# Fix: Ollama Connection Error Handling
function Connect-OllamaWithErrorHandling {
    try {
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/version" -TimeoutSec 5 -ErrorAction Stop
        $statusLabel.Text = "Ollama connected successfully"
        $statusLabel.ForeColor = [System.Drawing.Color]::Green
        return $true
    }
    catch {
        Show-ErrorToUser -ErrorMessage "Failed to connect to Ollama service: $($_.Exception.Message)" -Title "Ollama Connection Error"
        return $false
    }
}

# Fix: Chat Error Handling
function Send-ChatMessageSafely {
    param([string]$Message)
    
    if ([string]::IsNullOrWhiteSpace($Message)) {
        Show-ErrorToUser -ErrorMessage "Message cannot be empty" -Title "Chat Error" -Icon Warning
        return
    }
    
    try {
        # Add user message immediately
        Add-ChatMessage -Message $Message -Sender "User"
        
        # Send to Ollama
        $response = Invoke-OllamaRequest -Message $Message
        
        if ($response) {
            Add-ChatMessage -Message $response -Sender "AI"
        } else {
            Show-ErrorToUser -ErrorMessage "No response received from AI" -Title "Chat Error"
        }
    }
    catch {
        Show-ErrorToUser -ErrorMessage "Chat error: $($_.Exception.Message)" -Title "Chat Error"
    }
}

# ═══════════════════════════════════════════════════════════════
# INTEGRATION TEST FUNCTIONS
# ═══════════════════════════════════════════════════════════════

function Test-IntegrationFixes {
    Write-Host "🧪 Testing integration fixes..." -ForegroundColor Yellow
    
    # Test UI to Chat
    try {
        if ($sendButton -and $textBox) {
            Write-Host "✅ UI to Chat controls found" -ForegroundColor Green
        } else {
            Write-Host "❌ UI to Chat controls missing" -ForegroundColor Red
        }
    } catch { Write-Host "⚠️ UI to Chat test failed" -ForegroundColor Yellow }
    
    # Test File to UI
    try {
        if ($openMenuItem -and $saveMenuItem -and $textEditor) {
            Write-Host "✅ File to UI controls found" -ForegroundColor Green
        } else {
            Write-Host "❌ File to UI controls missing" -ForegroundColor Red
        }
    } catch { Write-Host "⚠️ File to UI test failed" -ForegroundColor Yellow }
    
    # Test Error Handling
    try {
        if (Get-Command Show-ErrorToUser -ErrorAction SilentlyContinue) {
            Write-Host "✅ Error handling functions available" -ForegroundColor Green
        } else {
            Write-Host "❌ Error handling functions missing" -ForegroundColor Red
        }
    } catch { Write-Host "⚠️ Error handling test failed" -ForegroundColor Yellow }
}

Write-Host "🔧 Integration fix code generated!" -ForegroundColor Green
Write-Host "📋 Copy the above code into your RawrXD.ps1 file to fix integration issues." -ForegroundColor Cyan

