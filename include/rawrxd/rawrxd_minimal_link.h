/* ============================================================================
 * rawrxd_minimal_link.h — Tier G minimal link (layout + symbol RVA + relocs)
 * ============================================================================
 *
 * From-scratch C: section alignment, symbol → RVA, REL32 / ABS64 patching.
 * Depends only on sovereign_som_minimal.h + C99.
 *
 * REL32 convention: @p offset_in_section is the **first byte of the 4-byte
 * displacement** (next instruction = section_rva + offset + 4).
 *
 * See docs/SOVEREIGN_TIER_G_REQUIREMENTS.md, SOVEREIGN_GLOBAL_USE_CONTRACT.md.
 * Tier P IDE: MSVC — §6/§7 in SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md.
 * ============================================================================ */

#ifndef RAWRXD_MINIMAL_LINK_H
#define RAWRXD_MINIMAL_LINK_H

#include "sovereign_som_minimal.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    enum RawrMinimalLinkResult
    {
        RAWR_ML_OK = 0,
        RAWR_ML_ERR_NULL = -1,
        RAWR_ML_ERR_RANGE = -2,
        RAWR_ML_ERR_NOT_FOUND = -3
    };

    uint32_t rawrxd_minimal_align_up(uint32_t value, uint32_t alignment);

/** PE-style defaults for @ref rawrxd_minimal_finalize_image_topography. */
#define RAWR_MINIMAL_TOPO_FIRST_RVA 0x1000u
#define RAWR_MINIMAL_TOPO_FIRST_FILE 0x400u
#define RAWR_MINIMAL_TOPO_SECTION_ALIGN 0x1000u
#define RAWR_MINIMAL_TOPO_FILE_ALIGN 0x200u

    /**
     * Topography pass — [The Topography Pass - Reverse Engineered]
     *
     * Reverse-engineered to only require the dual-cursor contract (PE-style
     * SectionAlignment vs FileAlignment). Canonical pseudocode:
     *
     *   void finalize_image(Builder* b) {
     *       uint32_t v_ptr = 0x1000; // Start of Virtual Space
     *       uint32_t f_ptr = 0x400;  // Start of File Space (after headers)
     *       for (int i = 0; i < b->section_count; i++) {
     *           Section* s = &b->sections[i];
     *           s->rva = v_ptr;
     *           s->file_offset = f_ptr;
     *           // The Alignment Contract
     *           v_ptr += (s->size + 0xFFF) & ~0xFFF; // 4KB Align
     *           f_ptr += (s->size + 0x1FF) & ~0x1FF; // 512B Align
     *       }
     *       b->total_image_size = v_ptr;
     *   }
     *
     * Implemented by rawrxd_minimal_finalize_image_topography(). Defaults:
     * RAWR_MINIMAL_TOPO_FIRST_RVA (0x1000), RAWR_MINIMAL_TOPO_FIRST_FILE (0x400),
     * RAWR_MINIMAL_TOPO_SECTION_ALIGN (0x1000), RAWR_MINIMAL_TOPO_FILE_ALIGN (0x200).
     * rawrxd_minimal_align_up() applies the same rounding for power-of-two alignments.
     * Total virtual span matches total_image_size; total on-disk extent is
     * *out_total_file_end (not named in the snippet).
     */
    typedef struct RawrMinimalTopographyOptions
    {
        uint32_t first_section_rva;
        uint32_t first_section_file_offset;
        uint32_t section_alignment;
        uint32_t file_alignment;
    } RawrMinimalTopographyOptions;

    int rawrxd_minimal_finalize_image_topography(RawrSomMinimalSection* sections, uint32_t section_count,
                                                 const RawrMinimalTopographyOptions* opts,
                                                 uint32_t* out_total_virtual_size, uint32_t* out_total_file_end);

    /**
     * Assign @p rva for each section (in order). Virtual size bumps use
     * @p section_align_rva. First section starts at @p first_rva (e.g. 0x1000).
     * Sets @p file_offset to 0 (no file topography). For PE-style **dual** layout,
     * use @ref rawrxd_minimal_finalize_image_topography.
     * @p out_total_rva_end optional — one past last used VA (aligned).
     */
    int rawrxd_minimal_layout_sections(RawrSomMinimalSection* sections, uint32_t section_count,
                                       uint32_t section_align_rva, uint32_t first_rva, uint32_t* out_total_rva_end);

    /**
     * Linear search for @p name; returns symbol's **RVA** (section rva + offset).
     */
    int rawrxd_minimal_symbol_rva(const RawrSomMinimalSymbol* symbols, uint32_t symbol_count,
                                  const RawrSomMinimalSection* sections, uint32_t section_count, const char* name,
                                  uint32_t* out_rva);

    /** Patch one relocation; sections must already have valid @p rva from layout. */
    int rawrxd_minimal_apply_reloc(const RawrSomMinimalReloc* reloc, RawrSomMinimalSection* sections,
                                   uint32_t section_count, const RawrSomMinimalSymbol* symbols, uint32_t symbol_count,
                                   uint64_t image_base);

    /** Apply array of relocs (stops on first error). */
    int rawrxd_minimal_apply_relocs(const RawrSomMinimalReloc* relocs, uint32_t reloc_count,
                                    RawrSomMinimalSection* sections, uint32_t section_count,
                                    const RawrSomMinimalSymbol* symbols, uint32_t symbol_count, uint64_t image_base);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_MINIMAL_LINK_H */
