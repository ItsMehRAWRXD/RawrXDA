/*
 * COFF object file reader — parse Phase 1 .obj (sections, symbols, relocs).
 * Same layout as Phase 1 coff.h; reader builds in-memory representation.
 */
#ifndef PHASE2_COFF_READER_H
#define PHASE2_COFF_READER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMAGE_REL_AMD64_ADDR64   0x0001
#define IMAGE_REL_AMD64_ADDR32NB 0x0003
#define IMAGE_REL_AMD64_REL32    0x0004

#define IMAGE_SYM_CLASS_EXTERNAL 2
#define IMAGE_SYM_CLASS_STATIC   3

/* Single relocation in a section */
typedef struct {
    uint32_t offset_in_section;
    uint32_t symbol_index;
    uint16_t type;
} CoffReloc;

/* Section with data and relocations */
typedef struct {
    char     name[16];
    uint8_t* data;
    uint32_t size;
    uint32_t characteristics;
    CoffReloc* relocs;
    uint32_t num_relocs;
} CoffSection;

/* Symbol (name owned by CoffFile string storage) */
typedef struct {
    const char* name;
    uint32_t value;
    int16_t  section_number;  /* 1-based, 0 = undefined */
    uint8_t  storage_class;
} CoffSymbol;

/* In-memory COFF object */
typedef struct {
    uint16_t machine;
    CoffSection* sections;
    uint32_t num_sections;
    CoffSymbol* symbols;
    uint32_t num_symbols;           /* number of primary symbols (cf->symbols length) */
    uint32_t num_symbol_table_entries; /* from file (primary + aux), for reloc index mapping */
    uint32_t* file_symbol_index_to_primary; /* [file index] -> symbols[] index, or 0xFFFFFFFF for aux */
    char* string_table;
    uint32_t string_table_size;
} CoffFile;

/* Load .obj from path. Returns NULL on error. Caller frees with coff_free. */
CoffFile* coff_read_file(const char* path);

/* Load .obj from buffer (len bytes). */
CoffFile* coff_read_mem(const uint8_t* buf, size_t len);

void coff_free(CoffFile* cf);

/* Get symbol name (from short name or string table). */
const char* coff_symbol_name(const CoffFile* cf, const CoffSymbol* sym);

#ifdef __cplusplus
}
#endif

#endif /* PHASE2_COFF_READER_H */
