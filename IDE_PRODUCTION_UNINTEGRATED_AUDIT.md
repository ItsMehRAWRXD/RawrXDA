# IDE Production Unintegrated Source Files Audit

**Report Generated:** 2026-03-12  
**Audit Scope:** d:\rawrxd directory  
**Purpose:** Identify all source files (.asm, .json, build scripts) that exist but are NOT integrated into production IDE build pipeline

---

## EXECUTION SUMMARY

**Total RawrXD_*.asm files discovered:** 75+  
**Integrated into production:** 5-7  
**Unintegrated/Orphaned:** 60+  
**Build scripts (non-integrated):** 40+  
**Syntax config files (non-integrated):** 1  
**Status:** ⚠️ CRITICAL GAP - Massive unintegrated inventory  

---

## SECTION A: PRODUCTION INTEGRATION BASELINE

### **STAGE 1: Build-Amphibious-ml64.ps1** (Active)
Currently linked files:
```
✅ RawrXD_Amphibious_Core2_ml64.asm
✅ RawrXD_StreamRenderer_DMA.asm
✅ RawrXD_ML_Runtime.asm
✅ RawrXD_Amphibious_CLI_ml64.asm
✅ RawrXD_GUI_RealInference.asm
```

Output artifacts:
- `build/amphibious-ml64/RawrXD_Amphibious_CLI_ml64.exe` ✅
- `build/amphibious-ml64/RawrXD_Amphibious_GUI_ml64.exe` ✅
- `build/amphibious-ml64/rawrxd_telemetry_cli.json` ✅
- `build/amphibious-ml64/rawrxd_telemetry_gui.json` ✅

### **STAGE 2: RawrXD_IDE_BUILD_Unified.ps1** (Active)
Optional linked files:
```
✅ RawrXD_IDE_unified.asm (if exists)
```

Then invokes: `Build-Amphibious-ml64.ps1` (see Stage 1)

### **STAGE 3: TextEditor Integration** (Partial)
Files in-use:
```
⚠️ RawrXD_TextEditorGUI.asm (loaded but stub implementation)
⚠️ config/masm_syntax_highlighting.json (recently modified per context)
⚠️ RawrXD_IDE_SyntaxHighlighting_Integration.asm (created but not wired)
```

---

## SECTION B: DISCOVERED BUT UNINTEGRATED SOURCE FILES

### **Category 1: Amphibious/Autonomous Runtime Variants** (11 files)

Files that appear to be duplicates, variants, or superseded versions:

```
❌ RawrXD_Amphibious_CLI.exe (dll not .asm)
❌ RawrXD_Amphibious_Complete.exe
❌ RawrXD_Amphibious_Complete_ASM.asm
❌ RawrXD_Amphibious_Full.exe
❌ RawrXD_Amphibious_FullKernel.exe
❌ RawrXD_Amphibious_FullKernel_Agent.exe
❌ RawrXD_Amphibious_Full_Working.asm
❌ RawrXD_Amphibious_ML64_Complete.asm
❌ RawrXD_Amphibious_GUI.exe
❌ RawrXD_AutoHeal_CLI.exe
❌ RawrXD_AutoHeal_GUI.asm
```

**Issue:** Multiple compiled .exe files (not source) + redundant .asm variants  
**Action:** Consolidate or clean up obsolete variants

---

### **Category 2: PE Writer Components** (5+ files, mostly blocked)

```
❌ RawrXD_PE_Writer.asm (blocked: ml64 syntax errors)
❌ RawrXD_PE_Writer.exe (compiled but broken)
❌ RawrXD_PE_Writer_Complete.asm (scaffold, ~60+ syntax errors)
❌ RawrXD_PE_Writer_Core_ml64.asm (Phase 1, unlinked)
❌ RawrXD_PE_Writer_Integration_ml64.asm (Phase 2, unlinked)
❌ RawrXD_PE_Writer_Structures_ml64.asm (Phase 3, unlinked)
❌ RawrXD_PE_Writer_Test_ml64.asm (test harness, not integrated)
```

**Issue:** PE Writer blocked on ml64 syntax constraints (awaiting Path A/B/C decision)  
**Status:** Decision pending (see SPRINT_C_STATUS_REPORT.md)

---

### **Category 3: Agentic/Sovereign Core** (6+ files)

These appear to be attempted autonomous agent/worker systems:

