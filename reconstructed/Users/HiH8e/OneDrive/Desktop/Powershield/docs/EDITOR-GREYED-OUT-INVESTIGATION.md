# Text Editor Greyed-Out Issue - Investigation Guide

**Issue**: Text editor pane becomes greyed out/unresponsive when loading files  
**Status**: ❓ Unknown root cause - requires debugging  
**Priority**: Lower (not startup-blocking)  
**Last Seen**: Mentioned in user feedback but not reproducible in test log

---

## Issue Description

The RawrXD IDE's text editor (main editing pane) sometimes becomes:
- Greyed out (visually disabled appearance)
- Unresponsive to input
- Unable to be edited or selected

**When It Occurs**: Appears to happen when files are loaded into the editor  
**When It Doesn't**: Unclear - sometimes works fine  
**Impact**: User cannot edit files until editor is reset/refreshed

---

## Suspected Causes (To Be Investigated)

### 1. ❌ Syntax Highlighting (UNLIKELY - Already Disabled)
**User Note**: "I am pretty sure it isn't BEING USED AT THE MOMENT"  
**Current Status**: If syntax highlighting was the culprit, it should be disabled or not active  
**Investigation**: Check if syntax highlighting is even loaded

**Where to Look**:
```powershell
# RawrXD.ps1 - search for syntax highlighting
grep -i "syntax" RawrXD.ps1
```

### 2. 🔴 WinForms Control Rendering Issue
**Hypothesis**: The RichTextBox or TextBox control rendering is locked after file load

**Symptoms to Check**:
- Is the control's `Enabled` property being set to false?
- Is the control's `ReadOnly` property being set unexpectedly?
- Is there a modal dialog blocking input?

**Code to Inspect**:
```powershell
# Lines 3400-3450: File double-click handler
# Line ~3400: $script:editor.Text = $content
# Check if anything modifies editor properties after this
```

**Test Procedure**:
```powershell
# Create minimal repro: Load a file and check editor state
$script:editor.Enabled    # Should be True
$script:editor.ReadOnly   # Should be False
$script:editor.CanFocus   # Should be True
$script:editor.Focused    # Check if focus is present
```

### 3. 🔴 DevTools Console Integration Issue
**Hypothesis**: IDE's developer console or debugging interface is blocking editor

**Related Code**:
- Lines ~12000+: Developer console initialization
- Possible: Console overlay is capturing focus or mouse events

**Test**:
```powershell
# If DevConsole is active, it might be intercepting input
# Check if closing DevConsole fixes the issue
```

### 4. 🔴 Event Handler Deadlock
**Hypothesis**: An event handler is running and not yielding control back

**Possible Handlers**:
- File content change events
- Text modification events
- UI refresh callbacks

**Code to Check**:
```powershell
# Lines ~3420+: After setting $script:editor.Text = $content
# Are there change event handlers firing?
# Could they be in an infinite loop?
```

### 5. 🔴 Encoding/Decoding Hang
**Hypothesis**: Large file processing is blocking UI thread

**Related Code** (Lines 3400-3420):
```powershell
$content = [System.IO.File]::ReadAllText($filePath)
# This is synchronous - if file is huge, UI might freeze
```

**Test**: Try loading a small file vs large file

### 6. 🔴 Encrypted File Decryption Hang
**Hypothesis**: If file ends with `.secure`, decryption callback hangs

**Related Code** (Lines 3410-3425):
```powershell
if ($extension -eq '.secure') {
    if (Get-Command "Unprotect-SensitiveString" -ErrorAction SilentlyContinue) {
        $content = Unprotect-SensitiveString -EncryptedData $content
    }
}
```

**Test**: Load an encrypted file and check if it hangs

---

## Debugging Strategy

### Step 1: Reproduce the Issue
```powershell
# Run RawrXD.ps1
pwsh -NoProfile -File RawrXD.ps1

# Try these scenarios:
# 1. Load a small .txt file
# 2. Load a large .ps1 file  
# 3. Load a .secure encrypted file
# 4. Load a Python file
# 5. Try editing immediately after load

# Note which scenario makes editor grey out
```

