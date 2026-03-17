# IMMEDIATE ACTION PLAN: IDE Production Unintegrated Components

**Generated:** 2026-03-12  
**Audience:** Development Team  
**Purpose:** Clear prioritized steps to close IDE production gaps

---

## EXECUTIVE SUMMARY

**Current State:**
- ✅ CLI executable: Working, 27 tokens generated, telemetry valid
- ✅ GUI executable: Working, 139 tokens generated, telemetry valid  
- ⚠️ IDE unified: TextEditor stub (incomplete), syntax highlighting unintegrated
- ❌ 60+ orphaned source files (unused variants, competing implementations)
- ❌ 40+ legacy build scripts (dead code)

**Gap Analysis:** 99% of code is unintegrated. Production consists of 5 .asm files + 1 config file.

**Recommendation:** Execute 3 immediate actions (8 hours) to unlock IDE text editor functionality.

---

## PRIORITY TIMELINE

### **TODAY (2-4 hours): Wire Syntax Highlighting**

**Target:** Get text colors displaying in IDE editor

**Steps:**

1. **Read & understand current state** (15 min)
   - [RawrXD_TextEditorGUI.asm](d:\rawrxd\RawrXD_TextEditorGUI.asm) - 556 lines
   - [RawrXD_MASM_SyntaxHighlighter.asm](d:\rawrxd\RawrXD_MASM_SyntaxHighlighter.asm) - 400+ lines (created in prior session)
   - [RawrXD_IDE_SyntaxHighlighting_Integration.asm](d:\rawrxd\RawrXD_IDE_SyntaxHighlighting_Integration.asm) - 250+ lines (created in prior session)
   - [config/masm_syntax_highlighting.json](d:\rawrxd\config\masm_syntax_highlighting.json) - 301 lines (valid JSON)

2. **Compile all three syntax files** (20 min)
   ```powershell
   $ml64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
   $outDir = "d:\rawrxd\build\syntax"
   
   & $ml64 /c /Zi /nologo /Fo"$outDir\RawrXD_MASM_SyntaxHighlighter.obj" `
       "d:\rawrxd\RawrXD_MASM_SyntaxHighlighter.asm"
   
   & $ml64 /c /Zi /nologo /Fo"$outDir\RawrXD_IDE_SyntaxHighlighting_Integration.obj" `
       "d:\rawrxd\RawrXD_IDE_SyntaxHighlighting_Integration.asm"
   ```

3. **Add initialization hook to TextEditorGUI** (45 min)
   - **Find:** EditorWindow_RegisterClass PROC
   - **Add after:** `call INIT_MASM_SYNTAX(configPath)` where configPath points to config/masm_syntax_highlighting.json
   - Ensure config JSON is embedded or loaded from disk

4. **Add event handler to TextEditorGUI input loop** (30 min)
   - **Find:** Message handling loop (likely in EditorWindow_HandlePaint or WM_PAINT handler)
   - **Add:** Call `ON_MASM_BUFFER_CHANGED()` after each keystroke
   - Pass: buffer pointer, changed line range, keystroke context

