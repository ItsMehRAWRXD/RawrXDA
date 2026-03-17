# IDE Production Integration Manifest
## Detailed Component Linking Status

**Report Date:** 2026-03-12  
**Focus:** Which .asm files are actually compiled and linked into production IDE binaries

---

## PART 1: PRODUCTION EXECUTABLES & THEIR COMPONENTS

### **RawrXD_Amphibious_CLI_ml64.exe**
**Entry Point:** `main` (from RawrXD_Amphibious_CLI_ml64.asm)

**Linked Object Files (5):**
```
✅ RawrXD_Amphibious_CLI_ml64.obj      ← Entry point (main procedure)
✅ RawrXD_Amphibious_Core2_ml64.obj    ← Inference loop (RunAutonomousCycle_ml64)
✅ RawrXD_StreamRenderer_DMA.obj       ← Output rendering
✅ RawrXD_ML_Runtime.obj               ← Tokenizer + inference fallback
   system imports: kernel32.lib, user32.lib (Windows console)
```

**Source Files (4 .asm):**
- [RawrXD_Amphibious_CLI_ml64.asm](d:\rawrxd\RawrXD_Amphibious_CLI_ml64.asm)
- [RawrXD_Amphibious_Core2_ml64.asm](d:\rawrxd\RawrXD_Amphibious_Core2_ml64.asm)
- [RawrXD_StreamRenderer_DMA.asm](d:\rawrxd\RawrXD_StreamRenderer_DMA.asm)
- [RawrXD_ML_Runtime.asm](d:\rawrxd\RawrXD_ML_Runtime.asm)

**Telemetry Gate Output:**
- `build/amphibious-ml64/rawrxd_telemetry_cli.json`
  - Mode: "cli"
  - Stage mask: 63 (all 6 stages complete)
  - Generated tokens: 27
  - Success: true ✅

---

### **RawrXD_Amphibious_GUI_ml64.exe**
**Entry Point:** `GuiMain` (from RawrXD_GUI_RealInference.asm)

**Linked Object Files (3):**
```
✅ RawrXD_GUI_RealInference.obj        ← Main window, message loop
✅ RawrXD_ML_Runtime.obj               ← Tokenizer + inference fallback
✅ (kernel32.lib, user32.lib)
```

**Source Files (2 .asm):**
- [RawrXD_GUI_RealInference.asm](d:\rawrxd\RawrXD_GUI_RealInference.asm)
- [RawrXD_ML_Runtime.asm](d:\rawrxd\RawrXD_ML_Runtime.asm)

**Telemetry Gate Output:**
- `build/amphibious-ml64/rawrxd_telemetry_gui.json`
  - Mode: "gui"
  - Stage mask: 63 (all 6 stages complete)
  - Generated tokens: 139
  - Success: true ✅

---

### **RawrXD_IDE_unified.exe** (Optional)
**Entry Point:** `_start_entry` (from RawrXD_IDE_unified.asm)

**Linked Object Files:**
```
✅ RawrXD_IDE_unified.obj              ← Text editor + IDE UI
   system imports: kernel32.lib, user32.lib, advapi32.lib, shell32.lib
```

**Source Files (1 .asm):**
- [RawrXD_IDE_unified.asm](d:\rawrxd\RawrXD_IDE_unified.asm) (optional, build skips if missing)

**Status:** Optional in build pipeline. If present, assembled and linked.

---

## PART 2: FILES WITH SOURCE CODE EXISTS BUT NOT COMPILED

### **Category A: Never Referenced in Any Build Script**

These .asm files exist but are NOT mentioned in any of the 3 canonical build scripts:
- Build-Amphibious-ml64.ps1
- RawrXD_IDE_BUILD_Unified.ps1  
- Build-PE-Writer-Phase3.ps1

**Complete List (60+ files):**

#### Amphibious Variants (7 files - superseded/alt implementations)
```
❌ RawrXD_Amphibious_Complete_ASM.asm
❌ RawrXD_Amphibious_Full_Working.asm
❌ RawrXD_Amphibious_ML64_Complete.asm
❌ RawrXD_AutoHeal_Test_v2.asm
❌ RawrXD_AutoHeal_GUI.asm
❌ RawrXD_AutoHeal_Test.asm
```
**Reason for non-integration:** Core2 variant selected as authoritative

