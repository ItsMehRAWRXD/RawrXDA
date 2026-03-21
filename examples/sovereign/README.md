# Tier G Sovereign — C examples

Small **no-deps** (beyond libc) programs for the **Sovereign** link/registry contract. See **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`** and **`docs/SOVEREIGN_TIER_G_REQUIREMENTS.md`**.

| Example | Purpose |
|---------|---------|
| **`symbol_registry_smoke.c`** | `RawrSymbolRegistry`: init, insert, FNV-1a `find` / `get`. |
| **`../som_minimal_usage.c`** | REL32 math proof (`RawrSomMinimalAtomicUnit`). |
| **`../rawrxd_minimal_link_example.c`** | Topography + `rawrxd_minimal_apply_reloc` on sections. |

## Build (repo root, `-I include`)

```bash
gcc -std=c99 -Wall -I include examples/sovereign/symbol_registry_smoke.c \
    toolchain/sovereign_minimal/rawrxd_symbol_registry.c -o build/symbol_registry_smoke

gcc -std=c99 -Wall -I include examples/som_minimal_usage.c -o build/som_minimal_usage

gcc -std=c99 -Wall -I include examples/rawrxd_minimal_link_example.c \
    toolchain/sovereign_minimal/rawrxd_minimal_link.c -o build/rawrxd_minimal_link_example
```

**MSVC:** same sources; compile each `.c` to `.obj` then `link` with `kernel32.lib` if you add a `main` that uses WinAPI (these smokes use only stderr).

## More

- **`toolchain/sovereign_minimal/README.md`**
- **`docs/SOVEREIGN_SYMBOL_REGISTRY_DCE.md`**
