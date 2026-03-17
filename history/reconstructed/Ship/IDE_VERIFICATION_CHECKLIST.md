# RawrXD Win32 IDE - Runtime Verification Checklist

## What You Should See When Running RawrXD_Win32_IDE.exe

### On Startup (should appear immediately):

**WINDOW LAYOUT:**
- [ ] Main window titled "RawrXD - Text Editor"
- [ ] Window is visible with menu bar at top
- [ ] Window is not minimized or hidden

**MENU BAR (top of window):**
- [ ] File menu visible
- [ ] Edit menu visible  
- [ ] View menu visible
- [ ] Tools menu visible

**LEFT SIDE (File Tree Panel):**
- [ ] File tree explorer visible on LEFT side
- [ ] Shows directory structure ("C:\", "D:\", etc.)
- [ ] Can expand/collapse folders
- If NOT visible → **ISSUE #1: File Tree Hidden**

**TOP MIDDLE (Editor Pane):**
- [ ] Blank text editor visible with white/light background
- [ ] Tab bar showing "(New File)" or similar
- [ ] Line numbers on left side of editor
- [ ] Status bar shows "Line 1 Column 1"
- If NOT visible → **ISSUE #2: Editor Not Showing**

**BOTTOM LEFT (PowerShell Terminal):**
- [ ] Terminal pane visible at BOTTOM LEFT
- [ ] Dark background (#0C0C0C or similar)
- [ ] Text visible and readable
- [ ] Shows "PowerShell Terminal" header or PS prompt
- [ ] White/light gray text on dark background
- If NOT visible or BLACK TEXT → **ISSUE #3: Terminal Hidden or Unreadable**

**BOTTOM RIGHT (Output Panel):**
- [ ] Output panel visible next to terminal
- [ ] Shows startup messages like:
  ```
  ========================================
    RawrXD Text Editor - Win32 Edition
  ========================================
  
  ACTUAL FEATURES (Honest Inventory):
    ✓ Multi-tab text editor
    ✓ File tree explorer
    ✓ Integrated PowerShell terminal
    ✓ Output panel with messages
    ...
  ```
- If NOT visible → **ISSUE #4: Output Panel Hidden**

**STATUS BAR (bottom of window):**
- [ ] Shows file info and cursor position
- [ ] Black text on gray background

---

## Verification Script

When the IDE loads, verify in this order:

### 1. FILE TREE CHECK
```
Action: Look at LEFT side of window
Expected: See folder/file tree
Report: "File tree IS visible" OR "File tree NOT visible"
```

### 2. EDITOR CHECK
```
Action: Type something in the text editor pane (top middle)
Expected: Text appears as you type
Report: "Can type in editor" OR "Cannot type/Editor not responding"
```

### 3. TERMINAL CHECK
```
Action: Look at BOTTOM LEFT pane
Expected: See dark box with "PowerShell Terminal" or "PS>"
Report: "Terminal visible with readable text" OR "Terminal black/unreadable" OR "Terminal missing"

Sub-check: Type 'dir' and press Enter in terminal
Expected: See directory listing output
Report: "Terminal responding to commands" OR "Terminal not responding"
```

### 4. OUTPUT CHECK
```
Action: Look at BOTTOM RIGHT pane (or bottom center)
Expected: See startup messages with feature list
Report: "Output panel shows startup messages" OR "Output panel empty/missing"
```

### 5. MENU CHECK
```
Action: Click File menu
Expected: Dropdown menu appears with options (New, Open, Save, etc.)
Report: "Menus functional" OR "Menus not working"
```

---

## Reports to Give Back

After checking, tell me:

1. **FILE TREE:** ☐ Visible  ☐ Hidden
2. **EDITOR:** ☐ Visible & Editable  ☐ Hidden  ☐ Visible but not responsive
3. **TERMINAL:** ☐ Visible & Readable  ☐ Hidden  ☐ Visible but text unreadable (black-on-black)
4. **OUTPUT:** ☐ Visible with messages  ☐ Hidden  ☐ Visible but empty
5. **MENUS:** ☐ Working  ☐ Not responding
6. **TEXT COLOR:** ☐ White text visible  ☐ Black or invisible text

---

## Known Issues to Watch For

**If you see:**
- Output says "No AI DLLs found - AI features disabled" → **EXPECTED** (DLLs don't exist)
- Terminal showing black text on dark background → **ISSUE #3** needs fixing
- File tree panel completely missing → **ISSUE #1** needs fixing
- Only 1 terminal pane instead of 2 (PowerShell + MASM CLI split) → **EXPECTED in current build**

---

## What "Fully Working" Looks Like

```
┌─────────────────────────────────────────────┐
│ RawrXD - Text Editor                   [_][□][X] │
├──────┬──────────────────────────┬──────────┤
│      │ (New File)        [x]    │          │
│ FILE │ 1 [cursor here]           │ OUTPUT   │
│ TREE │                           │ PANEL    │
│      │                           │          │
│      ├───────────────────────────┤          │
│      │ TERMINAL PANE             │ VISIBLE? │
│      │ PS D:\rawrxd\Ship>        │ Check!   │
├──────┴───────────────────────────┴──────────┤
│ Line: 1  Column: 1     RawrXD Win32 Ready   │
└──────────────────────────────────────────────┘
```

---

## Next Steps After Verification

Once you report what's visible/hidden:
1. I'll identify which specific UI fixes are needed
2. I'll make targeted code changes to make panels visible
3. I'll fix terminal text color if needed
4. I'll verify the changes by reviewing the code

**Important:** The current executable (RawrXD_Win32_IDE.exe from 8:15 PM Feb 16) was built BEFORE my recent header changes. It may still have old behavior. We may need to recompile after fixing to see the updated messages.