#### Agentic/Sovereign Cores (9 files - competing implementations)
```
❌ RawrXD_AgentHost_Sovereign.asm
❌ RawrXD_AgenticInference.asm
❌ RawrXD_AgenticSovereignCore.asm
❌ RawrXD_Agentic_Bridge.asm
❌ RawrXD_Agentic_Core_ml64.asm
❌ RawrXD_Agentic_GUI_ml64.asm
❌ RawrXD_Agentic_Master.asm
❌ RawrXD_NativeAgent_Engine.asm
❌ RawrXD_ChatService_Agentic.asm
```
**Reason:** Unclear which is canonical; not referenced in active build

#### Inference Engines (6 files)
```
❌ RawrXD_CPUInference_Engine.asm
❌ RawrXD_MLInference.asm
❌ RawrXD_InferenceAPI.asm
❌ RawrXD_CPUOps_Kernels.asm
❌ RawrXD_ContextExtractor_ml64.asm
❌ RawrXD_DiffValidator_ml64.asm
```
**Reason:** ML_Runtime.asm selected; these are alternative paths

#### Streaming/Rendering (4 files)
```
✅ RawrXD_StreamRenderer_DMA.asm  ← INTEGRATED
❌ RawrXD_StreamRenderer_Live.asm  ← Alt version, not linked
❌ RawrXD_InlineStream_ml64.asm
❌ RawrXD_P2PRelay.asm
```

#### Text Editor (5 files)
```
⚠️ RawrXD_TextEditorGUI.asm        ← Stub impl, loaded but not compiling/not linked
❌ RawrXD_TextBuffer.asm
❌ RawrXD_TextEditor_Main.asm
❌ RawrXD_InlineEdit_Week1_IntegrationTest.asm
❌ RawrXD_InlineEdit_Keybinding.asm
❌ RawrXD_UndoStack.asm
❌ RawrXD_CursorTracker.asm
```
**Reason:** No active IDE text editor in build pipeline

#### Syntax Highlighting (3 files - recently created, pending integration)
```
⚠️ RawrXD_MASM_SyntaxHighlighter.asm                    ← Created, NOT compiled into any exe
⚠️ RawrXD_IDE_SyntaxHighlighting_Integration.asm        ← Created, NOT compiled into any exe
✅ config/masm_syntax_highlighting.json                 ← Config file exists, not loaded anywhere
```

#### Network/IPC (4 files)
```
❌ RawrXD_P2PRelay.asm
❌ RawrXD_NetworkRelay.asm
❌ RawrXD_NetworkRelay_New.asm
❌ RawrXD_NetworkSniper.asm
```

#### PE Writer (5 files, blocked)
```
❌ RawrXD_PE_Writer.asm
❌ RawrXD_PE_Writer_Complete.asm
❌ RawrXD_PE_Writer_Core_ml64.asm
❌ RawrXD_PE_Writer_Integration_ml64.asm
❌ RawrXD_PE_Writer_Structures_ml64.asm
```
**Status:** These ARE referenced in Build-PE-Writer-Phase3.ps1, but assembly fails (ml64 syntax errors)

#### Infrastructure/Utilities (8+ files)
```
❌ RawrXD_SelfHealer.asm
❌ RawrXD_SwarmCoordinator.asm
❌ RawrXD_TaskDispatcher_Engine.asm
❌ RawrXD_FullAudit.asm
❌ RawrXD_OmegaDeobfuscator.asm
❌ RawrXD_Complete_Master_Implementation.asm
❌ RawrXD_AutonomyStack_Complete.asm
❌ RawrXD_IntegrationSpine.asm
```

