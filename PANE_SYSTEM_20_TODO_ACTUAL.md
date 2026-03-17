# RawrXD IDE Pane System - ACTUAL 20-TODO List

## Status: What's DECLARED but NOT IMPLEMENTED

This is the REAL problem. These panes are DECLARED as HWNDs in the header but have **NO creation code**.

---

## CRITICAL MISSING PANES (10 items)

### 1. ❌ Problems Panel NOT CREATED
- **Status**: DECLARED as `m_hwndProblemsPanel` but NO `createProblemsPanel()` method exists
- **Issue**: User can't see build errors/warnings in a dedicated panel
- **Fix Needed**: Implement `createProblemsPanel()` with TreeView for error list
- **File**: Win32IDE.cpp (need to add ~50 lines)
- **Dependencies**: Error output from build system → populate tree with severity colors

### 2. ❌ Module Browser NOT CREATED  
- **Status**: DECLARED as `m_hwndModuleBrowser` + `m_hwndModuleList` but NOT initialized in onCreate()
- **Issue**: View > Module Browser menu exists but does nothing
- **Current State**: HWNDs declared, but `createModuleBrowser()` never called
- **Fix Needed**: Call `createModuleBrowser()` in onCreate(), verify implementation works
- **File**: Win32IDE.cpp line ~2650 (onCreate section)
- **Dependencies**: PowerShell module loading integration

### 3. ❌ Search Panel NOT CREATED
- **Status**: DECLARED as `m_hwndSearchInput`, `m_hwndSearchResults`, etc. but no creation code
- **Issue**: Ctrl+F opens a dialog, but no integrated search panel in sidebar
- **Fix Needed**: Implement `createSearchPanel()` - add to sidebar as view
- **File**: Win32IDE.cpp (new method ~80 lines)
- **Expected**: Integrated find-in-folder with file results tree

### 4. ❌ Source Control View NOT CREATED
- **Status**: DECLARED as `m_hwndSCMFileList`, `m_hwndSCMToolbar` but never initialized
- **Issue**: Git Panel (Ctrl+Shift+G) shows git status text but no actual SCM view
- **Current**: `showGitPanel()` exists but only shows status in messagebox
- **Fix Needed**: Create proper Git changes list UI in sidebar or panel
- **File**: Win32IDE.cpp (refactor Git to use proper SCM view)
- **Dependencies**: Git integration with file change tracking

### 5. ❌ Debug View NOT CREATED
- **Status**: DECLARED as `m_hwndDebugConfigs`, `m_hwndDebugVariables`, `m_hwndDebugCallStack`
- **Issue**: Debug menu exists but no debug view in sidebar
- **Current**: `createDebuggerUI()` called but may not properly populate sidebar
- **Fix Needed**: Verify debugger panel integration with breakpoints/watch/variables
- **File**: Win32IDE_Debugger.cpp (check implementation, likely incomplete)
- **Dependencies**: Debugger state from Win32TerminalManager

### 6. ❌ Extensions View NOT CREATED
- **Status**: DECLARED as `m_hwndExtensionsList`, `m_hwndExtensionSearch` but no creation code
- **Issue**: No extensions management UI visible
- **Fix Needed**: Implement `createExtensionsPanel()` - list installed extensions
- **File**: Win32IDE.cpp (new method ~60 lines)
- **Expected**: Extension browser in sidebar with install/uninstall

### 7. ❌ Floating Panel NOT CREATED
- **Status**: Menu option exists (IDM_VIEW_FLOATING_PANEL) but `toggleFloatingPanel()` not working
- **Issue**: User can't create detachable panels
- **Current**: Click View > Floating Panel → does nothing
- **Fix Needed**: Implement floating window creation for any pane
- **File**: Win32IDE.cpp (implement toggleFloatingPanel() ~40 lines)
- **Dependencies**: Parent/child window management for floating

### 8. ❌ Command Palette NOT FULLY WORKING
- **Status**: DECLARED as `m_hwndCommandPalette` but Ctrl+Shift+P not properly wired
- **Issue**: Command palette exists but may not populate actual commands
- **Current**: `showCommandPalette()` exists but unclear if it shows real commands
- **Fix Needed**: Populate command palette with actual menu/action commands
- **File**: Win32IDE.cpp (verify `showCommandPalette()` implementation)
- **Expected**: Searchable command list like VS Code

### 9. ❌ Activity Bar Icon Layout NOT WORKING
- **Status**: `m_hwndActivityBar` created but no icons/buttons on it
- **Issue**: Sidebar toggles but activity bar (far left) doesn't show clickable icons
- **Current**: Static HWND created but no child buttons for Explorer/Search/Git/etc.
- **Fix Needed**: Add icon buttons to activity bar to switch sidebar views
- **File**: Win32IDE.cpp (add button creation to createSidebar() ~30 lines)
- **Expected**: Icon bar with 5-6 view switcher buttons (Explorer, Search, Git, Debug, Extensions)

### 10. ❌ Sidebar View Switching NOT WORKING
- **Status**: `m_currentSidebarView` enum exists with 6 values (Explorer/Search/SCM/Debug/Extensions/None)
- **Issue**: Sidebar always shows Explorer, can't switch to other views
- **Current**: No `setSidebarView()` implementation that actually swaps content
- **Fix Needed**: Implement `setSidebarView(SidebarView view)` to show/hide appropriate controls
- **File**: Win32IDE.cpp (implement setSidebarView() ~50 lines)
- **Expected**: Clicking activity bar buttons switches sidebar between Explorer, Search, Git, Debug, Extensions

