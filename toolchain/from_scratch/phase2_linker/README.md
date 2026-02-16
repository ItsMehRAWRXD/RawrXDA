# Phase 2: Linker (PE32+ Generator)

Consumes Phase 1 COFF objects and emits runnable `.exe` (PE32+ with optional header, IAT, and entry-point thunk).

**Phase 2: Production Ready.** Validated end-to-end: link → run → exit 42.

| Check | Status |
| ----- | ------ |
| File size | 1536 bytes (512 headers + 512 .text + 512 .idata) |
| Import directory | RVA 0x2000 mapped to `.idata` |
| IAT initialization | IAT[0] = 0x2048 (Hint/Name RVA) → loader resolves `ExitProcess` |
| Execution | `test.exe` returns **42** (full lifecycle) |

**Verification:** From repo root or `phase2_linker`:

```powershell
python toolchain/from_scratch/phase2_linker/check_imports.py build\test.exe
# Expected: IAT[0]: 0x00000000002048, OK: IAT initialized correctly
```

**Cross-reference (ige / qpl / dav / mea worktrees):** If `check_imports.py` shows `IAT[0]: 0x0000000000000000` and the exe crashes, apply the **tga fix** in `pe_writer.c`: after the ILT (offset 40), write the IAT at offset 56 with the Hint/Name RVA — e.g. `w32(buf+56, hint_name_rva); w32(buf+60, 0);` for IAT[0], then 8 null bytes for IAT[1]. The loader only resolves and overwrites slots that initially contain the hint/name RVA. See the comment block above the IAT write in `pe_writer.c`.

Ready for Phase 3 (C runtime) or immediate use.

## Quick start (copy-paste)

**Step 1 — Configure and build** (run from `phase2_linker`, not from `build`):

```powershell
cd D:\rawrxd\toolchain\from_scratch\phase2_linker
cmake -B build -G Ninja
cmake --build build
```

**Step 2 — Run the linker** (use a **real** path to a `.obj` file; there are no sample .obj files in this repo):

- If you are in **phase2_linker**:

  ```powershell
  .\build\rawrxd_link.exe C:\path\to\your.obj -o out.exe
  ```

- If you are in **phase2_linker\build**:

  ```powershell
  .\rawrxd_link.exe C:\path\to\your.obj -o out.exe
  ```

Do **not** use the literal text `path\to\foo.obj` — that is a placeholder. Replace it with an actual path to a COFF `.obj` (e.g. from the Phase 1 assembler or from `cl /c file.c`).

## Build (zero third-party tools)

From the **phase2_linker** directory, either:

**Option A — out-of-tree build (recommended):**

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Option B — build in a subdirectory:**

```powershell
mkdir build -Force
cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cd ..
```

Produces `build\rawrxd_link.exe`. Do not run `cmake --build .` from the source directory (no `CMakeCache.txt` there).

## Usage from current location

When you are in **`D:\rawrxd\toolchain\from_scratch\phase2_linker\build`**, the executable is in the current directory:

```powershell
# You are here: D:\rawrxd\toolchain\from_scratch\phase2_linker\build
.\rawrxd_link.exe C:\path\to\actual\file.obj -o output.exe
```

**Common mistakes to avoid:**

- ❌ `.\build\rawrxd_link.exe` — from inside `build`, that looks for `build\build\`
- ❌ `rawrxd_link.exe` without `.\` — may not be in PATH; use `.\`
- ❌ `path\to\foo.obj` — literal example path; use a real path to a COFF `.obj`

**Quick sanity check:**

```powershell
Get-Item .\rawrxd_link.exe
.\rawrxd_link.exe
# Shows: "RawrXD Linker Phase 2" usage (no args = usage message)
```

**Debug dump:** `.\rawrxd_link.exe -d obj.obj -o out.exe` prints sections and symbols from the first object.

**Stub verification:** After linking, confirm the stub bytes and disp32 in the file:

```powershell
python verify_stub.py test.exe
```

Expected: `disp32 at 0x0D` = 0x1027, target RVA 0x2038. If the bytes match and the exe still crashes (0xC0000005), the loader is not filling the IAT — the import directory layout is the next place to fix.

**PE validator:** If the linked exe fails with "invalid application", run the diagnostic tool:

```powershell
.\rawrxd_check.exe test.exe
```

It reports DOS/PE signature, COFF machine, optional header magic, **entry point RVA**, **subsystem** (must be non-zero), **subsystem version** (should be ≥ 5.1 for Win64), SizeOfImage, and section layout. Build with the same `cmake --build build` (produces `rawrxd_check.exe` in `build`).

## Validation (MSVC object)

Quick test with an MSVC-generated COFF object:

```powershell
# 1. Create and compile test source (Developer Command Prompt or build_test_obj.bat)
echo int main(){return 42;} > C:\temp\test.c
cd C:\temp
cl /c test.c

# 2. Link
cd D:\rawrxd\toolchain\from_scratch\phase2_linker\build
.\rawrxd_link.exe C:\temp\test.obj -o test.exe

# 3. Check
.\test.exe
echo $LASTEXITCODE   # Expected: 42
(Get-Item test.exe).Length   # e.g. ~1.5 KB
```

**Current status:** Phase 2 complete. Linking produces a ~1.5 KB PE; test.exe returns 42. IAT[0]=0x2048, stack 1MB, __main→stub ret. See `STATUS.md` and `design.md`.

**Test scaffold:** `tests/linker_smoke.c` is a minimal source; `tests/CMakeLists.txt` adds an optional `linker_smoke_test` target when using MSVC.

- Entry point: symbol **`main`** must be defined in one of the objects (e.g. a label `main:` in `.text` or `.text$mn`).
- The linker prepends a minimal C runtime stub that calls `main`, then `ExitProcess(EAX)` (return value in EAX = process exit code).
- Imports: **kernel32.dll** / **ExitProcess** only (extend `pe_writer` for more).

## Getting a test .obj

**Option 1 — MSVC (Developer Command Prompt):**

```powershell
cd C:\temp
"int main(){return 42;}" | Set-Content test.c
cl /c test.c

cd D:\rawrxd\toolchain\from_scratch\phase2_linker\build
.\rawrxd_link.exe C:\temp\test.obj -o test.exe
```

**Option 2 — Phase 1 assembler (when available):**

```powershell
cd D:\rawrxd\toolchain\from_scratch\phase1_assembler\build
.\rawrxd_asm.exe ..\samples\hello.asm -o hello.obj

cd ..\..\phase2_linker\build
.\rawrxd_link.exe ..\..\phase1_assembler\build\hello.obj -o hello.exe
```

**Option 3 — Verify .obj is valid COFF (MSVC):**

```powershell
dumpbin /headers C:\temp\test.obj
# Look for "FILE HEADER VALUES" and "IMAGE_FILE_MACHINE_AMD64"
```

## Layout

- **coff_reader** — Parse Phase 1 .obj (sections, symbols, relocations).
- **pe_writer** — Emit PE32+: DOS stub, PE signature, COFF header, optional header (64-bit), .text, .idata (IDT/ILT/IAT).
- **reloc_resolver** — Apply REL32/ADDR64 fixups in section data.
- **entry_stub** — Stub bytes: `sub rsp,0x28`; `call main`; `mov ecx,eax`; `call [IAT_ExitProcess]`.
- **main.c** — CLI, section merger, symbol resolution, stub patching.

## Extending

- More imports: extend `pe_writer_set_import` / .idata layout for multiple DLLs/functions.
- More sections: merge .rdata/.data from objects and add corresponding PE sections.
- Multiple entry symbols: add `-e name` and resolve that symbol as entry instead of `main`.
