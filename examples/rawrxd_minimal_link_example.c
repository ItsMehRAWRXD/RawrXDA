/*
 * rawrxd_minimal_link_example.c — Tier G: layout + symbol + REL32 via rawrxd_minimal_link
 *
 * CMake: cmake --build <build> --target rawrxd_minimal_link_example
 * Manual: gcc -std=c99 -Wall -I include examples/rawrxd_minimal_link_example.c \
 *         toolchain/sovereign_minimal/rawrxd_minimal_link.c -o rawrxd_minimal_link_example
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rawrxd/rawrxd_minimal_link.h"

static uint8_t g_sec0[] = {0xC3};
static uint8_t g_sec1[] = {0xE8, 0x00, 0x00, 0x00, 0x00, 0xC3};

int main(void)
{
    RawrSomMinimalSection sections[2];
    memset(&sections[0], 0, sizeof sections);
    sections[0].name = ".text_a";
    sections[0].data = g_sec0;
    sections[0].size = (uint32_t)sizeof g_sec0;
    sections[1].name = ".text_b";
    sections[1].data = g_sec1;
    sections[1].size = (uint32_t)sizeof g_sec1;

    RawrMinimalTopographyOptions topo;
    memset(&topo, 0, sizeof topo);
    topo.first_section_rva = RAWR_MINIMAL_TOPO_FIRST_RVA;
    topo.first_section_file_offset = RAWR_MINIMAL_TOPO_FIRST_FILE;
    topo.section_alignment = RAWR_MINIMAL_TOPO_SECTION_ALIGN;
    topo.file_alignment = RAWR_MINIMAL_TOPO_FILE_ALIGN;

    uint32_t total_va = 0u;
    uint32_t total_file = 0u;
    int lr = rawrxd_minimal_finalize_image_topography(sections, 2u, &topo, &total_va, &total_file);
    if (lr != RAWR_ML_OK)
    {
        fprintf(stderr, "topography failed: %d\n", lr);
        return 1;
    }

    RawrSomMinimalSymbol syms[1];
    syms[0].name = "callee";
    syms[0].section_index = 0u;
    syms[0].offset_in_section = 0u;

    RawrSomMinimalReloc rel;
    memset(&rel, 0, sizeof rel);
    rel.section_index = 1u;
    rel.offset_in_section = 1u; /* first byte of rel32 after E8 */
    rel.target_name = "callee";
    rel.type = RAWR_SOM_RELOC_REL32;

    lr = rawrxd_minimal_apply_reloc(&rel, sections, 2u, syms, 1u, 0x140000000ull);
    if (lr != RAWR_ML_OK)
    {
        fprintf(stderr, "reloc failed: %d\n", lr);
        return 1;
    }

    uint32_t callee_rva = 0u;
    (void)rawrxd_minimal_symbol_rva(syms, 1u, sections, 2u, "callee", &callee_rva);

    fprintf(stderr,
            "[minimal_link] rva[0]=0x%X file_off[0]=0x%X | rva[1]=0x%X file_off[1]=0x%X | callee_rva=0x%X "
            "total_va=0x%X total_file_end=0x%X\n",
            sections[0].rva, sections[0].file_offset, sections[1].rva, sections[1].file_offset, callee_rva, total_va,
            total_file);
    fprintf(stderr, "[minimal_link] patched: E8 %02X %02X %02X %02X C3\n", g_sec1[1], g_sec1[2], g_sec1[3], g_sec1[4]);

    return 0;
}
