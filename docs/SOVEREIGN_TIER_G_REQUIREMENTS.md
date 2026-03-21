# Tier G — **global** status (requirements, reversed from “lab”)

This is the **checklist** for treating Sovereign minimals as **first-class, reusable** artifacts—not a vague “not lab.” **Tier P** (RawrXD IDE) still builds with **MSVC** (**§7**). This doc does **not** grant **MSVC bitwise** parity.

---

## 1. What “global non-lab” means (requirements)

| # | Requirement | Evidence in repo |
|---|-------------|------------------|
| **G1** | **Stable names** — public structs / functions keep ABI-stable prefixes (`RawrSomMinimal*`, `rawrxd_minimal_*`) unless versioned bump. | Headers in `include/rawrxd/` |
| **G2** | **Documented semantics** — REL32 patch site = **first byte of the 4-byte displacement** (unless API states otherwise). | `docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`, `sovereign_som_minimal.h` |
| **G3** | **Runnable proof** — at least one **example** or **test** that compiles and demonstrates layout + patch math. | `examples/som_minimal_usage.c` (math), `examples/rawrxd_minimal_link_example.c` (**library**), `examples/sovereign/symbol_registry_smoke.c` (**registry**) — see **`examples/sovereign/README.md`** |
| **G4** | **Honest limits** — **§6/§7** (no omnibus linker claim, no Rich/PDB parity promise) stay visible in **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`**. | Linked from README |
| **G5** | **From-scratch minimals** — core link math (align, symbol RVA, apply reloc) + **symbol registry** (FNV-1a hash) in **standalone C** (no LLVM/binutils). | `rawrxd_minimal_link.c`, `rawrxd_symbol_registry.c`, headers in `include/rawrxd/` |
| **G6** | **Emission boundary** — PE bytes are **separate** from link; see **`docs/SOVEREIGN_EMISSION_LAYER.md`**. | |

---

## 2. What this does **not** require

- Replacing **`link.exe`** for the **IDE product** (Tier **P**).
- **Signing** inside the minimal link library (use **`signtool`** after emission).
- **PDB / Rich** parity (**§7**).

---

## 3. Minimal stack (from scratch)

```text
RawrSomMinimalSection[] + symbols + relocs
  → rawrxd_minimal_layout_sections()
  → rawrxd_minimal_symbol_rva() / apply
  → rawrxd_minimal_apply_reloc() or _apply_all()
  → [optional] PE emission (phase2 / compose*)
```

---

## Related

- **`docs/COMPILER_BOOTSTRAP_SURFACE.md`** — grass **on** built ground (surface + substrate); not vertical “roots/sky”
- **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`**
- **`toolchain/sovereign_minimal/README.md`**
