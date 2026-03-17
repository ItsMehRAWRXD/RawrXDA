#!/usr/bin/env pwsh
<#
    BIGDADDYG IDE - UI REPAIR & FUNCTIONALITY FIX
    Fixes chat input, scrolling, layout, and interactivity
#>

param(
    [switch]$Diagnose,
    [switch]$RepairCSS,
    [switch]$RepairJS,
    [switch]$RepairAll,
    [switch]$BackupFirst
)

$appPath = "E:\Everything\BigDaddyG-Standalone-40GB\app"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

function ColorOutput { param([string]$Text, [string]$Color = 'White')
    $Colors = @{ 'Green' = 32; 'Red' = 31; 'Yellow' = 33; 'Cyan' = 36; 'Blue' = 34; 'White' = 37 }
    $Code = $Colors[$Color]
    Write-Host "$([char]27)[$($Code)m$Text$([char]27)[0m"
}

function Backup-File {
    param([string]$FilePath)
    if (Test-Path $FilePath) {
        $backupPath = "$FilePath.backup.$timestamp"
        Copy-Item -Path $FilePath -Destination $backupPath -Force
        ColorOutput "  ✓ Backed up to: $(Split-Path $backupPath -Leaf)" 'Green'
        return $backupPath
    }
}

function Diagnose-UIIssues {
    ColorOutput "`n🔍 DIAGNOSING UI ISSUES...`n" 'Cyan'
    
    $issues = @()
    
    # Check index.html structure
    $html = Get-Content "$appPath\electron\index.html" -Raw
    
    ColorOutput "Checking HTML Structure:" 'Blue'
    
    if ($html -match 'id="ai-input"') {
        ColorOutput "  ✓ Chat input element exists" 'Green'
    } else {
        ColorOutput "  ✗ Chat input element MISSING" 'Red'
        $issues += "Missing #ai-input element"
    }
    
    if ($html -match 'id="ai-chat-messages"') {
        ColorOutput "  ✓ Chat messages container exists" 'Green'
    } else {
        ColorOutput "  ✗ Chat messages container MISSING" 'Red'
        $issues += "Missing #ai-chat-messages element"
    }
    
    if ($html -match 'id="right-sidebar"') {
        ColorOutput "  ✓ Right sidebar exists" 'Green'
    } else {
        ColorOutput "  ✗ Right sidebar MISSING" 'Red'
        $issues += "Missing #right-sidebar element"
    }
    
    # Check renderer.js
    ColorOutput "`nChecking Renderer Process:" 'Blue'
    $renderer = Get-Content "$appPath\electron\renderer.js" -Raw
    
    if ($renderer -match 'sendToAI') {
        ColorOutput "  ✓ sendToAI function exists" 'Green'
    } else {
        ColorOutput "  ✗ sendToAI function MISSING" 'Red'
        $issues += "Missing sendToAI function"
    }
    
    if ($renderer -match 'addEventListener.*ai-send-btn') {
        ColorOutput "  ✓ Send button event listener set up" 'Green'
    } else {
        ColorOutput "  ✗ Send button NOT wired up" 'Red'
        $issues += "Send button not wired"
    }
    
    # Check CSS
    ColorOutput "`nChecking CSS Styling:" 'Blue'
    
    if ($html -match '#right-sidebar\s*{[^}]*flex:\s*1') {
        ColorOutput "  ✓ Right sidebar flex layout correct" 'Green'
    } else {
        ColorOutput "  ⚠ Right sidebar flex may have issues" 'Yellow'
    }
    
    if ($html -match '#ai-chat-messages\s*{[^}]*overflow-y:\s*auto') {
        ColorOutput "  ✓ Messages scrolling enabled" 'Green'
    } else {
        ColorOutput "  ⚠ Messages scrolling may be broken" 'Yellow'
    }
    
    ColorOutput "`n📊 ISSUES FOUND: $($issues.Count)" 'Yellow'
    if ($issues.Count -gt 0) {
        $issues | ForEach-Object { ColorOutput "  • $_" 'Red' }
    }
    
    return $issues.Count -eq 0
}

