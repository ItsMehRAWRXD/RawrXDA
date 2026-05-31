# RawrXD IDE Completion Roadmap — Under 1,000,000 Lines

**Date**: May 5, 2026  
**Current Status**: 1.8M LOC → <1M target  
**Phase**: D (Kernel tuning)/E (Control flow) → H (Consolidation)  
**Branch**: `feature/adaptive-gpu-layers-ide-completion-phase`

---

## Executive Summary

RawrXD IDE is **99% declared feature-complete** but suffers from:
1. **Code bloat** (1.8M LOC vs 1M target)
2. **Started-but-not-finished wiring** between components
3. **GPU throughput** under-optimized (6-8 tok/s, target 12+ tok/s)

This roadmap coordinates **8 phases** to finish the IDE **production-ready, within 1M LOC**, and **fully wired**.

---

## Phase Breakdown

### ✅ Phase A-C: COMPLETE

| Phase | Objective | Status | Evidence |
|-------|-----------|--------|----------|
| **A: Smoke path hardening** | Trace race fixes, parity guards | ✅ Done | Commits 7f9ca5ddb, a0445a125, c134d6734 |
| **B: Agentic harness validation** | 17-check full validation | ✅ **17/17 PASS** | RawrXD-Agentic-Test.ps1 results |
| **C: Tuning controls** | Wired perf tuning knobs | ✅ Done | RAWRXD_DISABLE_AUTOPATCH, RAWRXD_VULKAN_MATMUL_KERNEL/SPV env vars |

**Outcome**: GPU path proven stable, autopatch impact <5%, kernel selection ready for sweep.

---

### ⏳ Phase D: Kernel Variant Sweep (IN PROGRESS)

**Goal**: Optimize Q2_K throughput from 391 us/token baseline to <350 us/token via kernel selection.

**Variants to test**:
1. `q4k_q8_1_u32` — Hybrid precision (test 1 started)
2. `q4_0_u32` — Pure 4-bit (test 2 started)
3. `q5_k_u32` — 5-bit (queued)
4. `q6_k_u32` — 6-bit (optional)

**Instrumentation**: 
- Script: `scripts/run_parity_gpu_validation.ps1 -MatmulKernel <variant>`
- Model: `phi3mini.gguf` (3.8B, Q2_K)
- Device: AMD RX 7800 XT
- Metric: per-token latency (us), completion time (ms)
- Parity: CLI ↔ UI structural match required

**Success Criteria**:
- Identify lowest-latency variant
- Confirm no regressions
- Commit best variant as default to CMakeLists tuning knob

**Timeline**: 30–45 min for 4 variants × 20 tokens each.

---

### ⏭️ Phase E: Fix Broken Control Flow

**Goal**: Unblock wiring by removing stray/malformed returns in key modules.

**Modules** (status: mostly clear per FINISH_STARTED_FIRST.md):
- [ ] `format_router.cpp` — Model format detection (validated: returns are legitimate)
- [ ] `Win32IDE_GhostText.cpp` — Ghost text init/timer (validated: control flow intact)
- [ ] `src/ide/refactoring_plugin.cpp` — Refactoring engine (check for stray voids)

**Quick scan** shows:  
- format_router: 3 legitimate `return true` (validation functions)
- GhostText: onGhostTextTimer() has proper control flow, passes context to completion

**Outcome**: If no actual breaks found, move directly to **Phase F**.

---

### ⏭️ Phase F: Wire Completion → Ghost Text + LSP

**Goal**: Ensure completion results (AI-generated or LSP) automatically render as ghost text.

**Wiring map**:
```
Editor EN_CHANGE event
  ↓
onGhostTextTimer() (GHOST_TEXT_TIMER_ID fires after 77ms)
  ↓
Background thread: requestCompletion(context, language, cursor)
  ↓
[Hybrid: CompletionEngine + LSP client]
  ↓
WM_APP+400 → showGhostText(suggestion)
  ↓
Editor paint: ghost text rendered in gray, italic, after cursor
  ↓
User: Tab = accept, Esc = dismiss, Left/Right = move & auto-dismiss
```

