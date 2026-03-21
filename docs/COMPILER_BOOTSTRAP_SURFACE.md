# Compiler bootstrap surface — grass on the ground

This doc names a **deliberate** way to start a compiler (or codegen experiment) on RawrXD’s **Sovereign Tier G** stack.

## Metaphor (read literally)

**Grass** is not a tree: it does **not** send anything **down** (no “roots” story) and it does **not** climb **up** (no endless vertical stack). It **sits on top of the place it is built in**—a **thin surface** on **that** ground, **lateral**, **where it is**. The ground is what you **engineer** (layout, relocs, container); the grass is what **lies on** it (closed emit templates, tests you can run). Neither layer pretends to be the other.

So: **compiler bootstrap here is horizontal**—bytes and symbols you lay **on** the Tier G substrate—not a depth-first dig into a full ISA and not a tower of abstractions “above” the work.

## Ground (the place built)

What you prepare so something can rest on it:

- Sections, symbols, **REL32** patch rules, optional **PE** packaging — `rawrxd_minimal_*`, validation, docs.
- Enough **structure** that a tiny emitter has a **defined** place to land.

## Grass (on that ground only)

- A **closed** palette of opcodes as **explicit** byte sequences.
- **No** claim to universal decode; **no** vertical growth metaphor for “more compiler.”

## What to implement first (emit-side)

1. **Fixed templates** — small palette (e.g. `NOP`, `RET`, `CALL rel32`, a few immediates) as **buffers**, not a full mnemonic→machine table on day one.
2. **Symbol + reloc model** — Tier G objects: sections, defined symbols, **IMAGE_REL_AMD64_REL32** (or your documented fixup), patch-site rules in `docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`.
3. **Validation** — REL32 in **signed 32-bit** range (`sovereign_rel32_validate.hpp`).

## What to defer (separate projects, not “under” or “over” grass)

- Full **x86-64 encoder**, **Windows x64 ABI**, IR, RA, diagnostics — **different** work, not “roots under” or “sky above” this surface.
- **Interactive “meaning of any single byte”**: x86 is **contextual**; use **Capstone**, **Zydis**, or **LLVM MC** for decode.
- **Hardware** pipelines (PCI/NVMe/EPT/DMA execution): out of scope; **`docs/SOVEREIGN_CONTRIBUTOR_COVENANT.md`**, **`docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md`**.

## Map to repo artifacts

| Need | Where |
|------|--------|
| REL32 identity + hole math | `examples/som_minimal_usage.c` |
| Layout + apply reloc (C) | `toolchain/sovereign_minimal/rawrxd_minimal_link.c`, `examples/rawrxd_minimal_link_example.c` |
| REL32 range checks | `include/rawrxd/sovereign_rel32_validate.hpp` |
| Optional link-time DCE | `include/rawrxd/sovereign_global_dce.hpp`, `tests/test_sovereign_global_dce.cpp` |
| PE emission vs link | `docs/SOVEREIGN_EMISSION_LAYER.md` |
| Tier G contract | `docs/SOVEREIGN_TIER_G_REQUIREMENTS.md` |

## Related

- **`docs/SOVEREIGN_ENTERPRISE_TOOLCHAIN_LAYERS.md`** — ingest → validate → optimize → emit
- **`docs/SOVEREIGN_FORGE_MASM.md`** — hand-built PE / MASM teaching notes