#### GPU/DMA/Hardware (8+ files)
```
❌ RawrXD_GPU_DMA_Expanded_Final.asm
❌ pifabric_chunk_planner.asm
❌ pifabric_core.asm
❌ pifabric_gguf_catalog.asm
❌ pifabric_gguf_loader_integration.asm
❌ pifabric_gguf_memory_map.asm
❌ pifabric_memory_compact.asm
❌ pifabric_memory_profiler.asm
❌ pifabric_pass_planner.asm
❌ pifabric_quant_policy.asm
❌ pifabric_scheduler.asm
❌ pifabric_stats.asm
❌ pifabric_thread_pool.asm
❌ pifabric_ui_*.asm (5+ UI files)
```

---

## PART 3: PRE-COMPILED OBJECT FILES (.obj) NOT LINKED

These .obj files exist but are NOT linked into any production executable:

```
❌ RawrXD_Amphibious_Core2_ml64.obj     (stored separately, not from unified build)
❌ RawrXD_GUI_RealInference.obj
❌ RawrXD_ML_Runtime.obj
❌ RawrXD_ChatService_Agentic.obj
❌ RawrXD_CPUInference_Engine.obj
❌ RawrXD_StreamRenderer_DMA.obj
❌ RawrXD_NativeAgent_Engine.obj
❌ various GPU/DMA .obj files
```

**Usually because:** These are intermediate build artifacts or old versions kept for reference

---

## PART 4: BUILD SCRIPT INVENTORY

### **Canonical (Active) Scripts** ✅
```
✅ Build-Amphibious-ml64.ps1            ([d:\rawrxd\Build-Amphibious-ml64.ps1](d:\rawrxd\Build-Amphibious-ml64.ps1), 191 lines)
✅ RawrXD_IDE_BUILD_Unified.ps1         ([d:\rawrxd\RawrXD_IDE_BUILD_Unified.ps1](d:\rawrxd\RawrXD_IDE_BUILD_Unified.ps1), 208 lines)
✅ Build-PE-Writer-Phase3.ps1           (referenced but blocked due to ml64 errors)
```

### **Legacy/Archive Scripts** ❌
```
❌ Build-Amphibious-Complete-ml64.ps1        (different variant, not canonical)
❌ Build-AutoHeal-Amphibious.ps1
❌ Build-CodexReverse.ps1
❌ Build-CursorFeatures-CLI.ps1
❌ Build-CursorFeatures.ps1
❌ Build-EXE.ps1
❌ Build-Protected-Reverser.ps1
❌ Build-Sovereign-Amphibious.ps1
❌ BUILD_IDE_EXECUTOR.ps1
❌ BUILD_IDE_FAST.ps1
❌ BUILD_IDE_PRODUCTION.ps1
... (30+ more variations)
```

**Count:** 40+ non-canonical build scripts (dead code, legacy attempts)

---

## PART 5: MISSING WIRING DIAGRAMS

### **Syntax Highlighter: NOT WIRED**

Files exist but chain is broken:

```
config/masm_syntax_highlighting.json
    ↓ (not loaded by)
RawrXD_MASM_SyntaxHighlighter.asm
    ↓ (not called by)
RawrXD_IDE_SyntaxHighlighting_Integration.asm  
    ↓ (not invoked by)
RawrXD_TextEditorGUI.asm
    ↓ (never used by)
RawrXD_IDE_unified.asm
    ↓ (never integrated into)
RawrXD_Amphibious_CLI_ml64.exe or RawrXD_Amphibious_GUI_ml64.exe
```

**Missing:** `INIT_MASM_SYNTAX()` call in TextEditorGUI or IDE_unified

---

### **Text Editor: STUB IMPLEMENTATION**

RawrXD_TextEditorGUI.asm exists but is incomplete:

```asm
EditorWindow_HandlePaint PROC FRAME
    .PUSHREG rbx
    .ALLOCSTACK 32
    .ENDPROLOG
    ; 90 lines of code that render... nothing visible
    mov rax, 1  ; just returns success
    ret
```

**Missing:** Actual rendering logic, cursor positioning, text measurement

---

## PART 6: INTEGRATION REQUIREMENTS CHECKLIST

To move unintegrated files into production, each requires:

