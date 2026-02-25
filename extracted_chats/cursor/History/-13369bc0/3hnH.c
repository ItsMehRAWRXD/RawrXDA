/*
 * Phase 2 linker CLI — rawrxd_link obj1.obj [obj2.obj ...] -o out.exe
 * Consumes Phase 1 COFF; emits PE32+ with entry stub and kernel32!ExitProcess.
 */
#include "coff_reader.h"
#include "pe_writer.h"
#include "reloc_resolver.h"
#include "entry_stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OBJS  32
#define TEXT_RVA  0x1000u
#define IDATA_RVA 0x2000u
#define IAT_EXITPROCESS_RVA (IDATA_RVA + 56u)  /* after IDT + ILT */

int main(int argc, char** argv) {
    const char* out_path = NULL;
    char* obj_paths[MAX_OBJS];
    int num_objs = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[++i];
            continue;
        }
        if (num_objs < MAX_OBJS)
            obj_paths[num_objs++] = argv[i];
    }
    if (num_objs == 0 || !out_path) {
        fprintf(stderr, "Usage: rawrxd_link obj1.obj [obj2.obj ...] -o out.exe\n");
        return 1;
    }

    CoffFile** objs = (CoffFile**)malloc((size_t)num_objs * sizeof(CoffFile*));
    if (!objs) { fprintf(stderr, "Out of memory\n"); return 1; }
    memset(objs, 0, (size_t)num_objs * sizeof(CoffFile*));

    for (int i = 0; i < num_objs; i++) {
        objs[i] = coff_read_file(obj_paths[i]);
        if (!objs[i]) {
            fprintf(stderr, "Failed to read: %s\n", obj_paths[i]);
            for (int j = 0; j < i; j++) coff_free(objs[j]);
            free(objs);
            return 1;
        }
    }

    /* Merge .text: stub + all obj .text sections */
    uint32_t stub_size = entry_stub_size;
    uint32_t merged_size = stub_size;
    uint32_t* obj_text_start = (uint32_t*)malloc((size_t)num_objs * sizeof(uint32_t));
    if (!obj_text_start) { for (int i = 0; i < num_objs; i++) coff_free(objs[i]); free(objs); return 1; }
    for (int i = 0; i < num_objs; i++) {
        obj_text_start[i] = merged_size;
        for (uint32_t s = 0; s < objs[i]->num_sections; s++)
            if (strcmp(objs[i]->sections[s].name, ".text") == 0) {
                merged_size += objs[i]->sections[s].size;
                break;
            }
    }
    uint8_t* merged_text = (uint8_t*)malloc(merged_size);
    if (!merged_text) { free(obj_text_start); for (int i = 0; i < num_objs; i++) coff_free(objs[i]); free(objs); return 1; }
    memcpy(merged_text, entry_stub_bytes, stub_size);
    uint32_t pos = stub_size;
    for (int i = 0; i < num_objs; i++) {
        for (uint32_t s = 0; s < objs[i]->num_sections; s++)
            if (strcmp(objs[i]->sections[s].name, ".text") == 0) {
                memcpy(merged_text + pos, objs[i]->sections[s].data, objs[i]->sections[s].size);
                pos += objs[i]->sections[s].size;
                break;
            }
    }

    /* Resolve "main" RVA (first definition wins) */
    uint32_t main_rva = 0;
    for (int i = 0; i < num_objs; i++) {
        for (uint32_t j = 0; j < objs[i]->num_symbols; j++) {
            CoffSymbol* sym = &objs[i]->symbols[j];
            const char* name = coff_symbol_name(objs[i], sym);
            if (strcmp(name, "main") == 0 && sym->section_number > 0) {
                main_rva = TEXT_RVA + obj_text_start[i] + sym->value;
                break;
            }
        }
        if (main_rva) break;
    }
    if (!main_rva) {
        fprintf(stderr, "No symbol 'main' defined\n");
        free(merged_text); free(obj_text_start); for (int i = 0; i < num_objs; i++) coff_free(objs[i]); free(objs);
        return 1;
    }

    /* Apply relocations from each obj: reloc at (obj_text_start[i] + reloc.offset), target = symbol RVA in same obj */
    for (int i = 0; i < num_objs; i++) {
        uint32_t base = obj_text_start[i];
        for (uint32_t s = 0; s < objs[i]->num_sections; s++) {
            CoffSection* sec = &objs[i]->sections[s];
            if (strcmp(sec->name, ".text") != 0) continue;
            for (uint32_t r = 0; r < sec->num_relocs; r++) {
                CoffReloc* rel = &sec->relocs[r];
                if (rel->symbol_index >= objs[i]->num_symbols) continue;
                CoffSymbol* sym = &objs[i]->symbols[rel->symbol_index];
                uint32_t target_rva;
                if (sym->section_number > 0) {
                    target_rva = TEXT_RVA + base + sym->value;
                } else {
                    /* External: resolve by name (e.g. main from another obj already in merged) */
                    const char* name = coff_symbol_name(objs[i], sym);
                    if (strcmp(name, "main") == 0)
                        target_rva = main_rva;
                    else
                        continue; /* skip unknown external */
                }
                reloc_apply(merged_text, TEXT_RVA, base + rel->offset_in_section, rel->type, target_rva);
            }
            break;
        }
    }

    /* Patch stub: rel32 to main, disp32 to IAT */
    reloc_apply(merged_text, TEXT_RVA, ENTRY_STUB_REL32_MAIN_OFFSET, 4, main_rva); /* REL32 */
    { /* disp32 for call [IAT]: rip after instruction = stub+11+6 = stub+17 */
        uint32_t rip_after = TEXT_RVA + 17u;
        uint32_t disp32 = IAT_EXITPROCESS_RVA - rip_after;
        uint8_t* at = merged_text + ENTRY_STUB_DISP32_IAT_OFFSET;
        at[0] = (uint8_t)(disp32 & 0xFF);
        at[1] = (uint8_t)((disp32 >> 8) & 0xFF);
        at[2] = (uint8_t)((disp32 >> 16) & 0xFF);
        at[3] = (uint8_t)(disp32 >> 24);
    }

    PeWriter* pw = pe_writer_create();
    if (!pw) { free(merged_text); free(obj_text_start); for (int i = 0; i < num_objs; i++) coff_free(objs[i]); free(objs); return 1; }
    pe_writer_set_entry(pw, 0);
    pe_writer_add_text(pw, merged_text, merged_size);
    pe_writer_set_import(pw, "kernel32.dll", "ExitProcess");

    uint8_t* pe_buf = NULL;
    uint32_t pe_size = pe_writer_emit(pw, &pe_buf);
    pe_writer_destroy(pw);
    free(merged_text);
    free(obj_text_start);
    for (int i = 0; i < num_objs; i++) coff_free(objs[i]);
    free(objs);

    if (!pe_size || !pe_buf) {
        fprintf(stderr, "PE emit failed\n");
        return 1;
    }

    FILE* f = fopen(out_path, "wb");
    if (!f) { fprintf(stderr, "Cannot write: %s\n", out_path); free(pe_buf); return 1; }
    fwrite(pe_buf, 1, pe_size, f);
    fclose(f);
    free(pe_buf);
    printf("Linked -> %s\n", out_path);
    return 0;
}
