# 🚀 Monaco Migration - Step-by-Step Guide

**Status:** Ready to implement  
**Estimated Time:** 2-3 hours

---

## Step 1: Load Monaco Bridge Functions

**Location:** Near the top of RawrXD.ps1 (after module loading)

**Add:**
```powershell
# Load Monaco Editor Bridge Functions
$monacoBridgePath = Join-Path $PSScriptRoot "Monaco-Bridge.ps1"
if (Test-Path $monacoBridgePath) {
    . $monacoBridgePath
    Write-StartupLog "✅ Monaco Bridge functions loaded" "SUCCESS"
}
else {
    Write-StartupLog "⚠️ Monaco-Bridge.ps1 not found - Monaco features unavailable" "WARNING"
}
```

---

## Step 2: Replace RichTextBox with Monaco WebView2

**Location:** Line ~4970 in RawrXD.ps1

**REPLACE THIS:**
```powershell
# Text Editor (Middle Pane)
$script:editor = New-Object System.Windows.Forms.RichTextBox
$script:editor.Dock = [System.Windows.Forms.DockStyle]::Fill
$script:editor.Font = New-Object System.Drawing.Font("Consolas", 10)
# ... (all RichTextBox setup code) ...
$leftSplitter.Panel2.Controls.Add($script:editor) | Out-Null
```

**WITH THIS:**
```powershell
# Text Editor (Middle Pane) - Monaco Editor via WebView2
Write-StartupLog "Creating Monaco Editor..." "INFO"

# Create container panel for Monaco
$monacoContainer = New-Object System.Windows.Forms.Panel
$monacoContainer.Dock = [System.Windows.Forms.DockStyle]::Fill
$leftSplitter.Panel2.Controls.Add($monacoContainer) | Out-Null

# Initialize Monaco Editor
$monacoHost = Initialize-MonacoEditor -ParentControl $monacoContainer
if ($monacoHost) {
    Write-StartupLog "✅ Monaco Editor initialized successfully" "SUCCESS"
    # Keep reference for compatibility
    $script:editor = $monacoHost  # For code that checks if editor exists
}
else {
    Write-StartupLog "❌ Failed to initialize Monaco Editor" "ERROR"
    # Fallback to RichTextBox if Monaco fails
    $script:editor = New-Object System.Windows.Forms.RichTextBox
    $script:editor.Dock = [System.Windows.Forms.DockStyle]::Fill
    $script:editor.Font = New-Object System.Drawing.Font("Consolas", 10)
    $script:editor.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
    $script:editor.ForeColor = [System.Drawing.Color]::White
    $monacoContainer.Controls.Add($script:editor) | Out-Null
}
```

---

## Step 3: Update File Opening Code

**Location:** Line ~4775 in RawrXD.ps1 (file explorer double-click handler)

**REPLACE THIS:**
```powershell
# Assign to editor (this is where the magic happens)
if ($script:editor) {
    $script:editor.Text = $content
    $global:currentFile = $filePath
    $form.Text = "RawrXD - AI Editor - $([System.IO.Path]::GetFileName($filePath))"
    Write-DevConsole "🎉 File opened successfully in editor!" "SUCCESS"
    # ...
}
```

**WITH THIS:**
```powershell
# Assign to editor (Monaco or RichTextBox fallback)
if ($script:MonacoEditor.IsReady) {
    # Use Monaco Editor
    $language = Get-FileLanguage -FilePath $filePath
    Set-MonacoContent -Content $content -Language $language
    $script:MonacoEditor.CurrentFile = $filePath
    $global:currentFile = $filePath
    $form.Text = "RawrXD - AI Editor - $([System.IO.Path]::GetFileName($filePath))"
    Write-DevConsole "🎉 File opened successfully in Monaco Editor!" "SUCCESS"
}
elseif ($script:editor -and $script:editor.GetType().Name -eq "RichTextBox") {
    # Fallback to RichTextBox
    $script:editor.Text = $content
    $global:currentFile = $filePath
    $form.Text = "RawrXD - AI Editor - $([System.IO.Path]::GetFileName($filePath))"
    Write-DevConsole "🎉 File opened successfully in editor!" "SUCCESS"
}
else {
    Write-DevConsole "❌ Editor not initialized!" "ERROR"
}

# Update last activity if session exists
if ($script:CurrentSession) {
    $script:CurrentSession.LastActivity = Get-Date
}

# Optional: Add to recent files
if ($script:RecentFiles -and $script:RecentFiles.Count -lt 10) {
    if ($filePath -notin $script:RecentFiles) {
        $script:RecentFiles.Add($filePath)
    }
}
```

---

## Step 4: Update File Saving Code

**Find:** `Save-CurrentFile` function or similar

**REPLACE:**
```powershell
$content = $script:editor.Text
```

**WITH:**
```powershell
if ($script:MonacoEditor.IsReady) {
    $content = Get-MonacoContent
}
elseif ($script:editor -and $script:editor.GetType().Name -eq "RichTextBox") {
    $content = $script:editor.Text
}
else {
    $content = ""
}
```

---

## Step 5: Update All Other `$script:editor.Text` References

**Find and replace all occurrences:**

**Pattern 1:** `$script:editor.Text =`
**Replace with:**
```powershell
if ($script:MonacoEditor.IsReady) {
    Set-MonacoContent -Content <value>
}
elseif ($script:editor -and $script:editor.GetType().Name -eq "RichTextBox") {
    $script:editor.Text = <value>
}
```

**Pattern 2:** `$script:editor.Text` (reading)
**Replace with:**
```powershell
if ($script:MonacoEditor.IsReady) {
    Get-MonacoContent
}
elseif ($script:editor -and $script:editor.GetType().Name -eq "RichTextBox") {
    $script:editor.Text
}
else {
    ""
}
```

**Locations to update:**
- Line 1373, 1377: Find/Replace operations
- Line 4889: File opening
- Line 7697, 7710: Undo/Redo
- Line 8303: Pop-out editor
- Line 8401: Content setting
- Line 9303: AI content
- Line 9453: Content setting

---

## Step 6: Update Undo/Redo (Optional - Monaco has built-in)

**Location:** Lines ~7697, 7710

**Monaco has built-in undo/redo, so you can:**
1. Remove the undo/redo stack code
2. Or keep it for RichTextBox fallback
3. For Monaco, use: `Invoke-MonacoCommand -Command 'undo'` or `'redo'`

---

## Step 7: Test

1. **Start RawrXD.ps1**
2. **Check console:** Should see "✅ Monaco Editor initialized"
3. **Open a file:** Should load in Monaco
4. **Edit text:** Should work smoothly
5. **Save file:** Should save correctly
6. **Check syntax highlighting:** Should work automatically

---

## Step 8: Wire Menu System (Already Works!)

The menu system (RawrXD-MenuBar-System.js) is already designed for Monaco, so it should work immediately once Monaco is loaded!

**Just ensure:**
- Monaco HTML loads RawrXD-MenuBar-System.js
- Or inject it via PowerShell after Monaco loads

---

## ⚠️ Important Notes

1. **Keep RichTextBox as fallback** - In case Monaco fails to load
2. **Test thoroughly** - File operations are critical
3. **Backup RawrXD.ps1** - Before making changes
4. **Incremental testing** - Test after each step

---

## 🎯 Success Criteria

✅ Monaco Editor loads and displays  
✅ Files open in Monaco  
✅ Files save from Monaco  
✅ Syntax highlighting works  
✅ Menu system works  
✅ No errors in console  

---

**Ready to implement?** Start with Step 1 and work through each step!