---

## IMPLEMENTATION WITH ACTUAL FUNCTION STUBS (10 items)

### 11. ❌ `createProblemsPanel()` - STUB/INCOMPLETE
- **Status**: Not found in codebase
- **Parameters**: None (uses m_hwndMain as parent)
- **Returns**: void
- **Creates**: TreeView for problems with severity icons (Error=red, Warning=yellow, Info=blue)

### 12. ❌ `createSearchPanel()` - STUB/INCOMPLETE  
- **Status**: Not found in codebase
- **Parameters**: None
- **Returns**: void
- **Creates**: Edit control for search input + ListBox for results

### 13. ❌ `setSidebarView(SidebarView view)` - STUB/INCOMPLETE
- **Status**: Not found in codebase - CRITICAL for sidebar functionality
- **Parameters**: `SidebarView view` (enum: Explorer, Search, SCM, Debug, Extensions)
- **Returns**: void
- **Behavior**: Should ShowWindow() content for selected view, HideWindow() others

### 14. ❌ `toggleFloatingPanel()` - DECLARED but BROKEN
- **Status**: Method declared in header but implementation may be empty (not found in grep)
- **Current**: Menu click IDM_VIEW_FLOATING_PANEL → does nothing
- **Fix Needed**: Implement to create detachable child window

### 15. ❌ `showCommandPalette()` - DECLARED but may be INCOMPLETE
- **Status**: Method exists but command population may be missing
- **Current**: Creates window but unclear if it shows commands
- **Fix Needed**: Populate all menu IDs into command list

### 16. ❌ `resizeSidebar()` - CALLED but may be INCOMPLETE
- **Status**: Called from onSize() but verify its implementation
- **Called From**: [Win32IDE.cpp](Win32IDE.cpp#L2590) onSize() line 2590
- **Should**: Resize Explorer tree + other sidebar content controls

### 17. ❌ `onFileTreeExpand()` - INCOMPLETE
- **Status**: Called when file tree expands but unclear if file loading works
- **File**: Need to verify implementation in createFileExplorer() section

### 18. ❌ `loadModelFromPath()` - INCOMPLETE
- **Status**: Called on double-click in file tree but may not work
- **Issue**: Model loading may fail silently

### 19. ❌ Git Panel Content NOT SHOWING
- **Status**: `showGitPanel()` exists but only shows messagebox, not actual panel
- **Current**: Should populate `m_hwndGitPanel` with file list
- **Fix Needed**: Implement git file list in `m_hwndGitPanel` UI

### 20. ❌ Pane Resizing/Docking NOT WORKING
- **Status**: Panes fixed in place, can't be moved/docked/resized
- **Issue**: Only splitter between terminal/output works
- **Missing**: Drag-to-dock, resize constraints, floating window support
- **Fix Needed**: Implement DockManager or similar for pane layout

---

## SUMMARY: WHAT'S ACTUALLY BROKEN RIGHT NOW

| Pane | Status | Can See | Can Use |
|------|--------|---------|---------|
| **Editor** | ✅ WORKS | YES | YES |
| **Terminal** (split H/V) | ✅ WORKS | YES | YES |
| **Output Tabs** (4 tabs) | ✅ WORKS | YES | YES |
| **File Explorer** | ⚠️ PARTIAL | YES | Partially (tree may not populate) |
| **Minimap** | ✅ WORKS | YES | YES |
| **Sidebar** | ⚠️ PARTIAL | YES (always Explorer) | NO (can't switch views) |
| **Status Bar** | ✅ WORKS | YES | YES |
| **Activity Bar** | ❌ BROKEN | NO (no icons) | NO |
| **Problems Panel** | ❌ MISSING | NO | NO |
| **Module Browser** | ⚠️ DECLARED | NO | NO |
| **Search Panel** | ❌ MISSING | NO | NO |
| **Git Panel** | ⚠️ BROKEN | Messagebox only | NO (no panel UI) |
| **Debug View** | ❌ MISSING | NO | NO |
| **Extensions View** | ❌ MISSING | NO | NO |
| **Command Palette** | ⚠️ INCOMPLETE | Maybe | Unclear |
| **Floating Panels** | ❌ BROKEN | NO | NO |

---

## FILES TO MODIFY

1. **d:\rawrxd\src\win32app\Win32IDE.h** - Already has declarations, no changes needed
2. **d:\rawrxd\src\win32app\Win32IDE.cpp** - Main implementation:
   - onCreate() - Add missing createXxx() calls (line ~2650)
   - Add ~10 missing create methods
   - Implement setSidebarView()
   - Fix toggleFloatingPanel()
   - Fix showCommandPalette() population

3. **d:\rawrxd\src\win32app\Win32IDE_Debugger.cpp** - Debug pane:
   - Verify createDebuggerUI() properly populates sidebar

---

## PRIORITY ORDER

**BLOCKER (stop broken right now):**
- Item #13: Implement setSidebarView() → unblocks sidebar view switching
- Item #20: Implement pane resizing/docking framework

**CRITICAL (complete core functionality):**
- Item #1: createProblemsPanel() → see build errors
- Item #10: Activity bar icon buttons → use sidebar
- Item #9: Command palette population → actually work

**IMPORTANT (missing IDE features):**
- Item #3: Search panel
- Item #4: Git view
- Item #5: Debug view  
- Item #6: Extensions view

**NICE TO HAVE:**
- Item #7: Floating panels
- Item #2: Module browser (already partially works)