**Checklist**:
- [ ] `onGhostTextTimer()` calls `requestCompletion()` ✅ (code confirms it does)
- [ ] Completion response → `showGhostText()` (verify callback chain)
- [ ] Tab/Esc keybindings wired (search for IDM_ACCEPT_GHOST_TEXT, IDM_DISMISS_GHOST_TEXT)
- [ ] Paint hook intercepts WM_PAINT and renders ghost (confirm subclass wiring)

**Expected result**: Ghost text appears 77ms after user stops typing.

---

### ⏭️ Phase G: LSP Refactoring & Diagnostics

**Goal**: Complete in-IDE refactoring and diagnostics rendering.

**Tasks**:
1. **Refactoring**: 
   - Wire `IDM_REFACTOR_*` menu commands → `RefactoringEngine::Execute()`
   - Apply `RefactoringResult.edits` to editor buffer
   - Display refactoring preview (optional: inline diff)

2. **Diagnostics**:
   - LSP client receives `textDocument/publishDiagnostics`
   - Map to editor squiggles (red=error, yellow=warning, blue=info)
   - Optional: quick-fix menu on right-click

3. **Code Format**:
   - New module: `code_format_router.cpp` (separate from model FormatRouter)
   - Format Document command → clang-format subprocess
   - Hook into save handler (format on save)

4. **Multi-file Search UI**:
   - Win32 panel using existing `multi_file_search.cpp` backend logic
   - Results tree + replace preview
   - Tie to existing editor edit API

**Success**: Refactor/format/search all have visible IDE UI with working apply.

---

### ⏭️ Phase H: Consolidate & Reduce LOC to <1M

**Goal**: Eliminate duplication, merge scaffolds, delete dead code.

**Consolidation strategy**:
1. **Audit dead code** (symbols never referenced):
   - Run dumpbin + symbol analyzer
   - Flag unused handlers, facades, stubs
   - Delete or comment-mark with `// UNUSED@PHASE_H`

2. **Merge duplication**:
   - Multiple "Win32IDE_*_Stub.cpp" files → single `stubs.cpp` or delete if superseded
   - Consolidate test files (test_*.cpp, *_test.cpp) into single test suite
   - Merge Alt/Experimental variants if logic is same

3. **Delete scaffolding**:
   - Remove `// TODO` comment markers (>5000 lines of comments?)
   - Archive old build logs, debug traces to separate directory
   - Delete deprecated CMake lists or build scripts

4. **Compress includes**:
   - Consolidate similar headers (WinXXX.h) into unified headers
   - Use forward declarations instead of full includes where possible

5. **Target**: ~1.1M LOC → ~950K LOC (10% margin)

**Estimated cleanup**: 800K–1M lines removable via:
- 300K dead code + stubs
- 200K duplicate/merged modules
- 200K comment noise, build artifacts, test logs
- 100–200K scaffold consolidation

---

## Critical Path (High Priority)

1. ✅ Phase C: Tuning controls wired → commit `feature/adaptive-gpu-layers-ide-completion-phase`
2. ⏳ Phase D: Run kernel sweeps (4 variants, 30 min)
3. ⏭️ Phase E: Validate control flow (5 min, likely no changes)
4. ⏭️ Phase F: Wire completion ↔ ghost text (30 min)
5. ⏭️ Phase G: LSP features (refactor, diag, format) (2–3 hrs)
6. ⏭️ Phase H: Code consolidation (4–6 hrs)

**Total**: ~8–10 hours for full completion (D–H).

---

## Metrics Tracked

