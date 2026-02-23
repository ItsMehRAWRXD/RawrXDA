/*
 * Section merge — stub injection, .text concatenation, RVA assignment, symbol table.
 * Consumes CoffFile objects; produces merged_image_t for PE writer and reloc resolver.
 */
#ifndef PHASE2_SECTION_MERGE_H
#define PHASE2_SECTION_MERGE_H

#include "coff_reader.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;
    uint32_t rva;
    uint8_t type;   /* 0 = code, 1 = data */
} merged_symbol_t;

typedef struct {
    uint8_t* data;
    size_t size;
    size_t virtual_size;
    uint32_t rva;
    uint32_t file_offset;

    merged_symbol_t* symbols;
    size_t num_symbols;
    size_t symbols_cap;
} merged_section_t;

typedef struct {
    merged_section_t text;
    uint64_t image_base;
    uint32_t entry_point_rva;
    uint32_t stub_size;
    uint32_t iat_rva;

    /* Offsets in .text.data where each object's .text starts (for reloc resolver). */
    uint32_t* obj_text_offsets;
    int num_objs;
} merged_image_t;

/* Build merged .text (stub + all object .text sections) and symbol table. */
merged_image_t* section_merge_create(CoffFile** objs, int num_objs, uint64_t image_base);

void section_merge_destroy(merged_image_t* img);

/* Look up symbol by name; returns RVA or 0 if not found. */
uint32_t section_merge_find_symbol(const merged_image_t* img, const char* name);

/* Patch stub: rel32 to main, disp32 to IAT (ExitProcess). */
int section_merge_patch_stub(merged_image_t* img, uint32_t main_rva, uint32_t exit_process_iat_rva);

#ifdef __cplusplus
}
#endif

#endif /* PHASE2_SECTION_MERGE_H */