5. **Test compilation** (15 min)
   ```powershell
   & $ml64 /c /Zi /nologo /Fo"d:\rawrxd\build\RawrXD_TextEditorGUI.obj" `
       "d:\rawrxd\RawrXD_TextEditorGUI.asm"
   ```

**Success Criteria:**
- Compiles without errors
- RawrXD_MASM_SyntaxHighlighter.obj created
- RawrXD_IDE_SyntaxHighlighting_Integration.obj created
- RawrXD_TextEditorGUI.obj updated with init calls

**Deliverable:** Wiring document + updated source files

---

### **TOMORROW (4-20 hours): Complete or Replace Text Editor**

**Target:** Get actual text rendering in IDE window

**Decision Point: Option A vs Option B**

#### **Option A: Complete TextEditorGUI Stubs** (RECOMMENDED if timeline permits)
**Effort:** 15-20 hours  
**Process:**

1. Analyze incomplete procedures:
   ```asm
   EditorWindow_HandlePaint    ; 90 lines, no rendering
   EditorWindow_HandleKeyDown  ; Placeholder
   EditorWindow_HandleMouseDown ; Placeholder
   EditorWindow_Render         ; Missing
   ```

2. Implement missing logic:
   - GDI text rendering (SelectObject, TextOutA, LineTo for cursor)
   - Cursor positioning (track caret row/col)
   - Line number rendering (optional but nice)
   - Selection highlighting (invert rect)
   - Keyboard input → buffer updates
   - Mouse click → position cursor

3. Test each procedure incrementally

**Files to modify:**
- [RawrXD_TextEditorGUI.asm](d:\rawrxd\RawrXD_TextEditorGUI.asm) (add ~200-300 lines of rendering logic)

---

#### **Option B: Find & Integrate Working Text Editor** (FASTER)
**Effort:** 2-4 hours  
**Process:**

1. Search repository history for prior implementations:
   ```powershell
   # Check RawrXD-IDE-Final repo
   cd d:\RawrXD-IDE-Final
   git log --oneline --grep="TextEditor" | head -20
   
   # Check for Monaco editor integration
   grep -r "MonacoEditor\|monaco\.d\.ts" . 2>/dev/null | head -10
   ```

2. Inspect candidates:
   - Any Win32 text editor in C++ that can be ported?
   - Any MASM64 text editor template in codebase?

3. If found: Replace TextEditorGUI.asm stub with working version

**Files to search:**
- `D:\rawrxd\RawrXD-IDE-Final\` (other repo)
- `C:\Users\HiH8e\Eon-ASM\compilers\` (prior work)

---

**DECISION NEEDED:**
- **Path A (complete stubs):** Higher quality, full control, takes 15-20 hours
- **Path B (find existing):** Faster (2-4h), risks integration complexity if code doesn't fit

**RECOMMENDATION:** Start with Path B (find existing). If nothing found, commit to Path A.

---

### **WEEK 2 (2 hours): Consolidate Unused Components**

**Target:** Clean up orphaned code, prepare future roadmap

**Steps:**

1. **Create consolidation directories:**
   ```powershell
   mkdir d:\rawrxd\build_archive
   mkdir d:\rawrxd\future_roadmap
   ```

2. **Archive legacy build scripts** (40+ files):
   ```powershell
   Move-Item d:\rawrxd\Build_*.ps1 d:\rawrxd\build_archive\
   Move-Item d:\rawrxd\BUILD_*.ps1 d:\rawrxd\build_archive\
   ```

3. **Archive unused .asm variants** (50+ files):
   Move to `future_roadmap\`:
   - All RawrXD_Agentic_* files
   - All RawrXD_Sovereign_* files
   - RawrXD_StreamRenderer_Live.asm
   - All PE Writer files (except current efforts)
   - All Network/IPC files
   - All pifabric_* scheduler files

4. **Create manifest** (`future_roadmap/manifest.md`):
   ```markdown
   # Future Roadmap Components
   
   ## Agentic Workers (11 files)
   - Purpose: Autonomous agent coordination
   - Status: Under evaluation (competing implementations)
   - Future: Select canonical variant, integrate if decision favors parallelism
   
   ## GPU/DMA/Pifabric (15+ files)
   - Purpose: Hardware acceleration + tensor scheduling
   - Status: Implemented but not integrated
   - Future: Evaluate ROI; integrate if performance bottleneck identified
   
   ## Network/IPC (4 files)
   - Purpose: Multi-machine IDE support
   - Status: Scaffold only
   - Future: Deferred; evaluate after single-machine stability
   ```

**Success Criteria:**
- 40+ build scripts moved to build_archive/
- 50+ unused .asm files moved to future_roadmap/
- Manifest.md created with clear status for each
- Main d:\rawrxd\ directory simplified

---

## UNBLOCKING pe Writer (Awaiting Decision)

**Status:** 5 PE Writer files exist but assembly fails (ml64 syntax ~60 errors)

**Reference:** SPRINT_C_STATUS_REPORT.md, SPRINT_C_DECISION.md

**Paths Available:**
```
Path A: Fix ml64 syntax blockers (4-6 hours)
  → Pre-calculate hex immediates, workaround strict register matching
  → Result: PE writer bootstrap (moat for $32M valuation)
  
