# IMMEDIATE ACTION PLAN - Start Here!

**Created:** December 21, 2025  
**Next Step:** Pick ONE task from this list

---

## READ THESE FIRST (30 minutes)

1. [ ] **QUICK_START_SUMMARY.md** - 2 minute overview
2. [ ] **COMPREHENSIVE_AUDIT_REPORT.md** - Full analysis (20 min)
3. [ ] **INTEGRATION_CHECKLIST.md** - Detailed tasks (10 min)

---

## DECISION: Do You Want To...

### Option A: "Just Get It Working" (Recommended)
**Skip cleanup, focus on getting IDE to run**

→ Go to **OPTION A - CRITICAL PATH** below

**Timeline:** 2-3 weeks to MVP
**Focus:** Tasks 1.1, 1.2, 1.3, 2.1, 2.2, then rest

---

### Option B: "Clean Up First Then Build"
**Remove 80 duplicate files, organize, then build**

→ Go to **OPTION B - CLEANUP FIRST** below

**Timeline:** 1-2 days cleanup + 2-3 weeks build = 3-4 weeks total
**Focus:** COMPONENT_CLASSIFICATION.md tasks first

---

## OPTION A - CRITICAL PATH (Start Here!)

### ✅ Task 1.1: main_complete.asm (6-8 hours)
**File:** Create `src/main_complete.asm`

Replace the stub main.asm with complete WinMain:

```asm
; REQUIRED: WinMain entry point with full initialization
; includes: COM init, window creation, message loop, cleanup

.386
.model flat, stdcall
include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

public WinMain

WinMain proc hInstance:DWORD, hPrevInst:DWORD, lpCmdLine:DWORD, nShowCmd:DWORD
    ; TODO: Complete implementation
    ; 1. CoInitializeEx(NULL, COINIT_MULTITHREADED)
    ; 2. Call Editor_Init(hInstance)
    ; 3. Call Editor_Create(...)
    ; 4. Main message loop
    ; 5. CoUninitialize()
    ret
WinMain endp

end WinMain
```

**Status Check When Done:**
- Compiles without errors: `ml.exe /c src/main_complete.asm`
- Links with other modules

---

### ✅ Task 1.2: dialogs.asm (8-10 hours)
**File:** Create `src/dialogs.asm`

Implement Windows dialog wrappers:

```asm
; REQUIRED: Dialog system with file dialogs
; Public functions:
;   Dialog_OpenFile(hwnd, ppszPath) → HRESULT
;   Dialog_SaveFile(hwnd, ppszPath) → HRESULT
;   Dialog_MessageBox(hwnd, title, msg, type) → INT

include \masm32\include\windows.inc
include \masm32\include\comdlg32.inc

public Dialog_OpenFile
public Dialog_SaveFile
public Dialog_MessageBox

; TODO: Implement using IFileDialog COM interface
; Reference: Windows API documentation
```

**Status Check When Done:**
- Dialog_OpenFile shows native file picker
- Path returned correctly
- No memory leaks

---

### ✅ Task 1.3: build_release.ps1 (2-4 hours)
**File:** Update/create `build_release.ps1`

Single command to compile and link everything:

```powershell
# build_release.ps1 - Unified build script

# 1. Compile all .asm files to .obj
# 2. Link all .obj files to single EXE
# 3. Handle errors with clear messages
# 4. Output: build/RawrXD.exe

# Pseudocode:
foreach ($asm in Get-ChildItem src/*.asm) {
    ml.exe /c /Zi /Fo build/$($asm.BaseName).obj $asm
    if (!$?) { Write-Error "Compile failed"; exit 1 }
}

link.exe /subsystem:windows /out:build/RawrXD.exe build/*.obj `
    kernel32.lib user32.lib gdi32.lib comdlg32.lib ole32.lib oleaut32.lib

