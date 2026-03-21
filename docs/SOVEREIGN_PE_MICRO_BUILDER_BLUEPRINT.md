# PE micro-builder blueprint (Tier G — global mechanical checklist)

This document captures the **minimal mechanical pipeline** for in-memory sections → symbols → relocations → PE32+ (imports, entry, optional `.reloc`). It aligns with the **`RawrSomMinimal*`** contract and pointer-based C sketches, with one **critical patch-site convention** for **REL32**.

**Tier framing:** **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`** — Tier **G** (reusable Sovereign tooling worldwide) vs Tier **P** (RawrXD IDE product built with **`cl.exe` + `link.exe` + CRT + PDB** — **§6** / **§7**). This blueprint is **for implementers**; it does **not** promise **MSVC bitwise** **`link.exe`** parity in-tree.

---

## Topography pass (virtual vs file — “finalize_image”)

PE maps **RVA** (virtual) and **raw file offset** with **different** alignments (typically **4 KiB** vs **512 B**). The mechanical loop is:

```c
/* v_ptr often starts 0x1000; f_ptr after headers often 0x200 or 0x400 */
v_ptr += align_up(section_size, SectionAlignment);   /* e.g. (size + 0xFFF) & ~0xFFF */
f_ptr += align_up(section_size, FileAlignment);      /* e.g. (size + 0x1FF) & ~0x1FF */
```

**In-repo:** `rawrxd_minimal_finalize_image_topography()` in **`rawrxd_minimal_link.c`** + **`RawrMinimalTopographyOptions`** (`RAWR_MINIMAL_TOPO_*` defaults in **`rawrxd_minimal_link.h`**).

---

## Map: pointer C model ↔ index-based lab headers

| Idea (your sketch) | In-repo lab types / notes |
|--------------------|---------------------------|
| `Section` + `data` + `rva` | `RawrSomMinimalSection` — use **indices** into a section array when serializing; raw pointers are fine **inside one process** only. |
| `Symbol` → section + offset | `RawrSomMinimalSymbol` (`section_index`, `offset_in_section`). |
| `Reloc` + target name | `RawrSomMinimalReloc` or `RawrSomMinimalFixup` on an atomic unit. |
| Import modules | `RawrSomMinimalImport`; PE IAT/IDT emission → **`toolchain/from_scratch/phase2_linker/`**, **`include/rawrxd/sovereign_emit_formats.hpp`**. |

---

## REL32 patch math (must match instruction layout)

For **`call rel32`** (opcode **`E8`**, then **4** bytes):

- Let **`R`** = RVA of **`E8`** (start of instruction).  
  **Next instruction** RVA = **`R + 5`**.  
  **rel32** = `(int32_t)(target_rva - (R + 5))`.

If your reloc records **`offset`** as the **first byte of the 4-byte displacement** (not `E8`), call that **`H`**. Then next RVA = **`H + 4`**, and:

**rel32** = `(int32_t)(target_rva - (H + 4))`.

A common bug is using **`(section_rva + offset + 4)`** when **`offset`** actually points at **`E8`** — you need **`+ 5`**, not **`+ 4`**, in that case.

**Worked example:** `examples/som_minimal_usage.c` (stderr `[SOM]` lines; proves both forms agree).

---

## Validation layer — REL32 must fit **int32_t** (≈ ±2 GiB)

All **REL32** encodings use a **signed 32-bit** displacement. The signed delta from the instruction’s reference RIP (typically the address **after** the full instruction, e.g. **`R+5`** for **`E8`**, **`R+6`** for **`FF 15`**) to the target must lie in **`[INT32_MIN, INT32_MAX]`** — often described loosely as **±2 GiB** reach on x64.

- **Header (check only):** `include/rawrxd/sovereign_rel32_validate.hpp` — `rel32DisplacementFits`, `ripRelativeDelta`, `encodeRel32Displacement`, `rel32ReachableFromRipAfter`.
- **Emit (checked):** `include/rawrxd/sovereign_emit_x64.hpp` — `tryEmitCallRel32`, `tryEmitCallRipRel32` (return `std::expected<void, Rel32OutOfRange>`). Unchecked `emitCall*` still truncate like a cast; prefer the **`tryEmit*`** path for production fixups.
- **Test:** `tests/test_sovereign_rel32_validate.cpp` (CMake target `test_sovereign_rel32_validate`).

---

## Topography pass — reverse-engineered alignment contract

Once each section has a **raw byte size** (and you are not yet emitting), a single **topography** (layout) pass assigns **RVA** (virtual) and **PointerToRawData** (file) per section. That is the mechanical core PE linkers share with this lab stack.

**Reference sketch** (pedagogical — masks are equivalent to `ALIGN_UP`):

```c
// [The Topography Pass — Reverse Engineered]
void finalize_image(Builder* b) {
    uint32_t v_ptr = 0x1000; // Start of first section RVA (typical; after PE header reservation)
    uint32_t f_ptr = 0x400;  // Start of file-backed sections (must be >= SizeOfHeaders, file-aligned)

    for (int i = 0; i < b->section_count; i++) {
        Section* s = &b->sections[i];
        s->rva = v_ptr;
        s->file_offset = f_ptr;

        // The alignment contract (must match OptionalHeader.SectionAlignment / FileAlignment)
        v_ptr += (s->size + 0xFFF) & ~0xFFF;   // 4 KiB virtual alignment (0x1000)
        f_ptr += (s->size + 0x1FF) & ~0x1FF;   // 512 B file alignment (0x200) — example only
    }
    b->total_image_size = v_ptr;               // then usually ALIGN_UP(v_ptr, SectionAlignment) → SizeOfImage
}
```

**Equivalence:** `(x + (A-1)) & ~(A-1)` **≡** `ALIGN_UP(x, A)` for power-of-two `A`.

**In-repo (`pe_writer.c`):** `SEC_ALIGN` = **0x1000**, `FILE_ALIGN` = **0x400** (**1024** bytes on disk — not 512). The **first** section raw offset is `headers_raw` = `ALIGN_UP(sect_tbl_end, FILE_ALIGN)`, not a magic constant — **`SizeOfHeaders`** must equal that value. **`SizeOfImage`** is the virtual span, ending with `ALIGN_UP(..., SEC_ALIGN)`.

**Tier honesty:** This pass does **not** replace symbol resolution, relocs, or imports — it only fixes **where** section bytes live in VA vs file.

---

## Pipeline (checklist)

1. **Collect** — sections, symbols, relocs, import list (all in memory).  
2. **Layout** — assign **RVA** / **file offset** with section/file alignment (PE: typical **0x1000** / **0x200** — see PE spec); **topography pass** above.  
3. **Resolve** — name → final **RVA** (or import thunk slot).  
4. **Patch** — **REL32** / **ABS64** (and PE **base reloc** for **DIR64** slots if ASLR).  
5. **Build imports** — IDT / ILT / IAT / names (no `.lib` required if you synthesize tables yourself — still **hard** to get right).  
6. **Emit** — MZ/PE + section raw data → **`pe_writer`**-style path (**`toolchain/from_scratch/phase2_linker/pe_writer.c`**).  
7. **Entry** — `AddressOfEntryPoint` → your stub (e.g. `_start` → `main` → `ExitProcess`).  
8. **Optional `.reloc`** — if image may load at a non-preferred base and you have absolute fixups.

---

## What not to claim

- **“Fully working MSVC clone”** — out of scope as a **product** narrative (**§6**).  
- **No CRT** — achievable for tiny demos; real C/C++ apps usually still need **some** runtime story.  
- **Cross-host “build PE on macOS without testing”** — SOM structs are portable; **emitters** and **ABI** still need validation on Windows.

---

## Emission layer (aligned PE, signed-ready, metadata)

After link/patch, the **emission** stage writes headers, section table, data directories, and alignment. See **`docs/SOVEREIGN_EMISSION_LAYER.md`** for **signed-ready** (post-`signtool`) vs **§7** “full metadata” honesty.

---

## Related

- **`docs/SOVEREIGN_EMISSION_LAYER.md`** — emission layer definition.  
- **`docs/SOVEREIGN_TOOLCHAIN_LAB_ARCHITECTURE.md`** — CIR/SOM, micro-linker floor.  
- **`examples/som_minimal_usage.c`** — REL32 proof only.  
- **`include/rawrxd/sovereign_som_minimal.h`**, **`sovereign_fabricator.h`**, **`sovereign_dispatch_table.h`**, **`som_core.h`**.
