# Fortress Toolchain — From-Scratch Progression

Overall pipeline: **asm → .obj (Phase 1) → .exe (Phase 2) → optional CRT/lib (Phase 3)**.

## Phase 1: Assembler

- **Role:** Produce COFF x64 `.obj` from assembly (and/or from C via external compiler).
- **Location:** Not in this tree; referenced as `phase1_assembler` in docs. Use MSVC `cl /c` or a Phase 1 assembler build elsewhere to obtain `.obj` files.
- **Status:** Assumed available for Phase 2 input. Phase 2 validates against MSVC-generated COFF.

## Phase 2: Linker — COMPLETE

- **Role:** Consume one or more COFF `.obj` files; emit runnable PE32+ with entry stub and kernel32!ExitProcess.
- **Location:** `phase2_linker/`
- **Status:** Production ready. Verified: test.exe returns 42, IAT[0] = Hint/Name RVA (0x2048), stack reserve 1MB, __main → stub ret.
- **Artifacts:** `rawrxd_link.exe`, `rawrxd_check.exe`
- **Docs:** `phase2_linker/design.md`, `phase2_linker/STATUS.md`, `phase2_linker/README.md`

## Phase 3: C Runtime / Library — Pending

- **Role:** Minimal CRT, libc stubs, optional .data/.rdata, multi-import support.
- **Spec:** `PHASE3_SPEC.md`
- **Status:** Not started. Phase 2 is sufficient for asm→obj→exe and for self-hosting the linker.

## Gaps and notes

- **Phase 1 in-tree:** No `phase1_assembler` directory here; use external assembler or `cl /c` for .obj.
- **Tests:** `phase2_linker/tests/linker_smoke.c` provides a minimal C source; optional target `linker_smoke_test` (MSVC) when tests are configured. Manual: `cl /c linker_smoke.c` then `rawrxd_link linker_smoke.obj -o linker_smoke.exe`; run and expect exit 42.
- **Hardening:** `.data`/`.rdata` merging described in `PHASE2_HARDENING.md` for use before or with Phase 3.
