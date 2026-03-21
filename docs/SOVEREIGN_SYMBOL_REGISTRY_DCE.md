# Symbol registry + DCE (Tier G)

## Registry (C)

**`include/rawrxd/rawrxd_symbol_registry.h`** + **`toolchain/sovereign_minimal/rawrxd_symbol_registry.c`** implement a **FNV-1a** hash table (`RAWR_SYM_HASH_SIZE` = **4096** buckets) with:

- **`RawrSymbol`**: name, hash, `section_idx`, `offset`, **`is_weak`**, **`is_defined`**, bucket chain.
- **Lookup / insert** for the “brain” of a minimal linker: *where* each name lives in section space.

String keys are **not** copied; callers must keep name pointers alive.

**Smoke:** `examples/sovereign/symbol_registry_smoke.c` — init, insert, `find` / `get`, FNV-1a check (build in **`examples/sovereign/README.md`**).

---

## DCE (C++ IR) — `sovereign_global_dce.hpp`

**`include/rawrxd/sovereign_global_dce.hpp`** implements **global dead code elimination** on **`LinkerContext`** / **CIR** symbols: seed roots (entry + optional exports), follow **relocation edges**, drop unreachable **defined** symbols — MSVC **`/OPT:REF` analog** at IR level.

**Hand-off:** the C registry answers **“where is this name?”**; the C++ DCE answers **“what must we keep?”** for emission. A full pipeline can:

1. Ingest relocs + symbols into **`LinkerContext`** (or mirror into **`RawrSymbolRegistry`** for C-only tools).
2. Run **`applyGlobalDeadCodeElimination`** (C++) **or** a future C reachability pass over the same graph.
3. Layout + patch (**`rawrxd_minimal_link`**) → emit (**`SOVEREIGN_EMISSION_LAYER`**).

---

## “Global ready”

- **Hash**: stable **FNV-1a** (same inputs → same buckets across hosts).
- **DCE**: entry-rooted reachability keeps images small when linking large static archives — policy and limits remain **§6/§7** (not a promise of **`link.exe`** parity).

## Related

- **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`**
- **`docs/SOVEREIGN_TIER_G_REQUIREMENTS.md`**
- **`toolchain/sovereign_minimal/README.md`**
