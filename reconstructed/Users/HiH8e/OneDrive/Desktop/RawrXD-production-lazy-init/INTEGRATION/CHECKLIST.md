# IDE Integration Checklist & Task Breakdown

## PHASE 1: Build System & Entry Point (CRITICAL PATH)

### Task 1.1: Create Unified Main Entry Point
**File:** `src/main_complete.asm`
**Status:** NOT STARTED
**Dependencies:** None
**Effort:** 6-8 hours

**Requirements:**
- [ ] WinMain function that handles command line
- [ ] COM initialization (CoInitializeEx)
- [ ] GDI+ initialization
- [ ] Main window creation with proper styles
- [ ] Message loop with proper error handling
- [ ] Graceful shutdown sequence
- [ ] Resource cleanup

**Code Structure:**
```asm
.code
public WinMain

WinMain proc hInstance:DWORD, hPrevInst:DWORD, lpCmdLine:DWORD, nShowCmd:DWORD
    ; Initialize COM
    ; Initialize GDI+
    ; Call Editor_Init
    ; Call Editor_Create
    ; Main message loop
    ; Cleanup
    ret
WinMain endp
```

**Success Criteria:**
- Compiles without errors
- Creates visible window
- Processes messages
- Closes cleanly

---

### Task 1.2: Create Dialog System Wrapper
**File:** `src/dialogs.asm` (NEW)
**Status:** NOT STARTED
**Dependencies:** windows.inc, comdlg32.lib
**Effort:** 8-10 hours

**Requirements:**
- [ ] File Open Dialog (IFileDialog)
- [ ] File Save Dialog (IFileDialog)
- [ ] Folder Browse Dialog (IFolderBrowserDialog)
- [ ] MessageBox wrappers (custom styled)
- [ ] Input Dialog (text entry)
- [ ] Color Picker Dialog
- [ ] Font Selection Dialog

**Public Functions:**
```asm
Dialog_OpenFile           proto :DWORD, :DWORD  ; hwnd, ppszPath
Dialog_SaveFile           proto :DWORD, :DWORD  ; hwnd, ppszPath
Dialog_BrowseFolder       proto :DWORD, :DWORD  ; hwnd, ppszPath
Dialog_MessageBox         proto :DWORD, :DWORD, :DWORD, :DWORD  ; hwnd, title, msg, type
Dialog_ShowInputBox       proto :DWORD, :DWORD, :DWORD, :DWORD  ; hwnd, prompt, title, result
```

**Success Criteria:**
- Dialog_OpenFile opens native file dialog
- Selected file path returned to caller
- All dialogs modal and properly centered
- Proper COM cleanup

---

### Task 1.3: Update Build Script to Link Everything
**File:** `build_release.ps1` (UPDATE)
**Status:** IN PROGRESS
**Dependencies:** All .asm files compiled to .obj
**Effort:** 2-4 hours

**Requirements:**
- [ ] Single command: `.\build_release.ps1`
- [ ] Compiles all .asm files
- [ ] Links all .obj files
- [ ] Embeds manifest
- [ ] Outputs: `build/RawrXD.exe`
- [ ] Proper error reporting
- [ ] Build time < 60 seconds

**Link Command Template:**
```powershell
link.exe /subsystem:windows `
  /out:build/RawrXD.exe `
  build/*.obj `
  kernel32.lib user32.lib gdi32.lib comdlg32.lib ole32.lib oleaut32.lib ...
```

**Success Criteria:**
- Single EXE produced
- No unresolved externals
- EXE runs without crashing
- Editor window appears

---

## PHASE 2: Editor I/O Integration

### Task 2.1: Wire File Load/Save Dialogs
**File:** `src/editor_enterprise.asm` (UPDATE)
**Status:** PARTIAL (load/save implementations exist, need dialog hooks)
**Dependencies:** dialogs.asm, main_complete.asm
**Effort:** 4-6 hours

**Requirements:**
- [ ] File > Open menu calls Dialog_OpenFile
- [ ] Path returned to Editor_LoadFile
- [ ] File > Save menu calls Dialog_SaveFile
- [ ] Save triggers Editor_SaveFile
- [ ] Recent files list maintained
- [ ] Unsaved changes indicator

**Hook Points:**
```asm
; In menu handler
.IF wParam == ID_FILE_OPEN
    call Editor_OpenFile_Dialog  ; NEW
