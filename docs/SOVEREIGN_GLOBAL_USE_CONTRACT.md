# Sovereign **global use** contract (Tier G)

This document **reframes** Sovereign headers, examples, and tooling so they are **not** “throwaway lab notes,” but a **deliberate, reusable contract** for anyone building tools on any host OS.

It does **not** contradict **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`**. That file stays the source of truth for **what the RawrXD IDE product ships** and **what MSVC-grade parity is *not* promised** from hand-rolled emitters (**§6**, **§7**).

---

## Two tiers (keep both in your head)

| Tier | What it is | Build / ship expectation |
|------|------------|---------------------------|
| **P — Product (RawrXD IDE)** | The Win32 IDE and its release pipeline | **Windows**: **`cl.exe` + `link.exe` + CRT + PDB + CI** per **§7**. |
| **G — Global (Sovereign contract)** | **Portable** structs, math, examples, analysis tools, tri-format lab emitters, phase-2 PE experiments | **Reusable anywhere**: embed in other apps, teach, bootstrap, cross-compile, run in CI. **Not** a claim that one in-repo C file replaces **`link.exe`** for every production scenario. |

**“Global use”** means: stable **meaning** of types (`RawrSomMinimal*`, fixups, REL32 conventions), **documented** limits, and **honest** scope — not “lab-only, ignore.”

---

## What Tier G *is* for

- **Address-space synthesis** — sections, symbols, relocations, imports as **data** (your “Sovereign Linker Contract”).
- **Cross-host tooling** — macOS/Linux developers generating or inspecting **PE** blobs without installing the full IDE.
- **Education and audit** — PE/ELF/Mach-O **minimal** images (`sovereign_emit_formats.hpp`), **heuristic** checks (`tools/sovereign_check_no_deps.py`).
- **Integration** — third-party tools may depend on **headers + examples** under the repo license; follow semver if APIs are published as a package later.

---

## What Tier G is *not* (unchanged honesty)

- **Bitwise MSVC / `link.exe` parity** (Rich, full load config, PDB pipeline, section “entropy”) as a **maintained guarantee** — still **§7**.
- **Replacing** platform signing, stores, NDK, or Xcode for real mobile **shipping** — still **§1** / gap matrix.
- **Syscall / bypass / “omnibus linker for all OSes”** — still **§6**.

---

## File map (Tier G surface)

| Area | Location |
|------|----------|
| SOM / CIR outlines | `include/rawrxd/sovereign_som_minimal.h`, `sovereign_cir_outline.hpp`, `sovereign_som_outline.hpp`, `som_core.h` |
| Atomic / fabricator / dispatch sketches | `sovereign_fabricator.h`, `sovereign_dispatch_table.h` |
| Tri-format compose | `include/rawrxd/sovereign_emit_formats.hpp` |
| PE experiments | `toolchain/from_scratch/phase2_linker/` |
| REL32 proof | `examples/som_minimal_usage.c` |
| Micro-builder checklist | `docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md` |
| **Emission layer** (aligned PE, metadata, signed-ready semantics) | `docs/SOVEREIGN_EMISSION_LAYER.md` |
| **Tier G requirements** (global vs “lab”) | `docs/SOVEREIGN_TIER_G_REQUIREMENTS.md` |
| **Minimal link** (layout + relocs, from scratch) | `toolchain/sovereign_minimal/rawrxd_minimal_link.c`, `include/rawrxd/rawrxd_minimal_link.h` |
| **Symbol registry** (FNV-1a, weak/defined) | `toolchain/sovereign_minimal/rawrxd_symbol_registry.c`, `include/rawrxd/rawrxd_symbol_registry.h` |
| **Registry + DCE** | `docs/SOVEREIGN_SYMBOL_REGISTRY_DCE.md`, `include/rawrxd/sovereign_global_dce.hpp` |
| Host matrix | `docs/BUILD_HOST_MATRIX.md` |

---

## Header banner language

Headers in **`include/rawrxd/`** use **Tier G** wording: *public contract; IDE product build = Tier P (MSVC)* — not “research only / do not use.”

---

## Related

- **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** — **§6**, **§7**, platform table.
- **`docs/SOVEREIGN_TOOLCHAIN_LAB_ARCHITECTURE.md`** — architecture (same components, Tier G framing).
- **`docs/SOVEREIGN_EMISSION_LAYER.md`** — aligned PE emission, signed-ready, metadata vs **§7**.
- **`docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md`** — PR boundaries.