function Repair-ChatInputCSS {
    ColorOutput "`n🎨 REPAIRING CHAT INPUT CSS...`n" 'Cyan'
    
    $htmlPath = "$appPath\electron\index.html"
    
    if ($BackupFirst) { Backup-File $htmlPath }
    
    $content = Get-Content $htmlPath -Raw
    
    # Fix 1: Ensure right-sidebar is properly flexed
    $oldSidebar = '#right-sidebar {
            width: 400px;
            background: var(--void);
            border-left: 1px solid rgba(0, 212, 255, 0.2);
            display: flex;
            flex-direction: column;
            transition: width 0.3s;
        }'
    
    $newSidebar = '#right-sidebar {
            width: 400px;
            background: var(--void);
            border-left: 1px solid rgba(0, 212, 255, 0.2);
            display: flex;
            flex-direction: column;
            transition: width 0.3s;
            min-height: 0;
            position: relative;
            z-index: 50;
        }'
    
    if ($content -contains $oldSidebar) {
        $content = $content -replace [regex]::Escape($oldSidebar), $newSidebar
        ColorOutput "  ✓ Fixed #right-sidebar flex layout" 'Green'
    }
    
    # Fix 2: Ensure chat messages can scroll
    $oldMessages = '#ai-chat-messages {
            flex: 1;
            overflow-y: auto;
            padding: 15px;
        }'
    
    $newMessages = '#ai-chat-messages {
            flex: 1;
            overflow-y: auto;
            overflow-x: hidden;
            padding: 15px;
            min-height: 200px;
            display: flex;
            flex-direction: column;
        }'
    
    if ($content -contains $oldMessages) {
        $content = $content -replace [regex]::Escape($oldMessages), $newMessages
        ColorOutput "  ✓ Fixed #ai-chat-messages scrolling" 'Green'
    }
    
    # Fix 3: Ensure input container doesn't get hidden
    $oldInputContainer = '#ai-input-container {
            padding: 15px;
            border-top: 1px solid rgba(0, 212, 255, 0.2);
        }'
    
    $newInputContainer = '#ai-input-container {
            padding: 15px;
            border-top: 1px solid rgba(0, 212, 255, 0.2);
            flex-shrink: 0;
            background: var(--void);
            z-index: 100;
        }'
    
    if ($content -contains $oldInputContainer) {
        $content = $content -replace [regex]::Escape($oldInputContainer), $newInputContainer
        ColorOutput "  ✓ Fixed #ai-input-container layout" 'Green'
    }
    
    # Fix 4: Ensure textarea is properly sized
    $oldInput = @"
#ai-input {
        width: 100%;
        padding: 12px;
        background: rgba(255, 255, 255, 0.05);
        border: 1px solid rgba(0, 212, 255, 0.3);
        border-radius: 5px;
        color: #fff;
        font-size: 13px;
        resize: vertical;
        min-height: 80px;
        font-family: 'Courier New', monospace;
    }
"@
    
    $newInput = @"
#ai-input {
        width: 100%;
        padding: 12px;
        background: rgba(255, 255, 255, 0.05);
        border: 1px solid rgba(0, 212, 255, 0.3);
        border-radius: 5px;
        color: #fff;
        font-size: 13px;
        resize: none;
        min-height: 60px;
        max-height: 120px;
        font-family: 'Courier New', monospace;
        box-sizing: border-box;
    }
"@
    
    if ($content -contains $oldInput) {
        $content = $content -replace [regex]::Escape($oldInput), $newInput
        ColorOutput "  ✓ Fixed #ai-input sizing" 'Green'
    }
    
    # Fix 5: Hide bottom panel by default if collapsed
    $oldBottomPanel = '#bottom-panel {
            height: 250px;
            background: var(--void);
            border-top: 1px solid rgba(0, 212, 255, 0.2);
            display: flex;
            flex-direction: column;
            transition: height 0.3s;
        }

        #bottom-panel.collapsed {
            height: 0;
            border: none;
        }'
    
    $newBottomPanel = '#bottom-panel {
            height: 250px;
            background: var(--void);
            border-top: 1px solid rgba(0, 212, 255, 0.2);
            display: flex;
            flex-direction: column;
            transition: height 0.3s;
            flex-shrink: 0;
        }

        #bottom-panel.collapsed {
            height: 0;
            border: none;
            overflow: hidden;
        }'
    
    if ($content -contains $oldBottomPanel) {
        $content = $content -replace [regex]::Escape($oldBottomPanel), $newBottomPanel
        ColorOutput "  ✓ Fixed #bottom-panel collapsing" 'Green'
    }
    
    # Fix 6: Main container should allow overflow properly
    $oldMainContainer = '#main-container {
            display: flex;
            flex: 1;
            overflow: hidden;
        }'
    
    $newMainContainer = '#main-container {
            display: flex;
            flex: 1;
            overflow: hidden;
            min-height: 0;
        }'
    
    if ($content -contains $oldMainContainer) {
        $content = $content -replace [regex]::Escape($oldMainContainer), $newMainContainer
        ColorOutput "  ✓ Fixed #main-container overflow" 'Green'
    }
    
    # Write back
    Set-Content -Path $htmlPath -Value $content -Encoding UTF8
    ColorOutput "`n✅ CSS repairs complete!" 'Green'
}

