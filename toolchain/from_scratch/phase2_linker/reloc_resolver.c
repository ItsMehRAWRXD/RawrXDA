/*
 * Relocation resolver — REL32 and ADDR64 fixups.
 * Type 0 = no-op; unsupported types emit a warning and are skipped (non-fatal).
 */
#include "reloc_resolver.h"
#include "coff_reader.h"
#include <stdio.h>
#include <string.h>

#define IMAGE_REL_AMD64_ABSOLUTE 0
#define IMAGE_REL_AMD64_ADDR64  0x0001
#define IMAGE_REL_AMD64_REL32   0x0004

static int is_text_section(const char* name) {
    return strcmp(name, ".text") == 0 || strncmp(name, ".text$", 6) == 0;
}

static void w32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)(v >> 24);
}

void reloc_apply(uint8_t* section_data, uint32_t section_rva, uint32_t offset, uint16_t type, uint32_t target_rva) {
    uint8_t* at = section_data + offset;
    if (type == IMAGE_REL_AMD64_ABSOLUTE) {
        /* No-op; common for absolute symbols in COFF */
        (void)at;
        (void)section_rva;
        (void)target_rva;
    } else if (type == IMAGE_REL_AMD64_REL32) {
        /* rel32 = target - (site_rva + 4); addend is already in the instruction, we overwrite */
        uint32_t current_next = section_rva + offset + 4u;
        uint32_t rel32 = target_rva - current_next;
        w32(at, rel32);
    } else if (type == IMAGE_REL_AMD64_ADDR64) {
        w32(at, target_rva);
        w32(at + 4, 0);
    } else {
        fprintf(stderr, "warning: unsupported relocation type %u (non-fatal)\n", (unsigned)type);
    }
}

int reloc_resolver_apply(uint8_t* text_data, uint32_t text_rva,
    const uint32_t* obj_text_offsets, CoffFile** objs, int num_objs, uint32_t main_rva, uint32_t __main_rva) {
    for (int i = 0; i < num_objs; i++) {
        CoffFile* cf = objs[i];
        uint32_t base = obj_text_offsets[i];

        for (uint32_t s = 0; s < cf->num_sections; s++) {
            CoffSection* sec = &cf->sections[s];
            if (!is_text_section(sec->name)) continue;

            for (uint32_t r = 0; r < sec->num_relocs; r++) {
                CoffReloc* rel = &sec->relocs[r];
                if (rel->symbol_index >= cf->num_symbol_table_entries) continue;
                uint32_t primary_idx = cf->file_symbol_index_to_primary[rel->symbol_index];
                if (primary_idx == 0xFFFFFFFFu) continue; /* aux entry */

                CoffSymbol* sym = &cf->symbols[primary_idx];
                uint32_t target_rva;

                if (sym->section_number > 0) {
                    target_rva = text_rva + base + sym->value;
                } else {
                    const char* name = coff_symbol_name(cf, sym);
                    if (strcmp(name, "main") == 0)
                        target_rva = main_rva;
                    else if (strcmp(name, "__main") == 0)
                        target_rva = __main_rva;  /* GCC CRT init; point to stub ret to avoid recursion */
                    else {
                        fprintf(stderr, "undefined symbol: %s\n", name);
                        return -1;
                    }
                }

                reloc_apply(text_data, text_rva, base + rel->offset_in_section, rel->type, target_rva);
            }
            break;
        }
    }
    return 0;
}
