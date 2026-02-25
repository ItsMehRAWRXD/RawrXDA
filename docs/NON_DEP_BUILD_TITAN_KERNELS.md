# Non-Dep (Qt-Free) Build — Titan & Agent Kernels Recovery

The Qt build was stripped into the **non-dep** (Qt-free) Win32 IDE build. This doc summarizes what was missing or broken and what was fixed so the non-dep build can recover.

---

## Build targets

| Target | Role |
|--------|------|
| **RawrXD-Win32IDE** | Main Qt-free IDE executable (no Qt). Uses WIN32IDE_SOURCES + MASM sidebar + optional kernel DLLs. |
| **Phase3_Agent_Kernel** | Optional DLL: KV cache + token generation (MASM + C++ bridge). Built when MASM is available. |
| **RawrEngine** | Separate executable that includes `asm_bridge.cpp` (Titan/ASM stubs). |
| **pocket_lab_turbo** | Optional thermal/turbo DLL (MASM). |

RawrXD-Win32IDE does **not** compile `test_titan_integration.cpp` or `Titan_Bridge.cpp`; those are used only by other targets or tests. The **agent kernel** pieces that matter for the non-dep IDE are:

- **Phase3_Agent_Kernel.dll** (Phase3_Agent_Kernel_Complete.asm + Phase3_Agent_Kernel_Bridge.cpp)
- **asm_bridge.cpp** (only in RawrEngine) — stubs for Titan_LoadModel, LogMessage, etc.

---

## What was missing / broken

1. **Phase3_Agent_Kernel_Complete.asm**  
   - `INCLUDE rawrxd_master.inc` failed: assembler could not find `rawrxd_master.inc`.  
   - **Fix:** CMake now passes `/I${CMAKE_CURRENT_SOURCE_DIR}/include` for the Phase3_Agent_Kernel target’s ASM_MASM files. The file lives in `include/rawrxd_master.inc`.

2. **Phase3_Agent_Kernel_Bridge.cpp**  
   - Stray `return true;` inside `extern "C"` and in void/BOOL functions; DllMain returned incorrectly.  
   - **Fix:** All stray returns removed; DllMain returns `TRUE`.

3. **asm_bridge.cpp** (RawrEngine)  
   - `void` stubs (LogMessage, Titan_*., Math_InitTables, etc.) contained `return true;` → C2562 (void function returning value).  
   - **Fix:** All such `return true;` blocks removed so every stub is a proper void function.

4. **Other Titan ASM** (e.g. RawrXD_Titan_Kernel.asm, Titan_Streaming_Orchestrator.asm)  
   - Many `.asm` files under `src/` and `src/agentic/` use `INCLUDE rawrxd_master.inc`. They are **not** currently part of the RawrXD-Win32IDE or Phase3_Agent_Kernel targets; they are assembled in batch/CI and fail if the MASM include path does not include `include/`.  
   - For any target that assembles these files, add the same MASM include: `/I${CMAKE_CURRENT_SOURCE_DIR}/include` (and ensure `include/rawrxd_master.inc` and its sub-includes exist).

5. **test_titan_integration.cpp / Titan_Bridge.cpp**  
   - Missing `Titan_API.h` or `cpu_inference_engine.h` when built in some configs.  
   - **Locations:** `include/Titan_API.h` and `src/cpu_inference_engine.h` (or `include/cpu_inference_engine.h`) exist. Any target that compiles these must have `include/` and `src/` in the include path; `Titan_Bridge.cpp` used `../cpu_inference_engine.h` — use `cpu_inference_engine.h` with `src/` in the path, or fix the path to match your tree.

---

## Files changed in this recovery

- **src/agentic/Phase3_Agent_Kernel_Bridge.cpp** — Removed stray returns; DllMain returns TRUE.
- **src/asm_bridge.cpp** — Removed all `return true;` from void stubs (Titan, Math, Pipe, Spinlock, etc.).
- **CMakeLists.txt** — Phase3_Agent_Kernel: added ASM include so `rawrxd_master.inc` is found.

---

## Verification

```powershell
cd D:\rawrxd
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RawrXD-Win32IDE
cmake --build build --config Release --target Phase3_Agent_Kernel
```

If Phase3_Agent_Kernel builds, `include/rawrxd_master.inc` and the MASM `/I` are correct. If you add more MASM kernels that use `rawrxd_master.inc`, give them the same include path.

---

## Summary

- **Non-dep build** = RawrXD-Win32IDE (and optional Phase3_Agent_Kernel, pocket_lab_turbo, etc.).
- **Titan agent kernels** in this tree: Phase3_Agent_Kernel DLL (MASM + bridge) and asm_bridge stubs in RawrEngine.
- **Recovery:** Phase3 ASM include path fixed; Phase3 bridge and asm_bridge void-return bugs fixed so the non-dep build can compile and recover.