Path B: Test masm32.exe compatibility (1-2 hours)
  → masm32 is older, possibly more lenient MASM dialect
  → Risk: May not support x64 fully
  
Path C: Defer to next sprint (strategic)
  → Deliver determinism + attestation layers first
  → PE writer as follow-up work
```

**Action Required:** Team decision on Path. Recommend Path A if moat narrative is critical for valuation.

---

## CROSS-REFERENCE: Current Unintegrated Files

### **Critical Integration Items (MUST DO)**
```
Task: Wire Syntax Highlighting
Files:
  - d:\rawrxd\RawrXD_MASM_SyntaxHighlighter.asm
  - d:\rawrxd\RawrXD_IDE_SyntaxHighlighting_Integration.asm
  - d:\rawrxd\config\masm_syntax_highlighting.json
  - d:\rawrxd\RawrXD_TextEditorGUI.asm (to add hooks)
Timeline: 2-4 hours
```

### **Important Integration Items (SHOULD DO)**
```
Task: Complete or Replace Text Editor
Files:
  - d:\rawrxd\RawrXD_TextEditorGUI.asm (complete Option A OR replace Option B)
Timeline: 4-20 hours (Path A) or 2-4 hours (Path B)
```

### **Consolidation Items (NICE TO HAVE)**
```
Files: 60+ orphaned .asm + 40+ legacy build scripts
Action: Move to ./build_archive/ and ./future_roadmap/
Timeline: 2 hours
```

### **Decision-Pending Items (TBD)**
```
Task: Resolve PE Writer bl‌ocker
Files: RawrXD_PE_Writer_*.asm (5 files)
Decision: Path A/B/C (moat vs defer debate)
Timeline: 4-6 hours (A) or 1-2 hours (B) or defer
```

---

## SPECIFIC FILES AT d:\rawrxd\ NEEDING ATTENTION

### **1. RawrXD_TextEditorGUI.asm** - INCOMPLETE STUB
**Location:** [d:\rawrxd\RawrXD_TextEditorGUI.asm](d:\rawrxd\RawrXD_TextEditorGUI.asm)  
**Lines:** 556  
**Status:** Skeleton procedures, no rendering logic  
**Example:**
```asm
EditorWindow_HandlePaint PROC FRAME
    ; 90 lines of prologue/setup but no actual rendering
    mov rax, 1  ; just return success
    ret