```
❌ RawrXD_AgentHost_Sovereign.asm
❌ RawrXD_AgenticInference.asm
❌ RawrXD_AgenticSovereignCore.asm
❌ RawrXD_Agentic_Bridge.asm
❌ RawrXD_Agentic_Core_ml64.asm
❌ RawrXD_Agentic_GUI_ml64.asm
❌ RawrXD_Agentic_Master.asm
❌ RawrXD_NativeAgent_Engine.asm
❌ RawrXD_Sovereign_CLI.asm
❌ RawrXD_Sovereign_GUI.asm
❌ RawrXD_Sovereign_Core.asm
```

**Issue:** Competing implementations, unclear which is authoritative  
**Potential overlap:** These may duplicate core2_ml64 or offer alternative architectures  
**Status:** Not wired into build pipeline

---

### **Category 4: Inference & Runtime Engines** (8+ files)

```
❌ RawrXD_CPUInference_Engine.asm
❌ RawrXD_MLInference.asm
❌ RawrXD_InferenceAPI.asm
❌ RawrXD_CPUOps_Kernels.asm
❌ RawrXD_InlineStream_ml64.asm
❌ RawrXD_ChatService_Agentic.asm (partially integrated in test)
❌ RawrXD_ContextExtractor_ml64.asm
❌ RawrXD_DiffValidator_ml64.asm
```

**Issue:** Multiple inference paths; core runtime selected but alternatives exist  
**Action:** Audit which path provides best performance/reliability

---

### **Category 5: Streaming & Rendering** (5+ files)

```
✅ RawrXD_StreamRenderer_DMA.asm (INTEGRATED)
❌ RawrXD_StreamRenderer_Live.asm (variant, not linked)
❌ RawrXD_InlineStream_ml64.asm (separate streaming path)
❌ RawrXD_P2PRelay.asm (network streaming, orphaned)
❌ RawrXD_NetworkRelay.asm (competing relay, orphaned)
❌ RawrXD_NetworkRelay_New.asm (newer variant)
```

**Issue:** Multiple streaming paths; only DMA variant in use  
**Action:** Evaluate if Live/P2P offer value vs maintenance burden

---

### **Category 6: Syntax Highlighting** (3 files)

```
⚠️ RawrXD_MASM_SyntaxHighlighter.asm (created, not compiled/integrated)
⚠️ RawrXD_IDE_SyntaxHighlighting_Integration.asm (created, not wired)
✅ config/masm_syntax_highlighting.json (exists, recently modified)
```

**Issue:** Syntax highlighter components exist but not integrated into IDE editor loop  
**Blocker:** No call from TextEditorGUI.asm to initialize/use highlighter  
**Action:** Wire INIT_MASM_SYNTAX() call into editor startup

---

### **Category 7: Text Editor & Buffer Management** (6+ files)

```
❌ RawrXD_TextBuffer.asm (orphaned)
❌ RawrXD_TextEditor_Main.asm (superseded?)
⚠️ RawrXD_TextEditorGUI.asm (PARTIAL - stub impl, 556 lines but incomplete)
❌ RawrXD_InlineEdit_Week1_IntegrationTest.asm
❌ RawrXD_InlineEdit_Keybinding.asm
❌ RawrXD_UndoStack.asm
```

**Issue:** TextEditorGUI.asm has placeholder implementations; real text buffer logic incomplete  
**Observation:** Many procedures just return without actual rendering  
**Action:** Complete TextEditorGUI.asm or replace with working implementation

---

### **Category 8: Network/IPC Components** (4+ files)

```
❌ RawrXD_P2PRelay.asm
❌ RawrXD_NetworkRelay.asm
❌ RawrXD_NetworkRelay_New.asm
❌ RawrXD_NetworkSniper.asm
```

**Issue:** Network stack not integrated; IDE appears to be single-process  
**Status:** Orphaned (no build script reference)

---

### **Category 9: GPU/DMA/Hardware Acceleration** (8+ files)

```
❌ RawrXD_GPU_DMA_Expanded_Final.asm
❌ gpu_dma.obj (compiled object, not source)
❌ gpu_requantize_rocm.obj (ROCm quantizer, orphaned)
❌ pifabric_*.asm (8 files, complete scheduler/tensor system)
```

**Issue:** GPU/DMA layer exists but not wired into inference engine  
**Observation:** Extensive pifabric scheduler exists but unreferenced  
**Status:** Dead code or future layer

---

### **Category 10: Utility/Infrastructure** (12+ files)

