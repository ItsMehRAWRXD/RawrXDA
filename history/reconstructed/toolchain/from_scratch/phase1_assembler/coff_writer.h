/*==========================================================================
 * coff_writer.h — COFF Object File Writer
 *
 * The inverse of phase2's coff_reader. Builds COFF .obj files from
 * assembled instruction bytes, section data, symbols, and relocations.
 *
 * COFF .obj layout:
 *   [File Header (20 bytes)]
 *   [Section Headers (40 bytes each)]
 *   [Section Raw Data]
 *   [Relocations (10 bytes each)]
 *   [Symbol Table (18 bytes each)]
 *   [String Table (4-byte size prefix + strings)]
 *
 * File Header fields:
 *   Machine (2): 0x8664 for x64
 *   NumberOfSections (2)
 *   TimeDateStamp (4)
 *   PointerToSymbolTable (4)
 *   NumberOfSymbols (4)
 *   SizeOfOptionalHeader (2): 0 for .obj
 *   Characteristics (2)
 *
 * Section Header fields:
 *   Name (8): zero-padded or /N string table offset
 *   VirtualSize (4): 0 for .obj
 *   VirtualAddress (4): 0 for .obj
 *   SizeOfRawData (4)
 *   PointerToRawData (4)
 *   PointerToRelocations (4)
 *   PointerToLinenumbers (4): 0
 *   NumberOfRelocations (2)
 *   NumberOfLinenumbers (2): 0
 *   Characteristics (4): section flags
 *=========================================================================*/
#ifndef COFF_WRITER_H
#define COFF_WRITER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ---- COFF Machine types ---- */
#define COFF_MACHINE_AMD64 0x8664

/* ---- COFF section flags (matching phase2 reader) ---- */
#define SCN_CNT_CODE            0x00000020
#define SCN_CNT_INITIALIZED     0x00000040
#define SCN_CNT_UNINITIALIZED   0x00000080
#define SCN_LNK_INFO            0x00000200
#define SCN_LNK_COMDAT          0x00001000
#define SCN_ALIGN_1             0x00100000
#define SCN_ALIGN_2             0x00200000
#define SCN_ALIGN_4             0x00300000
#define SCN_ALIGN_8             0x00400000
#define SCN_ALIGN_16            0x00500000
#define SCN_MEM_EXECUTE         0x20000000
#define SCN_MEM_READ            0x40000000
#define SCN_MEM_WRITE           0x80000000

/* ---- Relocation types (AMD64) ---- */
#define REL_AMD64_ABSOLUTE      0x0000
#define REL_AMD64_ADDR64        0x0001
#define REL_AMD64_ADDR32        0x0002
#define REL_AMD64_ADDR32NB      0x0003
#define REL_AMD64_REL32         0x0004
#define REL_AMD64_REL32_1       0x0005
#define REL_AMD64_REL32_2       0x0006
#define REL_AMD64_REL32_3       0x0007
#define REL_AMD64_REL32_4       0x0008
#define REL_AMD64_REL32_5       0x0009
#define REL_AMD64_SECTION       0x000A
#define REL_AMD64_SECREL        0x000B

/* ---- Symbol storage classes ---- */
#define SYM_CLASS_EXTERNAL      2
#define SYM_CLASS_STATIC        3
#define SYM_CLASS_LABEL         6
#define SYM_CLASS_FUNCTION      101
#define SYM_CLASS_FILE          103
#define SYM_CLASS_SECTION       104

/* ---- Symbol type ---- */
#define SYM_TYPE_NULL           0x0000
#define SYM_TYPE_FUNCTION       0x0020

/* ---- Relocation entry for builder ---- */
typedef struct {
    uint32_t offset;        /* offset within section */
    uint16_t type;          /* REL_AMD64_* */
    char     symbol[128];   /* symbol name */
    int      sym_index;     /* resolved symbol table index (filled at write) */
} coff_reloc_entry_t;

/* ---- Section builder ---- */
typedef struct {
    char     name[64];
    uint32_t flags;
    uint8_t *data;
    uint32_t data_size;
    uint32_t data_capacity;
    coff_reloc_entry_t *relocs;
    int      reloc_count;
    int      reloc_capacity;
} coff_section_builder_t;

/* ---- Symbol entry for builder ---- */
typedef struct {
    char     name[256];
    int32_t  value;          /* offset within section */
    int16_t  section_num;    /* 1-based, 0=UNDEF, -1=ABS, -2=DEBUG */
    uint16_t type;
    uint8_t  storage_class;
    uint8_t  aux_count;
} coff_symbol_entry_t;

/* ---- COFF object builder ---- */
typedef struct {
    coff_section_builder_t *sections;
    int section_count;
    int section_capacity;

    coff_symbol_entry_t *symbols;
    int symbol_count;
    int symbol_capacity;

    char **strings;
    int string_count;
    int string_capacity;
    uint32_t string_table_size;

    uint32_t timestamp;
} coff_obj_builder_t;

/* ---- API ---- */
coff_obj_builder_t *coff_obj_new(void);
void coff_obj_free(coff_obj_builder_t *obj);

/* Section management */
int coff_obj_add_section(coff_obj_builder_t *obj, const char *name, uint32_t flags);
int coff_obj_find_section(coff_obj_builder_t *obj, const char *name);
void coff_section_append(coff_obj_builder_t *obj, int sec_idx, const uint8_t *data, uint32_t len);
void coff_section_align(coff_obj_builder_t *obj, int sec_idx, uint32_t alignment);
uint32_t coff_section_size(coff_obj_builder_t *obj, int sec_idx);

/* Relocation management */
void coff_section_add_reloc(coff_obj_builder_t *obj, int sec_idx,
                            uint32_t offset, uint16_t type, const char *symbol);

/* Symbol management */
int coff_obj_add_symbol(coff_obj_builder_t *obj, const char *name, int32_t value,
                        int16_t section_num, uint16_t type, uint8_t storage_class);
int coff_obj_find_symbol(coff_obj_builder_t *obj, const char *name);

/* Write COFF .obj to file/buffer */
int coff_obj_write(coff_obj_builder_t *obj, const char *filename);
int coff_obj_write_fp(coff_obj_builder_t *obj, FILE *fp);

#endif
