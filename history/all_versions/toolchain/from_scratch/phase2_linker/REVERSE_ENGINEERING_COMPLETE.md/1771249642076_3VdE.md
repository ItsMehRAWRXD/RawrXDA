# Phase 2 Linker - Fully Reverse Engineered

## Architecture: 5-Phase PE/COFF Linking Pipeline

```
  .obj files ──► Phase 1 ──► Phase 2 ──► Phase 3 ──► Phase 4 ──► .exe
                 COFF        Section     Reloc       PE
                 Reader      Merger      Resolver    Writer
                                                       ▲
                                              Phase 5 ─┘
                                              Entry Stub
```

---

## Phase 1: COFF Reader (`coff_reader.c` / `coff_reader.h`)

**Purpose:** Parse PE/COFF object files produced by MASM, MSVC, or GCC.

### COFF File Layout
```
Offset 0x00:  COFF Header         (20 bytes)
Offset 0x14:  Section Headers      (40 bytes × N)
Variable:     Raw Section Data     (pointed to by headers)
Variable:     Per-Section Relocs   (10 bytes each)
sym_offset:   Symbol Table         (18 bytes × num_symbols)
After syms:   String Table         (uint32 size + strings)
```

### COFF Header (20 bytes)
| Offset | Size | Field |
|--------|------|-------|
| 0 | 2 | Machine (0x8664=x64, 0x14C=x86) |
| 2 | 2 | NumberOfSections |
| 4 | 4 | TimeDateStamp |
| 8 | 4 | PointerToSymbolTable |
| 12 | 4 | NumberOfSymbols |
| 16 | 2 | SizeOfOptionalHeader (0 for .obj) |
| 18 | 2 | Characteristics |

### Section Header (40 bytes)
| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | Name (or /N string table ref) |
| 8 | 4 | VirtualSize |
| 12 | 4 | VirtualAddress |
| 16 | 4 | SizeOfRawData |
| 20 | 4 | PointerToRawData |
| 24 | 4 | PointerToRelocations |
| 28 | 4 | PointerToLinenumbers |
| 32 | 2 | NumberOfRelocations |
| 34 | 2 | NumberOfLinenumbers |
| 36 | 4 | Characteristics |

### Symbol Entry (18 bytes)
| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | Name (inline or string table ptr) |
| 8 | 4 | Value (offset in section) |
| 12 | 2 | SectionNumber (1-based) |
| 14 | 2 | Type (0x20 = function) |
| 16 | 1 | StorageClass |
| 17 | 1 | NumberOfAuxSymbols |

### Name Resolution
- If bytes 0-3 are zero: bytes 4-7 = offset into string table
- Otherwise: up to 8 ASCII chars (NOT null-terminated if exactly 8)
- Section names starting with `/` + digits = string table offset

### Key Implementation Details
- String table is read BEFORE symbols (needed for name resolution)
- Aux symbol records (18 bytes each) follow their parent symbol
- Extended reloc count: if `IMAGE_SCN_LNK_NRELOC_OVFL` set and count=0xFFFF, first reloc entry's VirtualAddress field holds true count
- Section alignment decoded from characteristics bits 20-23: `align = 2^(N-1)`

---

## Phase 2: Section Merger (`section_merge.c` / `section_merge.h`)

**Purpose:** Merge same-named sections from multiple .obj files into a flat image layout.

### Algorithm
```
1. For each object file, for each section:
   a. Skip .drectve and .debug$ sections
   b. Find or create merged section with same name
   c. Align write cursor to section's alignment requirement
   d. Pad with zeros to alignment boundary
   e. Copy raw data (or account for virtual size if BSS)
   f. Record contribution map entry
   g. OR characteristics flags, track max alignment

2. Assign RVAs starting at 0x1000 (after PE headers):
   Each section aligned to SectionAlignment (0x1000)

3. Strip alignment bits from merged characteristics
   (PE headers encode alignment differently)
```