.ELSEIF wParam == ID_FILE_SAVE
    call Editor_SaveFile_Dialog  ; NEW
.ENDIF
```

**Success Criteria:**
- Open dialog appears on Ctrl+O
- File loads and displays in editor
- Save dialog appears on Ctrl+S
- File saves with changes persisted

---

### Task 2.2: Implement Search & Replace
**File:** `src/find_replace.asm` (NEW)
**Status:** NOT STARTED
**Dependencies:** editor_enterprise.asm
**Effort:** 6-8 hours

**Requirements:**
- [ ] Find toolbar (Ctrl+F)
- [ ] Find next/previous
- [ ] Case sensitivity toggle
- [ ] Regex support (basic)
- [ ] Replace dialog (Ctrl+H)
- [ ] Replace all with confirmation
- [ ] Highlight all matches

**Public Functions:**
```asm
FindReplace_Init          proto
FindReplace_ShowFindBar   proto
FindReplace_ShowReplaceDialog proto
FindReplace_FindNext      proto :DWORD  ; pszText
FindReplace_ReplaceAll    proto :DWORD, :DWORD  ; pszFind, pszReplace
```

**Success Criteria:**
- Find bar appears at bottom of editor
- Ctrl+F focuses find input
- Enter goes to next match
- Matches highlighted in editor
- Replace dialog functional

---

## PHASE 3: UI Polish & Features

### Task 3.1: Enhance Menu System
**File:** `src/menu_system.asm` (UPDATE)
**Status:** PARTIAL
**Effort:** 4-5 hours

**Missing Features:**
- [ ] Keyboard shortcut hints
- [ ] Icon support for menu items
- [ ] Accelerators properly registered
- [ ] Context menus (right-click)
- [ ] Recent files submenu
- [ ] Dynamic menu updates

**New Menus Needed:**
```
File:
  New, Open, Save, Save As, Recent Files (→), Exit
Edit:
  Undo, Redo, Cut, Copy, Paste, Select All, Find, Replace
View:
  Toggle Explorer, Toggle Terminal, Toggle Chat, Zoom In/Out
Tools:
  Run, Debug, Settings
Help:
  Documentation, About
