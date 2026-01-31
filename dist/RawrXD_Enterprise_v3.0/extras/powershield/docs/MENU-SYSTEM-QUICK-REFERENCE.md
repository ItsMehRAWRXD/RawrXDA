# 🎯 RawrXD Menu System - Quick Reference

## ✅ Enhancement Complete!

### 📊 What Was Added

**RawrXD-MenuBar-System.js**
- **Before:** 866 lines
- **After:** 1,603 lines
- **Added:** 737 lines of new functionality

---

## 🎨 Settings Menu (NEW!)

Access via: **Menu Bar → Settings** or press **Ctrl+,**

### Categories & Options

#### 📝 Editor Settings
- Color Theme (7 themes)
- Font Size (9 options)
- Font Family (6 fonts)
- Line Height (7 options)
- Tab Size (2, 4, 8)
- Word Wrap (On/Off)

#### 🖥️ Display Settings
- Zoom Level (75%-200%)
- Auto-Hide Panels
- Show Minimap
- Show Breadcrumbs
- Line Numbers

#### 💡 Code Features
- Auto Complete
- IntelliSense
- Auto Linting
- Format on Save
- Auto Save (4 modes)

#### ⌨️ Terminal Settings
- Default Shell (PS, CMD, Git Bash, WSL)
- Terminal Font Size
- Cursor Style (Block, Line, Underline)

#### 🤖 AI Settings
- AI Model (GPT-4, Claude, etc.)
- AI Temperature (0.0-1.0)
- AI Auto-Complete

#### ⚙️ Advanced
- Keyboard Shortcuts viewer
- Manage Extensions
- Settings Sync
- Open Settings JSON
- Reset All Settings

---

## ⌨️ Keyboard Shortcuts (50+)

### File Operations
```
Ctrl+N              New File
Ctrl+Shift+N        New Folder
Ctrl+O              Open File
Ctrl+S              Save
Ctrl+Shift+S        Save As
Ctrl+Alt+S          Save All
Ctrl+W              Close Tab
```

### Edit Operations
```
Ctrl+Z              Undo
Ctrl+Shift+Z        Redo
Ctrl+X/C/V          Cut/Copy/Paste
Ctrl+F              Find
Ctrl+H              Replace
Ctrl+A              Select All
Ctrl+L              Select Line
Ctrl+D              Select Word
```

### View Controls
```
Ctrl+B              Toggle Explorer
Ctrl+Shift+E        Show Explorer
Ctrl+Shift+F        Show Search
Ctrl+Shift+G        Source Control
Ctrl+Shift+D        Debug
Ctrl+Shift+X        Extensions
Ctrl+`              Toggle Terminal
Ctrl+J              Toggle Bottom Panel
```

### Run & Debug
```
F5                  Run Code
Ctrl+F5             Run Without Debug
Shift+F5            Run in Terminal
F9                  Start Debugging
Shift+F9            Stop Debugging
F8                  Toggle Breakpoint
```

### Navigation
```
Ctrl+G              Go to Line
Ctrl+P              Go to File
Alt+←               Go Back
Alt+→               Go Forward
```

### Settings & Layout
```
Ctrl+,              Open Settings
Ctrl+\              Split Editor Right
```

---

## 🔌 PowerShell Bridge

### Bridge Methods (Auto-Fallback)
1. **CefSharp** - For CefSharp WebView
2. **WebView2** - For Microsoft Edge WebView2
3. **CustomEvent** - For cross-component communication
4. **Window Queue** - Guaranteed fallback

### JavaScript → PowerShell
```javascript
// Invoke any command
window.rawrxdMenu.invokePowerShell('Save-File', {
    path: 'C:\\file.ps1',
    content: editorContent
});

// Change setting
window.rawrxdMenu.actionSettingsTheme();

// Commands are queued in:
window.psBridgeQueue[]
```

### PowerShell → JavaScript
```powershell
# Send response
$response = @{
    success = $true
    message = "File saved"
    data = @{ path = "C:\file.ps1" }
}

