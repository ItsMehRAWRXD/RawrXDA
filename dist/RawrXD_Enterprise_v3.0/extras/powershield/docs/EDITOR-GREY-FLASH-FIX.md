# Editor Grey/Black Background Flash - FIXED

## Problem
When typing in the editor, the background would flash grey and black, making it difficult to work.

## Root Cause
The issue was in two places:

### 1. **TextChanged Event Handler** (Line 7392)
The handler was calling `SelectAll()` implicitly through the SelectionColor setting, which caused the entire editor background to repaint and flash.

```powershell
# OLD CODE - CAUSED VISUAL GLITCH:
$script:editor.SelectionColor = $visibleColor  # This was selecting all text!
```

### 2. **Apply-SyntaxHighlighting Function** (Line 24479)
The function started with:
```powershell
$Editor.SelectAll()
$Editor.SelectionColor = $textColor
```

This reset ALL text color on every syntax highlighting pass, causing a massive visual repaint that appeared as a grey/black flash.

## Solution

### Fix #1: Don't Select All Text on Every Keystroke
Modified the TextChanged handler to only set color at the current cursor position:

```powershell
# NEW CODE - NO VISUAL GLITCH:
$editor.Select($selStart, 0)  # Select nothing, just position cursor
$script:editor.SelectionColor = $visibleColor  # Set color for new text only
```

### Fix #2: Remove SelectAll() from Syntax Highlighting
Removed the `SelectAll()` call and just ensure ForeColor is set:

```powershell
# Instead of SelectAll():
$Editor.ForeColor = $textColor  # Set default, only highlight specific tokens
```

### Fix #3: Fix Timer Reference Closure Bug
Fixed variable scope issue in the syntax highlight timer:

```powershell
# Use script-scoped reference instead of local closure
$localTimer = $script:syntaxHighlightTimer
```

## Impact
- ✅ Editor no longer flashes grey/black when typing
- ✅ Text input remains smooth and responsive
- ✅ Syntax highlighting still works but doesn't cause visual artifacts
- ✅ Timer cleanup is more reliable

## Testing
1. Open RawrXD in Windows PowerShell 5.1:
   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& 'C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1'"
   ```

2. Type in the editor - background should remain stable dark grey (#1e1e1e)

3. Open a file and edit it - text should be visible with no grey/black flashing

## Files Modified
- `C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1`
  - Lines 7392-7440: TextChanged event handler
  - Lines 24520-24570: Apply-SyntaxHighlighting function start
  - Lines 7468-7480: Timer reference closure