```
❌ RawrXD_SelfHealer.asm
❌ RawrXD_SwarmCoordinator.asm
❌ RawrXD_TaskDispatcher_Engine.asm
❌ RawrXD_CursorTracker.asm
❌ RawrXD_FullAudit.asm
❌ RawrXD_OmegaDeobfuscator.asm
❌ RawrXD_Complete_Master_Implementation.asm
❌ RawrXD_AutonomyStack_Complete.asm
❌ RawrXD_IntegrationSpine.asm
❌ RawrXD_ContextExtractor_ml64.asm
```

**Issue:** Infrastructure components exist but no caller references them  
**Status:** Orphaned experimental code

---

## SECTION C: BUILD SCRIPTS (Non-Production, Archive/Legacy)

### **Deprecated/Archive Build Scripts** (40+ files)

```
❌ Build-Amphibious-Complete-ml64.ps1 (not the one being used; different variant)
❌ Auto-complete-IDE.ps1
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
... (30+ more)
```

**Issue:** Massive graveyard of build attempts; only 2 scripts are canonical:
  - `Build-Amphibious-ml64.ps1` ✅
  - `RawrXD_IDE_BUILD_Unified.ps1` ✅

**Action:** Archive/delete unused build scripts or document which era they represent

---

## SECTION D: CONFIGURATION & INTEGRATION FILES

### **Syntax Highlighting Config** (1 file)

```
✅ config/masm_syntax_highlighting.json (exists, valid JSON, recently modified)
```

**Status:** Ready for integration  
**Blocker:** No integration glue in TextEditorGUI.asm

---

## SECTION E: CRITICAL GAPS & ACTION ITEMS

### **GAP 1: Syntax Highlighting Not Wired**
- **Files Created:** RawrXD_MASM_SyntaxHighlighter.asm, RawrXD_IDE_SyntaxHighlighting_Integration.asm, config/masm_syntax_highlighting.json
- **Missing:** Call from TextEditorGUI to initialize tokenizer
- **Impact:** IDE renders text but no syntax colors
- **Fix Time:** 1-2 hours (add initialization hook + compile)

### **GAP 2: TextEditorGUI.asm is Stub Implementation**
- **File:** RawrXD_TextEditorGUI.asm (556 lines)
- **Status:** Mostly procedural stubs returning without action
- **Example:** `EditorWindow_HandlePaint` has 90 lines but doesn't render anything
- **Impact:** No text actually displays in editor
- **Fix:** Either complete impl or replace with working version (~20 hours or find existing)

### **GAP 3: 60+ RawrXD_*.asm Files Unused**
- **Categories affected:** Agentic, PE Writer, GPU/DMA, Network, Utilities
- **Decision needed:** Keep as future options or delete?
- **Recommendation:** Create "future_roadmap.md" + move to .\future_roadmap\

### **GAP 4: PE Writer Blocked (Awaiting Decision)**
- **Status:** 5 PE Writer files exist but assembly fails (ml64 syntax constraints)
- **Decision Point:** Path A (fix ml64), Path B (try masm32), Path C (defer to next sprint)
- **Reference:** SPRINT_C_STATUS_REPORT.md, SPRINT_C_DECISION.md

### **GAP 5: Multiple Competing Inference Paths**
- **Variants:** Agentic_Core, Agentic_Master, Sovereign_Core, AutoHeal_Test_v2, ChatService_Agentic
- **Currently used:** RawrXD_Amphibious_Core2_ml64.asm (5 KB inference)
- **Unused alternatives:** 6+ other inference engines
- **Recommendation:** Document which is authoritative; archive others

---

## SECTION F: SPECIFIC FILES REQUIRING IMMEDIATE ATTENTION

### **Priority 1: Complete/Integrate**
1. **RawrXD_MASM_SyntaxHighlighter.asm** → Compile and wire to TextEditorGUI
2. **RawrXD_IDE_SyntaxHighlighting_Integration.asm** → Ensure event handlers active
3. **config/masm_syntax_highlighting.json** → Ready, just needs initialization call

### **Priority 2: Fix or Replace**
1. **RawrXD_TextEditorGUI.asm** → Complete implementation (~556 lines, mostly stubs)
   - Missing: GDI rendering loop, cursor logic, selection rendering, input handling
   - Option A: Complete the stubs (~20 hours)
   - Option B: Find working TextEditor implementation and swap

### **Priority 3: Await Decision**
1. **RawrXD_PE_Writer_Complete.asm** + 4 related files → Awaiting Path A/B/C decision
2. **SPRINT_C_STATUS_REPORT.md** for context