ENDP
```
**Action:** Complete stubs (Option A) or find replacement (Option B)

### **2. RawrXD_MASM_SyntaxHighlighter.asm** - NOT COMPILED
**Location:** [d:\rawrxd\RawrXD_MASM_SyntaxHighlighter.asm](d:\rawrxd\RawrXD_MASM_SyntaxHighlighter.asm)  
**Lines:** 400+  
**Status:** Complete tokenizer logic, ready to compile  
**Procedures:**
- `TOKENIZE_MASM_LINE` (classify each token)
- `STRCMP_CASE_INSENSITIVE` (keyword matching)
- Supports 8 token types (keywords, registers, numbers, comments, etc.)
**Action:** Compile to .obj, link into IDE

### **3. RawrXD_IDE_SyntaxHighlighting_Integration.asm** - NOT COMPILED
**Location:** [d:\rawrxd\RawrXD_IDE_SyntaxHighlighting_Integration.asm](d:\rawrxd\RawrXD_IDE_SyntaxHighlighting_Integration.asm)  
**Lines:** 250+  
**Status:** Integration layer, ready to compile  
**Procedures:**
- `INIT_MASM_SYNTAX()` (load config, register handlers)
- `ON_MASM_BUFFER_CHANGED()` (event handler)
- `HIGHLIGHT_MASM_LINE()` (apply colors to single line)
**Action:** Compile to .obj, wire into IDE_unified

### **4. config/masm_syntax_highlighting.json** - VALID BUT UNLOADED
**Location:** [d:\rawrxd\config\masm_syntax_highlighting.json](d:\rawrxd\config\masm_syntax_highlighting.json)  
**Lines:** 301  
**Status:** Valid JSON, color palette defined (8 token types)  
**Content:**
```json
{
  "syntaxHighlighting": {
    "masm": {
      "fileExtensions": [".asm"],
      "tokenRules": [
        {"name": "Comment", "pattern": ";.*$", "color": "#6A9955"},
        {"name": "String", "pattern": "\"[^\"]*\"|'[^']*'", "color": "#CE9178"},
        {"name": "Directive", "patterns": ["\\bPROC\\b", "\\bENDP\\b", ...]}
      ]
    }
  }
}
```
**Action:** Load via `INIT_MASM_SYNTAX()` call

### **5. RawrXD_IDE_unified.asm** - MASTER IDE CONTAINER
**Location:** [d:\rawrxd\RawrXD_IDE_unified.asm](d:\rawrxd\RawrXD_IDE_unified.asm)  
**Status:** Optional in build (compiled if present)  
**Expected to contain:** Main window, message loop, editor coordination  
**Action:** Update with syntax highlighting initialization

### **6. RawrXD_PE_Writer_Complete.asm** - BLOCKED
**Location:** [d:\rawrxd\RawrXD_PE_Writer_Complete.asm](d:\rawrxd\RawrXD_PE_Writer_Complete.asm)  
**Status:** ~60+ ml64 syntax errors during assembly  
**Errors reference:** Lines 79-403  
**Decision Needed:** Path A (fix), Path B (test masm32), Path C (defer)  
**Action:** Await team decision from SPRINT_C_DECISION.md

---

## BUILD COMMANDS IF PROCEEDING WITH SYNTAX INTEGRATION

### **Compile Syntax Components** (TODAY)
```powershell
$ml64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
$outDir = "d:\rawrxd\build\syntax"

# Ensure output dir exists
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Compile tokenizer
& $ml64 /c /Zi /nologo /Fo:"$outDir\RawrXD_MASM_SyntaxHighlighter.obj" `
    "d:\rawrxd\RawrXD_MASM_SyntaxHighlighter.asm" 2>&1 | Tee-Object -FilePath "$outDir\compile_asm_highlighter.log"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Tokenizer compilation FAILED" -ForegroundColor Red
    exit 1
}

# Compile integration layer
& $ml64 /c /Zi /nologo /Fo:"$outDir\RawrXD_IDE_SyntaxHighlighting_Integration.obj" `
    "d:\rawrxd\RawrXD_IDE_SyntaxHighlighting_Integration.asm" 2>&1 | Tee-Object -FilePath "$outDir\compile_asm_integration.log"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Integration layer compilation FAILED" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Syntax components compiled successfully" -ForegroundColor Green
```

### **Link with IDE** (if TextEditor fixed)
```powershell
# After TextEditorGUI.asm is updated with hooks:
& $ml64 /c /Zi /nologo /Fo:"d:\rawrxd\build\RawrXD_TextEditorGUI.obj" `
    "d:\rawrxd\RawrXD_TextEditorGUI.asm"

# Link new IDE with syntax components
& $linker /SUBSYSTEM:CONSOLE /NOLOGO /ENTRY:main /OUT:"d:\rawrxd\build\RawrXD_IDE_WithSyntax.exe" `
    "d:\rawrxd\build\RawrXD_TextEditorGUI.obj" `
    "d:\rawrxd\build\syntax\RawrXD_MASM_SyntaxHighlighter.obj" `
    "d:\rawrxd\build\syntax\RawrXD_IDE_SyntaxHighlighting_Integration.obj" `
    kernel32.lib user32.lib gdi32.lib
```

