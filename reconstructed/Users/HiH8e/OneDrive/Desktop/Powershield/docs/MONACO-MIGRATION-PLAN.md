# 🚀 Monaco Editor Migration Plan

**Date:** November 28, 2025  
**Goal:** Replace RichTextBox with Monaco Editor in WebView2  
**Status:** 🟡 In Progress

---

## 📋 Migration Overview

### Current State
- **Editor:** `System.Windows.Forms.RichTextBox` (native Windows Forms)
- **Location:** Lines ~4900-5000 in RawrXD.ps1
- **File Operations:** Direct PowerShell → RichTextBox.Text
- **Menu System:** JavaScript (designed for Monaco, not connected)

### Target State
- **Editor:** Monaco Editor in WebView2 control
- **File Operations:** PowerShell ↔ Monaco bridge
- **Menu System:** JavaScript → Monaco (works immediately)
- **Benefits:** IntelliSense, multi-cursor, better syntax highlighting

---

## 🔧 Implementation Steps

### Phase 1: Create Monaco WebView2 Control (2-3 hours)
**Replace RichTextBox with WebView2 containing Monaco**

1. **Create Monaco HTML Template**
   - Standalone HTML file with Monaco Editor
   - PowerShell bridge JavaScript
   - File operations handlers

2. **Replace RichTextBox in Layout**
   - Remove `$script:editor = New-Object RichTextBox`
   - Add `$script:editorWebView = New-WebView2Control`
   - Load Monaco HTML into WebView2

3. **Update Layout References**
   - Change `$leftSplitter.Panel2.Controls.Add($script:editor)`
   - To: `$leftSplitter.Panel2.Controls.Add($script:editorWebView)`

### Phase 2: PowerShell ↔ Monaco Bridge (3-4 hours)
**Set up bidirectional communication**

1. **WebView2 Message Handler**
   - Listen for messages from Monaco
   - Handle: `openFile`, `saveFile`, `setContent`, `getContent`

2. **PowerShell → Monaco Functions**
   - `Set-MonacoContent` - Send text to Monaco
   - `Get-MonacoContent` - Read text from Monaco
   - `Set-MonacoLanguage` - Change syntax highlighting
   - `Set-MonacoTheme` - Change theme

3. **Monaco → PowerShell Events**
   - Text changed events
   - File operations requests
   - Settings changes

### Phase 3: Migrate File Operations (2-3 hours)
**Update all file operations to use Monaco**

1. **File Opening**
   - Change: `$script:editor.Text = $content`
   - To: `Set-MonacoContent -Content $content -Language $language`

2. **File Saving**
   - Change: `$content = $script:editor.Text`
   - To: `$content = Get-MonacoContent`

3. **File Explorer Integration**
   - Update double-click handler
   - Send files to Monaco instead of RichTextBox

### Phase 4: Wire Menu System (1-2 hours)
**Connect JavaScript menu to Monaco**

1. **Menu System Already Works!**
   - RawrXD-MenuBar-System.js designed for Monaco
   - Just needs PowerShell bridge handlers

2. **Add Command Handlers**
   - `Set-Theme` → `Set-MonacoTheme`
   - `Set-FontSize` → `Set-MonacoFontSize`
   - `Save-File` → `Get-MonacoContent` + save
   - `New-File` → `Set-MonacoContent -Content ""`

### Phase 5: Remove RichTextBox Code (1 hour)
**Clean up old code**

1. **Remove RichTextBox References**
   - Remove `$script:editor` variable
   - Remove RichTextBox event handlers
   - Remove RichTextBox-specific functions

2. **Update All References**
   - Find all `$script:editor.` references
   - Replace with Monaco bridge calls

### Phase 6: Testing (2-3 hours)
**Verify everything works**

1. **File Operations**
   - Open file → Monaco displays content
   - Edit in Monaco → Save → File updated
   - Syntax highlighting works

2. **Menu System**
   - All menu commands work
   - Settings apply to Monaco
   - Keyboard shortcuts work

3. **Integration**
   - File explorer → Monaco
   - Chat → Monaco (if needed)
   - Terminal → Monaco (if needed)

---

## 📝 Code Structure

### New Files to Create

1. **`Monaco-Editor.html`**
   - Standalone Monaco Editor HTML
   - PowerShell bridge JavaScript
   - File operations UI

2. **`Monaco-Bridge.ps1`** (functions)
   - `Set-MonacoContent`
   - `Get-MonacoContent`
   - `Set-MonacoLanguage`
   - `Set-MonacoTheme`
   - `Set-MonacoFontSize`
   - `Invoke-MonacoCommand`

### Files to Modify

1. **`RawrXD.ps1`**
   - Replace RichTextBox with WebView2
   - Add Monaco bridge functions
   - Update file operations
   - Remove RichTextBox code

---

## 🎯 Implementation Details