```

**Success Criteria:**
- All menu items have shortcuts
- Alt+F opens File menu
- Recent files shows and works
- Context menus appear on right-click

---

### Task 3.2: File Explorer Enhancement
**File:** `src/file_explorer.asm` (UPDATE - use file_tree_complete.asm)
**Status:** PARTIAL
**Effort:** 5-6 hours

**Requirements:**
- [ ] Double-click opens file in editor
- [ ] Right-click context menu (rename, delete, copy path)
- [ ] Drag file to editor to open
- [ ] Filter bar to search files
- [ ] Expand/collapse folders
- [ ] Selected file highlight
- [ ] Icon indicators (modified, git status)

**Success Criteria:**
- Can navigate folders
- Can open files by double-click
- Context menu functional
- File icons show

---

### Task 3.3: Status Bar Information
**File:** `src/status_bar.asm` (UPDATE)
**Status:** PARTIAL
**Effort:** 3-4 hours

**Information to Display:**
- [ ] Current line:column
- [ ] File encoding
- [ ] Line ending type (CRLF/LF)
- [ ] File size
- [ ] Selection info
- [ ] Mode indicator (INSERT/NORMAL)
- [ ] Git status indicator

**Success Criteria:**
- Status bar shows cursor position
- Updates on every cursor move
- Shows file encoding
- Shows git status

---

## PHASE 4: Backend Integration

### Task 4.1: GGUF Model Loading Integration
**File:** `src/gguf_loader_unified.asm` (use existing)
**Status:** EXISTS
**Dependencies:** GGUF files, model paths
**Effort:** 6-8 hours

**Requirements:**
- [ ] Unify variants into single implementation
- [ ] Error handling for corrupt files
- [ ] Progress callback
- [ ] Tensor inspection API
- [ ] Model metadata display
- [ ] Integration with chat interface

**Success Criteria:**
- Can load .gguf files without crash
- Model info displays
- Tensors accessible
- No memory leaks

---

### Task 4.2: LSP Client Completion
**File:** `src/lsp_client.asm` (UPDATE)
**Status:** STUB
**Dependencies:** json_parser.asm, http_client_full.asm
**Effort:** 8-10 hours

**Requirements:**
- [ ] LSP initialization handshake
- [ ] Text document synchronization
- [ ] Diagnostics handling
- [ ] Hover information
- [ ] Completion requests
- [ ] Definition/reference queries
- [ ] Error recovery

**Success Criteria:**
- Connects to LSP server
- Receives diagnostics
- Shows errors in editor
- Completion works

---

### Task 4.3: Chat Interface Integration
**File:** `src/chat_interface.asm` (UPDATE - use chat_agent_44tools.asm)
**Status:** PARTIAL
**Dependencies:** ollama_client_full.asm, json_parser.asm
**Effort:** 6-8 hours

**Requirements:**
- [ ] Message input field
- [ ] Message history display
- [ ] Streaming response handling
- [ ] Model selector dropdown
- [ ] System prompt editor
- [ ] Clear history button
- [ ] Export chat to file

**Success Criteria:**
- Chat pane visible
- Can type message
- Message appears in history
- Response streams in
- Can switch models

---

## PHASE 5: Testing & Deployment

### Task 5.1: Create Test Suite
**File:** `test/integration_test.asm` (NEW)
**Status:** NOT STARTED
**Effort:** 8-10 hours

**Test Cases:**
- [ ] Window creation
- [ ] File load/save roundtrip
- [ ] Editor navigation
- [ ] Text insertion/deletion
- [ ] Dialog operations
- [ ] Menu handling
- [ ] Resource cleanup

---

### Task 5.2: Performance Profiling
**File:** `src/performance_monitor.asm` (UPDATE)
**Status:** EXISTS
**Effort:** 4-6 hours

**Metrics to Track:**
- [ ] Startup time
- [ ] File load time
- [ ] Render frame rate
- [ ] Memory usage
- [ ] CPU usage
- [ ] Hot path identification

---

### Task 5.3: Documentation
**File:** `docs/BUILD.md`, `docs/ARCHITECTURE.md` (NEW)
**Status:** NOT STARTED
**Effort:** 4-6 hours

**Document:**
- [ ] Build instructions
- [ ] Architecture overview
- [ ] Module reference
- [ ] Contributing guide
- [ ] User manual
- [ ] Troubleshooting

---

## DEPENDENCIES & ORDERING

### Must Complete First (Blocking):
1. ✅ Task 1.1: main_complete.asm
2. ✅ Task 1.2: dialogs.asm
3. ✅ Task 1.3: build_release.ps1
4. ✅ Task 2.1: Wire dialogs to editor

### Can Parallelize:
- Task 2.2 (Find/Replace)
- Task 3.1-3.3 (UI Polish)
- Task 4.1-4.3 (Backend Integration)

### Must Complete Before Release:
- Task 5.1 (Testing)
- Task 5.2 (Profiling)
- Task 5.3 (Documentation)

---

## EFFORT SUMMARY

| Phase | Duration | Tasks |
|-------|----------|-------|
| **Phase 1** | 2-3 days | Build system & entry point |
| **Phase 2** | 2-3 days | Editor I/O integration |
| **Phase 3** | 2-3 days | UI polish |
| **Phase 4** | 3-4 days | Backend integration |
| **Phase 5** | 2-3 days | Testing & deployment |
| **TOTAL** | **2-3 weeks** | Full IDE completion |

---

## Success Milestones

### Week 1 End:
- ✅ RawrXD.exe executable exists
- ✅ Can load and save files
- ✅ Editor fully functional
- ✅ All dialogs working

### Week 2 End:
- ✅ Find & Replace working
- ✅ File explorer navigation
- ✅ Menu system complete
- ✅ Status bar showing info

### Week 3 End:
- ✅ GGUF models load
- ✅ LSP integration active
- ✅ Chat interface working
- ✅ All tests passing
- ✅ Performance optimized
- ✅ Ready for release

