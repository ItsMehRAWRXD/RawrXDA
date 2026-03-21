# RawrXD tri-format emitter — **safe** architecture (PE64 / ELF64 / Mach-O)

This document defines a **legitimate** cross-platform binary pipeline: one internal representation (IR) + per-target **adapters**, finalized with **normal** linkers and platform signing. It replaces “sovereign bypass” narratives (direct syscalls to evade libraries, editing IDE settings, disabling Gatekeeper, `/etc/hosts` hacks, etc.) — those are **out of scope** for RawrXD.

For **what “production” means** vs aspirational claims — see **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`**.

**Hands-on:** **`docs/SOVEREIGN_LAB_SYNTHESIS_GETTING_STARTED.md`** — raw `.bin` → `compose*` walkthrough.

**Imports / “resolvers” (conceptual only):** **`docs/SOVEREIGN_INTERNAL_RESOLVER_STRATEGY.md`** — why IAT+linker stays canonical; what is **not** shipped (**§6**).

**PR boundaries (Sovereign/binary):** **`docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md`** — technical checklist vs **§6**.

---

## 1. Goals

| Goal | Approach |
|------|----------|
| Same codegen idea for Windows / Linux / macOS | Shared **IR** + target **ABI** layer |
| Runnable, maintainable binaries | **`link.exe`**, **`lld`**, **`ld64`** / **`clang`**, not hand-waving a full dynamic loader |
| Apple platforms | **`codesign`** (ad-hoc or real cert) where the OS requires it |
| Mobile (Android / iOS) | Normal NDK / Xcode pipelines; iOS is **not** “drop a raw Mach-O and run” |

---

## 1b. `TargetManifest` (neutral schema)

Use a small manifest to drive **CMake / compiler flags** — not to hand-roll loaders or syscalls:

| Field | Meaning |
|-------|--------|
| `os` | `windows` \| `linux` \| `macos` \| `android` \| `ios` |
| `arch` | `x86_64` \| `arm64` |
| `objectFormat` | `pe` \| `elf` \| `macho` (usually implied by OS + toolchain) |
| `linker` | `msvc` \| `lld` \| `ld64` \| `ndk` |
| `runtime` | `native-apis` \| `libc` \| `framework-apis` |
| `llvmTriple` | optional, e.g. `aarch64-apple-darwin` |

C++ definition: **`include/rawrxd/sovereign_target_manifest.hpp`**.

Container bootstrap helpers: **`include/rawrxd/sovereign_emit_formats.hpp`** — `emitBlueprintFromManifest` / `emitMinimalHeaderFromManifest` map `TargetManifest` → `EmitBlueprint` → header bytes. **Tri-format compose** (each appends `codeBytes` and patches sizes so the blob is internally consistent — lab only, not a linker):

| Helper | What gets patched |
|--------|-------------------|
| `composeElf64MinimalImage` / `composeElf64MinimalImageFromManifest` | `e_entry`, `PT_LOAD` `p_filesz` / `p_memsz` |
| `composePe64MinimalImage` / `composePe64MinimalImageFromManifest` | Optional `AddressOfEntryPoint`, `SizeOfImage`, `SizeOfHeaders`, `.text` section sizes (no imports) |
| `composeMacho64MinimalImage` / `composeMacho64MinimalImageFromManifest` | `LC_SEGMENT_64` `__TEXT`/`__text` sizes, `LC_MAIN` `entryoff` (not codesigned) |

**Lab blob I/O (file-based, no host introspection):** `include/rawrxd/sovereign_lab_blob_io.hpp` — `readWholeFile`, `tryExtractCoffSection` (short COFF section names), `tryExtractElf64ProgBitsSection` (ELF64 `ET_REL` + `.text`). Use these to load `codeBytes` from a flat `.bin` or from object files, then call the `compose*` helpers. Validation: `tests/test_sovereign_compose_lab.cpp` — `testTriFormatEntryContract()` asserts NOP+RET payloads, ELF `e_entry` / `p_filesz`, **`p_memsz` ≥ `p_filesz`** and page-aligned `p_memsz`, PE `AddressOfEntryPoint` at RVA `0x1000`, **`SizeOfImage` divisible by `SectionAlignment`**, **`CheckSum` == 0** in the PE64 optional header (`+0x40` from optional start), Mach-O **`cputype`** (`0x01000007` x86_64 / `0x0100000C` ARM64 per manifest), `LC_MAIN.entryoff` at file offset **192**, **`sizeofcmds` % 8 == 0**, **`sizeofcmds` == 152 + 24**, plus a **5000-byte** payload branch (ELF/PE/Mach-O multi-page sizing). **`tests/sovereign_header_tests.cpp`** adds overlapping multi-page / AArch64 NOP coverage.

**Mach-O layout note:** `maxprot` and `initprot` in `LC_SEGMENT_64` are **four bytes each**. Emitting them as 64-bit values shifts `LC_MAIN` and breaks `entryoff` offsets; the lab emitter and test encode this correctly.

**Out of scope:** “Universal Binary Assembler,” direct `syscall`/`svc` tables, overwriting Cursor settings, disabling Gatekeeper/AMFI/SmartScreen, or fabricating signed iOS binaries outside normal Xcode/signing flows. **MSVC++**-grade PE extras (**Rich**, full **load config**, **`IMAGE_DEBUG_DIRECTORY`** / **PDB** parity) — use **`cl` + `link.exe`**; see **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` §7**.

