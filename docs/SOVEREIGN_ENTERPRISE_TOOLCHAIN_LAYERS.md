# Sovereign toolchain — four layers (ingest → validate → optimize → emit)

This document maps **Ingest**, **Validation**, **Optimization**, and **Emission** to **concrete modules** in-repo, with **honest scope**: Tier **G** (reusable lab / sovereign tooling) vs production **P** (MSVC-class product — see **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`** §6 / **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`**).

---

## 1. Ingest layer — `.obj` (COFF), `.asm`, `.a` / `.lib`

| Input | Status | Module / notes |
|--------|--------|------------------|
| **COFF `.obj`** (AMD64) | **Implemented** (phase2) | `toolchain/from_scratch/phase2_linker/coff_reader.c` — sections, symbols, relocations. |
| **Assembler `.asm` → `.obj`** | **Pipeline** (phase1 → phase2) | `toolchain/from_scratch/phase1_assembler/` emits COFF; phase2 links. Same repo; not a single “one-shot” CLI that ingests all three extensions in one argv list **yet** — compose: `asm → obj`, then `link obj …`. |
| **Static archive `.a` (GNU ar)** / **`.lib` (COFF import / static)** | **Specified, not unified in one driver** | **GNU ar**: parse global header `!<arch>\n`, members, extract `*.o` COFF payloads → existing `coff_read_file` per member. **MSVC static `.lib`**: often COFF objects + symbol index — separate reader. **Planned** as `archive_reader.c` + CLI glue. |

**Enterprise posture:** one **link driver** eventually accepts a **mixed argv** of paths (`.obj`, `.a`, `.asm`) by classifying extensions, running **asm → obj** for `.asm`, **extracting COFF** from archives, then **one merged link** — same as `clang`/`link` ergonomically.

---

## 2. Validation layer — REL32 within x64 reach (±2 GiB)

| Concern | Status | Module |
|---------|--------|--------|
| **REL32** fits **signed int32** (informally ±2 GiB) | **C++ helpers** | `include/rawrxd/sovereign_rel32_validate.hpp` — `rel32DisplacementFits`, `encodeRel32Displacement`, `std::expected`. |
| **Same check in phase2 (C)** | **Implemented** | `toolchain/from_scratch/phase2_linker/rel32_validate.h` + **`reloc_resolver.c`** fails link with stderr if delta out of range (fixes silent **unsigned wrap**). |
| **Patch-site convention** (+4 vs +5) | **Documented** | `docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`, `examples/som_minimal_usage.c`. |

**Note:** Full **image** validation (all fixups after layout) belongs in a **post-layout pass**; phase2 validates at **apply** time.

---

## 3. Optimization layer — dead code elimination (DCE)

| Technique | Status | Notes |
|-----------|--------|--------|
| **Unreachable functions stripped** | **Design / backlog** | Requires **call graph** (symbols + reloc targets + entry symbol), **COMDAT** / `.text$*` folding for MSVC-style objects, and **LTO**-style visibility — not yet in `rawrxd_link`. |
| **Practical near-term** | **Documented** | Per-translation-unit **strip** at asm/C compile time; link-time **GC** (`/OPT:REF` analogue) as future `opt_pass.c`. |

**Enterprise posture:** DCE is a **separate pass** after ingest + symbol resolution, before final PE layout, with **diagnostics** (which symbols removed, why).

---

## 4. Emission layer — aligned PE, metadata, “signed-ready”

| Feature | Status | Module |
|---------|--------|--------|
| **PE32+** sections, imports, entry | **Implemented** | `toolchain/from_scratch/phase2_linker/pe_writer.c`, `import_builder.c`. |
| **Section / file alignment** | **Implemented** | `FILE_ALIGN` / `SEC_ALIGN` in `pe_writer.c`. |
| **Debug directory / RSDS (PDB path)** | **Implemented** | `toolchain/from_scratch/phase3_imports/debug_directory_builder.c`, `-pdb` in `main.c` — see `phase2_linker/README.md`. |
| **Base relocations `.reloc`** | **Partial / blueprint** | Blueprint in `SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`; full ASLR reloc list not guaranteed in minimal linker. |
| **Authenticode / “signed-ready”** | **Metadata-ready, signing out-of-band** | PE **CheckSum** field optional; **Certificate Table** (data dir index 4) for Authenticode is **not** populated by phase2 — real signing uses **`signtool` / CI** on emitted PE. “Signed-ready” = **aligned, valid PE on disk** + documented hook for post-link signing. |

---

## Build matrix

| Target | Command |
|--------|---------|
| Phase2 linker | `cmake -B build -G Ninja && cmake --build build` from `toolchain/from_scratch/phase2_linker` |
| C++ consumers | Include `sovereign_rel32_validate.hpp` / future `sovereign_toolchain_layers.hpp` |

---

## Related docs

- `docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md` — Tier G vs P.  
- `docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md` — mechanical PE checklist.  
- `docs/SOVEREIGN_TOOLCHAIN_LAB_ARCHITECTURE.md` — lab pipeline.  
- `include/rawrxd/sovereign_toolchain_layers.hpp` — layer IDs + pointers to headers.