### **Priority 4: Clean Up / Archive**
1. **All Build_*.ps1 scripts** (except 2 canonical scripts) → Move to ./build_archive/
2. **Competing inference engines** → Move to ./future_roadmap/ with manifest
3. **GPU/DMA non-integrated** → Move to ./future_roadmap/ with pifabric scheduler

---

## SECTION G: INVENTORY TABLE

| Component | File Count | Status | Action |
|-----------|-----------|--------|--------|
| **Integrated** | 5-7 | ✅ Production | Monitor |
| **Syntax Highlighter** | 3 | ⚠️ Ready but unwired | Integrate now |
| **Text Editor** | 6 | ⚠️ Stub impl | Complete or replace |
| **PE Writer** | 5 | ❌ Blocked (ml64) | Await Path decision |
| **Agentic/Sovereign** | 11 | ❌ Unused | Archive/future |
| **Streaming variants** | 5 | ❌ Only 1 used | Archive alternatives |
| **Network/IPC** | 4 | ❌ Unused | Archive |
| **GPU/DMA/pifabric** | 8+ | ❌ Unused | Archive |
| **Build scripts** | 40+ | ❌ Legacy | Archive |
| **Utilities/infra** | 12 | ❌ Unused | Archive |
| **TOTAL ORPHANED** | **60+** | ❌ | **CONSOLIDATE** |

---

## SECTION H: RECOMMENDED CONSOLIDATION PLAN

### **Phase 1: Finalize Syntax Highlighting (2-4 hours)**
```
1. Compile RawrXD_MASM_SyntaxHighlighter.asm via ml64
2. Link into executable
3. Add INIT_MASM_SYNTAX() call to TextEditorGUI startup
4. Add ON_MASM_BUFFER_CHANGED() hook to editor buffer event
5. Test: Open .asm file, verify colors display
```

### **Phase 2: Complete or Replace TextEditor (4-20 hours)**
```
Option A (20h): Complete stubs in RawrXD_TextEditorGUI.asm
Option B (4h): Find/integrate working Win32 text editor component
  - Scour RawrXD-IDE-Final repo for prior work?
  - Search c:\Users\...history for MonacoEditor or other impl
```

### **Phase 3: Archive Unused Components (2 hours)**
```
Create ./build_archive/ and ./future_roadmap/ folders
Move:
  - All Build_*.ps1 scripts (except canonical 2)
  - Unused RawrXD_*.asm files with manifest.json
  - PE Writer files (except current blocked set)
Document: "See roadmap.md for future integration timeline"
```

### **Phase 4: PE Writer Path Decision (TBD)**
```
1. Review SPRINT_C_DECISION.md
2. Select Path A/B/C
3. If Path A: Spend 4-6 hours on ml64 syntax workarounds
4. If Path B: Test masm32.exe (1-2 hours)
5. If Path C: Defer to next sprint, lock decision in sprint docs
```

---

## SECTION I: PRODUCTION READINESS ASSESSMENT

| Criterion | Status | Notes |
|-----------|--------|-------|
| **CLI Executable** | ✅ 27 tokens | Compiles, runs, telemetry valid (stage_mask=63) |
| **GUI Executable** | ✅ 139 tokens | Compiles, runs, telemetry valid |
| **Base IDE** | ⚠️ Partial | IDE unified exists but TextEditor stub impl |
| **Syntax Highlighting** | ⚠️ Ready | Code written, not integrated |
| **Text Rendering** | ❌ Stub | TextEditorGUI missing render logic |
| **PE Writer** | ❌ Blocked | ml64 syntax errors, awaiting decision |
| **Overall** | ⚠️ Partial | Core inference stable; IDE text rendering incomplete |

---

## DELIVERABLES FROM THIS AUDIT

This audit file serves as:
1. **Gap Analysis** - What's missing from production
2. **Consolidation Manifest** - Which files to archive/move
3. **Priority Queue** - What needs immediate attention (syntax integration, text editor fix)
4. **Roadmap** - What awaits decision (PE Writer path) or future work (GPU/network stack)

---

**Next Steps:**  
1. Address Priority 1 items (syntax highlighting integration): ~3 hours
2. Decide on TextEditor approach: 4-20 hours depending on option
3. Consolidate/archive unused components: ~2 hours
4. Make PE Writer path decision (reference SPRINT_C_STATUS_REPORT.md): TBD

---

**Report prepared by:** Copilot Code Auditor  
**Date:** 2026-03-12 12:00:00 UTC  
**Scope:** Complete IDE production integration audit