function Repair-ChatInputJS {
    ColorOutput "`n⚙️  REPAIRING CHAT INPUT JAVASCRIPT...`n" 'Cyan'
    
    $rendererPath = "$appPath\electron\renderer.js"
    
    if ($BackupFirst) { Backup-File $rendererPath }
    
    $content = Get-Content $rendererPath -Raw
    
    # Check if sendToAI function exists
    if ($content -notmatch 'function sendToAI') {
        ColorOutput "  ! Adding sendToAI function..." 'Yellow'
        
        $sendToAIFunc = @"

// ============================================================================
// CHAT INPUT HANDLER
// ============================================================================

function sendToAI() {
    const inputElement = document.getElementById('ai-input');
    const messagesContainer = document.getElementById('ai-chat-messages');
    
    if (!inputElement || !messagesContainer) {
        console.error('[Chat] Elements not found:', { input: !!inputElement, messages: !!messagesContainer });
        return;
    }
    
    const message = inputElement.value.trim();
    
    if (!message) {
        console.warn('[Chat] Empty message');
        return;
    }
    
    console.log('[Chat] User message:', message);
    
    // Add user message to UI
    const userMessageDiv = document.createElement('div');
    userMessageDiv.className = 'user-message';
    userMessageDiv.innerHTML = \`<strong style="color: var(--orange);">You:</strong><br>\${escapeHtml(message)}\`;
    messagesContainer.appendChild(userMessageDiv);
    
    // Clear input
    inputElement.value = '';
    inputElement.focus();
    
    // Scroll to bottom
    setTimeout(() => {
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }, 0);
    
    // Send to backend
    sendChatToBackend(message);
}

function sendChatToBackend(message) {
    const model = document.getElementById('model-selector')?.value || 'bigdaddyg-latest';
    const temperature = parseFloat(document.getElementById('temp-slider')?.value || 0.7);
    const maxTokens = parseInt(document.getElementById('tokens-slider')?.value || 2048);
    
    const payload = {
        model: model,
        messages: [
            { role: 'user', content: message }
        ],
        temperature: temperature,
        max_tokens: maxTokens
    };
    
    console.log('[Chat] Sending to backend:', { model, temperature, maxTokens });
    
    fetch('http://localhost:11441/v1/chat/completions', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(r => r.json())
    .then(data => {
        console.log('[Chat] Backend response:', data);
        
        const messagesContainer = document.getElementById('ai-chat-messages');
        if (!messagesContainer) return;
        
        // Add AI response
        const aiMessageDiv = document.createElement('div');
        aiMessageDiv.className = 'ai-message';
        
        let responseText = 'No response received';
        if (data.choices && data.choices[0]) {
            responseText = data.choices[0].message?.content || data.choices[0].text || 'Error: Empty response';
        } else if (data.error) {
            responseText = \`Error: \${data.error}\`;
        }
        
        aiMessageDiv.innerHTML = \`<strong style="color: var(--cyan);">BigDaddyG:</strong><br>\${escapeHtml(responseText)}\`;
        messagesContainer.appendChild(aiMessageDiv);
        
        // Scroll to bottom
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    })
    .catch(error => {
        console.error('[Chat] Fetch error:', error);
        
        const messagesContainer = document.getElementById('ai-chat-messages');
        if (messagesContainer) {
            const errorDiv = document.createElement('div');
            errorDiv.className = 'ai-message';
            errorDiv.style.borderLeftColor = 'var(--red)';
            errorDiv.innerHTML = \`<strong style="color: var(--red);">Error:</strong><br>\${escapeHtml(error.message)}\`;
            messagesContainer.appendChild(errorDiv);
            messagesContainer.scrollTop = messagesContainer.scrollHeight;
        }
    });
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function clearChat() {
    const messagesContainer = document.getElementById('ai-chat-messages');
    if (messagesContainer) {
        messagesContainer.innerHTML = '<div class="ai-message"><strong style="color: var(--cyan);">BigDaddyG:</strong><br>Chat cleared. Ready to help!</div>';
    }
}

function startVoiceInput() {
    console.log('[Voice] Voice input not yet implemented');
    alert('Voice input coming soon! For now, type your message.');
}
"@
        
        $content = $content + "`n" + $sendToAIFunc
        ColorOutput "  ✓ Added sendToAI and related functions" 'Green'
    }
    
    # Wire up the send button
    if ($content -notmatch 'document\.getElementById\(.*ai-send-btn.*addEventListener') {
        ColorOutput "  ! Adding send button event listener..." 'Yellow'
        
        $buttonWiring = @"

// Wire up send button and enter key
if (typeof document !== 'undefined') {
    document.addEventListener('DOMContentLoaded', function() {
        const sendBtn = document.getElementById('ai-send-btn');
        const inputField = document.getElementById('ai-input');
        
        if (sendBtn) {
            sendBtn.addEventListener('click', sendToAI);
            console.log('[Chat] Send button wired');
        }
        
        if (inputField) {
            inputField.addEventListener('keypress', function(e) {
                if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    sendToAI();
                }
            });
            console.log('[Chat] Input field wired (Enter to send)');
        }
    });
}
"@
        
        $content = $content + "`n" + $buttonWiring
        ColorOutput "  ✓ Wired send button and enter key" 'Green'
    }
    
    Set-Content -Path $rendererPath -Value $content -Encoding UTF8
    ColorOutput "`n✅ JavaScript repairs complete!" 'Green'
}

