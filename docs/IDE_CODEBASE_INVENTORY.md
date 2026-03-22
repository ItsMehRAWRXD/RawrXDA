# IDE codebase inventory (from `ide_inventory_in.txt`)

This doc ties the external export **`D:\ide_inventory_report\ide_inventory_in.txt`** to the repository.

**Normalized copy (repo-relative paths):** [`docs/inventory/ide_inventory_paths.txt`](inventory/ide_inventory_paths.txt)  
**Regeneration:** [`docs/inventory/README.md`](inventory/README.md)

---

## Row counts by top-level tree

Derived from `ide_inventory_paths.txt` (1726 non-empty lines):

| Top-level | Files (in export) | Role |
|-----------|-------------------|------|
| **`src/`** | 1448 | Primary RawrXD sources (Win32 IDE, agentic, core, ggml copies under `src/`, tests, etc.) |
| **`3rdparty/`** | 100 | Vendored **ggml** (and peers under that tree) |
| **`RawrXD-ModelLoader/`** | 70 | Sibling / legacy loader project inside monorepo |
| **`node_modules/`** | 42 | **ffi-napi** libffi sources only in this export — not application code |
| **`Ship/`** | 37 | Shared “Ship” layer sources |
| **`itsmehrawrxd-master/`** | 10 | Templates / experiments |
| **`Tiny-Home/`** | 8 | Small satellite project |
| **`OrganizedPiProject/`** | 5 | Legacy / sample |
| **`asm/`** | 4 | Root-level asm snippets |
| **`RawrZ-Security/`** | 2 | Security / payload-builder samples |

---

## How to use this

- **Scope questions:** “What touches native code?” — filter `ide_inventory_paths.txt` by extension (`.cpp`, `.c`, `.asm`, …).
- **IDE focus:** Win32 product code lives mainly under **`src/win32app/`** and **`src/core/`**; the inventory includes many experimental and duplicate trees by design (monorepo honesty).
- **Policy:** Paths under **`node_modules/`** and large **`3rdparty/`** blocks are not “RawrXD-authored”; treat **`src/`** + **`Ship/`** as first-class for reviews.

---

## Traceability

| Topic | Document |
|-------|----------|
| Production / syscall rejection | `docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` §3, §6 |
| Platform gaps | `docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md` |

**Last updated:** Inventory snapshot normalized 2026-03-20 from `D:\ide_inventory_report\ide_inventory_in.txt`.
