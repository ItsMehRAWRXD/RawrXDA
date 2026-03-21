# Sovereign contributor boundaries (technical)

This document summarizes **technical** expectations for changes that touch **binary emission**, **“sovereign” lab code**, **imports**, or **host integration**. It does **not** replace a general open-source **Code of Conduct** for community behavior; it is a **review checklist** so pull requests stay aligned with **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** (**especially §6**).

---

## 1. Canonical references (read before large changes)

| Document | Use |
|----------|-----|
| `docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` | What “production” means; **§6** rejected work; **§7** MSVC-only artifacts (Rich, **`IMAGE_LOAD_CONFIG_DIRECTORY64`**, **§7.3** section layout / alignment / entropy, **§7.4** **`IMAGE_DEBUG_DIRECTORY`** / PDB). |
| `docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md` | Tri-format **lab** compose scope; out-of-scope list. |
| `docs/SOVEREIGN_INTERNAL_RESOLVER_STRATEGY.md` | IAT vs “manual resolve”; tiers **A–D**. |
| `docs/SOVEREIGN_LAB_SYNTHESIS_GETTING_STARTED.md` | Lab synthesis DX; examples. |

---

## 2. Expectations for reviewers and authors

- **Prefer normal toolchains** for shippable binaries: **`cl` / `clang` + `link.exe` / `lld`**, documented PE/IAT paths (`tools/pe_emitter.asm`, `src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm`, `cmake/RawrXD_SovereignIAT.cmake`).
- **Keep lab code honest:** `compose*` helpers are **bootstrap / education**, not a replacement OS loader or linker.
- **Do not** frame contributions as “bypassing” platform security, signing, telemetry, or IDE policy.

---

## 3. Changes that are unlikely to be accepted

Aligned with **§6** and **`SOVEREIGN_INTERNAL_RESOLVER_STRATEGY.md`** **Tier D**:

| Category | Examples |
|----------|----------|
| **Direct syscall / `sysenter` tables** for user-mode “kernel direct” I/O or hiding imports | x64 syscall number tables, “fresh” `Nt*` invocation without documented APIs |
| **Host lock / IDE manipulation** | Scripts that rewrite **Cursor** / **VS Code** `settings.json`, workspace storage, or telemetry to “seal” the environment |
| **Signing / policy bypass** | Instructions or code whose purpose is to defeat Gatekeeper, AMFI, SmartScreen, Defender, or execution policy as a feature |
| **“Universal linker replacement” claims** without build targets and tests | Marketing-only docs for byte-level omnibus emitters |
| **PEB / export walk as shipped product code** | In-tree libraries whose *stated* purpose is manual `InLoadOrderModuleList` resolution for production RawrXD (see resolver doc — conceptual discussion is fine; push-button loaders are not) |
| **MSVC Rich blob or full `IMAGE_LOAD_CONFIG_DIRECTORY64` / CFG parity** as a maintained universal emitter | **§7** — use **`link.exe`** (and CRT) for production; lab `compose*` does not target this |
| **MSVC-identical section layout, padding, or per-section “entropy” profile** as a maintained universal emitter | **§7.3** — **`link.exe`** owns layout; lab uses fixed documented alignments; no anti-scanner entropy tuning (**§6**) |
| **Full PE debug directory / CodeView / PDB parity** (`IMAGE_DEBUG_DIRECTORY`, **`RSDS`**, `.pdb` matching **`cl`/`link`/`mspdb`**) as a maintained universal emitter | **§7.4** — use **`cl.exe` + `link.exe` + `/Zi`** (or pipeline equivalent); lab `compose*` does not synthesize PDB-grade debug |
| **“Reverse engineered to be EXACT ‘Full’ MSVC++ parity”** as a **shipped** or **maintained** in-repo emitter narrative | **§6** / **§7** — parity is **`cl`/`link`/CI**, not a guaranteed bitwise reimplementation |

---

## 4. Changes that are welcome (when scoped)

| Area | Examples |
|------|----------|
| **Tests & docs** | Regression tests for `compose*`/`sovereign_lab_blob_io`; clarifications in specs |
| **Lab examples** | Additional `examples/*.cpp` that stay header-only or clearly documented; no malware payloads |
| **Honest IAT / PE smoke** | Fixes to `pe_emitter.asm`, fabricator, or `check_imports.py` that improve correctness or diagnostics |
| **Tri-format compose** | Bugfixes and invariants in `include/rawrxd/sovereign_emit_formats.hpp` with tests |

---

## 5. Escalation

If a change sits on the boundary (e.g. security research, reverse-engineering pedagogy), **open an issue first** with:

- Goal (what user problem is solved)
- Why normal toolchains + imports are insufficient
- Acknowledgement of **§6** limits

---

## 6. Summary

**Sovereign** in this repository means **disciplined, inspectable, toolchain-aligned** work — not a mandate to ship syscall bridges, stealth loaders, or host manipulation scripts. PRs should make that **easier to maintain**, not harder.
