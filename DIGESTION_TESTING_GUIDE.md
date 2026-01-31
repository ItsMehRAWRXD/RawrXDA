# Digestion Engine Integration - Testing Guide

## Quick Start Verification

### Test 1: Engine Works (Baseline)
Status: ✅ **VERIFIED WORKING**

Run the test harness:
```powershell
&"d:\lazy init ide\build\bin\Release\digestion_test_harness.exe" `
  "d:\lazy init ide\src\win32app\Win32IDE.cpp" `
  "d:\lazy init ide\build\bin\Release\test_engine.json"
```

Expected Output:
```
[Task 1] Progress: 0%
[Task 1] Progress: 33%
[Task 1] Progress: 66%
[Task 1] Progress: 100%
✓ Digestion completed successfully
✓ Output file created
```

---

## Test 2: IDE Integration via Hotkey

### Prerequisites
1. IDE executable built: `d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe`
2. Test file available: `d:\lazy init ide\src\win32app\Win32IDE.cpp`
3. DebugView installed (optional but recommended)

### Steps

#### Without DebugView (Basic Test)
1. Start IDE:
   ```powershell
   &"d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe"
   ```

2. Open a file:
   - Click: File > Open
   - Navigate to and select: `d:\lazy init ide\src\win32app\Win32IDE.cpp`
   - Click: Open

3. Trigger digestion:
   - Press: **Ctrl+Shift+D**
   - **Observation:** Watch the status bar (bottom of window) for:
     - "Digestion Task X: starting..." message
     - Progress updates
     - "Digestion Task X: SUCCESS" completion message

4. Verify output:
   - Check for file created: `d:\lazy init ide\src\win32app\Win32IDE.cpp.digest`
   - If found, open to verify JSON format

**Expected Timeline:**
- Hotkey press → Immediate: Status bar shows "starting..."
- 1-2 seconds: Progress updates (0% → 33% → 66% → 100%)
- After: Status bar shows "SUCCESS"
- File created: `filename.digest` in same directory as source

#### With DebugView (Detailed Trace)
1. Download DebugView from: https://docs.microsoft.com/en-us/sysinternals/downloads/debugview

2. Run DebugView as Administrator

3. Configure DebugView:
   - Edit > Filter/Highlight
   - Enter: `IDE-Digest`
   - Capture > Capture Win32

4. Start IDE (from DebugView running in background)

5. Open file in IDE (Ctrl+O)

6. Press Ctrl+Shift+D

7. In DebugView, you should see trace like:
   ```
   [IDE-Digest] HOTKEY: Ctrl+Shift+D detected
   [IDE-Digest] Current file: d:\lazy init ide\src\win32app\Win32IDE.cpp
   [IDE-Digest] About to call queueDigestionJob
   [IDE-Digest] queueDigestionJob ENTRY
   [IDE-Digest] DIGESTION_CTX allocated
   [IDE-Digest] Paths copied - Task 1, Source: ...
   [IDE-Digest] SENDING WM_RUN_DIGESTION message
   [IDE-Digest] SendMessageW returned: 0
   [IDE-Digest] queueDigestionJob EXIT SUCCESS
   [IDE-Digest] WM_RUN_DIGESTION handler ENTRY
   [IDE-Digest] Calling _beginthreadex
   [IDE-Digest] Thread created successfully
   [IDE-Digest] Status bar updated
   [IDE-Digest] WM_RUN_DIGESTION handler EXIT SUCCESS
   [IDE-Digest] DigestionThreadProc ENTRY
   [IDE-Digest] Thread started - Task 1, ...
   [IDE-Digest] Calling RawrXD_DigestionEngine_Avx512
   [IDE-Digest] Engine returned: 0
   [IDE-Digest] Posting WM_DIGESTION_COMPLETE
   [IDE-Digest] DigestionThreadProc EXIT
   ```

**If trace shows:**
- `[IDE-Digest] HOTKEY: Ctrl+Shift+D detected` - ✓ Hotkey received
- `queueDigestionJob ENTRY` - ✓ Function called
- `SENDING WM_RUN_DIGESTION message` - ✓ Message queued
- `WM_RUN_DIGESTION handler ENTRY` - ✓ Message processed
- `Calling RawrXD_DigestionEngine_Avx512` - ✓ Engine invoked

**If trace doesn't show:**
- `HOTKEY: Ctrl+Shift+D detected` → Hotkey not reaching IDE (focus issue?)
- `Current file:` → File not open (m_currentFile is empty)
- `ERROR:` → Specific validation failed (see error message)

---

## Test 3: CLI Auto-Digest Mode