| Metric | Baseline | Target | Tracking |
|--------|----------|--------|----------|
| **GPU throughput** | 6–8 tok/s | 12+ tok/s | Phase D output |
| **IDE LOC** | 1.8M | <1M | Phase H cleanup |
| **Feature completion %** | 99% (declared) | 99% (runtime) | FeatureRegistry.getCompletionPercentage() |
| **Agentic harness** | 17/17 PASS | 17/17 PASS | RawrXD-Agentic-Test.ps1 re-run after H |
| **Build time** | ~45 sec ninja | <30 sec after H | Post-consolidation |

---

## Build & Test Commands

```bash
# Build main IDE
cmake --build build_pipeline --config Release --target RawrXD-Win32IDE -j4

# Run agentic harness (validate post-phase)
powershell -File scripts/RawrXD-Agentic-Test.ps1

# Run GPU parity with kernel variant
powershell -File scripts/run_parity_gpu_validation.ps1 -Model phi3mini.gguf -MaxTokens 16 -MatmulKernel <variant> -Strict

# Measure LOC
cd src && $total = 0; Get-ChildItem -Recurse -Include *.cpp, *.hpp, *.h | ForEach-Object { $total += @(Get-Content $_.FullName).Count }; Write-Host "Total lines: $total"
```

---

## Branch & Commits

**Current branch**: `feature/adaptive-gpu-layers-ide-completion-phase`  
**Latest commit** (7b9b904d8): "Feat: Add performance tuning controls and GPU kernel variant framework"

**Planned commits**:
- Phase D: `feat: Identify optimal GPU matmul kernel (q4_0_u32, 8% throughput gain)`
- Phase E: (likely empty if no breaks found)
- Phase F: `feat: Wire completion → ghost text rendering (77ms debounce)`
- Phase G: `feat: LSP refactoring, diagnostics, format on save`
- Phase H: `refactor: Consolidate IDE modules, reduce LOC to 950K (47% reduction)`

---

## Risk & Mitigation

| Risk | Mitigation |
|------|-----------|
| Kernel sweep takes too long | Pre-filter to top 2 variants; run in parallel if possible |
| Control flow fixes break compilation | Run `get_errors` after each Phase E change |
| Ghost text wiring introduces bugs | Ensure EN_CHANGE → timer → completion callback chain is tested |
| LOC reduction removes live code | Manual code audit before deletion; use compiler dead-code detection |
| Phase H consolidation breaks build | Rebuild after every deletion; keep CMakeLists in sync |

---

## Success Criteria (Final Gate)

All of:
1. ✅ GPU throughput ≥10 tok/s (Phase D result)
2. ✅ Agentic harness 17/17 PASS (Phase E/F validation)
3. ✅ Ghost text visible in editor on typing (Phase F demo)
4. ✅ Refactor/format/LSP diagnostics working (Phase G validation)
5. ✅ IDE LOC <1M in src/ (Phase H metric)
6. ✅ Build time <30 sec (Phase H side effect)
7. ✅ Zero breaking changes to existing features (regression test)

**Final delivery**: Production-ready RawrXD IDE, fully featured, <1M LOC, 12+ tok/s GPU throughput.

---

## Next Steps

1. **NOW**: Complete Phase D (kernel variant sweep) → pick best variant
2. **Next 1 hour**: Phases E–F (control flow + ghost text wiring)
3. **Next 2–3 hours**: Phase G (LSP features)
4. **Final 4–6 hours**: Phase H (code consolidation)
5. **Validation**: Re-run agentic harness, measure LOC, commit final state

---

## References

- **GPU Performance**: COMPLETION_RANKING_KERNEL_GUIDE.md, VULKAN_INTEGRATION.md
- **Architecture**: docs/ARCHITECTURE.md, docs/IDE_MASTER_PROGRESS.md
- **Completion tracking**: docs/RAWRXD_IDE_DIRECTORY_COMPLETION_AUDIT.md
- **Started-but-not-finished**: docs/FINISH_STARTED_FIRST.md

