# RawrXD-SCC v4.0 — Restore Notice

**File `rawrxd_scc.asm` is missing** from this tree. It was part of the Compiler/Linker Fortress work that was deleted.

## What it is

- **RawrXD-SCC v4.0**: ~700-line x64 assembler that emits valid COFF64 (.obj).
- **Build:** `ml64 rawrxd_scc.asm /link /entry:main /subsystem:console /out:rawrxd_scc.exe`
- **Self-host:** After one bootstrap with ml64, `rawrxd_scc rawrxd_scc.asm` produces the .obj; link with `rawrxd_link` or MSVC link.

## How to restore

1. See **FORTRESS_AUDIT_RESTORATION.md** in the repo root for the full audit and restoration checklist.
2. Recreate `rawrxd_scc.asm` in `src/asm/` with:
   - Two-pass assembly (symbol table then emit).
   - COFF64 output (Machine 0x8664, section headers, raw data).
   - Token types: T_EOF, T_ID, T_NUM, T_STR, T_REG, T_DIR.
   - Directives: .code, .data, proc/endp, public/extern, db/dd/dq.
   - Instructions: mov, push/pop, call, ret, add/sub, lea, jmp/je/jne, cmp, xor/and/or.
   - Registers: RAX–R15; Windows API (VirtualAlloc, CreateFileA, ReadFile, WriteFile, CloseHandle, GetStdHandle, WriteConsoleA, ExitProcess, GetCommandLineA).
3. Run **toolchain/masm/Build-Fortress-Compiler-Linker.ps1** to build after the file is restored.

Once `rawrxd_scc.asm` is back, the fortress build script will produce `rawrxd_scc.exe` in D:\RawrXD\bin (or -BinRoot).
