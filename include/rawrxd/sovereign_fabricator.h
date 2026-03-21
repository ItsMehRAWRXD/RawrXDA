/* ============================================================================
 * sovereign_fabricator.h — Linker-less PE64 "wrapper" contract (pure C)
 * ============================================================================
 *
 * PURPOSE: Capture the *smallest* mechanical story the user described: a linear
 * opcode buffer + entry RVA + image base, then a PE/MZ "envelope" the loader
 * understands. No symbol table is *required* if all addresses are pre-patched
 * (or single-section); with fixups, combine with sovereign_som_minimal.h and
 * sovereign_dispatch_table.h.
 *
 * IN-REPO IMPLEMENTATION: The tri-format lab builds headers via C++ helpers in
 *   include/rawrxd/sovereign_emit_formats.hpp  (composePe64MinimalImage, …)
 * and/or toolchain/from_scratch/phase2_linker/pe_writer.c — this header does
 * not call them; it documents the *inputs* those layers expect.
 *
 * TIER G: Global-use contract — docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md.
 * Tier P (IDE): MSVC cl + link.exe — §6/§7 in
 * docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md.
 * ============================================================================ */

#ifndef RAWRXD_SOVEREIGN_FABRICATOR_H
#define RAWRXD_SOVEREIGN_FABRICATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Minimal description of "I have bytes; put them in a PE image."
 * - code_bytes: pointer to x64 machine code (or data if you know what you're doing).
 * - code_size: size in bytes.
 * - entry_rva: RVA of the instruction pointer start (often first byte of .text).
 * - image_base: PE32+ preferred load address (typical 0x140000000 for 64-bit EXE).
 *
 * Section placement: many lab emitters place the first section at RVA 0x1000
 * and map file offset consistently; entry_rva is usually inside that range.
 */
typedef struct RawrSovereignFabricatorInput {
    const uint8_t *code_bytes;
    uint32_t code_size;
    uint32_t entry_rva;
    uint64_t image_base;
} RawrSovereignFabricatorInput;

/** Optional: single-segment layout hint (matches many minimal PE examples). */
enum RawrSovereignFabricatorDefaults {
    RAWR_FABRICATOR_FIRST_SECTION_RVA = 0x1000u,
    RAWR_FABRICATOR_COMMON_SECTION_ALIGN = 0x1000u,
    RAWR_FABRICATOR_COMMON_FILE_ALIGN = 0x200u
};

/**
 * If the image uses ASLR (DYNAMIC_BASE) and absolute VAs in the blob, the
 * loader may apply base relocations. PE base reloc types (for .reloc content)
 * include IMAGE_REL_BASED_DIR64 (10) for 64-bit absolute slots — see PE spec.
 * If you fix ImageBase and disable relocation, you may omit .reloc (policy +
 * loader behavior are OS/version dependent; lab only).
 */

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_SOVEREIGN_FABRICATOR_H */
