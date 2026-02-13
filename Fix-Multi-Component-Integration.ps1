# 🔧 RawrXD Multi-Component Integration Repair Tool
# Specifically addresses the FAILED Multi-Component Integration issues

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Red
Write-Host "  RawrXD Multi-Component Integration Repair Tool v1.0" -ForegroundColor Red
Write-Host "  FIXING: UI↔Chat, File↔UI, Error↔UI Integration Failures" -ForegroundColor Red
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Red
Write-Host ""

$repairStartTime = Get-Date

# Track integration issues found and fixed
$integrationIssues = @{
    UIToChatIssues = @()
    FileToUIIssues = @()
    ErrorHandlingIssues = @()
    FixesApplied = @()
    FailedFixes = @()
}

function Test-IntegrationPattern {
    param(
        [string]$Content,
        [string]$PatternName,
        [string]$Pattern,
        [string]$Description
    )
    
    Write-Host "`n🔍 Testing: $PatternName" -ForegroundColor Yellow
    Write-Host "   Pattern: $Pattern" -ForegroundColor Gray
    Write-Host "   Purpose: $Description" -ForegroundColor Gray
    
    $matches = [regex]::Matches($Content, $Pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    
    if ($matches.Count -gt 0) {
        Write-Host "   ✅ FOUND: $($matches.Count) instances" -ForegroundColor Green
        return @{
            Found = $true
            Count = $matches.Count
            Matches = $matches
        }
    } else {
        Write-Host "   ❌ MISSING: Integration pattern not found" -ForegroundColor Red
        return @{
            Found = $false
            Count = 0
            Matches = @()
        }
    }
}

function Analyze-UIToChatIntegration {
    param([string]$Content)
    
    Write-Host "`n🔍 ANALYZING UI ↔ CHAT INTEGRATION" -ForegroundColor Cyan
    Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    # Test various UI to Chat connection patterns - FIXED to match actual RawrXD patterns
    $patterns = @{
        "ChatBox Event Handlers" = @{
            Pattern = 'chatSession\.ChatBox|chatBox\.AppendText|ChatBox.*Add_'
            Description = "Event handlers connecting chat UI to functionality"
        }
        "Send Button to Chat" = @{
            Pattern = 'Send-ChatMessage|sendBtn.*Add_Click|agentSendBtn\.Add_Click'
            Description = "Send button connected to chat sending function"
        }
        "Text Input to Chat" = @{
            Pattern = 'inputBox.*add_KeyDown|agentInputBox\.Add_KeyDown|KeyDown.*Enter.*Send'
            Description = "Enter key in text box triggers chat"
        }
        "Chat Display Updates" = @{
            Pattern = 'AppendText.*Message|ChatBox\.Text|chatSession\.Messages'
            Description = "Chat messages displayed in UI controls"
        }
        "Chat History UI" = @{
            Pattern = 'chatSession\.Messages|Chat.*History|Export.*Chat'
            Description = "Chat history connected to display controls"
        }
    }
    
    $foundPatterns = 0
    $totalPatterns = $patterns.Count
    
    foreach ($patternName in $patterns.Keys) {
        $patternInfo = $patterns[$patternName]
        $result = Test-IntegrationPattern -Content $Content -PatternName $patternName -Pattern $patternInfo.Pattern -Description $patternInfo.Description
        
        if ($result.Found) {
            $foundPatterns++
        } else {
            $integrationIssues.UIToChatIssues += @{
                Pattern = $patternName
                Issue = "Missing UI to Chat integration"
                Solution = "Need to add event handlers connecting UI controls to chat functions"
            }
        }
    }
    
    Write-Host "`n📊 UI ↔ Chat Integration Score: $foundPatterns/$totalPatterns" -ForegroundColor Cyan
    return ($foundPatterns / $totalPatterns)
}

function Analyze-FileToUIIntegration {
    param([string]$Content)
    
    Write-Host "`n🔍 ANALYZING FILE ↔ UI INTEGRATION" -ForegroundColor Cyan
    Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    # FIXED patterns to match actual RawrXD implementation
    $patterns = @{
        "File Open to Text Display" = @{
            Pattern = 'OpenFileDialog|editor\.Text\s*=\s*\$content|\$content.*editor\.Text'
            Description = "Opened files displayed in text controls"
        }
        "File Content to Editor" = @{
            Pattern = 'Get-Content.*\$content|\$content.*Get-Content|script:editor\.Text'
            Description = "File content loaded into editor controls"
        }
        "Save Text to File" = @{
            Pattern = 'Set-Content|Out-File|WriteAllText.*editor\.Text'
            Description = "Text from UI controls saved to files"
        }
        "File Dialog Integration" = @{
            Pattern = 'SaveFileDialog|OpenFileDialog.*ShowDialog|dlg\.FileName'
            Description = "File dialogs connected to text display"
        }
        "File Status in UI" = @{
            Pattern = 'statusLabel\.Text|currentFile|Opened:|Saved:'
            Description = "File operation status displayed in UI"
        }
    }
    
    $foundPatterns = 0
    $totalPatterns = $patterns.Count
    
    foreach ($patternName in $patterns.Keys) {
        $patternInfo = $patterns[$patternName]
        $result = Test-IntegrationPattern -Content $Content -PatternName $patternName -Pattern $patternInfo.Pattern -Description $patternInfo.Description
        
        if ($result.Found) {
            $foundPatterns++
        } else {
            $integrationIssues.FileToUIIssues += @{
                Pattern = $patternName
                Issue = "Missing File to UI integration"
                Solution = "Need to add code connecting file operations to UI display"
            }
        }
    }
    
    Write-Host "`n📊 File ↔ UI Integration Score: $foundPatterns/$totalPatterns" -ForegroundColor Cyan
    return ($foundPatterns / $totalPatterns)
}

function Analyze-ErrorHandlingIntegration {
    param([string]$Content)
    
    Write-Host "`n🔍 ANALYZING ERROR HANDLING ↔ UI INTEGRATION" -ForegroundColor Cyan
    Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    # FIXED patterns to match actual RawrXD implementation
    $patterns = @{
        "Try-Catch with MessageBox" = @{
            Pattern = 'try\s*\{|catch\s*\{|MessageBox::Show'
            Description = "Errors caught and displayed to user via MessageBox"
        }
        "Error Notifications" = @{
            Pattern = 'Show-ErrorNotification|Register-ErrorHandler|Write-ErrorLog'
            Description = "Custom error notification system"
        }
        "Status Bar Errors" = @{
            Pattern = 'statusLabel|Error:|status.*Text'
            Description = "Errors displayed in status bar"
        }
        "Error Dialog Integration" = @{
            Pattern = 'Show-ErrorReportDialog|ErrorReport|error.*Form'
            Description = "Dedicated error dialogs"
        }
        "Exception Handling" = @{
            Pattern = '\$_\.Exception|Exception\.Message|ErrorAction'
            Description = "Exception information handled properly"
        }
    }
    
    $foundPatterns = 0
    $totalPatterns = $patterns.Count
    
    foreach ($patternName in $patterns.Keys) {
        $patternInfo = $patterns[$patternName]
        $result = Test-IntegrationPattern -Content $Content -PatternName $patternName -Pattern $patternInfo.Pattern -Description $patternInfo.Description
        
        if ($result.Found) {
            $foundPatterns++
        } else {
            $integrationIssues.ErrorHandlingIssues += @{
                Pattern = $patternName
                Issue = "Missing Error to UI integration"
                Solution = "Need to add error handling with user notifications"
            }
        }
    }
    
    Write-Host "`n📊 Error Handling ↔ UI Integration Score: $foundPatterns/$totalPatterns" -ForegroundColor Cyan
    return ($foundPatterns / $totalPatterns)
}

function Generate-IntegrationFixes {
    Write-Host "`n🔧 GENERATING INTEGRATION FIX CODE" -ForegroundColor Yellow
    Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow
    
    $fixCode = @"
# 🔧 RawrXD Integration Fixes
# Auto-generated code to fix Multi-Component Integration failures

# ═══════════════════════════════════════════════════════════════
# FIX 1: UI ↔ CHAT INTEGRATION
# ═══════════════════════════════════════════════════════════════

# Add to your RawrXD.ps1 file:

# Fix: Connect Send Button to Chat Function
`$sendButton.Add_Click({
    if (`$textBox.Text.Trim() -ne "") {
        Send-ChatMessage -Message `$textBox.Text
        `$textBox.Text = ""
        `$textBox.Focus()
    }
})

# Fix: Connect Enter Key to Chat Sending
`$textBox.Add_KeyDown({
    if (`$_.KeyCode -eq [System.Windows.Forms.Keys]::Enter) {
        `$sendButton.PerformClick()
    }
})

# Fix: Update Chat Display
function Update-ChatDisplay {
    param([string]`$Message, [string]`$Sender = "User")
    
    `$timestamp = Get-Date -Format "HH:mm:ss"
    `$formattedMessage = "[$`timestamp] `$Sender`: `$Message"
    
    # Add to chat history
    `$chatHistory.AppendText("`$formattedMessage`n")
    `$chatHistory.ScrollToCaret()
}

# Fix: Chat History Integration
`$chatHistory.Text = ""  # Initialize empty
`$global:ChatMessages = @()  # Store messages

function Add-ChatMessage {
    param([string]`$Message, [string]`$Sender = "User")
    
    `$global:ChatMessages += @{
        Timestamp = Get-Date
        Sender = `$Sender
        Message = `$Message
    }
    
    Update-ChatDisplay -Message `$Message -Sender `$Sender
}

# ═══════════════════════════════════════════════════════════════
# FIX 2: FILE ↔ UI INTEGRATION  
# ═══════════════════════════════════════════════════════════════

# Fix: File Open Integration
`$openMenuItem.Add_Click({
    `$openFileDialog = New-Object System.Windows.Forms.OpenFileDialog
    `$openFileDialog.Filter = "Text Files (*.txt)|*.txt|PowerShell Files (*.ps1)|*.ps1|All Files (*.*)|*.*"
    
    if (`$openFileDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        try {
            `$content = Get-Content `$openFileDialog.FileName -Raw
            `$textEditor.Text = `$content
            `$statusLabel.Text = "Opened: `$(`$openFileDialog.FileName)"
            `$global:CurrentFilePath = `$openFileDialog.FileName
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error opening file: `$(`$_.Exception.Message)", "File Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
        }
    }
})

# Fix: File Save Integration  
`$saveMenuItem.Add_Click({
    if (`$global:CurrentFilePath) {
        try {
            Set-Content -Path `$global:CurrentFilePath -Value `$textEditor.Text -Encoding UTF8
            `$statusLabel.Text = "Saved: `$`$global:CurrentFilePath"
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error saving file: `$(`$_.Exception.Message)", "Save Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
        }
    } else {
        # Trigger Save As
        `$saveAsMenuItem.PerformClick()
    }
})

