/*==========================================================================
 * coff_writer.c — COFF Object File Writer implementation
 *
 * Builds a complete COFF .obj file from sections, symbols, and relocations.
 * All offsets are computed at write time for correct layout.
 *
 * Layout computation:
 *   file_offset = 20 (file header)
 *                 + num_sections * 40 (section headers)
 *   For each section:
 *     raw_data_ptr = current file_offset
 *     file_offset += section.data_size
 *     reloc_ptr = file_offset
 *     file_offset += section.reloc_count * 10
 *   sym_table_ptr = file_offset
 *   file_offset += num_symbols * 18
 *   string_table follows immediately after
 *=========================================================================*/
#include "coff_writer.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- Little-endian writers ---- */
static void wr16(FILE *f, uint16_t v) {
    uint8_t b[2] = { (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF) };
    fwrite(b, 1, 2, f);
}
static void wr32(FILE *f, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF),
        (uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 24) & 0xFF)
    };
    fwrite(b, 1, 4, f);
}

/* ---- Builder management ---- */
coff_obj_builder_t *coff_obj_new(void) {
    coff_obj_builder_t *obj = (coff_obj_builder_t *)calloc(1, sizeof(coff_obj_builder_t));
    obj->section_capacity = 16;
    obj->sections = (coff_section_builder_t *)calloc(16, sizeof(coff_section_builder_t));
    obj->symbol_capacity = 256;
    obj->symbols = (coff_symbol_entry_t *)calloc(256, sizeof(coff_symbol_entry_t));
    obj->string_capacity = 256;
    obj->strings = (char **)calloc(256, sizeof(char *));
    obj->string_table_size = 4; /* 4 bytes for size field */
    obj->timestamp = (uint32_t)time(NULL);
    return obj;
}

void coff_obj_free(coff_obj_builder_t *obj) {
    if (!obj) return;
    for (int i = 0; i < obj->section_count; i++) {
        free(obj->sections[i].data);
        free(obj->sections[i].relocs);
    }
    free(obj->sections);
    for (int i = 0; i < obj->string_count; i++)
        free(obj->strings[i]);
    free(obj->strings);
    free(obj->symbols);
    free(obj);
}

/* ---- String table management ---- */
static uint32_t add_string(coff_obj_builder_t *obj, const char *s) {
    uint32_t offset = obj->string_table_size;
    uint32_t len = (uint32_t)strlen(s) + 1;

    if (obj->string_count >= obj->string_capacity) {
        obj->string_capacity *= 2;
        obj->strings = (char **)realloc(obj->strings,
                       (size_t)obj->string_capacity * sizeof(char *));
    }
    obj->strings[obj->string_count] = strdup(s);
    obj->string_count++;
    obj->string_table_size += len;

    return offset;
}

/* ---- Section management ---- */
int coff_obj_add_section(coff_obj_builder_t *obj, const char *name, uint32_t flags) {
    if (obj->section_count >= obj->section_capacity) {
        obj->section_capacity *= 2;
        obj->sections = (coff_section_builder_t *)realloc(obj->sections,
                        (size_t)obj->section_capacity * sizeof(coff_section_builder_t));
    }

    coff_section_builder_t *sec = &obj->sections[obj->section_count];
    memset(sec, 0, sizeof(*sec));
    strncpy(sec->name, name, 63);
    sec->flags = flags;
    sec->data_capacity = 1024;
    sec->data = (uint8_t *)calloc(1, sec->data_capacity);
    sec->reloc_capacity = 64;
    sec->relocs = (coff_reloc_entry_t *)calloc(64, sizeof(coff_reloc_entry_t));

    /* Also add a section symbol to the symbol table */
    int sec_num = obj->section_count + 1; /* 1-based */
    coff_obj_add_symbol(obj, name, 0, (int16_t)sec_num, SYM_TYPE_NULL, SYM_CLASS_STATIC);

    return obj->section_count++;
}

int coff_obj_find_section(coff_obj_builder_t *obj, const char *name) {
    for (int i = 0; i < obj->section_count; i++) {
        if (strcmp(obj->sections[i].name, name) == 0) return i;
    }
    return -1;
}

void coff_section_append(coff_obj_builder_t *obj, int sec_idx, const uint8_t *data, uint32_t len) {
    if (sec_idx < 0 || sec_idx >= obj->section_count) return;
    coff_section_builder_t *sec = &obj->sections[sec_idx];

    while (sec->data_size + len > sec->data_capacity) {
        sec->data_capacity *= 2;
        sec->data = (uint8_t *)realloc(sec->data, sec->data_capacity);
    }
    memcpy(sec->data + sec->data_size, data, len);
    sec->data_size += len;
}