---

## 2. Layered architecture

```
[ IR + relocations + symbols ]
        ↓
[ ISA encoder: x86_64 | AArch64 ]
        ↓
[ Object emission: COFF | ELF .o | Mach-O .o ]
        ↓
[ Platform link + sign ]
```

- **Instruction / ISA layer:** opcodes, registers, calling convention (Win64 vs SysV vs Darwin).
- **Object layer:** relocations, sections, debug metadata (optional).
- **Container layer:** only at link time unless you intentionally build a **research** PE/ELF/Mach-O writer (see `include/rawrxd/sovereign_emit_formats.hpp` — **bootstrap bytes only**, not a full product backend).

---

## 3. Adapter responsibilities

| Adapter | Owns |
|--------|------|
| `PEAdapter` | PE sections, import table, TLS, resources, subsystem, entry RVA |
| `ELFAdapter` | `PT_LOAD` segments, dynamic section if needed, GNU_STACK, interpreter path |
| `MachOAdapter` | `LC_SEGMENT_64`, `LC_MAIN` / entry, `__LINKEDIT`, dyld chains, codesign inputs |

Shared across adapters:

- **Virtual layout:** section/segment alignment, file offsets, image base / PIE.
- **Entry point semantics:** PE uses optional header `AddressOfEntryPoint`; ELF uses `e_entry` VA; Mach-O often uses **`LC_MAIN`** (entry offset), not only `mach_header` fields.

---

## 4. Minimal interface (conceptual)

These names are **spec**; implement in C++ as you wire a real backend.

```text
emit_object(target_triple, ir) -> object_bytes
emit_reloc_table(ir) -> reloc_stream
link(image_spec, objects[], libs[]) -> executable
sign(platform_rules, executable) -> signed_executable   // Apple / store policies
```

For the existing RawrXD Windows path, **`cmake/RawrXD_SovereignIAT.cmake`** + NASM objects + `link.exe` already match the **object → link** model.

---

## 5. Syscalls and “zero libc”

- **Linux/macOS:** documenting raw `syscall` tables for **evasion** or “unkillable” user-mode behavior is not a project goal.
- **Windows:** **do not** hardcode `syscall` numbers into shipped tooling; they are **not** a stable public ABI across builds.
- **Recommended:** call documented APIs (`WriteFile`, `exit`, libc on Unix) via normal **imports** or **static** CRT where policy allows.

---

## 6. Related files

| File | Role |
|------|------|
| `docs/SOVEREIGN_LAB_SYNTHESIS_GETTING_STARTED.md` | Tutorial: payload bytes + `compose*` API |
| `docs/SOVEREIGN_INTERNAL_RESOLVER_STRATEGY.md` | Conceptual: imports vs “manual resolve”; policy (**§6**) |
| `docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md` | Technical PR boundaries (Sovereign / binary / host) |
| `examples/01_minimal_nop_synthesis.cpp` | Minimal tri-format compose sample (NOP;RET payload) |
| `examples/02_shellcode_payload_wrap.cpp` | Larger payload or `readWholeFile` → tri-format wrap |
| `docs/SOVEREIGN_MASTER_TEMPLATE_v224.md` | Windows PE + IAT smoke, honest limits |
| `include/rawrxd/sovereign_emit_x64.hpp` | `FF 15` / `E8` helpers for machine-code tests |
| `include/rawrxd/sovereign_emit_formats.hpp` | Minimal header bootstrap (education / tests); **not** a full linker |
| `include/rawrxd/sovereign_lab_blob_io.hpp` | Read `.bin` / extract `.text` from COFF or ELF64 `.o` for lab pipelines |
| `tests/test_sovereign_compose_lab.cpp` | Structural checks for `compose*` + blob I/O |
| `tests/sovereign_header_tests.cpp` | Regression: magics + patched sizes (ELF/PE/Mach-O) |

---

## 7. Out of scope (do not add to the repo)

- Scripts that edit **Cursor** / **VS Code** `settings.json`, wipe `workspaceStorage`, or toggle telemetry as a “lock”.
- **Gatekeeper** / **SmartScreen** / **Defender** suppression.
- **`/etc/hosts`** redirection to block vendor APIs.
- Marketing “universal synthesizer” claims without **build targets** and **tests**.

When a feature is real, add **files + CMake targets + verification** — not checklist-only docs.
