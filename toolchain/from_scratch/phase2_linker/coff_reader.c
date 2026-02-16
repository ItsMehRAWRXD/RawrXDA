/*
 * COFF object file reader — parse Phase 1 .obj layout.
 */
#include "coff_reader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} CoffFileHeader;

typedef struct {
    char     Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLineNumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLineNumbers;
    uint32_t Characteristics;
} CoffSectionHeader;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t SymbolTableIndex;
    uint16_t Type;
} CoffRelocation;

typedef struct {
    union {
        char     ShortName[8];
        struct { uint32_t Zeros; uint32_t StringTableOffset; } Long;
    } Name;
    uint32_t Value;
    int16_t  SectionNumber;
    uint16_t Type;
    uint8_t  StorageClass;
    uint8_t  NumberOfAuxSymbols;
} CoffSymbolRaw;
#pragma pack(pop)

#define COFF_FILE_HEADER_SIZE 20
#define COFF_SECTION_HEADER_SIZE 40

static uint16_t read_u16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t read_u32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

CoffFile* coff_read_mem(const uint8_t* buf, size_t len) {
    if (!buf || len < COFF_FILE_HEADER_SIZE) return NULL;
    const CoffFileHeader* fh = (const CoffFileHeader*)buf;
    uint16_t num_sec = read_u16((const uint8_t*)&fh->NumberOfSections);
    uint32_t sym_off = read_u32((const uint8_t*)&fh->PointerToSymbolTable);
    uint32_t num_sym = read_u32((const uint8_t*)&fh->NumberOfSymbols);
    if (num_sec == 0 || sym_off + num_sym * (uint32_t)sizeof(CoffSymbolRaw) > len) return NULL;

    /* Count primary symbols (skip auxiliary entries in the symbol table). */
    uint32_t num_primary = 0;
    for (uint32_t i = 0; i < num_sym; ) {
        const CoffSymbolRaw* r = (const CoffSymbolRaw*)(buf + sym_off + i * sizeof(CoffSymbolRaw));
        num_primary++;
        i += 1u + r->NumberOfAuxSymbols;
    }

    CoffFile* cf = (CoffFile*)calloc(1, sizeof(CoffFile));
    if (!cf) return NULL;
    cf->machine = read_u16((const uint8_t*)&fh->Machine);
    cf->num_sections = num_sec;
    cf->sections = (CoffSection*)calloc(num_sec, sizeof(CoffSection));
    if (!cf->sections) { free(cf); return NULL; }
    cf->num_symbols = num_primary;
    cf->num_symbol_table_entries = num_sym;
    cf->symbols = (CoffSymbol*)calloc(num_primary, sizeof(CoffSymbol));
    if (!cf->symbols) { free(cf->sections); free(cf); return NULL; }
    cf->file_symbol_index_to_primary = (uint32_t*)malloc(num_sym * sizeof(uint32_t));
    if (!cf->file_symbol_index_to_primary) { free(cf->symbols); free(cf->sections); free(cf); return NULL; }
    for (uint32_t k = 0; k < num_sym; k++) cf->file_symbol_index_to_primary[k] = 0xFFFFFFFFu;

    const uint8_t* p = buf + sizeof(CoffFileHeader);
    uint32_t file_off = (uint32_t)sizeof(CoffFileHeader) + num_sec * (uint32_t)sizeof(CoffSectionHeader);

    for (uint32_t i = 0; i < num_sec; i++) {
        const CoffSectionHeader* sh = (const CoffSectionHeader*)p;
        CoffSection* s = &cf->sections[i];
        memcpy(s->name, sh->Name, 8);
        s->name[8] = '\0';
        s->size = read_u32((const uint8_t*)&sh->SizeOfRawData);
        s->characteristics = read_u32((const uint8_t*)&sh->Characteristics);
        uint32_t rel_count = read_u16((const uint8_t*)&sh->NumberOfRelocations);
        uint32_t data_off = read_u32((const uint8_t*)&sh->PointerToRawData);
        uint32_t rel_off = read_u32((const uint8_t*)&sh->PointerToRelocations);

        if (data_off + s->size > len) { coff_free(cf); return NULL; }
        s->data = (uint8_t*)malloc(s->size);
        if (!s->data) { coff_free(cf); return NULL; }
        memcpy(s->data, buf + data_off, s->size);

        s->num_relocs = rel_count;
        if (rel_count) {
            s->relocs = (CoffReloc*)malloc(rel_count * sizeof(CoffReloc));
            if (!s->relocs) { coff_free(cf); return NULL; }
            for (uint32_t j = 0; j < rel_count; j++) {
                const CoffRelocation* r = (const CoffRelocation*)(buf + rel_off + j * sizeof(CoffRelocation));
                s->relocs[j].offset_in_section = read_u32((const uint8_t*)&r->VirtualAddress);
                s->relocs[j].symbol_index = read_u32((const uint8_t*)&r->SymbolTableIndex);
                s->relocs[j].type = read_u16((const uint8_t*)&r->Type);
            }
        } else {
            s->relocs = NULL;
        }
        p += sizeof(CoffSectionHeader);
    }

    const CoffSymbolRaw* sym_raw = (const CoffSymbolRaw*)(buf + sym_off);
    uint32_t str_tbl_len = 0;
    if (sym_off + num_sym * sizeof(CoffSymbolRaw) + 4 <= len)
        str_tbl_len = read_u32(buf + sym_off + num_sym * (uint32_t)sizeof(CoffSymbolRaw));
    if (str_tbl_len > 0 && sym_off + num_sym * (uint32_t)sizeof(CoffSymbolRaw) + str_tbl_len <= len) {
        cf->string_table_size = str_tbl_len;
        cf->string_table = (char*)malloc(str_tbl_len);
        if (cf->string_table)
            memcpy(cf->string_table, buf + sym_off + num_sym * (uint32_t)sizeof(CoffSymbolRaw), str_tbl_len);
    }

    /* Resolve section long names (first 4 bytes 0 => bytes 4-7 are string table offset). */
    for (uint32_t i = 0; i < num_sec; i++) {
        CoffSection* s = &cf->sections[i];
        if (read_u32((const uint8_t*)s->name) == 0 && cf->string_table) {
            uint32_t off = read_u32((const uint8_t*)s->name + 4);
            if (off < cf->string_table_size) {
                const char* src = cf->string_table + off;
                size_t n = 0;
                while (n < 15 && src[n] != '\0') { s->name[n] = src[n]; n++; }
                s->name[n] = '\0';
            }
        }
    }

    for (uint32_t i = 0, dst = 0; i < num_sym && dst < num_primary; ) {
        const CoffSymbolRaw* r = &sym_raw[i];
        cf->file_symbol_index_to_primary[i] = dst;
        CoffSymbol* sym = &cf->symbols[dst];
        sym->value = read_u32((const uint8_t*)&r->Value);
        sym->section_number = (int16_t)read_u16((const uint8_t*)&r->SectionNumber);
        sym->storage_class = r->StorageClass;
        uint32_t stroff = read_u32((const uint8_t*)&r->Name.Long.StringTableOffset);
        if (read_u32((const uint8_t*)&r->Name.Long.Zeros) == 0 && stroff != 0 && cf->string_table && stroff < cf->string_table_size)
            sym->name = cf->string_table + stroff;
        else {
            char* copy = (char*)malloc(9);
            if (copy) {
                memcpy(copy, r->Name.ShortName, 8);
                copy[8] = '\0';
                sym->name = copy;
            } else
                sym->name = "";
        }
        dst++;
        for (uint8_t aux = 0; aux < r->NumberOfAuxSymbols; aux++)
            cf->file_symbol_index_to_primary[i + 1u + aux] = 0xFFFFFFFFu;
        i += 1u + r->NumberOfAuxSymbols;
    }
    return cf;
}

void coff_free(CoffFile* cf) {
    if (!cf) return;
    for (uint32_t i = 0; i < cf->num_sections; i++) {
        free(cf->sections[i].data);
        free(cf->sections[i].relocs);
    }
    free(cf->sections);
    free(cf->file_symbol_index_to_primary);
    for (uint32_t i = 0; i < cf->num_symbols; i++) {
        const char* n = cf->symbols[i].name;
        if (n && n[0] != '\0' && (!cf->string_table || n < cf->string_table || n >= cf->string_table + cf->string_table_size))
            free((void*)n);
    }
    free(cf->symbols);
    free(cf->string_table);
    free(cf);
}

CoffFile* coff_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 16*1024*1024) { fclose(f); return NULL; }
    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    CoffFile* cf = coff_read_mem(buf, n);
    free(buf);
    return cf;
}

const char* coff_symbol_name(const CoffFile* cf, const CoffSymbol* sym) {
    (void)cf;
    return sym->name ? sym->name : "";
}
