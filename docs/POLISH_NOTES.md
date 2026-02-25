# Polish notes (duplicate return / for-loop fixes)

Notes on the hand-applied polish pass for stray `return true`, duplicate returns, and for-loop mangles in `src/win32app/*.cpp`. The bulk script `scripts/fix_duplicate_return_true.ps1` is **disabled** (do not run); it caused control-flow mangles. All fixes are done by hand.

---

## One fix that failed (and why)

- **File:** `src/win32app/Win32IDE_Sidebar.cpp`
- **Location:** `showExtensionDetails()` — `if (details.empty())` block and end of function.
- **Intended fix:** The function is `void`. The block had `return;` inside the `if (details.empty())` and a final `return;` before the closing `}`. The correct shape is: close the `if` with `}` (no early return so we still show "No details available..."), then call `MessageBoxA` and `appendToOutput`, then close the function.
- **Why it wasn’t applied earlier:** The file was reported **busy/locked** (`EBUSY`) when the edit was attempted, so the tool could not write to the file.
- **Status:** Applied in a later pass when the file was no longer locked (edit succeeded on retry).

---

## Pattern to fix elsewhere

- **Stray return:** A line `return true;` (or `return false;` etc.) immediately after another `return <something>;` in the same function. Remove the second line; keep the first.
- **For-loop mangle:** A `for (...) { ... body ... return true; }` where `return true` is inside the loop. Replace with `for (...) { ... body ... }` and, if the function is bool and should return after the loop, add `return true;` (or the appropriate return) after the loop’s `}`.
- **Void functions:** Remove any `return true;` or `return false;` (and duplicate `return;` where it breaks structure). Use plain `return;` only where early exit is intended.

---

## Files already polished (reference)

Commands, main_win32, Win32IDE (helpers), Win32IDE_Commands, Win32IDE_TestExplorerTree, Win32IDE_Tier5Cosmetics, Win32IDE_BackendSwitcher, Win32IDE_AgenticBridge, Win32IDE_SubAgent, Win32IDE_FlagshipFeatures, Win32IDE_Core, Win32IDE_LocalServer, Win32IDE_FailureDetector, Win32IDE_AgentCommands, Win32IDE_VSCodeUI, AutonomousAgent, TodoManager, Win32IDE_SourceFilePicker, Win32IDE_FileOps, Win32IDE_Plugins, Win32IDE_Sidebar (including showExtensionDetails), Win32IDE_Tier1Cosmetics (WndProc + for-loops), Win32IDE_Settings, Win32IDE_ShortcutEditor, Win32IDE_Telemetry, Win32IDE_SwarmPanel.

**Win32IDE_Tier2Cosmetics.cpp** still has many in-block `return true` mangles (e.g. inside Git diff parsing, pipe read loops, switch cases). Those need case-by-case review: replace the erroneous `return true` with the correct control flow (e.g. close the block with `}` or `continue`/`break` as appropriate).

---

## Pre-existing Win32 IDE build-error fixes (2026-02)

Fixes applied so the Win32 IDE build gets further (ninja `RawrXD-Win32IDE` in `build_ide_ninja`):

- **include/rawrxd_telemetry_exports.h**  
  ScopedTimer on Win32 now always includes `<windows.h>` when `_WIN32` is defined (no longer guarded by `_WINDOWS_`), so `LARGE_INTEGER` and the real `QueryPerformanceCounter`/`QueryPerformanceFrequency` are in scope and we avoid extern "C" overload conflicts.

- **src/agentic/DeterministicReplayEngine.h**  
  `WorkspaceSnapshot::fromJson` files-array loop changed to iterator form (`filesArr.cbegin()`/`cend()`) so the compiler treats elements as `nlohmann::json` (fixes "value is not a member of std::pair" in that context).

- **src/agentic/DeterministicReplayEngine.cpp**  
  Removed duplicate/stray `return true` and fixed control flow (Configure void; LoadTranscript; FNV1a64 return hash; ReplayStep; ExecuteToolFromTranscript; CompareToolOutput; CheckTimingAnomaly; SetProgressCallback/SetRecoveryCallback void).

- **src/agentic/autonomous_recovery_orchestrator.cpp**  
  Removed duplicate returns in HotpatchCascade and SourceEdit paths; fixed `return snap` in capture function.

- **src/win32app/Win32IDE_Core.cpp**  
  `runMessageLoop()`: fixed int return (use `return 0`/`return 1` where needed, `continue` in loop instead of stray `return`).  
  `trySendToOllama()`: fixed bool return (all paths now `return true` or `return false`).

- **src/win32app/Win32IDE_SyntaxHighlight.cpp**  
  Removed stray `return;` after `return allTokens;` in `tokenizeDocument`.

- **src/win32app/Win32IDE_NativePipeline.cpp**  
  Restored brace structure in `initNativePipeline`, `loadNativeModel`, and `generateNativeResponse`; fixed void `shutdownNativePipeline` (removed `return true`); fixed `if (m_hwndStatusBar)` blocks that had `return true; }` instead of `}`.

**Still failing (Win32 IDE build):**  
`Win32IDE_AgenticBridge.cpp` — API mismatches: `AgenticEngine::GenerationConfig` (maxMode, deepThinking, deepResearch, noRefusal), `AgenticEngine::chat`, `runDumpbin`, `runCodex`, `runCompiler`, `RawrXD::CPUInferenceEngine::GetContextLimit`; and several “function must return a value” paths in `ExecuteAgentCommand` and `LoadModel`. These require aligning the bridge with the current `agentic_engine.h` and `cpu_inference_engine.h` APIs (or adding the missing members/stubs).
