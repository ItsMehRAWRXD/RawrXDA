# SINGLE_KB_FINAL_AUDIT

Date: 2026-02-16  
Branches: `cursor/final-kb-audit-bd84`, `cursor/rawrxd-universal-access-dc41`  
Status: Remediation complete for high-priority findings

---

## Scope and interpretation

Because "single KB" can be interpreted two ways, this audit uses both:

1. **Strict single-KiB:** `size == 1024` bytes  
2. **Single-KiB band:** `1024 <= size < 2048` bytes

## Audit summary

- Files in single-KiB band: **624**
- Files at exactly 1024 bytes: **3**
- Code-like files in single-KiB band: **374**
- Files with stub/TODO/placeholder indicators in single-KiB band: **80**
- Suspicious code files (non-doc) in single-KiB band: **44**
- Files in single-KiB band that are >=80% NUL bytes: **23**

Top extensions in the single-KiB band:

- `.h` (93), `.comp` (68), `.md` (65), `.cpp` (62), `.png` (46), `.ps1` (40), `.asm` (40)

---

## Strict single-KiB inventory (exactly 1024 bytes)

| File | Size | SHA-256 | Audit result |
|---|---:|---|---|
| `src/ggml-vulkan/vulkan-shaders/add_id.comp` | 1024 | `9cb609f88d7da55dfbcf030fe4a782f1cb2076a0bb3408ca1258661a28cabd33` | Valid compute shader source |
| `3rdparty/ggml/src/ggml-vulkan/vulkan-shaders/add_id.comp` | 1024 | `9cb609f88d7da55dfbcf030fe4a782f1cb2076a0bb3408ca1258661a28cabd33` | Byte-identical vendored mirror |
| `src/visualization/VISUALIZATION_FOLDER_AUDIT.md` | 1024 | `4cfc9e106645c061e37e934b7e7c4e062d880eddaf9616e98737970d488fc63d` | Documentation-only; no executable risk |

**Strict 1 KiB verdict:** no direct runtime/security defect found in the three exact-1-KiB files.

---

## Findings in the expanded single-KiB band (1-2 KiB)

### High (remediated)

1. **Stack address returned as a persistent instance handle** — FIXED
   - Was: `src/masm/interconnect/RawrXD_Model_StateMachine.asm:25-28` returned `lea rax, [rsp]`
   - Now: returns `xor rax, rax` (NULL). Callers must check for null.

2. **Digestion engine stub returns success while real pipeline is TODO** — FIXED
   - Was: `src/digestion/RawrXD_DigestionEngine.asm:33-35` returned S_DIGEST_OK for non-null args
   - Now: returns `ERROR_CALL_NOT_IMPLEMENTED` (120).

### Medium (documented)

3. **Agentic Vulkan fabric wired to stub** — Intentional; documented in stub.
4. **Iterative reasoning object is no-op placeholder** — Intentional; documented in header.
5. **Reverse-engineered bridge fallback stubs linked in Win32** — By design.
6. **NUL-padded asm artifacts** — Quarantine or clean in separate pass.

### Low

7. **Exact-1-KiB shader duplication** across `src` and vendored `3rdparty` trees — Low risk.

---

## Remediation applied

| File | Change |
|------|--------|
| `src/masm/interconnect/RawrXD_Model_StateMachine.asm` | Return NULL instead of [rsp] |
| `src/digestion/RawrXD_DigestionEngine.asm` | Return ERROR_CALL_NOT_IMPLEMENTED (120) |
| `src/agentic/vulkan/NEON_VULKAN_FABRIC_STUB.asm` | Audit note added |
| `include/agentic_iterative_reasoning.h` | Stub documentation added |
| `VALIDATE_REVERSE_ENGINEERING.ps1` | Use $env:LAZY_INIT_IDE_ROOT or $PSScriptRoot |
| `scripts/RawrXD-IDE-Bridge.ps1` | Portable paths |
| `config/env.paths.json` | Portability note |

---

## Priority remediation plan (remaining)

1. **Quarantine or clean NUL-padded asm placeholders**
2. **Add CI policy checks for small-file quality**
3. **Path migration:** Add $env:LAZY_INIT_IDE_ROOT to remaining auto_generated_methods scripts

---

## Final verdict

- **Exact 1 KiB files:** no direct execution defects.
- **High-priority stubs:** remediated (ModelState, Digestion).
- **Medium-risk stubs:** documented; replace with real impl when ready.
