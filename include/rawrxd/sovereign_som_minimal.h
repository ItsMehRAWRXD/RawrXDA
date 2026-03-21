/* ============================================================================
 * sovereign_som_minimal.h — Atomic "byte + label + delta" contract (pure C)
 * ============================================================================
 *
 * PURPOSE: Minimal in-memory shapes for a *micro-linker* pipeline: opcodes,
 * holes (fixups), sections, symbols, relocs. No filesystem object format is
 * implied — buffers only.
 *
 * TARGET OUTPUT: The *emitter* stage may be PE (Windows), ELF, Mach-O, etc.
 * These structs are OS-agnostic; PE-specific RVAs / image base are applied
 * when patching (see docs).
 *
 * TIER G (global use): Public contract for embedding and cross-host tooling.
 * See docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md. Tier P (RawrXD IDE product) builds
 * with MSVC (cl + link.exe + CRT + PDB) — docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md §6/§7.
 *
 * C standard: C99 or later. Safe to include from C++ (extern "C" wrapper).
 * ============================================================================ */

#ifndef RAWRXD_SOVEREIGN_SOM_MINIMAL_H
#define RAWRXD_SOVEREIGN_SOM_MINIMAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Relocation / fixup kinds — patch math only; ABI is the emitter’s job. */
enum RawrSomMinimalRelocType {
    RAWR_SOM_RELOC_NONE = 0,
    /** REL32: target - (patch_rva + 4)  (x86-64 RIP-relative) */
    RAWR_SOM_RELOC_REL32 = 1,
    /** ABS64: image_base + target_rva (or VA for some targets) */
    RAWR_SOM_RELOC_ABS64 = 2
};

/**
 * Single "hole" in a byte buffer: write patch at @p patch_offset bytes from
 * the start of this unit’s opcode blob; @p target_name resolves in the
 * global symbol map (or import slot) at link time.
 */
typedef struct RawrSomMinimalFixup {
    uint32_t patch_offset;
    /** NUL-terminated; lifetime = caller / arena until link completes */
    const char *target_name;
    uint16_t type; /* RawrSomMinimalRelocType */
    uint16_t reserved;
} RawrSomMinimalFixup;

/**
 * Smallest "frontend output": raw bytes + fixups. No sections yet — the
 * linker may place this blob into a section and assign an RVA base.
 */
typedef struct RawrSomMinimalAtomicUnit {
    uint8_t *raw_bytes;
    uint32_t raw_size;
    RawrSomMinimalFixup *fixups;
    uint32_t fixup_count;
} RawrSomMinimalAtomicUnit;

/** Section flags — bitfield; PE/ELF/Mach-O map these differently at emit. */
enum RawrSomMinimalSectionFlags {
    RAWR_SOM_SEC_NONE = 0,
    RAWR_SOM_SEC_READ = 1u << 0,
    RAWR_SOM_SEC_WRITE = 1u << 1,
    RAWR_SOM_SEC_EXEC = 1u << 2,
    RAWR_SOM_SEC_DISCARDABLE = 1u << 3
};

/**
 * One contiguous section after lowering (in-memory .text / .data / ...).
 * @p rva and @p file_offset are filled by the layout pass.
 */
typedef struct RawrSomMinimalSection {
    const char *name;
    uint8_t *data;
    uint32_t size;
    uint32_t rva;
    uint32_t file_offset;
    uint32_t flags; /* RawrSomMinimalSectionFlags */
} RawrSomMinimalSection;

/** Global symbol: name → section index + offset within section. */
typedef struct RawrSomMinimalSymbol {
    const char *name;
    uint32_t section_index;
    uint32_t offset_in_section;
} RawrSomMinimalSymbol;

/** Relocation referencing a symbol by name (linker resolves to VA/RVA). */
typedef struct RawrSomMinimalReloc {
    uint32_t section_index;
    uint32_t offset_in_section;
    const char *target_name;
    uint16_t type; /* RawrSomMinimalRelocType */
    uint16_t reserved;
} RawrSomMinimalReloc;

/**
 * Optional import request (DLL + name); linker assigns IAT/thunk for PE.
 * Other OSes map to GOT/PLT or equivalent in their emitter.
 */
typedef struct RawrSomMinimalImport {
    const char *dll_name;
    const char *symbol_name;
    uint16_t ordinal; /* 0 = use name */
    uint8_t by_ordinal;
    uint8_t reserved;
} RawrSomMinimalImport;

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_SOVEREIGN_SOM_MINIMAL_H */