function Show-RepairMenu {
    ColorOutput "`n╔════════════════════════════════════════╗" 'Cyan'
    ColorOutput "║  BIGDADDYG IDE - UI REPAIR TOOLKIT    ║" 'Cyan'
    ColorOutput "╚════════════════════════════════════════╝`n" 'Cyan'
    
    ColorOutput "Usage: .\IDE-UI-Repair.ps1 [option]`n" 'White'
    
    ColorOutput "Options:" 'Yellow'
    ColorOutput "  -Diagnose     Scan for UI issues" 'White'
    ColorOutput "  -RepairCSS    Fix CSS layout issues" 'White'
    ColorOutput "  -RepairJS     Fix JavaScript functionality" 'White'
    ColorOutput "  -RepairAll    Fix everything (CSS + JS)" 'White'
    ColorOutput "  -BackupFirst  Create backups before changes" 'White'
    
    ColorOutput "`nExamples:" 'Cyan'
    ColorOutput "  .\IDE-UI-Repair.ps1 -Diagnose" 'White'
    ColorOutput "  .\IDE-UI-Repair.ps1 -RepairAll -BackupFirst" 'White'
    ColorOutput "  .\IDE-UI-Repair.ps1 -RepairCSS" 'White'
}

# ============================================================================
# MAIN
# ============================================================================

if (-not $Diagnose -and -not $RepairCSS -and -not $RepairJS -and -not $RepairAll) {
    Show-RepairMenu
    exit 0
}

if ($Diagnose) {
    $ok = Diagnose-UIIssues
    if ($ok) {
        ColorOutput "`n✅ All checks passed!" 'Green'
    }
}

if ($RepairCSS) {
    Repair-ChatInputCSS
}

if ($RepairJS) {
    Repair-ChatInputJS
}

if ($RepairAll) {
    Repair-ChatInputCSS
    Repair-ChatInputJS
}

ColorOutput "`n📝 Next steps:" 'Yellow'
ColorOutput "  1. Close the IDE (Ctrl+Q)" 'White'
ColorOutput "  2. cd '$appPath'" 'White'
ColorOutput "  3. npm start" 'White'
ColorOutput "`nThe chat input should now work properly!`n" 'Green'
