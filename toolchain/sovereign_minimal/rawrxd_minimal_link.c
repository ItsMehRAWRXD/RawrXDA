/* rawrxd_minimal_link.c — Tier G minimal link implementation (from scratch) */

#include "rawrxd/rawrxd_minimal_link.h"

#include <string.h>

uint32_t rawrxd_minimal_align_up(uint32_t value, uint32_t alignment)
{
    if (alignment == 0u)
    {
        return value;
    }
    const uint32_t mask = alignment - 1u;
    return (value + mask) & ~mask;
}

/*
 * The Topography Pass — [The Topography Pass - Reverse Engineered]
 * Full canonical pseudocode (finalize_image) lives in rawrxd_minimal_link.h
 * above RawrMinimalTopographyOptions. Summary:
 *   v_ptr = first_section_rva;       // e.g. 0x1000
 *   f_ptr = first_section_file_off;  // e.g. 0x400 (after headers)
 *   per section: assign rva/file_offset, then
 *     v_ptr += align_up(size, section_alignment);  // (size+0xFFF)&~0xFFF when align=4KiB
 *     f_ptr += align_up(size, file_alignment);     // (size+0x1FF)&~0x1FF when align=512B
 *   *out_total_virtual_size = final v_ptr  (like b->total_image_size in the RE snippet)
 *   *out_total_file_end = final f_ptr
 */
int rawrxd_minimal_finalize_image_topography(RawrSomMinimalSection *sections, uint32_t section_count,
                                             const RawrMinimalTopographyOptions *opts,
                                             uint32_t *out_total_virtual_size, uint32_t *out_total_file_end)
{
    if (sections == NULL || opts == NULL || section_count == 0u)
    {
        return RAWR_ML_ERR_NULL;
    }

    uint32_t sec_a = opts->section_alignment;
    uint32_t file_a = opts->file_alignment;
    if (sec_a == 0u)
    {
        sec_a = RAWR_MINIMAL_TOPO_SECTION_ALIGN;
    }
    if (file_a == 0u)
    {
        file_a = RAWR_MINIMAL_TOPO_FILE_ALIGN;
    }

    uint64_t v_ptr = (uint64_t)opts->first_section_rva;
    uint64_t f_ptr = (uint64_t)opts->first_section_file_offset;
    uint32_t i;

    for (i = 0u; i < section_count; ++i)
    {
        if (v_ptr > 0xFFFFFFFFull || f_ptr > 0xFFFFFFFFull)
        {
            return RAWR_ML_ERR_RANGE;
        }
        sections[i].rva = (uint32_t)v_ptr;
        sections[i].file_offset = (uint32_t)f_ptr;

        const uint32_t sz = sections[i].size;
        const uint64_t v_delta = (uint64_t)rawrxd_minimal_align_up(sz, sec_a);
        const uint64_t f_delta = (uint64_t)rawrxd_minimal_align_up(sz, file_a);
        v_ptr += v_delta;
        f_ptr += f_delta;
    }

    if (v_ptr > 0xFFFFFFFFull || f_ptr > 0xFFFFFFFFull)
    {
        return RAWR_ML_ERR_RANGE;
    }
    if (out_total_virtual_size != NULL)
    {
        *out_total_virtual_size = (uint32_t)v_ptr;
    }
    if (out_total_file_end != NULL)
    {
        *out_total_file_end = (uint32_t)f_ptr;
    }
    return RAWR_ML_OK;
}

int rawrxd_minimal_layout_sections(RawrSomMinimalSection *sections, uint32_t section_count,
                                   uint32_t section_align_rva, uint32_t first_rva,
                                   uint32_t *out_total_rva_end)
{
    if (sections == NULL || section_count == 0u)
    {
        return RAWR_ML_ERR_NULL;
    }

    uint64_t cursor = (uint64_t)first_rva;
    uint32_t i;
    for (i = 0u; i < section_count; ++i)
    {
        const uint64_t aligned_start = (uint64_t)rawrxd_minimal_align_up((uint32_t)cursor, section_align_rva);
        if (aligned_start > 0xFFFFFFFFull)
        {
            return RAWR_ML_ERR_RANGE;
        }
        sections[i].rva = (uint32_t)aligned_start;
        sections[i].file_offset = 0u;
        const uint64_t vsize = (uint64_t)sections[i].size;
        const uint64_t padded = (uint64_t)rawrxd_minimal_align_up((uint32_t)vsize, section_align_rva);
        cursor = aligned_start + padded;
        if (cursor > 0xFFFFFFFFull)
        {
            return RAWR_ML_ERR_RANGE;
        }
    }

    if (out_total_rva_end != NULL)
    {
        *out_total_rva_end = (uint32_t)cursor;
    }
    return RAWR_ML_OK;
}