### Section Ordering (PE Convention)
| Priority | Section | Purpose |
|----------|---------|---------|
| 0 | .text | Executable code |
| 1 | .rdata | Read-only data |
| 2 | .data | Read-write data |
| 3 | .pdata | Exception handlers |
| 4 | .xdata | Unwind info |
| 5 | .bss | Uninitialized data |
| 6 | .rsrc | Resources |
| 7 | .reloc | Base relocations |

### Contribution Map
Each piece of merged data tracks:
- `obj_index`: which object file
- `sec_index`: which section in that object
- `src_offset`: offset in original section
- `dst_offset`: offset in merged buffer
- `rva`: absolute RVA in final image

### Global Symbol Table
Built by iterating all objects' symbol tables. Each symbol's RVA is resolved by finding its contribution entry:
```
symbol_rva = contribution.rva + symbol.value
```

---

## Phase 3: Relocation Resolver (`reloc_resolver.c`)

**Purpose:** Patch machine code with final addresses based on the merged layout.

### Relocation Entry (10 bytes)
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | VirtualAddress (offset in section) |
| 4 | 4 | SymbolTableIndex |
| 8 | 2 | Type |

### x64 Relocation Types

| Type | Value | Size | Formula |
|------|-------|------|---------|
| ABSOLUTE | 0x00 | - | No-op |
| ADDR64 | 0x01 | 8 | `sym_rva + image_base + addend` |
| ADDR32 | 0x02 | 4 | `sym_rva + image_base + addend` |
| ADDR32NB | 0x03 | 4 | `sym_rva + addend` (no base) |
| REL32 | 0x04 | 4 | `sym_rva - (reloc_rva + 4) + addend` |
| REL32_1 | 0x05 | 4 | `sym_rva - (reloc_rva + 5) + addend` |
| REL32_2 | 0x06 | 4 | `sym_rva - (reloc_rva + 6) + addend` |
| REL32_3 | 0x07 | 4 | `sym_rva - (reloc_rva + 7) + addend` |
| REL32_4 | 0x08 | 4 | `sym_rva - (reloc_rva + 8) + addend` |
| REL32_5 | 0x09 | 4 | `sym_rva - (reloc_rva + 9) + addend` |
| SECTION | 0x0A | 2 | Section index of symbol |
| SECREL | 0x0B | 4 | Symbol offset within its section |
| SECREL7 | 0x0C | 1 | 7-bit section-relative |

### REL32 Deep Dive (Most Common)
```
; Example: call MyFunc
; Assembled as: E8 xx xx xx xx
; The 4 bytes after E8 are the rel32 displacement
; CPU executes: RIP = RIP + 5 + displacement
; So: displacement = target - (instruction_addr + 5)
; In COFF terms: target_rva - (reloc_rva + 4)
; The +4 is because the reloc field starts at offset 1 of the 5-byte instruction
```

### Application Pipeline
```
For each merged section:
  For each contribution in that section:
    Find original COFF file and section
    For each relocation in that COFF section:
      1. Calculate patch_offset = contribution.dst_offset + reloc.virtual_addr
      2. Calculate reloc_rva = merged_section.rva + patch_offset
      3. Resolve target symbol RVA from global symbol table
      4. Read existing addend from patch location
      5. Calculate result based on relocation type
      6. Write result to merged data buffer
```

---

## Phase 4: PE Writer (`pe_writer.c` / `pe_writer.h`)

**Purpose:** Generate a complete PE32+ (64-bit) Windows executable.

### PE File Layout
```
Offset 0x000:  DOS Header          (64 bytes)
  [0x00] MZ signature
  [0x3C] e_lfanew -> PE offset

Offset 0x040:  DOS Stub            (128 bytes, "cannot be run in DOS mode")

Offset 0x0C0:  PE Signature        (4 bytes: "PE\0\0")

Offset 0x0C4:  COFF Header         (20 bytes)
  Machine, NumSections, TimeDateStamp
  OptionalHdrSize = 0xF0 (PE32+)
  Characteristics = EXECUTABLE | LARGE_ADDRESS_AWARE

Offset 0x0D8:  Optional Header     (240 bytes = 0xF0)
  Standard (24 bytes):
    Magic=0x020B, EntryPoint, BaseOfCode
  Windows-specific (88 bytes):
    ImageBase, SectionAlignment, FileAlignment
    SizeOfImage, SizeOfHeaders, Subsystem
    Stack/Heap sizes
  Data Directories (128 bytes):
    16 entries × 8 bytes (RVA + Size)

After headers: Section Headers      (40 bytes × N)

File-aligned:  Section Raw Data
```