if (Test-Path build/RawrXD.exe) {
    Write-Host "✅ Build successful: build/RawrXD.exe"
} else {
    Write-Error "❌ Link failed"
}
```

**Status Check When Done:**
- Run: `.\build_release.ps1`
- File `build/RawrXD.exe` exists
- Can execute: `.\build\RawrXD.exe`

---

## VERIFICATION: After All 3 Tasks

### Quick Test Checklist
```
□ Run .\build_release.ps1
□ Check: build/RawrXD.exe exists
□ Run: .\build\RawrXD.exe
□ Check: Window appears with title bar
□ Check: Can see menu bar
□ Check: Can see editor pane
□ Close: Window closes cleanly
```

**If ALL checkboxes pass:** Continue to Task 2.1
**If ANY fail:** Debug accordingly

---

## OPTION B - CLEANUP FIRST

### Pre-Build Cleanup (1-2 days)

**Goal:** Reduce 200 files → 40-50 files

#### Step 1: Back Up (1 hour)
```powershell
cp -Recurse src src_backup_20251221
```

#### Step 2: Delete Obsolete (2-3 hours)
From COMPONENT_CLASSIFICATION.md:
- Delete all `*_test.asm` files
- Delete all `*_simple.asm` files  
- Delete all `*_minimal.asm` files
- Delete qt_ide_integration.asm (Qt legacy)
- Delete all *.asm with "legacy" in name

```powershell
# Delete test files
rm src/*_test.asm
rm src/*_simple*.asm
rm src/*_minimal.asm
rm src/qt_*.asm
# ... etc
```

#### Step 3: Consolidate Variants (2-3 hours)
Keep ONE version of each:
```powershell
# KEEP: editor_enterprise.asm
rm src/editor.asm, src/simple_editor.asm

# KEEP: file_tree_complete.asm
rm src/file_tree.asm, src/file_tree_enhanced.asm, ...

# KEEP: gguf_loader_unified.asm
rm src/gguf_loader.asm, src/gguf_loader_enterprise.asm, ...
```

#### Step 4: Reorganize (1-2 hours)
Create folder structure:
```
src/
├── CORE/
│   ├── main_complete.asm
│   ├── window.asm
│   ├── editor_enterprise.asm
│   └── dialogs.asm
├── UI/
│   ├── menu_system.asm
│   ├── toolbar.asm
│   ├── file_explorer.asm
│   └── ...
├── FEATURES/
│   ├── find_replace.asm
│   ├── syntax_highlighting.asm
│   └── ...
└── BACKEND/
    ├── gguf_loader_unified.asm
    ├── ollama_client_full.asm
    └── ...
```

#### Step 5: Verify Build Still Works (1 hour)
```powershell
.\build_release.ps1
```

**Then:** Continue with OPTION A tasks above

---

## RECOMMENDED: Do OPTION A Now!

### Why?
1. Faster to completion (2-3 weeks vs 3-4 weeks)
2. Can cleanup incrementally later
3. See results sooner
4. Keep all backup files (safety)
5. Cleanup doesn't block functionality

### Action Right Now:
1. Create `src/main_complete.asm` (start coding)
2. Implement WinMain with minimal functionality
3. Get it compiling
4. Move to Task 1.2

---

## DETAILED TASK BREAKDOWN

### TASK 1.1: main_complete.asm (Code Template)

**Copy this, expand it:**

```asm
; ==============================================================================
; main_complete.asm - Application Entry Point
; ==============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\ole32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\ole32.lib
includelib \masm32\lib\oleaut32.lib

; Externs from other modules
extern Editor_Init:PROC
extern Editor_Create:PROC
extern Editor_Destroy:PROC

.data
    szAppTitle      db "RawrXD IDE", 0

.code

public WinMain

WinMain proc hInstance:DWORD, hPrevInst:DWORD, lpCmdLine:DWORD, nShowCmd:DWORD
    LOCAL hWnd:DWORD
    LOCAL msg:MSG
    LOCAL bRet:DWORD
    
    ; Initialize COM (required for dialogs)
    invoke CoInitializeEx, NULL, COINIT_MULTITHREADED
    cmp eax, S_OK
    jne @InitFailed
    
    ; Initialize editor subsystem
    invoke Editor_Init, hInstance
    
    ; Create main window
    invoke Editor_Create, NULL, 0, 0, 800, 600
    mov hWnd, eax
    test eax, eax
    jz @CreateFailed
    
    ; Show window
    invoke ShowWindow, hWnd, nShowCmd
    invoke UpdateWindow, hWnd
    
    ; Main message loop
@@MessageLoop:
    invoke GetMessageA, addr msg, NULL, 0, 0
    cmp eax, 0
    jle @LoopDone
    
    invoke TranslateMessage, addr msg
    invoke DispatchMessageA, addr msg
    jmp @@MessageLoop
    
@LoopDone:
    ; Cleanup
    invoke Editor_Destroy
    invoke CoUninitialize
    
    mov eax, msg.wParam
    ret
    
@CreateFailed:
    invoke MessageBoxA, NULL, CSTR("Failed to create window"), addr szAppTitle, MB_OK
    jmp @Cleanup
    
@InitFailed:
    invoke MessageBoxA, NULL, CSTR("Failed to initialize COM"), addr szAppTitle, MB_OK
    
@Cleanup:
    xor eax, eax
    ret
    
WinMain endp

end WinMain
```

**When done:**
1. Save as `src/main_complete.asm`
2. Update `build_release.ps1` to include it
3. Try building: `ml.exe /c src/main_complete.asm`
4. Fix any errors (likely missing includes)

---

## COMMON ISSUES & SOLUTIONS

### Issue: "unresolved external symbol"
**Solution:** Add the missing extern declaration and ensure .obj file is linked

### Issue: "missing include file"
**Solution:** Check path in `include` directives, adjust for your environment

### Issue: "invalid instruction"
**Solution:** Memory-to-memory moves not allowed in x86, use register intermediate

### Issue: "build.ps1 not found"
**Solution:** Create it from the template above

---

## PROGRESS TRACKING

### Week 1 Milestones
- [ ] Day 1: main_complete.asm compiles
- [ ] Day 1: dialogs.asm started
- [ ] Day 2: dialogs.asm compiles
- [ ] Day 2: build_release.ps1 produces EXE
- [ ] Day 3: RawrXD.exe runs and shows window

### Week 2 Milestones
- [ ] Day 5: File > Open/Save dialogs work
- [ ] Day 6: Can load text files
- [ ] Day 7: Can save text files
- [ ] Day 8: Find/Replace basic working
- [ ] Day 9: Menu system complete

### Week 3 Milestones
- [ ] Day 10: Backend integration started
- [ ] Day 12: GGUF models loading
- [ ] Day 14: Chat interface functional
- [ ] Day 15: Testing complete
- [ ] Day 16: Release ready!

---

## COMMUNICATION

During development:
- **Stuck?** → Re-read the relevant audit document
- **Unsure?** → Check INTEGRATION_CHECKLIST.md for task details
- **Lost?** → Return to this file for context
- **Progress?** → Update milestone checkboxes above

---

## YOUR NEXT ACTION (Right Now!)

Pick ONE and start:

**Option 1:** Create `src/main_complete.asm` (6-8 hours)
- Full entry point implementation

**Option 2:** Create `src/dialogs.asm` (8-10 hours)
- Dialog system wrapper

**Option 3:** Update `build_release.ps1` (2-4 hours)
- Unified build script

**Recommendation:** Start with **Option 1** (main_complete.asm)

---

## Good Luck! 🚀

You have everything you need. The blueprint is clear.
Let's ship the IDE!