### Step 2: Check Control State When Greyed Out
Open DevTools console and run:
```powershell
# Check editor object properties
$editor.Enabled              # Should be True
$editor.ReadOnly             # Should be False
$editor.BackColor            # Check if it's a disabled grey
$editor.ForeColor            # Check color
$editor.Focus()              # Try to force focus

# Check if parent form is disabled
$form.Enabled                # Should be True

# Check for modal dialogs
[System.Windows.Forms.Form]::ActiveForm  # Should return main form
```

### Step 3: Monitor Event Log
Create a test that logs all TextBox events:
```powershell
# In initialization, add event logging
$editor.Add_TextChanged({
    Write-Host "TextBox changed event fired at $(Get-Date)" -ForegroundColor Gray
})

$editor.Add_SizeChanged({
    Write-Host "TextBox size changed at $(Get-Date)" -ForegroundColor Gray
})

# Try to edit and watch for spurious events
```

### Step 4: Check Thread State
If UI is frozen but responsive:
```powershell
# Could be an infinite loop or hung thread
# Check if runspaces are blocked
$runspaces = [runspacefactory]::RunspacePool
Write-Host "Available runspaces: $($runspaces.AvailableRunspaces)"
```

### Step 5: Isolate File Loading Code
Create a minimal test case:
```powershell
# Minimal RawrXD test - just file loading
Add-Type -AssemblyName System.Windows.Forms

$form = New-Object System.Windows.Forms.Form
$editor = New-Object System.Windows.Forms.RichTextBox
$editor.Dock = [System.Windows.Forms.DockStyle]::Fill
$form.Controls.Add($editor)

# Try loading file
$filePath = "C:\path\to\test.txt"
$editor.Text = [System.IO.File]::ReadAllText($filePath)

Write-Host "Editor enabled: $($editor.Enabled)"
Write-Host "Editor can focus: $($editor.CanFocus)"

$form.ShowDialog()
```

---

## Potential Fixes (Once Root Cause Found)

### If it's WinForms Rendering
```powershell
# Force refresh
$editor.Invalidate()
$editor.Refresh()
```

### If it's Input Focus Issue
```powershell
# Ensure focus is on editor
$editor.Focus()
$editor.Select(0, 0)  # Place cursor at start
```

### If it's Event Handler Deadlock
```powershell
# Temporarily disable event handlers while loading
$editor.SuspendLayout()
$editor.Text = $content
$editor.ResumeLayout()
```

### If it's Synchronous Blocking
```powershell
# Load file asynchronously
$job = Start-Job -ScriptBlock {
    [System.IO.File]::ReadAllText($args[0])
} -ArgumentList $filePath

# Show loading indicator
# Then update editor when ready
$content = Receive-Job -Job $job
$editor.Text = $content
```

---

## Recommended Next Steps

1. **Create Repro**: Get a minimal test case that reliably reproduces the issue
2. **Log Everything**: Add verbose logging to file loading code
3. **Isolate**: Test if it's WinForms issue or RawrXD code issue
4. **Fix**: Once root cause identified, implement fix
5. **Test**: Add test to prevent regression

---

## Related Code Locations in RawrXD.ps1

- **File Double-Click Handler**: Lines 3400-3450
- **File Content Assignment**: Line 3420 (`$script:editor.Text = $content`)
- **Encrypted File Handling**: Lines 3410-3425
- **Editor Initialization**: Search for `RichTextBox` creation
- **DevConsole Integration**: Lines ~12000+

---

## Workarounds for Users

Until root cause is fixed:
1. **Refresh UI**: Click View menu or press F5 to refresh
2. **Switch Tabs**: Click another tab and back
3. **Resize Window**: Drag window edge to force redraw
4. **Reopen Editor**: Close and reopen file
5. **Restart App**: `pwsh -NoProfile -File RawrXD.ps1`

---

## Notes

- This issue is NOT preventing the application from starting
- It's NOT blocking chat, file browsing, or other features
- It appears to be specific to the text editing experience
- May be related to specific file types or sizes
- User mentions DevConsole loads file but "text editor turns greyed"

**Investigation Status**: Requires active debugging session to reproduce and trace.
