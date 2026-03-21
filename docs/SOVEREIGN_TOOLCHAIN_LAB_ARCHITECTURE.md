# Sovereign toolchain architecture (Tier G — global contract)

This document describes the **mechanical layers** of a self-contained “builder”: frontends → IR → link → emit. It applies to **MASM / TASM / NASM / C / C++**-style lowering into a common in-memory model.

**Framing:** **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`** — **Tier G** (reusable Sovereign API / tooling) vs **Tier P** (RawrXD IDE product built with **MSVC `cl.exe` + `link.exe` + CRT + PDB** — **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** **§6**, **§7**). Tier G is **for global use** (embed, teach, cross-host); it is **not** a promise of a **full MSVC-class** in-repo linker as **product parity** with **`link.exe`**.

---

## Why this is not “just a PE writer”

A **PE emitter** (e.g. `toolchain/from_scratch/phase2_linker/pe_writer.c`, tri-format `compose*` in `include/rawrxd/sovereign_emit_formats.hpp`) writes **image bytes** from **already-resolved** layout decisions.

A **toolchain** adds **front ends**, a **common IR**, **symbol resolution**, **relocation application**, **archives**, and **diagnostics**. That is **orders of magnitude** more surface area than a minimal image synthesizer.

---

## Minimal layered stack (conceptual)

| Layer | Role |
|-------|------|
| **Front ends** | Parse/assemble/compile **into** a common IR (per language or dialect). |
| **IR / object model** | Sections, symbols, relocations, imports, optional debug metadata. |
| **Preprocessor** | For C/C++ and macro-rich asm: includes, defines, conditional expansion (if targeting compatibility). |
| **Linker** | Resolve symbols, assign RVAs/file offsets, apply relocations, build import/export/relocation directories. |
| **Archive** | COFF `.lib` membership and symbol index (see also **`tools/lib_parser.py`** for **inspection** only). |
| **Emitters** | PE / ELF / Mach-O **from** linked model (existing lab emitters are intentionally minimal). |
| **Runtime policy** | CRT-less vs static CRT vs dynamic — explicit choice; “no CRT imports” is a **heuristic** only (`tools/sovereign_check_no_deps.py`). |

Pipeline (conceptual):

```text
sources → preprocess → parse → IR units → link (resolve + layout + patch) → emit binary
```

---

## What is already in-tree (lab / partial)

| Piece | Location / tool |
|-------|------------------|
| Minimal PE writer + import path experiments | `toolchain/from_scratch/phase2_linker/` |
| Tri-format minimal image synthesis (lab) | `include/rawrxd/sovereign_emit_formats.hpp`, tests |
| IAT / opcode helpers | `include/rawrxd/sovereign_emit_x64.hpp`, `src/asm/sovereign_macros.inc` |
| `.lib` archive **listing** (not a linker) | `tools/lib_parser.py` |
| PE **heuristic** “no base reloc dir / no common CRT DLL” | `tools/sovereign_check_no_deps.py` |

---

## Sovereign Object Model (SOM) vs CIR

In documentation, **SOM** (“Sovereign Object Model”) and **CIR** (“Common Internal Representation”) describe the **same contract**: a **relocatable unit** made of sections, symbols, relocations, and import requests. Front ends lower into this shape; only the linker/emitter need target OS knowledge.

| Component | Role | Rough MSVC parallel |
|-----------|------|---------------------|
| **Unit** (`SOM_Unit`) | One TU’s contributions (`main.cpp`, `logic.asm`). | `.obj` (pre-merge) |
| **Section** | Raw bytes + alignment + name (`.text`, `.data`, …). | Section in COFF object |
| **Symbol** | Name, section index, offset, binding. | COFF symbol table |
| **Relocation** | Patch offset, kind (`REL32` / `ABS64`), target symbol id. | COFF reloc / fixup |
| **Import** | DLL + name/ordinal. | `__imp_*` / import lib input |

## Outline headers (POD contract sketch)

Optional **non-integrated** C++ outlines:

- `include/rawrxd/sovereign_cir_outline.hpp` — `CirSection`, `CirSymbol`, `CirRelocation`, `LinkerContext`
- `include/rawrxd/sovereign_som_outline.hpp` — **`SOM_Unit`**, aliases (`SOM_Section`, `SOM_Symbol`, `SOM_Reloc`, …), `SOM_LinkContext` = `LinkerContext`

**No** CMake target is required to link these into `RawrXD-Win32IDE` unless explicitly wired later.

### Proposed experimental tree (not created as a product)

A future **lab** tree might mirror:

```text
sovereign_builder/   # hypothetical; not a committed product layout
  src/main.c
  som.c / som.h
  frontends/  preproc, asm, c
  linker/     resolve, reloc
  emitter/    pe_writer.c   # e.g. toolchain/from_scratch/phase2_linker
```

Existing **`toolchain/from_scratch/phase2_linker/`** already covers part of **emitter** scope; a full **SOM → link → PE** path would still be large and remains **out of production scope** per **§6**.

---

## Micro-linker “atomic floor” (opcodes + holes)

Below full **CIR/SOM** with named sections, the subtractive minimum is:

1. **Encoder output** — byte blob + list of fixups (`patch_offset`, `target_name`, `REL32` / `ABS64`).
2. **Global map** — symbol name → final RVA (or import slot).
3. **Patch pass** — apply deltas; optional **`.reloc`** / rebasing for PE if VA-dependent slots exist.
4. **Emitter** — wrap in PE / ELF / Mach-O **headers** for the chosen OS.

Pure **C** POD for this contract (host-agnostic; emitter chooses target OS):

- `include/rawrxd/sovereign_som_minimal.h` — `RawrSomMinimalAtomicUnit`, `RawrSomMinimalFixup`, `RawrSomMinimalSection`, `RawrSomMinimalSymbol`, `RawrSomMinimalReloc`, `RawrSomMinimalImport`.

**Worked example (REL32 math, no PE emit):** `examples/som_minimal_usage.c` — two atomic units, one fixup, prints patch bytes.

**Index + registry smoke + topography example:** `examples/sovereign/README.md` — `symbol_registry_smoke.c`, `rawrxd_minimal_link_example.c`, build lines.

**Cross-platform build** of *tools and headers* (not necessarily the full Win32 IDE): see **`docs/BUILD_HOST_MATRIX.md`** and root **`CMakePresets.json`**.

---

## Staged order (if you ever prototype)

Practical order for **capability per effort**:

1. IR + reloc model + linker skeleton (in-memory).
2. One assembler path → IR (even a tiny subset).
3. PE emitter from linked IR (reuse/extend lab emitters).
4. C subset frontend (large).
5. C++ subset (larger).

---

## Related docs

- **`docs/SOVEREIGN_TIER_G_REQUIREMENTS.md`** — G1–G6 checklist (global Tier G vs “lab”); **from-scratch** `rawrxd_minimal_link`.
- **`docs/SOVEREIGN_SYMBOL_REGISTRY_DCE.md`** — FNV-1a **`rawrxd_symbol_registry`** + **`sovereign_global_dce.hpp`**.
- **`docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`** — Section/Symbol/Reloc pipeline checklist; REL32 **+4 vs +5** patch-site note; maps to `RawrSomMinimal*`; points at `pe_writer` / phase2 linker (**not** a full MSVC replacement promise).
- **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** — production vs lab; **§7** MSVC artifacts.
- **`docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md`** — tri-format lab scope.
- **`docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md`** — platform × toolchain honesty.
- **`docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md`** — PR boundaries.
