# Internal “resolver” strategy — Windows (conceptual, policy-bound)

This note explains **how** one might think about obtaining **`kernel32.dll`** (and other module) entry points **without** a pre-populated **IAT** in the PE image, and why RawrXD still treats **normal imports + `link.exe`** as the supported path.

It is **not** a cookbook for loaders, **not** a syscall table, and **not** an evasion guide. For project boundaries, read **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** (**§3**, **§6**).

---

## 1. Terms

| Term | Meaning |
|------|--------|
| **IAT** | Import Address Table — loader-filled slots your code calls through (`FF 15 [rip+disp32]` etc.). |
| **ILT** | Import Lookup Table — name/ordinal references the loader reads before patching the IAT. |
| **“Internal resolver”** (informal) | Any technique that obtains function pointers **without** relying on a fully linked import directory in *your* image. |

The tri-format **`compose*`** helpers (`include/rawrxd/sovereign_emit_formats.hpp`) emit **minimal** containers; they **do not** implement a Windows **import binder**. Examples **`examples/01_*.cpp`** / **`02_*.cpp`** wrap raw bytes — they do **not** add resolution logic.

---

## 2. What RawrXD already implements (preferred)

| Mechanism | Location | Role |
|-----------|----------|------|
| **Manual PE + IAT/ILT + hint/name** | `tools/pe_emitter.asm`, `src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm` | Standard loader semantics; **`ExitProcess`** and friends resolve through normal import tables. |
| **CMake smoke** | `cmake/RawrXD_SovereignIAT.cmake` | Documented **object → link** path. |
| **Opcode helpers** | `include/rawrxd/sovereign_emit_x64.hpp` | Documented **`FF 15`** / **`E8`** patterns for tests — not a resolver. |

**Recommendation:** If you need **`kernel32.dll`** exports in a real PE, **emit or link a normal IAT** and let **`link.exe`** / the loader do binding.

---

## 3. Conceptual tiers (not a roadmap to ship)

### Tier A — Documented Win32 APIs (always API-bound)

Once **any** valid path exists to call **`GetProcAddress`** and **`LoadLibraryA`** (or **`GetModuleHandleW`**), you can resolve additional exports at runtime **without** extending the static IAT for every function. You still depend on:

- The loader having mapped **known** DLLs, and/or  
- **`LoadLibraryA`** + **`GetProcAddress`** (which themselves require an initial import or bootstrap).

This is normal application design, not “sovereign bypass.”

### Tier B — Bootstrap chicken-and-egg

A **minimal** image might import only **`GetProcAddress`** + **`LoadLibraryA`** (or a tiny set), then resolve everything else manually. That is still **import-table-based bootstrap**, not “no imports forever.”

### Tier C — PEB / export table walking (high risk, not a RawrXD deliverable)

Operating systems document **calling** APIs. Walking the **PEB**, **PE headers**, and **export directories** manually to find **`GetProcAddress`** without imports is a **fragile** technique (build-to-build differences, security products, maintenance cost). RawrXD **does not** implement, test, or document step-by-step **PEB walk** code in-tree as a supported feature.

### Tier D — **Excluded** (see **§6**)

| Approach | Status |
|------------|--------|
| **Direct `syscall` / `sysenter`** to resolve or invoke **`Nt*`** without documented APIs | **Out of scope** — unstable across Windows builds; disallowed narrative for this repo. |
| **“Hash-only” import resolution** framed for stealth / AMSI / EDR evasion | **Out of scope.** |
| **Editing host IDE settings / execution policy** to run unsigned blobs | **Out of scope.** |

---

## 4. How this relates to “unlinked bytes” (examples 02)

- **`examples/02_shellcode_payload_wrap.cpp`** demonstrates **wrapping** arbitrary bytes with **`compose*`** — **no** DLL resolution inside that C++ path.
- If you need **`kernel32.dll`** in the same process model, you either:
  - **Link** a normal PE with imports (recommended), or  
  - Maintain **separate** assembly / tooling that follows **Tier A/B** with clear engineering ownership — **outside** the minimal tri-format compose scope.

---

## 5. Summary

| Question | Answer |
|----------|--------|
| Does RawrXD ship a “manual **`kernel32`** resolver” library? | **No.** |
| What should we use for PE imports? | **IAT + linker** path already in-repo. |
| Can we discuss resolvers abstractly? | **Yes** (this doc). |
| Will we add syscall tables / PEB tutorials as product features? | **No** (**§6**). |

---

## 6. Related documents

| Document | Role |
|----------|------|
| `docs/SOVEREIGN_LOADER_PATTERN_GLOSSARY.md` | Informal names: **Siphon–Slam**, append+**RWX**, **PEB** / **zero-import** sketches (reference / threat vocabulary). |
| `docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` | Production vs lab; **§6** firewall. |
| `docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md` | Tri-format compose scope. |
| `docs/SOVEREIGN_MASTER_TEMPLATE_v224.md` | Windows PE + IAT narrative. |
| `docs/SOVEREIGN_LAB_SYNTHESIS_GETTING_STARTED.md` | Raw-byte wrap examples. |