void coff_section_align(coff_obj_builder_t *obj, int sec_idx, uint32_t alignment) {
    if (sec_idx < 0 || sec_idx >= obj->section_count) return;
    coff_section_builder_t *sec = &obj->sections[sec_idx];

    if (alignment == 0) alignment = 1;
    uint32_t pad = (alignment - (sec->data_size % alignment)) % alignment;
    if (pad > 0) {
        while (sec->data_size + pad > sec->data_capacity) {
            sec->data_capacity *= 2;
            sec->data = (uint8_t *)realloc(sec->data, sec->data_capacity);
        }
        memset(sec->data + sec->data_size, 0, pad);
        sec->data_size += pad;
    }
}

uint32_t coff_section_size(coff_obj_builder_t *obj, int sec_idx) {
    if (sec_idx < 0 || sec_idx >= obj->section_count) return 0;
    return obj->sections[sec_idx].data_size;
}

/* ---- Relocation management ---- */
void coff_section_add_reloc(coff_obj_builder_t *obj, int sec_idx,
                            uint32_t offset, uint16_t type, const char *symbol) {
    if (sec_idx < 0 || sec_idx >= obj->section_count) return;
    coff_section_builder_t *sec = &obj->sections[sec_idx];

    if (sec->reloc_count >= sec->reloc_capacity) {
        sec->reloc_capacity *= 2;
        sec->relocs = (coff_reloc_entry_t *)realloc(sec->relocs,
                      (size_t)sec->reloc_capacity * sizeof(coff_reloc_entry_t));
    }

    coff_reloc_entry_t *r = &sec->relocs[sec->reloc_count++];
    r->offset = offset;
    r->type = type;
    strncpy(r->symbol, symbol, 127);
    r->sym_index = -1;
}

/* ---- Symbol management ---- */
int coff_obj_add_symbol(coff_obj_builder_t *obj, const char *name, int32_t value,
                        int16_t section_num, uint16_t type, uint8_t storage_class) {
    /* Check if symbol already exists */
    int existing = coff_obj_find_symbol(obj, name);
    if (existing >= 0) {
        /* Update if going from UNDEF to defined */
        if (obj->symbols[existing].section_num == 0 && section_num != 0) {
            obj->symbols[existing].value = value;
            obj->symbols[existing].section_num = section_num;
            obj->symbols[existing].type = type;
            obj->symbols[existing].storage_class = storage_class;
        }
        return existing;
    }

    if (obj->symbol_count >= obj->symbol_capacity) {
        obj->symbol_capacity *= 2;
        obj->symbols = (coff_symbol_entry_t *)realloc(obj->symbols,
                       (size_t)obj->symbol_capacity * sizeof(coff_symbol_entry_t));
    }

    coff_symbol_entry_t *sym = &obj->symbols[obj->symbol_count];
    memset(sym, 0, sizeof(*sym));
    strncpy(sym->name, name, 255);
    sym->value = value;
    sym->section_num = section_num;
    sym->type = type;
    sym->storage_class = storage_class;
    sym->aux_count = 0;

    return obj->symbol_count++;
}

int coff_obj_find_symbol(coff_obj_builder_t *obj, const char *name) {
    for (int i = 0; i < obj->symbol_count; i++) {
        if (strcmp(obj->symbols[i].name, name) == 0) return i;
    }
    return -1;
}

/* ============================================================
 * Write COFF .obj file
 *
 * Layout:
 *   1. File Header (20 bytes)
 *   2. Section Headers (40 * N bytes)
 *   3. For each section: Raw Data + Relocations
 *   4. Symbol Table (18 * M bytes)
 *   5. String Table (4 + strings)
 * ============================================================ */
