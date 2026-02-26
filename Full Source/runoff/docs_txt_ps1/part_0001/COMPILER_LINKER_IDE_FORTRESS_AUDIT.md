# RawrXD IDE — Custom Compiler & Linker: Full Effect + E→D Consolidation Audit

**Purpose:** Document the full effect of making the IDE its own compiler/linker fortress, and audit/bring back everything from the E: drive into `D:\RawrXD`.

---

## 1. Full Effect of Your Own Compiler and Linker in the IDE

### 1.1 What the IDE Already Has (In-Repo)

| Component | Location | Role |
|-----------|----------|------|
| **RawrCompiler** | `src/reverse_engineering/RawrCompiler.hpp` | JIT compiler & codegen; invokes `g++.exe`/`gcc.exe` for C/C++, can assemble ASM. Used by RE suite. |
| **Reverser compiler** | `src/reverse_engineering/reverser_compiler/` | Self-hosting Reverser language compiler (NASM). Build: `build_reverser.bat`. Some .asm sources may be on E: (see audit). |
| **PE inline compiler** | `src/reverse_engineering/pe_tools/re_tools.cpp` (`run_compiler`) | Inline MASM compilation for PE analysis. |
| **rawrxd-compiler engine** | `src/modules/engine_manager.cpp`, `HeadlessIDE.cpp`, `main_win32.cpp` | Engine ID `rawrxd-compiler`; loads `engines/rawrxd-compiler/compiler.dll` for MASM64/AVX-512. |
| **CLI compiler** | `src/cli/rawrxd_cli_compiler.cpp` | Full CLI (multi-file, -j, exe/dll/lib/obj/asm/ir, x64/x86/ARM64/WASM, watch, JSON). Present in worktree. |
| **Swarm worker toolchain** | `src/core/swarm_worker.cpp` | `getCompilerPath()`: C/C++ → config or `g++`, MASM → `ml64.exe`, NASM → `nasm`. |
| **RegisterCompiler** | `engine_manager.cpp` | `RegisterCompiler(compiler_id, compiler_path)` for agentic access. |
| **Decompiler view** | `Win32IDE_DecompilerView.cpp` | D2D decompiler + disassembly; no compiler invocation. |
| **RawrXD-SCC v4.0** | `src/asm/rawrxd_scc.asm` | ~700-line x64 assembler; emits COFF64 (.obj). Bootstrap: `ml64 rawrxd_scc.asm /link /entry:main /out:rawrxd_scc.exe`; then SCC can replace ml64 (self-hosting). |

### 1.2 What “Your Own Compiler/Linker” Adds to the Fortress