int rawrxd_minimal_symbol_rva(const RawrSomMinimalSymbol *symbols, uint32_t symbol_count,
                              const RawrSomMinimalSection *sections, uint32_t section_count,
                              const char *name, uint32_t *out_rva)
{
    if (symbols == NULL || sections == NULL || name == NULL || out_rva == NULL)
    {
        return RAWR_ML_ERR_NULL;
    }

    uint32_t i;
    for (i = 0u; i < symbol_count; ++i)
    {
        if (symbols[i].name != NULL && strcmp(symbols[i].name, name) == 0)
        {
            const uint32_t si = symbols[i].section_index;
            if (si >= section_count)
            {
                return RAWR_ML_ERR_RANGE;
            }
            *out_rva = sections[si].rva + symbols[i].offset_in_section;
            return RAWR_ML_OK;
        }
    }
    return RAWR_ML_ERR_NOT_FOUND;
}

int rawrxd_minimal_apply_reloc(const RawrSomMinimalReloc *reloc, RawrSomMinimalSection *sections,
                               uint32_t section_count, const RawrSomMinimalSymbol *symbols,
                               uint32_t symbol_count, uint64_t image_base)
{
    if (reloc == NULL || sections == NULL || symbols == NULL)
    {
        return RAWR_ML_ERR_NULL;
    }

    const uint32_t si = reloc->section_index;
    if (si >= section_count || sections[si].data == NULL)
    {
        return RAWR_ML_ERR_RANGE;
    }
    if (reloc->offset_in_section + 4u > sections[si].size && reloc->type == RAWR_SOM_RELOC_REL32)
    {
        return RAWR_ML_ERR_RANGE;
    }
    if (reloc->offset_in_section + 8u > sections[si].size && reloc->type == RAWR_SOM_RELOC_ABS64)
    {
        return RAWR_ML_ERR_RANGE;
    }

    uint32_t target_rva = 0u;
    const int sr =
        rawrxd_minimal_symbol_rva(symbols, symbol_count, sections, section_count, reloc->target_name, &target_rva);
    if (sr != RAWR_ML_OK)
    {
        return sr;
    }

    uint8_t *patch = sections[si].data + reloc->offset_in_section;

    if (reloc->type == RAWR_SOM_RELOC_REL32)
    {
        const uint32_t sec_rva = sections[si].rva;
        const uint32_t next_inst_rva = sec_rva + reloc->offset_in_section + 4u;
        const int32_t rel32 = (int32_t)((uint64_t)target_rva - (uint64_t)next_inst_rva);
        memcpy(patch, &rel32, sizeof rel32);
    }
    else if (reloc->type == RAWR_SOM_RELOC_ABS64)
    {
        const uint64_t va = image_base + (uint64_t)target_rva;
        memcpy(patch, &va, sizeof va);
    }
    else
    {
        return RAWR_ML_ERR_RANGE;
    }

    return RAWR_ML_OK;
}

int rawrxd_minimal_apply_relocs(const RawrSomMinimalReloc *relocs, uint32_t reloc_count,
                                RawrSomMinimalSection *sections, uint32_t section_count,
                                const RawrSomMinimalSymbol *symbols, uint32_t symbol_count,
                                uint64_t image_base)
{
    if (relocs == NULL && reloc_count > 0u)
    {
        return RAWR_ML_ERR_NULL;
    }

    uint32_t i;
    for (i = 0u; i < reloc_count; ++i)
    {
        const int r = rawrxd_minimal_apply_reloc(&relocs[i], sections, section_count, symbols, symbol_count,
                                                 image_base);
        if (r != RAWR_ML_OK)
        {
            return r;
        }
    }
    return RAWR_ML_OK;
}
