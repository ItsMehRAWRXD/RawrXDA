# RawrXD Sovereign Master Template v22.4.0 — Consolidated Spec (In-Repo Truth)

This document **consolidates** what exists in the repository today, what is **physically possible**, and what is **out of scope**. It replaces narrative “audit complete” checklists that describe capabilities **not** implemented as code.

**Production vs fantasy:** **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`**.  
**All-platform gap table (honest):** **`docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md`**.

---

## 1. Component map (live sources)

| Layer | Role | Location |
|-------|------|----------|
| **PE header / optional header / sections (minimal PE writer)** | Builds a runnable PE32+ in a buffer (MASM tool) | `tools/pe_emitter.asm` |
| **Manual import directory (IAT/ILT/IDT, NASM COFF)** | Supplies `.idata` as an object; **no import .lib** for listed APIs | `src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm` |
| **Minimal entry (smoke)** | `MessageBoxA` → `ExitProcess`; proves IAT path | `src/asm/RawrXD_Sovereign_MinimalEntry_v224.asm` |
| **CMake wiring** | NASM → objects; **direct `link.exe`** for smoke (no implicit `kernel32.lib`) | `cmake/RawrXD_SovereignIAT.cmake` (included from root `CMakeLists.txt`) |
| **IAT / PE documentation** | RVAs, data directories, honest linker note | `docs/PE64_IAT_FABRICATOR_v224.md` |

There is **no** checked-in `pe_header_logic.inc` + `iat_logic.inc` pair as a single **MASM** “master” TU: the repo uses **MASM** for `pe_emitter.asm` and **NASM** for the IAT fabricator. Unifying into one assembler dialect is a **follow-up** (either port IAT to MASM or port PE emitter to NASM).

---

## 2. “Zero-linker” — precise meaning

- **Achievable:** Emit a PE **without** `link.exe` only if you implement a **full** on-disk PE layout (headers + section raw data + checksums as needed), e.g. extending `tools/pe_emitter.asm` or a dedicated emitter.
- **Not achieved by IAT alone:** Manual `.idata` removes **import library** dependency for those symbols; **COFF objects still must be merged** into a PE by `link.exe`/`lld-link` **or** your own writer.
- **~4KB binary** with real **window + message loop + GDI+ + local AI + ghost text** is **not** a credible single-binary claim: GDI+ pulls **`gdiplus.dll`** (and startup/shutdown), UI stacks need substantial code/data, and an “AI completion engine” implies model/runtime I/O — far beyond a 4KB toy PE.

---

## 3. IMAGE_OPTIONAL_HEADER64 — Data directories (imports + IAT)

Values are always **RVA** = offset from **`ImageBase`** to the start of the structure in the mapped image.

- **Directory [1] — Import Table**  
  - `VirtualAddress` = RVA of first `IMAGE_IMPORT_DESCRIPTOR` (your `IDT` / first descriptor).  
  - `Size` = byte size of the **whole** import table region the loader walks (descriptors + thunks + names + terminating zero descriptor), not “one struct”.

- **Directory [12] — IAT** (bound import / loader bookkeeping; optional but recommended when exposing a distinct IAT range)  
  - `VirtualAddress` = RVA of the **first IAT thunk slot** you expose (e.g. start of `iat_k32`).  
  - `Size` = span of all **writable/import** slots you want described (per your layout).

If `.idata` starts at section RVA **`S`**, and `IDT` begins at offset **`o_idt`** from section start, then:

`Import Directory VirtualAddress = S + o_idt` (same rule for IAT: `S + o_iat`).

---

## 4. Calling through the IAT (x64)

- Use **indirect** calls: `call qword ptr [__imp_Symbol]` (MASM) / `call qword [__imp_Symbol]` (NASM).
- Obey **Windows x64 ABI**: **32-byte shadow space** and **16-byte stack alignment** before `call`.
- **`FF 15 disp32`** encodes `call qword ptr [rip+disp32]`. Displacement is **`target_abs - (rip_at_next_instruction)`** where `target_abs` is the **absolute** address of the IAT **slot** (not the function).

C++ helper (also in `include/rawrxd/sovereign_emit_x64.hpp`):

```text
rip_after = address_of_first_byte_after_instruction  (for FF 15: current_rip + 6)
disp32    = int32_t(iat_slot_address - rip_after)
```

---

## 5. GDI+ vs GDI32

**GDI+** APIs are **`gdiplus.dll`** (flat `Gdip*` entry points + `GdiplusStartup` / `GdiplusShutdown`). They are **not** satisfied by importing only `GDI32.dll`. Extend the **import manifest** (extra descriptor) or resolve dynamically via `LoadLibraryA` / `GetProcAddress` from **KERNEL32** imports.

---

## 6. Dynamic emitter / section synthesizer / `.rsrc` / PEB resolver

The long MASM/C fragments in external writeups are **design sketches**. In this repo they are **not** completed, tested subsystems unless a matching file exists under `src/`, `tools/`, or `docs/` with build wiring.

**Planned / future (not claimed done here):**

- **Dynamic section table** emitter (arbitrary count of `.text` / `.rdata` / `.data` / `.rsrc` / `.reloc`).
- **Resource directory** (`.rsrc`) fabricator: `IMAGE_RESOURCE_DIRECTORY`, type/name/language IDs, aligned leaf nodes.
- **PEB walking** to find `kernel32` / `GetProcAddress` without imports: **sensitive** (malware-adjacent). We do **not** ship a “total independence from IAT” PEB resolver as a product feature; document only if strictly for **defensive / educational** context in a controlled doc.

---

## 7. Out of scope — do not add to this repository

The following have **no** place as checked-in “Sovereign” tooling:

- PowerShell that edits **`%AppData%\Cursor\User\settings.json`** for arbitrary users, deletes **`workspaceStorage`**, kills **Cursor** processes, or toggles **telemetry** flags as a “lock”.
- **`Add-MpPreference -ExclusionPath`** or other **Defender** suppression.
- **SmartScreen** / **AppHost** registry tweaks to silence warnings.
- **Self-signed “injector”** logic marketed to **bypass SmartScreen** or reputation systems.

Use normal **code signing** and enterprise **policy** for distribution and trust.

---

## 8. Build commands (reference)

```powershell
# Optional smoke EXE (NASM + link.exe, no import .libs on link line)
cmake -S . -B build -G Ninja
cmake --build build --target rawrxd_sovereign_iat_smoke
```

```bat
nasm -f win64 -o iat.obj src\asm\RawrXD_PE64_IAT_Fabricator_v224.asm
nasm -f win64 -o ent.obj src\asm\RawrXD_Sovereign_MinimalEntry_v224.asm
link /nologo /machine:x64 /subsystem:windows /entry:main /nodEFAULTLIB ent.obj iat.obj /out:sovereign_smoke.exe
```

---

## 9. Honest status line

**Consolidated:** PE writer (MASM) + manual IAT (NASM) + smoke + CMake + PE docs + opcode helper header.  
**Not consolidated:** single-assembler “master” TU; full dynamic meta-emitter; `.rsrc`; GDI+ import table; AI/ghost-text runtime in a sub-4KB binary.

When you implement the next slice, add **real files** and **build targets**—not markdown-only “[x] Done” audits.

---

## 10. Cross-platform tri-format bootstrap (new)

A minimal tri-format container abstraction now exists in:

- `include/rawrxd/sovereign_emit_formats.hpp`

**Legitimate multi-OS plan (adapters + normal link/sign):** see **`docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md`** — IR → object → **link.exe / lld / ld64** → **codesign** where required. Neutral target description: **`include/rawrxd/sovereign_target_manifest.hpp`** (`TargetManifest`). Raw hand-written containers stay **experimental** unless completed like the PE smoke path.

What it provides:

- `BinaryFormat` selector: `Pe64`, `Elf64`, `MachO64`
- `TargetArch` selector: `X64`, `Arm64`
- Shared `EmitBlueprint` + `SegmentBlueprint` data model
- `emitMinimalHeader(...)` to synthesize boot headers for each container format (ELF path includes a **single `PT_LOAD` program header** stub; sizes default to **0** — patch for real images)
- Helper primitives: alignment + little-endian appenders

What it does **not** claim:

- Full loader-complete binaries for ELF/Mach-O (no complete dynamic / dyld / `LC_MAIN` graph)
- Runtime bypass scripts, host security tampering, or IDE profile mutation
- “No linker needed everywhere” claims without complete per-format image layout writers
