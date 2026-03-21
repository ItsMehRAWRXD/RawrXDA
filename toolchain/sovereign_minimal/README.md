# `sovereign_minimal` — from-scratch link math (Tier G)

| File | Role |
|------|------|
| **`rawrxd_minimal_link.c`** | **Topography** (`rawrxd_minimal_finalize_image_topography` — RVA + file offset, 4K/512B), layout, symbol → RVA, **REL32** / **ABS64** patching |
| **`rawrxd_symbol_registry.c`** | **FNV-1a** symbol registry (`RAWR_SYM_HASH_SIZE`), weak/defined flags — “brain” for lookups + DCE hand-off |
| **`include/rawrxd/rawrxd_minimal_link.h`** | Public C API (link) |
| **`include/rawrxd/rawrxd_symbol_registry.h`** | Public C API (registry) |

**No** external linker dependency — only **`sovereign_som_minimal.h`** + C99.

**Does not** emit PE; use **`docs/SOVEREIGN_EMISSION_LAYER.md`** / `phase2_linker` / `sovereign_emit_formats.hpp` after you have patched bytes.

## Build

From repository root (`-I` points at `include/`):

```bash
gcc -std=c99 -Wall -I include -c toolchain/sovereign_minimal/rawrxd_minimal_link.c -o build/rawrxd_minimal_link.o
gcc -std=c99 -Wall -I include examples/rawrxd_minimal_link_example.c build/rawrxd_minimal_link.o -o build/rawrxd_minimal_link_example
```

```bat
cl /std:c11 /W3 /I include /c toolchain\sovereign_minimal\rawrxd_minimal_link.c /Fobuild\rawrxd_minimal_link.obj
cl /std:c11 /W3 /I include /Fe:build\rawrxd_minimal_link_example.exe examples\rawrxd_minimal_link_example.c build\rawrxd_minimal_link.obj
```

## Requirements checklist

**`docs/SOVEREIGN_TIER_G_REQUIREMENTS.md`**

## Related

- **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`**
- **`docs/SOVEREIGN_SYMBOL_REGISTRY_DCE.md`** — registry + **`sovereign_global_dce.hpp`** (DCE)
- **`docs/SOVEREIGN_FORGE_MASM.md`** — MASM PE “forge” vs **`tools/pe_emitter.asm`**
- **`examples/som_minimal_usage.c`** — standalone REL32 proof without this library
- **`examples/sovereign/README.md`** — Tier G index + **`symbol_registry_smoke.c`**
