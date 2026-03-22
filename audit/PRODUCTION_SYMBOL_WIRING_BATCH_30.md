# Production Symbol Wiring - Batch 30

Batch 30 hardens Win32IDE strict-lane stub detection/enforcement parity and clarifies real-lane messaging to reduce dissolved-symbol masking from naming-pattern gaps.

## Batch 30 fixes applied (15)

1. Updated Win32IDE real-lane status text to avoid implying compatibility link shims are always active.
2. Clarified comments that `win32_ide_link_stubs.cpp` is appended only in explicit non-strict MSVC compatibility lanes.
3. Clarified strict policy comment: `ai_agent_masm_stubs.cpp` is the only explicit stub-pattern exception.
4. Added a guard in `EnforceNoStubs` to fail if target `SOURCES` property is unresolved (`SOURCES-NOTFOUND`).
5. Extended `EnforceNoStubs` detection regex with `/stubs.cpp` basename coverage.
6. Extended `EnforceNoStubs` detection regex with `/stubs.asm` basename coverage.
7. Extended `EnforceNoStubs` detection regex with `/stub_*.cpp` prefix coverage.
8. Extended `EnforceNoStubs` detection regex with `/stub_*.asm` prefix coverage.
9. Extended `EnforceNoStubs` detection regex with `/shim_*.{cpp,asm}` prefix coverage.
10. Added `list(REMOVE_DUPLICATES STUB_VIOLATIONS)` in `EnforceNoStubs` before reporting.
11. Hardened Win32IDE broad compatibility-strip regex to include `/stubs.cpp`.
12. Hardened Win32IDE broad compatibility-strip regex to include `/stubs.asm`.
13. Hardened Win32IDE broad compatibility-strip regex to include `/stub_*.{cpp,asm}` and `/shim_*.{cpp,asm}`.
14. Added strict-lane `FATAL_ERROR` if required MASM bridge provider `src/core/ai_agent_masm_stubs.cpp` is missing.
15. Added `list(REMOVE_ITEM WIN32IDE_SOURCES src/core/ai_agent_masm_stubs.cpp)` before re-append to keep deterministic single-provider wiring; also tightened post-filter strict regex to include `/stubs.(cpp|asm)`/`stub_` variants.

## Why this batch matters

This batch closes remaining strict enforcement gaps where basename/prefix variants (`stubs.cpp`, `stub_*.cpp`, `shim_*.asm`) could evade one branch of policy checks, while keeping the required MASM bridge provider explicitly controlled.

## Remaining for next batches

- Replace history-indirected `src/core/missing_handler_stubs.cpp` with canonical in-tree implementation ownership.
- Continue collapsing non-strict compatibility lanes into explicit presets/options.
- Add deeper symbol-closure validation for RawrEngine/Gold/Inference (map/symbol parity checks similar to Win32IDE).
