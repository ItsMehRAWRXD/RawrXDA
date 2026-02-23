/*
 * Section merge — layout engine: stub + .text concatenation, RVA assignment, symbol table.
 */
#include "section_merge.h"
#include "entry_stub.h"
#include <stdlib.h>
#include <string.h>
#ifdef DEBUG_STUB
#include <stdio.h>
#endif

#define IMAGE_SCN_CNT_CODE 0x00000020
#define SYMBOLS_INIT_CAP  256

static int is_text_section(const char* name) {
    return strcmp(name, ".text") == 0 || strncmp(name, ".text$", 6) == 0;
}

merged_image_t* section_merge_create(CoffFile** objs, int num_objs, uint64_t image_base) {
    (void)image_base;
    merged_image_t* img = (merged_image_t*)calloc(1, sizeof(merged_image_t));
    if (!img) return NULL;

    img->stub_size = entry_stub_size;
    img->text.rva = 0x1000u;
    img->text.symbols_cap = SYMBOLS_INIT_CAP;
    img->text.symbols = (merged_symbol_t*)malloc(SYMBOLS_INIT_CAP * sizeof(merged_symbol_t));
    if (!img->text.symbols) { free(img); return NULL; }
    img->num_objs = num_objs;
    img->obj_text_offsets = (uint32_t*)malloc((size_t)num_objs * sizeof(uint32_t));
    if (!img->obj_text_offsets) {
        free(img->text.symbols);
        free(img);
        return NULL;
    }
    for (int i = 0; i < num_objs; i++) img->obj_text_offsets[i] = 0;

    /* Total .text size: stub + all object .text sections */
    size_t total = entry_stub_size;
    for (int i = 0; i < num_objs; i++) {
        for (uint32_t s = 0; s < objs[i]->num_sections; s++) {
            if (!is_text_section(objs[i]->sections[s].name)) continue;
            {
                img->obj_text_offsets[i] = (uint32_t)total;
                total += objs[i]->sections[s].size;
                break;
            }
        }
    }

    img->text.size = total;
    img->text.virtual_size = (total + 0xFFFu) & ~0xFFFu;
    img->text.data = (uint8_t*)malloc(img->text.size);
    if (!img->text.data) {
        free(img->obj_text_offsets);
        free(img->text.symbols);
        free(img);
        return NULL;
    }

    memcpy(img->text.data, entry_stub_bytes, entry_stub_size);
    size_t write_pos = entry_stub_size;

    for (int i = 0; i < num_objs; i++) {
        for (uint32_t s = 0; s < objs[i]->num_sections; s++) {
            CoffSection* sec = &objs[i]->sections[s];
            if (!is_text_section(sec->name)) continue;

            memcpy(img->text.data + write_pos, sec->data, sec->size);

            /* Record symbols for this section (section_number is 1-based). */
            for (uint32_t j = 0; j < objs[i]->num_symbols; j++) {
                CoffSymbol* sym = &objs[i]->symbols[j];
                if (sym->section_number != (int16_t)(s + 1)) continue;

                if (img->text.num_symbols >= img->text.symbols_cap) {
                    size_t new_cap = img->text.symbols_cap * 2;
                    merged_symbol_t* new_syms = (merged_symbol_t*)realloc(
                        img->text.symbols, new_cap * sizeof(merged_symbol_t));
                    if (!new_syms) break;
                    img->text.symbols = new_syms;
                    img->text.symbols_cap = new_cap;
                }
                merged_symbol_t* out = &img->text.symbols[img->text.num_symbols];
                out->name = coff_symbol_name(objs[i], sym);
                out->rva = img->text.rva + (uint32_t)write_pos + sym->value;
                out->type = 0; /* code */
                img->text.num_symbols++;
            }

            write_pos += sec->size;
            break;
        }
    }

    img->entry_point_rva = img->text.rva;
    return img;
}

uint32_t section_merge_find_symbol(const merged_image_t* img, const char* name) {
    if (!img || !name) return 0;
    for (size_t i = 0; i < img->text.num_symbols; i++) {
        if (strcmp(img->text.symbols[i].name, name) == 0)
            return img->text.symbols[i].rva;
    }
    return 0;
}

int section_merge_patch_stub(merged_image_t* img, uint32_t main_rva, uint32_t iat_rva) {
    if (!img || !img->text.data) return -1;

    uint8_t* stub = img->text.data;
    uint32_t stub_base = img->entry_point_rva;

#ifdef DEBUG_STUB
    fprintf(stderr, "[STUB DEBUG] Raw stub before patch (stub size %u):\n  ", (unsigned)img->stub_size);
    for (unsigned i = 0; i < img->stub_size && i < 32u; i++) fprintf(stderr, "%02X ", stub[i]);
    fprintf(stderr, "\n");
    fprintf(stderr, "[STUB] stub_base = 0x%X, main_rva = 0x%X, iat_rva = 0x%X\n",
            stub_base, main_rva, iat_rva);
#endif

    /* 1. Patch call main. call at 0x04, rel32 at offset 5, length 5; next RIP = stub_base+9. */
    uint32_t call_main_next_rip = stub_base + 9u;
    int32_t rel32_main = (int32_t)(main_rva - call_main_next_rip);
    memcpy(stub + ENTRY_STUB_REL32_MAIN_OFFSET, &rel32_main, 4);
#ifdef DEBUG_STUB
    fprintf(stderr, "[STUB] rel32 = %d (0x%X)\n", rel32_main, (unsigned)rel32_main);
    fprintf(stderr, "[STUB] Target of call = 0x%X (should equal main_rva 0x%X)\n",
            call_main_next_rip + (uint32_t)rel32_main, main_rva);
#endif

    /* 2. Patch call [ExitProcess]. Next RIP after call [rip+disp32] = stub_base + 17. */
    uint32_t next_rip = stub_base + ENTRY_STUB_CALL_IAT_NEXT_RIP_OFFSET;
    int32_t disp32 = (int32_t)(iat_rva - next_rip);
    memcpy(stub + ENTRY_STUB_DISP32_IAT_OFFSET, &disp32, 4);
#ifdef DEBUG_STUB
    fprintf(stderr, "[STUB DEBUG] call [IAT]: disp32 = 0x%X (%d) at offset %u\n",
            (unsigned)disp32, disp32, (unsigned)ENTRY_STUB_DISP32_IAT_OFFSET);
    fprintf(stderr, "[STUB DEBUG]   Next RIP: 0x%X, Target IAT: 0x%X\n", next_rip, iat_rva);
    fprintf(stderr, "[STUB DEBUG] Raw stub after patch:\n  ");
    for (unsigned i = 0; i < img->stub_size && i < 32u; i++) fprintf(stderr, "%02X ", stub[i]);
    fprintf(stderr, "\n");
#endif

    img->iat_rva = iat_rva;
    return 0;
}

void section_merge_destroy(merged_image_t* img) {
    if (!img) return;
    free(img->text.data);
    /* Symbol names point into COFF string table or short copies; we don't own them. */
    free(img->text.symbols);
    free(img->obj_text_offsets);
    free(img);
}