---

## SUCCESS CRITERIA FOR EACH MILESTONE

### **Milestone 1: Syntax Wiring (Today)**
- [ ] RawrXD_MASM_SyntaxHighlighter.obj created (no errors)
- [ ] RawrXD_IDE_SyntaxHighlighting_Integration.obj created (no errors)
- [ ] INIT_MASM_SYNTAX() hook added to TextEditorGUI startup
- [ ] ON_MASM_BUFFER_CHANGED() hook added to editor loop
- [ ] Integration wiring document reviewed

### **Milestone 2: Text Editor (Tomorrow-This Week)**
- [ ] Decision made: Complete stubs (Path A) or find existing (Path B)
- [ ] If Path A: RawrXD_TextEditorGUI.asm updated with rendering logic
- [ ] If Path B: Replacement component integrated
- [ ] RawrXD_TextEditorGUI.obj compiles without errors
- [ ] Text appears in IDE window when file is opened

### **Milestone 3: Consolidation (Week 2)**
- [ ] 40+ build scripts moved to ./build_archive/
- [ ] 50+ unused .asm files moved to ./future_roadmap/
- [ ] Manifest.md created documenting future components
- [ ] Main d:\rawrxd\ directory simplified

### **Milestone 4: PE Writer (Awaiting Decision)**
- [ ] Decision made: Path A (fix ml64), Path B (test masm32), or Path C (defer)
- [ ] If Path A: ml64 syntax errors documented + workarounds applied
- [ ] If Path B: masm32.exe compatibility tested
- [ ] If Path C: Decision documented in sprint notes

---

## RISKS & MITIGATION

| Risk | Impact | Mitigation |
|------|--------|-----------|
| TextEditorGUI stubs incomplete | IDE text won't render | Path B: Find existing impl within 2-4 hours |
| Syntax highlighting compile fails | Colors won't work | Test each .asm file separately first |
| Integration introduces new bugs | Build breaks | Test each component in isolation before linking |
| PE Writer decisions delayed | Moat strategy blocked | Schedule decision meeting TODAY |
| Orphaned files never cleaned | Repository bloat increases | Set deadline for consolidation (Week 2) |

---

## OWNERSHIP & ACCOUNTABILITY

| Task | Owner | Deadline | Status |
|------|-------|----------|--------|
| Syntax highlighting wiring | [Team Lead] | TODAY | Not started |
| Text editor decision (Path A/B) | [Tech Lead] | Tomorrow morning | Pending |
| Text editor implementation | [Dev Lead] | This week | Awaiting decision |
| File consolidation | [DevOps] | Week 2 | Not started |
| PE Writer path decision | [Product/Arch] | TODAY | URGENT |

---

## WHAT NOT TO DO RIGHT NOW

❌ Don't spend time on PE Writer syntax fixes until Path decided  
❌ Don't merge unused .asm files into production builds  
❌ Don't leave legacy build scripts in main directory  
❌ Don't modify TextEditorGUI stubs without completing them  
❌ Don't attempt to link syntax component without testing each piece  

---

## NEXT IMMEDIATE STEP

**This afternoon (next 30 minutes):**

1. Team lead reviews this action plan
2. Confirm syntax highlighting wiring task (TODAY)
3. Schedule TextEditor decision meeting (tomorrow morning)
4. Make PE Writer path decision (A/B/C) TODAY
5. Set consolidation deadline (Week 2, TBD day)

**This evening (after decisions):**
1. If proceeding with syntax wiring: Execute commands from "BUILD COMMANDS" section
2. If TextEditor Path A: Begin identifying missing rendering procedures
3. If TextEditor Path B: Begin search in RawrXD-IDE-Final repo

**Tomorrow:**
1. Report on syntax compilation results
2. Proceed with TextEditor chosen path
3. Validate colors displaying in IDE

---

**Report Prepared By:** AI Code Auditor  
**Date:** 2026-03-12 12:00 UTC  
**Distribution:** Development Team, Tech Lead, Product Management