```powershell
&"d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe" `
  --auto-digest `
  --src "d:\lazy init ide\src\win32app\Win32IDE.cpp" `
  --out "d:\lazy init ide\build\bin\Release\cli_test_output.json"

# Wait for window to appear and disappear
Start-Sleep -Seconds 10

# Check output
Get-Content "d:\lazy init ide\build\bin\Release\cli_test_output.json" | Format-List
```

Expected: JSON file created with digestion results

---

## Troubleshooting

### Symptom: No response to Ctrl+Shift+D

**Possible Causes:**

1. **File not open**
   - Solution: Press Ctrl+O, select any file
   - Try: Open `d:\lazy init ide\src\win32app\Win32IDE.cpp`

2. **Focus on wrong window**
   - Solution: Click main IDE window area before pressing hotkey
   - Try: Click on editor area, then Ctrl+Shift+D

3. **Hotkey not registered**
   - Solution: Try other hotkeys (Ctrl+O for Open, Ctrl+B for sidebar)
   - If other hotkeys work: Specific issue with Ctrl+Shift+D handler
   - If no hotkeys work: General keyboard input issue

4. **IDE crashed or frozen**
   - Solution: Kill process and restart
   - Check: `Get-Process RawrXD-Win32IDE | Stop-Process -Force`

### Symptom: Status bar shows progress but no output file

**Possible Causes:**

1. **Output path invalid**
   - Check: Is path writable? (Usually `filename.digest` in source dir)
   - Try: Ensure parent directory exists and is writable

2. **Engine returned error**
   - Look for: "Digestion Task X: FAILED" in status bar
   - Check: Source file exists and is readable

3. **Message not reaching IDE**
   - Check: WM_DIGESTION_COMPLETE handler in code
   - Trace: Use DebugView to see message dispatch

### Symptom: DebugView shows no IDE-Digest output

**Possible Causes:**

1. **DebugView not capturing debug output**
   - Solution: Check "Capture Win32" is enabled
   - Try: Run DebugView as Administrator

2. **Filter too restrictive**
   - Solution: Clear filter, then search for "IDE-Digest"
   - Try: Check "Global Win32" option

3. **IDE running on different desktop**
   - Solution: Make sure IDE window is visible
   - Try: Click IDE window to bring to foreground

4. **Debug builds needed**
   - Note: Release builds still emit OutputDebugString
   - Current build: Release mode (sufficient for tracing)

---

## Expected Behaviors

### Success Case: Hotkey works end-to-end
1. Press Ctrl+Shift+D
2. Status bar: "Digestion Task 1: starting..."
3. Status bar updates: 0% → 33% → 66% → 100%
4. Status bar: "Digestion Task 1: SUCCESS"
5. File created: `filename.digest`
6. Execution time: ~1-2 seconds

### Partial Success: Message receives but engine fails
1. Status bar shows progress (0%→100%)
2. Status bar: "Digestion Task 1: FAILED (error code)"
3. No output file created
4. Check: Source file still exists?

### Failure: Hotkey not received
1. Press Ctrl+Shift+D
2. No status bar change
3. No messagebox
4. DebugView trace: No "HOTKEY detected" message
5. Try: Click IDE window, try again
6. Try: Other hotkeys (Ctrl+O) - do they work?

---

## File Locations

| Description | Path |
|-------------|------|
| IDE Executable | `d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe` |
| Test Harness | `d:\lazy init ide\build\bin\Release\digestion_test_harness.exe` |
| Test File | `d:\lazy init ide\src\win32app\Win32IDE.cpp` |
| IDE Log | `C:\RawrXD_IDE.log` |
| Output Digest | `{source-file-path}.digest` |

---

## Build/Rebuild Instructions

If you need to rebuild with latest instrumentation:

```powershell
cd "d:\lazy init ide\build"

# Clean build
Remove-Item CMakeCache.txt

# Configure
cmake .. -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release --target RawrXD-Win32IDE

# Verify
Test-Path "d:\lazy init ide\build\bin\Release\RawrXD-Win32IDE.exe"
```

Expected output: `True` (file exists)

---

## Summary

| Test | Status | Action |
|------|--------|--------|
| Engine (Harness) | ✅ WORKING | Baseline verified |
| IDE Hotkey | ❓ NEEDS TEST | Run Test 2 |
| IDE Integration | ❓ NEEDS TEST | Use DebugView |
| CLI Mode | ❓ NEEDS TEST | Run Test 3 |

**Next Step:** Execute Test 2 (IDE hotkey test) and report findings from DebugView trace.

---

*Documentation generated for: Digestion Engine - Win32IDE Integration*  
*Last updated: 2026-01-24*
