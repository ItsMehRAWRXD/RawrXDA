# PE64 IAT Fabricator (Sovereign-Link v22.4.0-IAT)

**Master index:** `docs/SOVEREIGN_MASTER_TEMPLATE_v224.md` (full Sovereign layout + out-of-scope policy).  
**Opcode helpers:** `include/rawrxd/sovereign_emit_x64.hpp` (`FF 15` / `E8` displacement emitters).

## What this is

A **hand-built PE32+ import directory** in NASM (`src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm`): **IDT + ILT + hint/name + IAT** for:

| DLL | Imports |
|-----|---------|
| **KERNEL32.dll** | `ExitProcess`, `GetModuleHandleA`, `LoadLibraryA`, `GetProcAddress` |
| **USER32.dll** | `MessageBoxA`, `CreateWindowExA`, `RegisterClassExA`, `DefWindowProcA` |
| **GDI32.dll** | `CreateSolidBrush`, `CreatePen`, `SelectObject`, `DeleteObject` |

## Honest scope: “no import .lib” ≠ “no linker”

- This object lets you **avoid `kernel32.lib` / `user32.lib` / `gdi32.lib`** for those symbols (the PE’s import table carries the names; the loader resolves them).
- You still need **`link.exe` or `lld-link`** (or your own PE packer) to **merge COFF objects** into a valid **PE file** with MZ/PE headers, section table, and optional header.
- **True zero-linker** end-to-end means emitting the **entire PE** yourself (see `tools/pe_emitter.asm` for a minimal MASM example that writes bytes to disk).

## PE optional header — Data Directory entries

When you place `.idata` at section RVA **`IDATA_RVA`** (e.g. `0x2000`), the **RVA fields in the directory are always relative to `ImageBase`**, not “ILT minus IDT”:

| Index | Name | VirtualAddress | Size (typical for this table) |
|-------|------|----------------|-------------------------------|
| **1** | Import Table | **`IDATA_RVA + (IDT_Start - section_start)`** often equals **`IDATA_RVA`** if `IDT_Start` is first in `.idata` | **80** bytes = 4 × `IMAGE_IMPORT_DESCRIPTOR` (3 DLLs + null) |
| **12** | IAT | **`IDATA_RVA + (iat_k32 - section_start)`** | **96** bytes = 12 × 8-byte slots + null terminators as laid out |

**Size** values must match your actual layout; recompute after any edit.

**Section characteristics:** the loader writes **IAT** slots at load time. In practice, images often place IAT in a **read/write** or **copy-on-write** mapping; if you emit your own section headers, **do not** mark the IAT page as purely read-only unless you know your binder’s behavior.

## Loader model (three-tier)

1. **IDT** — which DLLs, where ILT/IAT/name RVAs live.  
2. **ILT** — original thunks (hint/name RVAs); **static** for the life of the mapping.  
3. **IAT** — **same RVAs as ILT on disk**; loader **overwrites** with real function addresses.

## Calling convention (x64 Windows)

- Provide **32-byte shadow space** before `call` (and keep **16-byte stack alignment** before `call` after the caller’s adjustment).  
- Use **indirect** calls: `call qword [__imp_ExitProcess]`, not `call ExitProcess` as an undefined label.

## NASM build

```bat
nasm -f win64 -o RawrXD_PE64_IAT_Fabricator_v224.obj src\asm\RawrXD_PE64_IAT_Fabricator_v224.asm
```

## CMake (in-tree)

`CMakeLists.txt` includes `cmake/RawrXD_SovereignIAT.cmake` when **NASM** is on `PATH`:

- **`rawrxd_pe64_iat_v224`** — static library = IAT object only.  
- **`rawrxd_sovereign_iat_smoke`** — optional EXE (`EXCLUDE_FROM_ALL`): `RawrXD_Sovereign_MinimalEntry_v224.asm` + IAT, `/subsystem:windows /entry:main /nodEFAULTLIB`.

Build smoke (optional):

```bat
cmake -S . -B build_sov -G Ninja
cmake --build build_sov --target rawrxd_sovereign_iat_smoke
```

## Minimal link (no import .libs, smoke test)

```bat
nasm -f win64 -o ent.obj src\asm\RawrXD_Sovereign_MinimalEntry_v224.asm
nasm -f win64 -o iat.obj src\asm\RawrXD_PE64_IAT_Fabricator_v224.asm
link /nologo /subsystem:windows /entry:main /nodEFAULTLIB ent.obj iat.obj /out:sovereign_smoke.exe
```

You may see **LNK4078** (*multiple `.idata` sections with different attributes*) when splitting IAT + entry across two objects; the smoke CMake target uses `/IGNORE:4078`. A single TU for both IAT + entry removes the warning.

## Verification (acid test)

```bat
dumpbin /imports sovereign_smoke.exe
link /dump /imports sovereign_smoke.exe
```

You should see the three DLLs and the listed names.

## GDI+ (not in GDI32)

**GDI+** flat APIs (`Gdip*`) live in **`gdiplus.dll`**, not `GDI32.dll`. Add a separate import descriptor or resolve via `LoadLibraryA` / `GetProcAddress` using this table.

## Export directory (EAT) — next structural piece

To expose **`RawrXD_Inference_Execute`** (or similar) from a **DLL** for other tools, add an **export directory** (`.edata`), **ordinal/name tables**, and forwarders as needed. That is **orthogonal** to the PE64 IAT fabricator; it is not wired in this batch.

## Out of scope

- **Scripts that edit Cursor `settings.json` or disable telemetry** on arbitrary user profiles — not part of RawrXD build tooling; do not commit.

## Related in-tree

- `tools/pe_emitter.asm` — MASM PE builder that constructs a minimal PE with one import.