int coff_obj_write_fp(coff_obj_builder_t *obj, FILE *fp) {
    int num_sections = obj->section_count;
    int num_symbols = obj->symbol_count;

    /* ---- Resolve relocation symbol indices ---- */
    for (int s = 0; s < num_sections; s++) {
        coff_section_builder_t *sec = &obj->sections[s];
        for (int r = 0; r < sec->reloc_count; r++) {
            int idx = coff_obj_find_symbol(obj, sec->relocs[r].symbol);
            if (idx < 0) {
                /* Auto-create as external/undefined */
                idx = coff_obj_add_symbol(obj, sec->relocs[r].symbol,
                                          0, 0, SYM_TYPE_NULL, SYM_CLASS_EXTERNAL);
                num_symbols = obj->symbol_count; /* may have grown */
            }
            sec->relocs[r].sym_index = idx;
        }
    }

    /* ---- Compute layout offsets ---- */
    uint32_t offset = 20 + (uint32_t)num_sections * 40;

    /* Per-section raw data and relocation offsets */
    uint32_t *raw_data_ptrs = (uint32_t *)calloc((size_t)num_sections, sizeof(uint32_t));
    uint32_t *reloc_ptrs = (uint32_t *)calloc((size_t)num_sections, sizeof(uint32_t));

    for (int s = 0; s < num_sections; s++) {
        coff_section_builder_t *sec = &obj->sections[s];
        if (sec->data_size > 0) {
            raw_data_ptrs[s] = offset;
            offset += sec->data_size;
        } else {
            raw_data_ptrs[s] = 0;
        }

        if (sec->reloc_count > 0) {
            reloc_ptrs[s] = offset;
            offset += (uint32_t)sec->reloc_count * 10;
        } else {
            reloc_ptrs[s] = 0;
        }
    }

    uint32_t sym_table_ptr = offset;

    /* ---- 1. File Header (20 bytes) ---- */
    wr16(fp, COFF_MACHINE_AMD64);          /* Machine */
    wr16(fp, (uint16_t)num_sections);      /* NumberOfSections */
    wr32(fp, obj->timestamp);              /* TimeDateStamp */
    wr32(fp, sym_table_ptr);               /* PointerToSymbolTable */
    wr32(fp, (uint32_t)num_symbols);       /* NumberOfSymbols */
    wr16(fp, 0);                           /* SizeOfOptionalHeader */
    wr16(fp, 0);                           /* Characteristics */

    /* ---- 2. Section Headers (40 bytes each) ---- */
    for (int s = 0; s < num_sections; s++) {
        coff_section_builder_t *sec = &obj->sections[s];

        /* Name (8 bytes) — if > 8 chars, use /N string table reference */
        uint8_t name_field[8];
        memset(name_field, 0, 8);
        if (strlen(sec->name) <= 8) {
            memcpy(name_field, sec->name, strlen(sec->name));
        } else {
            uint32_t str_off = add_string(obj, sec->name);
            snprintf((char *)name_field, 8, "/%u", str_off);
        }
        fwrite(name_field, 1, 8, fp);

        wr32(fp, 0);                           /* VirtualSize (0 for .obj) */
        wr32(fp, 0);                           /* VirtualAddress (0 for .obj) */
        wr32(fp, sec->data_size);              /* SizeOfRawData */
        wr32(fp, raw_data_ptrs[s]);            /* PointerToRawData */
        wr32(fp, reloc_ptrs[s]);               /* PointerToRelocations */
        wr32(fp, 0);                           /* PointerToLinenumbers */
        wr16(fp, (uint16_t)sec->reloc_count);  /* NumberOfRelocations */
        wr16(fp, 0);                           /* NumberOfLinenumbers */
        wr32(fp, sec->flags);                  /* Characteristics */
    }

    /* ---- 3. Section Raw Data + Relocations ---- */
    for (int s = 0; s < num_sections; s++) {
        coff_section_builder_t *sec = &obj->sections[s];

        /* Raw data */
        if (sec->data_size > 0) {
            fwrite(sec->data, 1, sec->data_size, fp);
        }

        /* Relocations (10 bytes each):
         *   VirtualAddress (4): offset within section
         *   SymbolTableIndex (4)
         *   Type (2)
         */
        for (int r = 0; r < sec->reloc_count; r++) {
            coff_reloc_entry_t *rel = &sec->relocs[r];
            wr32(fp, rel->offset);
            wr32(fp, (uint32_t)rel->sym_index);
            wr16(fp, rel->type);
        }
    }

    /* ---- 4. Symbol Table (18 bytes each) ----
     * Name: 8 bytes (inline if <= 8 chars, else {0,0,0,0, string_offset})
     * Value: 4 bytes
     * SectionNumber: 2 bytes (int16_t)
     * Type: 2 bytes
     * StorageClass: 1 byte
     * NumberOfAuxSymbols: 1 byte
     */
    for (int i = 0; i < num_symbols; i++) {
        coff_symbol_entry_t *sym = &obj->symbols[i];

        /* Name field (8 bytes) */
        if (strlen(sym->name) <= 8) {
            uint8_t name_field[8];
            memset(name_field, 0, 8);
            memcpy(name_field, sym->name, strlen(sym->name));
            fwrite(name_field, 1, 8, fp);
        } else {
            /* Long name: 4 zero bytes + 4-byte string table offset */
            uint32_t str_off = add_string(obj, sym->name);
            wr32(fp, 0);
            wr32(fp, str_off);
        }

        wr32(fp, (uint32_t)sym->value);           /* Value */
        wr16(fp, (uint16_t)sym->section_num);      /* SectionNumber */
        wr16(fp, sym->type);                       /* Type */
        fputc(sym->storage_class, fp);             /* StorageClass */
        fputc(sym->aux_count, fp);                 /* NumberOfAuxSymbols */
    }

    /* ---- 5. String Table ----
     * 4-byte size (includes the 4 bytes of the size field)
     * followed by null-terminated strings
     */
    wr32(fp, obj->string_table_size);
    for (int i = 0; i < obj->string_count; i++) {
        uint32_t len = (uint32_t)strlen(obj->strings[i]) + 1;
        fwrite(obj->strings[i], 1, len, fp);
    }

    free(raw_data_ptrs);
    free(reloc_ptrs);
    return 0;
}

int coff_obj_write(coff_obj_builder_t *obj, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot create '%s'\n", filename);
        return -1;
    }
    int result = coff_obj_write_fp(obj, fp);
    fclose(fp);
    return result;
}