- [ ] **Source exists** (✅ yes, but many have stubs/incomplete)
- [ ] **Compiles with ml64.exe** (⚠️ partially; PE Writer fails)
- [ ] **Properly linked** (❌ most are NOT linked to any exe)
- [ ] **Called by active code** (❌ most have no call sites)
- [ ] **Telemetry validates** (❌ not applicable, dead code)
- [ ] **No duplicate/competing paths** (❌ many duplicates exist)
- [ ] **Build script updated** (❌ legacy scripts not maintained)

---

## PART 7: UNINTEGRATED FILES BY CONFIDENCE LEVEL

### **High Confidence These Should Be Integrated** (2 files)
```
Priority: ASAP (3-4 hours)
- RawrXD_MASM_SyntaxHighlighter.asm
- RawrXD_IDE_SyntaxHighlighting_Integration.asm
→ Action: Wire to TextEditorGUI, test colors
```

### **Medium Confidence These Might Be Useful** (5 files)
```
Priority: Future (2-4 weeks)
- RawrXD_StreamRenderer_Live.asm (alt to DMA)
- RawrXD_PE_Writer_* (5 files, blocked)
- RawrXD_TextEditor alternative if current stub can't be completed
→ Action: Keep in future_roadmap folder, revisit if direction changes
```

### **Low Confidence These Are Needed** (50+ files)
```
Priority: Archive (consolidation pass)
- All RawrXD_Agentic_* variants
- All RawrXD_Sovereign_* variants
- All Network/IPC files
- All GPU/DMA files
- All pifabric scheduler files
- All competing inference engines
→ Action: Move to ./build_archive/ with manifest
```

---

## DELIVERABLE: INTEGRATION STATUS MATRIX

| File | Compiled | Linked | Referenced | Status |
|------|----------|--------|-----------|--------|
| RawrXD_Amphibious_CLI_ml64.asm | ✅ | ✅ | ✅ Main build script | INTEGRATED |
| RawrXD_Amphibious_Core2_ml64.asm | ✅ | ✅ | ✅ Build script | INTEGRATED |
| RawrXD_StreamRenderer_DMA.asm | ✅ | ✅ | ✅ Build script | INTEGRATED |
| RawrXD_ML_Runtime.asm | ✅ | ✅ | ✅ Build script | INTEGRATED |
| RawrXD_GUI_RealInference.asm | ✅ | ✅ | ✅ Build script | INTEGRATED |
| RawrXD_MASM_SyntaxHighlighter.asm | ✅ | ❌ | ❌ Not called | READY/UNWIRED |
| RawrXD_IDE_SyntaxHighlighting_Integration.asm | ✅ | ❌ | ❌ Not called | READY/UNWIRED |
| config/masm_syntax_highlighting.json | N/A | ❌ | ❌ Not loaded | READY/UNWIRED |
| RawrXD_TextEditorGUI.asm | ⚠️ Stub | ❌ | ❌ Incomplete | STUB/INCOMPLETE |
| RawrXD_PE_Writer_Complete.asm | ❌ Errors | ❌ | ✅ Ref'd but fails | BLOCKED |
| All other RawrXD_*.asm | ⚠️ Varies | ❌ | ❌ None | ORPHANED |

---

## RECOMMENDATIONS

**Immediate (3-4 hours):**
1. Wire syntax highlighter into TextEditorGUI
2. Compile all three syntax files together
3. Test: Open .asm file, verify colors

**Short-term (4-20 hours):**
1. Complete RawrXD_TextEditorGUI stubs OR find working alternative
2. Integrate into IDE_unified.exe

**Medium-term (awaiting decision):**
1. Resolve PE Writer ml64 syntax issues (Path A/B/C)
2. Update Build-PE-Writer-Phase3.ps1

**Long-term (archive pass):**
1. Move 40+ legacy build scripts to ./build_archive/
2. Move 50+ unused .asm files to ./future_roadmap/ with manifest
3. Document future direction (GPU/DMA, network, etc.)

---

**Report Generated:** 2026-03-12  
**Total Unintegrated Files:** 60+ .asm, 40+ build scripts, 1 config  
**Critical Gaps:** Syntax highlighting not wired, text editor stub impl, 60+ orphaned components