### Monaco HTML Template Structure
```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        body { margin: 0; padding: 0; overflow: hidden; }
        #monaco-container { width: 100%; height: 100vh; }
    </style>
</head>
<body>
    <div id="monaco-container"></div>
    
    <!-- Monaco Editor from CDN -->
    <script src="https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js"></script>
    <script>
        require.config({ paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' } });
        
        let editor;
        
        require(['vs/editor/editor.main'], function() {
            editor = monaco.editor.create(document.getElementById('monaco-container'), {
                value: '',
                language: 'powershell',
                theme: 'vs-dark',
                fontSize: 14,
                automaticLayout: true,
                minimap: { enabled: true },
                wordWrap: 'on',
                lineNumbers: 'on'
            });
            
            // Expose globally
            window.editor = editor;
            window.monaco = monaco;
            
            // PowerShell bridge
            window.chrome.webview.addEventListener('message', (event) => {
                const msg = event.data;
                handlePowerShellMessage(msg);
            });
            
            // Notify PowerShell that Monaco is ready
            window.chrome.webview.postMessage({ type: 'ready' });
        });
        
        function handlePowerShellMessage(msg) {
            switch(msg.command) {
                case 'setContent':
                    editor.setValue(msg.content || '');
                    if (msg.language) {
                        monaco.editor.setModelLanguage(editor.getModel(), msg.language);
                    }
                    break;
                case 'getContent':
                    window.chrome.webview.postMessage({
                        type: 'content',
                        content: editor.getValue()
                    });
                    break;
                case 'setTheme':
                    monaco.editor.setTheme(msg.theme || 'vs-dark');
                    break;
                case 'setLanguage':
                    monaco.editor.setModelLanguage(editor.getModel(), msg.language);
                    break;
            }
        }
        
        // Listen for content changes
        editor?.onDidChangeModelContent(() => {
            window.chrome.webview.postMessage({
                type: 'contentChanged',
                content: editor.getValue()
            });
        });
    </script>
</body>
</html>
```

### PowerShell Bridge Functions
```powershell
function Set-MonacoContent {
    param(
        [string]$Content,
        [string]$Language = 'powershell'
    )
    
    $json = @{
        command = 'setContent'
        content = $Content
        language = $Language
    } | ConvertTo-Json -Compress
    
    $script = "handlePowerShellMessage($json);"
    $script:editorWebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null
}

function Get-MonacoContent {
    # Request content from Monaco
    $script:monacoContent = $null
    $script:monacoContentReceived = $false
    
    $script = "window.chrome.webview.postMessage({ type: 'getContent' });"
    $script:editorWebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null
    
    # Wait for response (with timeout)
    $timeout = 30
    $elapsed = 0
    while (-not $script:monacoContentReceived -and $elapsed -lt $timeout) {
        Start-Sleep -Milliseconds 100
        $elapsed += 0.1
    }
    
    return $script:monacoContent
}
```

---

## ⚠️ Challenges & Solutions

### Challenge 1: Async Communication
**Problem:** WebView2 messages are async, need to wait for responses

**Solution:** Use callback pattern with timeout
```powershell
$script:monacoCallbacks = @{}
$callbackId = [Guid]::NewGuid().ToString()
$script:monacoCallbacks[$callbackId] = $null

# Send message with callback ID
# Wait for response with matching callback ID
```

### Challenge 2: File Explorer Integration
**Problem:** File explorer currently sends to RichTextBox

**Solution:** Update double-click handler
```powershell
# Old:
$script:editor.Text = $content

# New:
Set-MonacoContent -Content $content -Language (Get-FileLanguage $filePath)
```

### Challenge 3: Settings Application
**Problem:** Settings need to apply to Monaco, not RichTextBox

**Solution:** Add Monaco-specific setting handlers
```powershell
function Apply-EditorSettings {
    # Get settings
    $settings = Get-EditorSettings
    
    # Apply to Monaco
    Set-MonacoTheme -Theme $settings.Theme
    Set-MonacoFontSize -Size $settings.FontSize
    Set-MonacoFontFamily -Family $settings.FontFamily
}
```

---

## 📊 Progress Tracking

- [ ] Phase 1: Create Monaco WebView2 Control
- [ ] Phase 2: PowerShell ↔ Monaco Bridge
- [ ] Phase 3: Migrate File Operations
- [ ] Phase 4: Wire Menu System
- [ ] Phase 5: Remove RichTextBox Code
- [ ] Phase 6: Testing

---

## 🎯 Success Criteria

✅ Monaco Editor loads and displays  
✅ File open/save works  
✅ Syntax highlighting works  
✅ Menu system commands work  
✅ Settings apply to Monaco  
✅ File explorer opens files in Monaco  
✅ No RichTextBox code remains  

---

**Estimated Total Time:** 12-16 hours (2-3 work days)