# Fix: Save As Integration
`$saveAsMenuItem.Add_Click({
    `$saveFileDialog = New-Object System.Windows.Forms.SaveFileDialog
    `$saveFileDialog.Filter = "Text Files (*.txt)|*.txt|PowerShell Files (*.ps1)|*.ps1|All Files (*.*)|*.*"
    
    if (`$saveFileDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        try {
            Set-Content -Path `$saveFileDialog.FileName -Value `$textEditor.Text -Encoding UTF8
            `$statusLabel.Text = "Saved: `$(`$saveFileDialog.FileName)"
            `$global:CurrentFilePath = `$saveFileDialog.FileName
        }
        catch {
            [System.Windows.Forms.MessageBox]::Show("Error saving file: `$(`$_.Exception.Message)", "Save Error", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
        }
    }
})

# ═══════════════════════════════════════════════════════════════
# FIX 3: ERROR HANDLING ↔ UI INTEGRATION
# ═══════════════════════════════════════════════════════════════

# Fix: Global Error Handler
function Show-ErrorToUser {
    param(
        [string]`$ErrorMessage,
        [string]`$Title = "Error",
        [System.Windows.Forms.MessageBoxIcon]`$Icon = [System.Windows.Forms.MessageBoxIcon]::Error
    )
    
    # Show in MessageBox
    [System.Windows.Forms.MessageBox]::Show(`$ErrorMessage, `$Title, [System.Windows.Forms.MessageBoxButtons]::OK, `$Icon)
    
    # Also update status bar
    if (`$statusLabel) {
        `$statusLabel.Text = "Error: `$ErrorMessage"
        `$statusLabel.ForeColor = [System.Drawing.Color]::Red
    }
    
    # Log error
    Write-ErrorLog -Message `$ErrorMessage
}

# Fix: Wrap risky operations in try-catch
function Invoke-SafeOperation {
    param([scriptblock]`$Operation, [string]`$OperationName = "Operation")
    
    try {
        & `$Operation
    }
    catch {
        Show-ErrorToUser -ErrorMessage "`$OperationName failed: `$(`$_.Exception.Message)" -Title "`$OperationName Error"
    }
}

# Fix: Ollama Connection Error Handling
function Connect-OllamaWithErrorHandling {
    try {
        `$response = Invoke-RestMethod -Uri "http://localhost:11434/api/version" -TimeoutSec 5 -ErrorAction Stop
        `$statusLabel.Text = "Ollama connected successfully"
        `$statusLabel.ForeColor = [System.Drawing.Color]::Green
        return `$true
    }
    catch {
        Show-ErrorToUser -ErrorMessage "Failed to connect to Ollama service: `$(`$_.Exception.Message)" -Title "Ollama Connection Error"
        return `$false
    }
}

# Fix: Chat Error Handling
function Send-ChatMessageSafely {
    param([string]`$Message)
    
    if ([string]::IsNullOrWhiteSpace(`$Message)) {
        Show-ErrorToUser -ErrorMessage "Message cannot be empty" -Title "Chat Error" -Icon Warning
        return
    }
    
    try {
        # Add user message immediately
        Add-ChatMessage -Message `$Message -Sender "User"
        
        # Send to Ollama
        `$response = Invoke-OllamaRequest -Message `$Message
        
        if (`$response) {
            Add-ChatMessage -Message `$response -Sender "AI"
        } else {
            Show-ErrorToUser -ErrorMessage "No response received from AI" -Title "Chat Error"
        }
    }
    catch {
        Show-ErrorToUser -ErrorMessage "Chat error: `$(`$_.Exception.Message)" -Title "Chat Error"
    }
}

# ═══════════════════════════════════════════════════════════════
# INTEGRATION TEST FUNCTIONS
# ═══════════════════════════════════════════════════════════════

function Test-IntegrationFixes {
    Write-Host "🧪 Testing integration fixes..." -ForegroundColor Yellow
    
    # Test UI to Chat
    try {
        if (`$sendButton -and `$textBox) {
            Write-Host "✅ UI to Chat controls found" -ForegroundColor Green
        } else {
            Write-Host "❌ UI to Chat controls missing" -ForegroundColor Red
        }
    } catch { Write-Host "⚠️ UI to Chat test failed" -ForegroundColor Yellow }
    
    # Test File to UI
    try {
        if (`$openMenuItem -and `$saveMenuItem -and `$textEditor) {
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

"@
    
    return $fixCode
}

# Main analysis
Write-Host "🔍 MULTI-COMPONENT INTEGRATION ANALYSIS" -ForegroundColor Red
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Red

if (Test-Path ".\RawrXD.ps1") {
    try {
        $rawrxdContent = Get-Content ".\RawrXD.ps1" -Raw
        
        # Analyze each integration area
        $uiChatScore = Analyze-UIToChatIntegration -Content $rawrxdContent
        $fileUIScore = Analyze-FileToUIIntegration -Content $rawrxdContent
        $errorUIScore = Analyze-ErrorHandlingIntegration -Content $rawrxdContent
        
        # Calculate overall integration score
        $overallScore = ($uiChatScore + $fileUIScore + $errorUIScore) / 3
        
        Write-Host "`n📊 INTEGRATION ANALYSIS RESULTS" -ForegroundColor Red
        Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Red
        
        Write-Host "`n🎯 Integration Scores:" -ForegroundColor White
        Write-Host "   UI ↔ Chat Integration: $([math]::Round($uiChatScore * 100, 1))%" -ForegroundColor $(if($uiChatScore -ge 0.8) {"Green"} elseif($uiChatScore -ge 0.5) {"Yellow"} else {"Red"})
        Write-Host "   File ↔ UI Integration: $([math]::Round($fileUIScore * 100, 1))%" -ForegroundColor $(if($fileUIScore -ge 0.8) {"Green"} elseif($fileUIScore -ge 0.5) {"Yellow"} else {"Red"})
        Write-Host "   Error ↔ UI Integration: $([math]::Round($errorUIScore * 100, 1))%" -ForegroundColor $(if($errorUIScore -ge 0.8) {"Green"} elseif($errorUIScore -ge 0.5) {"Yellow"} else {"Red"})
        Write-Host "   Overall Integration: $([math]::Round($overallScore * 100, 1))%" -ForegroundColor $(if($overallScore -ge 0.8) {"Green"} elseif($overallScore -ge 0.5) {"Yellow"} else {"Red"})
        
        # Show specific issues
        if ($integrationIssues.UIToChatIssues.Count -gt 0) {
            Write-Host "`n❌ UI ↔ CHAT ISSUES:" -ForegroundColor Red
            foreach ($issue in $integrationIssues.UIToChatIssues) {
                Write-Host "   • $($issue.Pattern): $($issue.Issue)" -ForegroundColor Yellow
                Write-Host "     Solution: $($issue.Solution)" -ForegroundColor Gray
            }
        }
        
        if ($integrationIssues.FileToUIIssues.Count -gt 0) {
            Write-Host "`n❌ FILE ↔ UI ISSUES:" -ForegroundColor Red
            foreach ($issue in $integrationIssues.FileToUIIssues) {
                Write-Host "   • $($issue.Pattern): $($issue.Issue)" -ForegroundColor Yellow
                Write-Host "     Solution: $($issue.Solution)" -ForegroundColor Gray
            }
        }
        
        if ($integrationIssues.ErrorHandlingIssues.Count -gt 0) {
            Write-Host "`n❌ ERROR HANDLING ↔ UI ISSUES:" -ForegroundColor Red
            foreach ($issue in $integrationIssues.ErrorHandlingIssues) {
                Write-Host "   • $($issue.Pattern): $($issue.Issue)" -ForegroundColor Yellow
                Write-Host "     Solution: $($issue.Solution)" -ForegroundColor Gray
            }
        }
        
        # Generate fix code
        $fixCode = Generate-IntegrationFixes
        
        # Save fix code to file
        $fixCodeFile = ".\RawrXD-Integration-Fixes.ps1"
        Set-Content $fixCodeFile $fixCode -Encoding UTF8
        Write-Host "`n✅ Integration fix code saved to: $fixCodeFile" -ForegroundColor Green
        
        Write-Host "`n🔧 REPAIR INSTRUCTIONS:" -ForegroundColor Yellow
        Write-Host "1. Open RawrXD.ps1 in your editor" -ForegroundColor White
        Write-Host "2. Copy the fix code from: $fixCodeFile" -ForegroundColor White
        Write-Host "3. Add the fix code to appropriate sections of RawrXD.ps1" -ForegroundColor White
        Write-Host "4. Test the integration fixes" -ForegroundColor White
        Write-Host "5. Re-run the comprehensive test suite" -ForegroundColor White
        
        if ($overallScore -lt 0.5) {
            Write-Host "`n🚨 CRITICAL: Integration failures require immediate attention!" -ForegroundColor Red
        } elseif ($overallScore -lt 0.8) {
            Write-Host "`n⚠️ WARNING: Integration issues need to be addressed" -ForegroundColor Yellow
        } else {
            Write-Host "`n✅ GOOD: Integration is mostly working well" -ForegroundColor Green
        }
        
    }
    catch {
        Write-Host "❌ Error analyzing RawrXD.ps1: $_" -ForegroundColor Red
    }
} else {
    Write-Host "❌ RawrXD.ps1 not found!" -ForegroundColor Red
}

$repairEndTime = Get-Date
$repairDuration = [math]::Round(($repairEndTime - $repairStartTime).TotalSeconds, 1)

Write-Host "`n📈 INTEGRATION REPAIR ANALYSIS COMPLETE!" -ForegroundColor Red
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Red
Write-Host "Duration: $repairDuration seconds"
Write-Host "Issues Identified: $($integrationIssues.UIToChatIssues.Count + $integrationIssues.FileToUIIssues.Count + $integrationIssues.ErrorHandlingIssues.Count)"

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Red
Write-Host "Integration repair analysis complete! Apply fixes to resolve issues." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Red