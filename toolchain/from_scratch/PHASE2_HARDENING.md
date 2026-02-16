# Phase 2 Hardening: More Sections (.data, .rdata)

Before or in parallel with Phase 3, you can harden the linker so the PE has **read-only data** and **initialized data** sections. This allows globals and string literals when you add a C compiler or hand-written asm that references them.

## Current state

- **.text** — Merged from all objects, stub prepended; RVA 0x1000, relocations applied.
- **.idata** — Single import section (kernel32 / ExitProcess); RVA 0x2000.

## Add .data (read-write initialized data)

1. **Section merge** — In `section_merge.c` (or equivalent), add a second merged section: collect all `.data` (and optionally `.data$*`) from objects, concatenate, assign RVA after .text (e.g. next 4KB page). Record `img->data.rva`, `img->data.size`, `img->data.data`, and per-object offsets for relocations.
2. **Relocations** — Extend `reloc_resolver` to apply REL32/ADDR64 in .data using `data.rva` and object offsets. Symbol resolution: internal = `data.rva + obj_data_offsets[i] + value`; external unchanged.
3. **PE writer** — Add a third section header “.data”, `VirtualAddress` = data RVA, `PointerToRawData` = file offset after .text (and after .idata if .idata stays before .data in file). Emit raw bytes for .data. Update `SizeOfImage` and optional header `SizeOfInitializedData` to include .data size (aligned to SectionAlignment).
4. **File layout** — Decide order: e.g. .text, .idata, .data. RVAs: .text 0x1000, .idata 0x2000, .data 0x3000. File offsets: headers 0x400, .text 0x400, .idata 0x800, .data 0xC00 (example for small sizes and FILE_ALIGN 0x400).

## Add .rdata (read-only data)

1. **Section merge** — Merge `.rdata` (and `.rdata$*`) from objects; assign RVA (e.g. after .data). Same pattern as .data: `img->rdata.rva`, size, data, per-object offsets.
2. **Relocations** — Apply in .rdata (e.g. for ADDR64 to string literals).
3. **PE writer** — Add “.rdata” section; Characteristics = read-only (no MEM_WRITE). Place in RVA and file layout (e.g. .text, .rdata, .data, .idata to keep read-only together, or .text, .idata, .rdata, .data).

## COFF section names

- MSVC: `.text`, `.text$mn`, `.data`, `.rdata`, `.bss` (no raw data).
- Map `.text$*` → .text, `.data$*` → .data, `.rdata$*` → .rdata for merge.

## Testing

- Assemble a small object that defines a global in .data and one in .rdata; link with Phase 2; verify in PE dump that sections exist and RVAs/raw offsets are correct. Optionally call from .text and check at runtime.

## Order of work

1. Implement .data merge + relocs + PE section.
2. Validate with one .obj that has .data.
3. Add .rdata the same way.
4. (Optional) .bss: section with `SizeOfRawData == 0`, `VirtualSize > 0`; loader zero-fills. Requires BSS symbol resolution and SizeOfImage to include BSS.

---

*Use this before or alongside Phase 3 when you need globals and literals.*
