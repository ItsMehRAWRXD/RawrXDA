# Phase 3: C Runtime / Library — Spec

Phase 2 links a single `.obj` with one `main` and one import (ExitProcess). Phase 3 extends the fortress toolchain so you can link **multiple objects** and/or provide a **minimal C runtime** (startup, libc stubs, optional .data/.rdata).

## Goals (pick a subset)

1. **Multi-object linking** — Already supported by Phase 2 (multiple .obj on the command line). Validate with two .obj files (e.g. `main.obj` + `util.obj`).
2. **Minimal CRT** — Optional `crt0` or `_start` that does stack alignment, then calls `main`, then `ExitProcess(exit_code)`. Phase 2’s entry stub *is* this minimal CRT; Phase 3 can rename it conceptually and add more startup (e.g. clear BSS, run init array) if needed.
3. **Libc stubs** — Provide weak or real implementations for common symbols so code compiled with MSVC or GCC can link: e.g. `malloc`/`free` (stub to VirtualAlloc or fail), `memcpy`, `memset`, `puts`/`printf` (stub to kernel32 WriteFile or no-op). Start with a **skeleton lib** (one .obj with stub symbols) that the linker can pull in.
4. **More imports** — Extend `pe_writer` to accept multiple DLLs/functions (e.g. kernel32: ExitProcess, GetStdHandle, WriteFile) and grow .idata/ILT/IAT accordingly. Resolve relocations to IAT slots by symbol name.

## Out of scope for “minimal” Phase 3

- Full C library (no full libc).
- C++ runtime (no `_CxxFrameHandler`, no RTTI).
- TLS, exceptions, or complex init/fini.

## Suggested order

| Step | Deliverable | Validation |
|------|-------------|------------|
| 3.1  | Multi-object test: `main.obj` + `helper.obj`, link → exe returns 42 | `.\hello.exe` → 42 |
| 3.2  | Add 1–2 more imports in pe_writer (e.g. GetStdHandle, WriteFile) and optional “console write” in stub or a tiny lib | exe that prints "Hello" then exits |
| 3.3  | Single “skeleton” lib .obj with stub symbols (e.g. `malloc` → no-op or VirtualAlloc), link user .obj + lib.obj | Link succeeds; exe runs |
| 3.4  | (Optional) .data / .rdata in PE: merge .data/.rdata sections, emit in PE, apply relocations | Global variables and string literals work |

## Dependencies

- Phase 1 (assembler) producing valid COFF .obj.
- Phase 2 (linker) producing runnable PE32+ with correct IAT (Hint/Name RVA in slots) and 24-byte stub.

## Notes

- **Self-hosting the linker** (rewriting `rawrxd_link` in assembly) is independent: you can do it after Phase 3 or in parallel. It dogfoods Phase 1 + Phase 2 without requiring a C runtime library.
- **.data / .rdata** can be done as “Phase 2 hardening” (see PHASE2_HARDENING.md) and then reused in Phase 3.

---

*Phase 2 complete. Phase 3 spec v0.1.*