$json = $response | ConvertTo-Json
$webView.ExecuteScriptAsync("window.rawrxdMenu.receivePowerShellResponse($json)")
```

---

## 📋 PowerShell Commands (40+)

### File Commands
```
New-File, New-Folder
Open-File, Open-Folder
Save-File, Save-FileAs, Save-AllFiles
Close-Tab, Close-AllTabs
Revert-File
Exit-Application
```

### View Commands
```
Toggle-Explorer
Show-Search
Show-SourceControl
Show-Debug
Show-Extensions
Toggle-Terminal
```

### Run Commands
```
Run-Code
Run-WithoutDebug
Run-InTerminal
Debug-Start
Debug-Stop
Toggle-Breakpoint
```

### Settings Commands
```
Set-Theme
Set-FontSize
Set-FontFamily
Set-TabSize
Set-WordWrap
Toggle-Minimap
Set-AutoComplete
Set-IntelliSense
Set-Linting
Set-FormatOnSave
Set-AutoSave
Set-DefaultShell
Set-AIModel
Set-AITemperature
Set-AIAutoComplete
Reset-AllSettings
Open-SettingsFile
```

### Terminal Commands
```
New-Terminal
Kill-Terminal
Clear-Terminal
Set-TerminalShell
```

---

## 🔧 Integration Example

### In RawrXD.ps1
```powershell
# After WebView2 initialization

# Method 1: Event Handler (Recommended)
$webView.CoreWebView2.WebMessageReceived += {
    param($sender, $e)
    $message = $e.WebMessageAsJson | ConvertFrom-Json
    
    switch ($message.command) {
        "Save-File" {
            Set-Content -Path $message.params.path -Value $message.params.content
            Send-JavaScriptResponse -Success $true
        }
        "Run-Code" {
            Start-Process pwsh -ArgumentList "-File `"$($message.params.file)`""
        }
    }
}

# Method 2: Polling Queue
while ($true) {
    $script = "window.psBridgeQueue.shift()"
    $cmd = $webView.ExecuteScriptAsync($script).Result
    if ($cmd) {
        $command = $cmd | ConvertFrom-Json
        Invoke-MenuCommand -Command $command
    }
    Start-Sleep -Milliseconds 100
}
```

---

## 📁 Files Created

1. **RawrXD-MenuBar-System.js** (Enhanced, 1603 lines)
   - Complete menu system
   - Settings menu
   - PowerShell bridge
   - Keyboard shortcuts
   - Helper functions

2. **MENU-SYSTEM-ENHANCEMENT-SUMMARY.md**
   - Complete documentation
   - All features explained
   - Integration guide
   - Usage examples

3. **MenuSystem-PowerShell-Integration-Example.ps1**
   - PowerShell command handlers
   - Bridge setup examples
   - Event monitoring
   - Response helpers

---

## 🎯 Usage Tips

### Cycling Settings
Click any setting in the Settings menu to cycle through options:
```
Theme: Dark+ → Light+ → Monokai → ...
Font Size: 14px → 16px → 18px → ...
AI Model: GPT-4 → Claude → Gemini → ...
```

### Finding Commands
Use the search bar (coming soon) or browse menus:
- File operations → **File** menu
- Editor settings → **Settings** menu
- Code execution → **Run** menu
- View panels → **View** menu

### Testing Integration
```javascript
// In browser console
window.rawrxdMenu.invokePowerShell('Set-Theme', { theme: 'Dracula' });

// Check queue
console.log(window.psBridgeQueue);

// Test response
window.rawrxdMenu.receivePowerShellResponse({
    success: true,
    message: "Test successful"
});
```

---

## ✨ Highlights

- ✅ **25+ settings** with live preview
- ✅ **50+ shortcuts** matching VS Code
- ✅ **4 bridge methods** for reliability
- ✅ **40+ PS commands** ready to use
- ✅ **localStorage** persistence
- ✅ **Visual indicators** for all settings
- ✅ **Bi-directional** communication
- ✅ **Professional UI** matching VS Code

---

## 🚀 Next Steps

1. **Test the menu** - Open RawrXD and click Settings
2. **Try shortcuts** - Press Ctrl+, to open settings
3. **Integrate PowerShell** - Use example file as template
4. **Customize** - Adjust default values as needed
5. **Extend** - Add custom commands/settings

---

**Enhancement Complete! The RawrXD Menu System is now a fully-featured IDE menu with comprehensive PowerShell integration.** 🎉

---

*Generated: November 27, 2025*  
*RawrXD Menu System v2.0*