### Optional Header PE32+ Layout (240 bytes)
| Offset | Size | Field |
|--------|------|-------|
| 0 | 2 | Magic (0x020B) |
| 2 | 2 | Linker version |
| 4 | 4 | SizeOfCode |
| 8 | 4 | SizeOfInitializedData |
| 12 | 4 | SizeOfUninitializedData |
| 16 | 4 | AddressOfEntryPoint |
| 20 | 4 | BaseOfCode |
| 24 | 8 | ImageBase |
| 32 | 4 | SectionAlignment (0x1000) |
| 36 | 4 | FileAlignment (0x200) |
| 40-50 | - | OS/Image/Subsystem versions |
| 56 | 4 | SizeOfImage |
| 60 | 4 | SizeOfHeaders |
| 64 | 4 | CheckSum |
| 68 | 2 | Subsystem (2=GUI, 3=Console) |
| 70 | 2 | DllCharacteristics |
| 72-104 | 32 | Stack/Heap reserve/commit |
| 108 | 4 | NumberOfRvaAndSizes (16) |
| 112-239 | 128 | Data Directories |

### Key Constants
```
FileAlignment    = 0x200  (512 bytes)
SectionAlignment = 0x1000 (4096 bytes)
ImageBase        = 0x0000000140000000 (default x64)
PE offset        = 0xC0 (e_lfanew)
```

---

## Phase 5: Entry Stub + Driver (`entry_stub.c` / `main.c`)

### Entry Stub Machine Code
```asm
; Console application entry (20 bytes)
48 83 EC 28          sub rsp, 0x28          ; shadow space + 16-byte align
E8 xx xx xx xx       call main              ; REL32 patched by linker
89 C1                mov ecx, eax           ; exit code
48 83 C4 28          add rsp, 0x28          ; restore stack
E8 xx xx xx xx       call ExitProcess       ; REL32 patched by linker
CC                   int 3                  ; safety breakpoint
```

### Entry Point Search Order
1. `mainCRTStartup` (MSVC CRT)
2. `main` (standard C)
3. `wmainCRTStartup` / `wmain` (wide char)
4. `WinMainCRTStartup` / `WinMain` (GUI)
5. `_start` (minimal)
6. `entry` (custom)

### CLI Options
```
rawrxd_link [-o output.exe] [-v] [-base 0xNNNN] [-subsys N]
            [-entry symbol] [-stack N] [-heap N] [-map] file.obj ...
```

---

## Build & Test

```bash
# Build with CMake + Ninja
cd D:\RawrXD\toolchain\from_scratch\phase2_linker
cmake -B build -G Ninja
cmake --build build

# Test: assemble and link a simple program
ml64 /c /Fo test.obj test.asm
build\rawrxd_link -o test.exe -v test.obj

# Inspect output
build\rawrxd_check test.exe
```

## File Manifest

| File | Phase | Lines | Purpose |
|------|-------|-------|---------|
| `coff_reader.h` | 1 | ~100 | COFF structures, constants, API |
| `coff_reader.c` | 1 | ~310 | Full COFF parser with diagnostics |
| `section_merge.h` | 2 | ~60 | Merge types, contribution tracking |
| `section_merge.c` | 2 | ~300 | Section merger with RVA assignment |
| `reloc_resolver.c` | 3 | ~260 | All x64 relocation types |
| `pe_writer.h` | 4 | ~60 | PE builder API |
| `pe_writer.c` | 4 | ~280 | Complete PE32+ writer |
| `entry_stub.c` | 5 | ~70 | x64 CRT stubs (console/GUI/DLL) |
| `main.c` | 5 | ~180 | CLI driver, all phases orchestrated |
| `rawrxd_check.c` | util | ~170 | COFF/PE diagnostic dumper |