- **Single canonical toolchain under `D:\RawrXD`**  
  All compilers and linkers (MSVC, GCC, MASM, NASM, and any custom RawrXD compilers) can be configured to live under `D:\RawrXD` (e.g. `D:\RawrXD\compilers\`, `D:\RawrXD\toolchains\`). The IDE and scripts then reference only D:.

- **IDE as sole entry point**  
  Build, compile, link, and run can all be triggered from the IDE (command palette, menus, agent). No dependency on “whatever is on E:” for primary workflows.

- **Audit trail and reproducibility**  
  One root (`D:\RawrXD`) for source + toolchains makes it clear what the fortress contains and allows scripts/CI to assume one layout.

- **E: as optional/source-only**  
  E: can be used only as a source to copy from (or as a backup). After consolidation, tests and docs should point to `D:\RawrXD` (see Test-UniversalCompiler.ps1 and this audit).

### 1.3 Recommended Layout Under D:\RawrXD (Fortress)

```
D:\RawrXD\
├── src\                    # All canonical source (from repo + brought back from E:)
│   ├── cli\                # rawrxd_cli_compiler.cpp (already in repo)
│   ├── compiler\           # Qt compiler (bring from E: if desired)
│   ├── asm\                # rawrxd_scc.asm (SCC v4.0), rawrxd_link.asm, solo_standalone_compiler.asm, RawrCodex.asm, etc.
│   └── reverse_engineering\# RawrCompiler, reverser_compiler, omega_suite, etc.
├── compilers\              # Your real, functioning compilers (from E: or installed)
│   ├── msvc\               # Or symlink to VS/BuildTools
│   ├── gcc\                # MinGW/GCC if used
│   ├── nasm\
│   └── masm\
├── toolchains\             # Optional: custom toolchain configs
├── build\                  # Build output
├── bin\                    # rawrxd_scc.exe, rawrxd_link.exe, rawrxd.exe, reverser, re_tools, etc.
└── engines\                # IDE engines (e.g. rawrxd-compiler)
    └── rawrxd-compiler\
        └── compiler.dll
```

---

## 2. E-Drive Audit and “Bring Back to D:\RawrXD”

### 2.1 What the Repo/Docs Say Is on E:

| Referenced path / doc | Description |
|------------------------|-------------|
| `E:\RawrXD` | Root used by `Test-UniversalCompiler.ps1` (RootDir). Should become D:\RawrXD. |
| `E:\RawrXD\src\cli\rawrxd_cli_compiler.cpp` | Same as repo `src/cli/rawrxd_cli_compiler.cpp`; ensure D: copy is canonical. |
| `E:\RawrXD\src\asm\solo_standalone_compiler.asm` | Pure MASM standalone compiler (COMPILER_VERSIONS_SUMMARY.md). Bring to `D:\RawrXD\src\asm\`. |
| `E:\RawrXD\src\compiler\rawrxd_compiler_qt.hpp/.cpp` | Qt IDE compiler. Bring to `D:\RawrXD\src\compiler\` if you want Qt compiler in fortress. |
| `E:\models` | Referenced in multi_engine_system.h, react_generator.cpp (2TB). Optional; not compiler. |
| `E:\Epic Games` | Unreal integration (unreal_engine_integration.cpp). Optional; not compiler. |
| “E:\\ GitHub” (Win32IDE_AirgappedEnterprise.cpp) | RawrXD_Camellia256.asm provenance. If source is on E:, bring to D:\RawrXD. |

### 2.2 What to Audit on E: (Script Below)

- **E:\RawrXD** (entire tree)  
  - Source: every `*.cpp`, `*.h`, `*.hpp`, `*.c`, `*.asm`, `*.rev`, `*.eon`.  
  - Build artifacts: `build\`, `bin\`, `*.exe`, `*.obj`, `*.dll`.  
  - Compilers/toolchains: any `compilers\`, `toolchains\`, `nasm\`, `masm\`, or similar folders.

- **E:\** (optional, if you keep compilers elsewhere)  
  - Folders that look like toolchains (e.g. `E:\nasm`, `E:\mingw`, `E:\VS*`, `E:\RawrXD\compilers`).

The script **Audit-E-Drive-And-BringTo-RawrXD.ps1** (see below) will:

1. List every file under `E:\RawrXD` (and optionally other E: roots you add).
2. Produce a manifest (name, path, size, date) for review.
3. Optionally copy selected categories (e.g. all source, or all compilers) to `D:\RawrXD` with structure preserved.

### 2.3 Files to “Bring Back” (Checklist)

- [ ] **Source**  
  - `E:\RawrXD\src\**\*` → `D:\RawrXD\src\` (merge with repo; avoid overwriting newer in repo).  
  - `solo_standalone_compiler.asm`, `rawrxd_compiler_qt.*`, any `reverser_*.asm` missing from repo.

- [ ] **Reverser compiler**  
  - From RE_ARCHITECTURE: reverser_lexer.asm, reverser_parser.asm, reverser_ast.asm, reverser_bytecode_gen.asm, reverser_compiler.asm, reverser_vtable.asm, reverser_runtime.asm, reverser_platform.asm, reverser_syscalls.asm, bootstrap_reverser.asm, reverser_self_compiler.rev, tests.  
  - If any are only on E: (e.g. under `E:\RawrXD` or `D:\rawrxd\itsmehrawrxd-master\`), copy into `D:\RawrXD` and into repo `src/reverse_engineering/reverser_compiler/` so the fortress is self-contained.

- [ ] **Compilers (real, functioning)**  
  - Any folder on E: that contains `cl.exe`, `g++.exe`, `gcc.exe`, `ml64.exe`, `nasm.exe`, `link.exe` → copy or symlink to `D:\RawrXD\compilers\` (or document path in IDE config).

- [ ] **Engines**  
  - `engines/rawrxd-compiler/compiler.dll`: if built from E: layout, build from D:\RawrXD and place under `D:\RawrXD\engines\rawrxd-compiler\`.

- [ ] **Config / paths**  
  - Replace `E:\RawrXD` with `D:\RawrXD` in scripts and config (Test-UniversalCompiler.ps1, language_model_registry.psm1, tool_server.cpp, etc.).  
  - Keep E: paths only where you explicitly want “optional E: source” (e.g. one optional -RootDir for tests).

---

## 3. Codebase References to Update (E: → D:\RawrXD)

| File | Current | Action |
|------|---------|--------|
| `tests/Test-UniversalCompiler.ps1` | `RootDir = "E:\RawrXD"` | Default to `D:\RawrXD`; allow `-RootDir E:\RawrXD` for legacy. |
| `test_suite.ps1` | Already D:\RawrXD | Keep; ensure no E: as primary. |
| `src/tool_server.cpp` | `D:\rawrxd`, `D:\RawrXD` allowed | Keep D:; add note that E: is not required. |
| `src/win32app/language_model_registry.psm1` | `D:\lazy init ide\compilers` | Consider `D:\RawrXD\compilers` if consolidating. |
| `scripts/language_model_registry.psm1` | Same | Same. |
| `scripts/language_model_integration.ps1` | Same | Same. |
| `docs/COMPILER_VERSIONS_SUMMARY.md` | `E:\RawrXD\...` paths | Update to `D:\RawrXD\...` after consolidation. |

---

## 4. MASM / NASM / C++ Fallback Parity

Custom (non-MASM x64 and non-NASM) builds are required to function the **exact same way** as the real ones:

| Area | Real (MASM/NASM) | Custom / fallback | Parity |
|------|------------------|-------------------|--------|
| **run_compiler** | MASM `ml64` or NASM `nasm -f win64` | Same return shape: success string or `Compilation Errors:\n` + log | `src/re_tools.cpp` and `pe_tools/re_tools.cpp` try MASM first, then NASM; same response format. |
| **deflate** | MASM 3-arg `deflate_masm(src,len,out_len)`; NASM 4-arg with `hash_buf` | C++ stub `deflate_brutal_masm` same 3-arg API, returns nullptr, *out_len=0 | `tests/bench_deflate_masm.cpp` calls correct signature per backend; `codec/deflate_brutal_stub.cpp` matches real API. |
| **ModelBridge / Swarm** | MASM bridge `masm-x64`, profile table from ASM | C++ fallback `cpp-fallback`, static profile list | `tool_server.cpp`: same JSON shape (profiles, profile_count, success, bridge). |
| **RawrCompiler::AssembleASM** | MASM (PROC/ENDP/.code) or NASM (section .text) | Same `CompilationResult` (success, objectFile, errors, compileTimeMs) | `RawrCompiler.hpp` detects syntax and invokes ml64 or nasm; one struct for both. |

**Run parity test:** `.\tests\Test-MASM-NASM-Parity.ps1`

### 4.1 RawrXD-SCC v4.0 (self-hosting assembler)

- **Bootstrap (one-time):** `ml64 rawrxd_scc.asm /link /entry:main /subsystem:console /out:rawrxd_scc.exe`
- **Self-host:** `rawrxd_scc rawrxd_scc.asm` → produces `rawrxd_scc.obj`; link with `rawrxd_link out.exe rawrxd_scc.obj` or MSVC `link` (no ml64 required after first build).
- **Build script:** `toolchain/masm/Build-Fortress-Compiler-Linker.ps1` builds `rawrxd_scc.exe` and `rawrxd_link.exe` into `D:\RawrXD\bin\`.

### 4.2 RAWRXD-LINK v1.0 (link.exe replacement)

- **Location:** `src/asm/rawrxd_link.asm`. Dual x64/x86 (IFDEF RAX): build with `ml64` or `ml /c /coff`.
- **Usage:** `rawrxd_link <out.exe> <obj1.obj> [obj2.obj ...]`. Input: COFF32/COFF64 .obj. Output: PE32/PE32+ .exe.
- **Features:** Multi-object linking, section merging (.text/.data), symbol resolution (COFF symbol table), RVA assignment, PE header + optional header + section headers + raw section data write. Relocations and full import table (kernel32) are stubbed for extension.
- **Custom toolchain:** With RawrXD-SCC and RAWRXD-LINK the fortress can assemble and link without MSVC `ml64`/`link` after one-time bootstrap.

## 5. Summary

- **Full effect:** The IDE becomes a fortress by owning one root (`D:\RawrXD`) for source and toolchains, with its own compiler/linker (RawrCompiler, reverser, CLI, rawrxd-compiler engine, swarm toolchain). E: is no longer required for normal use.
- **Audit:** Run **Audit-E-Drive-And-BringTo-RawrXD.ps1** on your machine to list every file on E: (and optionally copy to D:\RawrXD).
- **Bring back:** Source and compilers from E: → `D:\RawrXD`; update scripts to default to `D:\RawrXD` and document the layout in this file.
- **Parity:** Custom non-MASM/non-NASM builds behave like real ones; run `tests\Test-MASM-NASM-Parity.ps1` to verify.

**Run the audit script** (from repo root or from `D:\RawrXD`):

```powershell
# Audit only — list every file on E:\RawrXD, write manifest to D:\RawrXD\audit_manifest\
.\scripts\Audit-E-Drive-And-BringTo-RawrXD.ps1 -AuditOnly

# Bring source + compiler folders to D:\RawrXD
.\scripts\Audit-E-Drive-And-BringTo-RawrXD.ps1 -CopySource -CopyCompilers

# Bring everything (full tree)
.\scripts\Audit-E-Drive-And-BringTo-RawrXD.ps1 -CopyAll
```

See script header in `scripts/Audit-E-Drive-And-BringTo-RawrXD.ps1` for all parameters.
