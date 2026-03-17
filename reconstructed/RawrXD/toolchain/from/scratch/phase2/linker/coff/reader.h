#ifndef COFF_READER_H
#define COFF_READER_H

/*==========================================================================
 * Phase 1: COFF Object File Reader - Fully Reverse Engineered
 *
 * Parses PE/COFF .obj files produced by MASM (ml64), MSVC (cl), or GCC/MinGW.
 * Handles the complete COFF specification including:
 *   - 20-byte COFF file header
 *   - 40-byte section headers with raw data loading
 *   - 18-byte symbol table entries with aux symbols
 *   - String table for long names (>8 chars)
 *   - 10-byte relocation entries per section
 *   - Machine type validation (x64 = 0x8664, x86 = 0x14C)
 *
 * Layout: [COFF Header][Section Headers][Raw Data][Relocations][Symbol Table][String Table]
 *=========================================================================*/

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ---- COFF machine types ---- */
#define COFF_MACHINE_AMD64      0x8664
#define COFF_MACHINE_I386       0x014C
#define COFF_MACHINE_UNKNOWN    0x0000

/* ---- COFF section characteristics (IMAGE_SCN_*) ---- */
#define SCN_CNT_CODE            0x00000020
#define SCN_CNT_INITIALIZED     0x00000040
#define SCN_CNT_UNINITIALIZED   0x00000080
#define SCN_LNK_COMDAT          0x00001000
#define SCN_LNK_NRELOC_OVFL     0x01000000
#define SCN_MEM_DISCARDABLE     0x02000000
#define SCN_MEM_NOT_CACHED      0x04000000
#define SCN_MEM_NOT_PAGED       0x08000000
#define SCN_MEM_SHARED          0x10000000
#define SCN_MEM_EXECUTE         0x20000000
#define SCN_MEM_READ            0x40000000
#define SCN_MEM_WRITE           0x80000000

/* Alignment encoding: bits 20-23 encode alignment as 2^(N-1) */
#define SCN_ALIGN_MASK          0x00F00000
#define SCN_ALIGN_SHIFT         20

/* ---- COFF symbol storage classes ---- */
#define SYM_CLASS_EXTERNAL      2
#define SYM_CLASS_STATIC        3
#define SYM_CLASS_LABEL         6
#define SYM_CLASS_FUNCTION      101
#define SYM_CLASS_FILE          103
#define SYM_CLASS_SECTION       104

/* ---- x64 relocation types (IMAGE_REL_AMD64_*) ---- */
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
#define REL_AMD64_SECREL7       0x000C
#define REL_AMD64_TOKEN         0x000D
#define REL_AMD64_SREL32        0x000E
#define REL_AMD64_PAIR          0x000F
#define REL_AMD64_SSPAN32       0x0010

/* ---- Forward declarations ---- */
typedef struct coff_file coff_file_t;
typedef struct coff_section coff_section_t;
typedef struct coff_symbol coff_symbol_t;
typedef struct coff_reloc coff_reloc_t;

/* ---- Section header (40 bytes in file) ---- */
struct coff_section {
  char     name[9];           /* null-terminated section name (or /N string table ref) */
  uint32_t virtual_size;      /* size in memory (0 for .obj, used in images) */
  uint32_t virtual_addr;      /* address in memory (0 for .obj) */
  uint32_t raw_size;          /* size of raw data on disk */
  uint32_t raw_offset;        /* file offset to raw data */
  uint32_t reloc_offset;      /* file offset to relocations */
  uint16_t num_line_numbers;  /* deprecated, usually 0 */
  uint32_t reloc_count;       /* number of relocations for this section */
  uint32_t characteristics;   /* flags: code/data/read/write/execute/alignment */
  uint8_t *data;              /* loaded raw data (malloc'd) */
  coff_reloc_t *relocs;       /* loaded per-section relocations */
  uint32_t alignment;         /* decoded alignment (1, 2, 4, 8, 16, ...) */
};

/* ---- Symbol table entry (18 bytes in file) ---- */
struct coff_symbol {
  char     name[256];         /* resolved name (from inline or string table) */
  int32_t  value;             /* value/offset within section */
  int16_t  section_num;       /* 1-based section index, 0=UNDEF, -1=ABS, -2=DEBUG */
  uint16_t type;              /* type info (0x20 = function) */
  uint8_t  storage_class;     /* SYM_CLASS_* */
  uint8_t  num_aux;           /* number of auxiliary symbol records following */
  int      obj_index;         /* which object file this symbol came from */
  uint32_t resolved_rva;      /* filled in during linking */
};

/* ---- Relocation entry (10 bytes in file) ---- */
struct coff_reloc {
  uint32_t virtual_addr;      /* offset within the section's raw data */
  uint32_t symbol_index;      /* index into the symbol table */
  uint16_t type;              /* REL_AMD64_* relocation type */
};

/* ---- COFF file container ---- */
struct coff_file {
  char     source_path[260];  /* original file path for diagnostics */
  uint16_t machine;           /* machine type */
  uint16_t num_sections;      /* number of section headers */
  uint32_t timestamp;         /* time/date stamp */
  uint32_t sym_table_offset;  /* file offset to symbol table */
  uint32_t num_symbols;       /* number of symbol table entries */
  uint16_t optional_hdr_size; /* should be 0 for .obj files */
  uint16_t characteristics;   /* file-level flags */

  coff_section_t *sections;   /* array of parsed sections with loaded data */
  coff_symbol_t  *symbols;    /* array of parsed symbols with resolved names */

  char    *string_table;      /* raw string table data */
  uint32_t string_table_size; /* size of string table in bytes */
};

/* ---- API ---- */
coff_file_t *coff_read(const char *path);
void         coff_free(coff_file_t *cf);

/* Lookup helpers */
coff_symbol_t *coff_find_symbol(coff_file_t *cf, const char *name);
coff_section_t *coff_find_section(coff_file_t *cf, const char *name);
uint32_t coff_section_alignment(uint32_t characteristics);
const char *coff_reloc_type_name(uint16_t type);
void coff_dump(coff_file_t *cf, FILE *out);

#endif
