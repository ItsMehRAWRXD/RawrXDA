/*
 * Phase 2 linker CLI — rawrxd_link obj1.obj [obj2.obj ...] -o out.exe
 * Consumes Phase 1 COFF; emits PE32+ with entry stub and kernel32!ExitProcess.
 * Uses section_merge (layout) and reloc_resolver (fixups) as separate units.
 */
#include "coff_reader.h"
#include "entry_stub.h"
#include "pe_writer.h"
#include "reloc_resolver.h"
#include "section_merge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OBJS  32
#define IDATA_RVA 0x2000u
#define IAT_EXITPROCESS_RVA (IDATA_RVA + 56u)

static int debug_dump = 0;

int main(int argc, char** argv) {
    const char* out_path = NULL;
    char* obj_paths[MAX_OBJS];
    int num_objs = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_dump = 1;
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

    if (debug_dump && num_objs > 0) {
        CoffFile* cf = objs[0];
        fprintf(stderr, "[debug] sections=%u symbols=%u\n", cf->num_sections, cf->num_symbols);
        for (uint32_t s = 0; s < cf->num_sections; s++)
            fprintf(stderr, "  section %u: name=\"%s\" size=%u\n", s, cf->sections[s].name, cf->sections[s].size);
        for (uint32_t j = 0; j < cf->num_symbols; j++)
            fprintf(stderr, "  symbol %u: name=\"%s\" section=%d value=%u\n", j,
                coff_symbol_name(cf, &cf->symbols[j]), cf->symbols[j].section_number, cf->symbols[j].value);
    }

    merged_image_t* img = section_merge_create(objs, num_objs, 0x140000000ULL);
    if (!img) {
        fprintf(stderr, "Section merge failed\n");
        for (int i = 0; i < num_objs; i++) coff_free(objs[i]);
        free(objs);
        return 1;
    }

    uint32_t main_rva = section_merge_find_symbol(img, "main");
    if (!main_rva) {
        fprintf(stderr, "No symbol 'main' defined\n");
        section_merge_destroy(img);
        for (int i = 0; i < num_objs; i++) coff_free(objs[i]);
        free(objs);
        return 1;
    }

    if (section_merge_patch_stub(img, main_rva, IAT_EXITPROCESS_RVA) != 0) {
        fprintf(stderr, "Stub patch failed\n");
        section_merge_destroy(img);
        for (int i = 0; i < num_objs; i++) coff_free(objs[i]);
        free(objs);
        return 1;
    }

    uint32_t __main_rva = img->entry_point_rva + ENTRY_STUB_RET_OFFSET;
    if (reloc_resolver_apply(img->text.data, img->text.rva, img->obj_text_offsets, objs, num_objs, main_rva, __main_rva) != 0) {
        fprintf(stderr, "Relocation resolution failed (undefined symbol)\n");
        section_merge_destroy(img);
        for (int i = 0; i < num_objs; i++) coff_free(objs[i]);
        free(objs);
        return 1;
    }

    PeWriter* pw = pe_writer_create();
    if (!pw) {
        section_merge_destroy(img);
        for (int i = 0; i < num_objs; i++) coff_free(objs[i]);
        free(objs);
        return 1;
    }
    pe_writer_set_entry(pw, 0);
    pe_writer_add_text(pw, img->text.data, (uint32_t)img->text.size);
    pe_writer_set_import(pw, "kernel32.dll", "ExitProcess");

    uint8_t* pe_buf = NULL;
    uint32_t pe_size = pe_writer_emit(pw, &pe_buf);
    pe_writer_destroy(pw);
    section_merge_destroy(img);
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
