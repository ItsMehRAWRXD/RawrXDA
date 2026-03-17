/*==========================================================================
 * Phase 1: COFF Object File Reader - Fully Reverse Engineered
 *
 * Complete implementation parsing PE/COFF object files:
 *   1. Read 20-byte COFF header, validate machine type
 *   2. Read N x 40-byte section headers
 *   3. Load raw data for each section (seek to raw_offset, read raw_size)
 *   4. Read symbol table (18 bytes per entry + aux records)
 *   5. Read string table (immediately follows symbol table)
 *   6. Resolve symbol names: inline (<=8 chars) or string table offset
 *   7. Read per-section relocations (10 bytes each)
 *   8. Decode section alignment from characteristics bits 20-23
 *
 * File layout:
 *   Offset 0:    COFF header (20 bytes)
 *   Offset 20:   Section headers (40 bytes each)
 *   Variable:    Raw section data (pointed to by each header)
 *   Variable:    Per-section relocations (pointed to by each header)
 *   sym_table_offset: Symbol table (18 bytes * num_symbols)
 *   After syms:  String table (uint32 size + null-terminated strings)
 *=========================================================================*/
#include "coff_reader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- Little-endian read helpers ---- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1]<<8) | (p[2]<<16) | ((uint32_t)p[3]<<24); }
static int32_t  rds32(const uint8_t *p) { return (int32_t)rd32(p); }
static int16_t  rds16(const uint8_t *p) { return (int16_t)rd16(p); }

/*--------------------------------------------------------------------------
 * Decode alignment from section characteristics bits 20-23.
 * Value N (1-14) means alignment = 2^(N-1). 0 means default (1).
 *------------------------------------------------------------------------*/
uint32_t coff_section_alignment(uint32_t characteristics) {
  uint32_t align_val = (characteristics & SCN_ALIGN_MASK) >> SCN_ALIGN_SHIFT;
  if (align_val == 0) return 1;
  if (align_val > 14) align_val = 14;
  return 1u << (align_val - 1);
}

/*--------------------------------------------------------------------------
 * Resolve a symbol name.
 * COFF symbol names are stored in an 8-byte field:
 *   - If first 4 bytes are zero, next 4 bytes are offset into string table
 *   - Otherwise, the 8 bytes are the name (NOT null-terminated if exactly 8)
 *------------------------------------------------------------------------*/
static void resolve_symbol_name(const uint8_t *raw_name, const char *string_table,
                                uint32_t string_table_size, char *out, size_t out_sz) {
  if (raw_name[0] == 0 && raw_name[1] == 0 && raw_name[2] == 0 && raw_name[3] == 0) {
    /* String table reference */
    uint32_t offset = rd32(raw_name + 4);
    if (string_table && offset < string_table_size) {
      size_t len = strlen(string_table + offset);
      if (len >= out_sz) len = out_sz - 1;
      memcpy(out, string_table + offset, len);
      out[len] = '\0';
    } else {
      snprintf(out, out_sz, "<strtab@%u>", offset);
    }
  } else {
    /* Inline name, up to 8 chars */
    size_t len = 0;
    while (len < 8 && raw_name[len]) len++;
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, raw_name, len);
    out[len] = '\0';
  }
}

/*--------------------------------------------------------------------------
 * Resolve a section name (also supports /N string table references).
 * Section names starting with '/' followed by decimal digits are offsets
 * into the string table (used by MSVC for long section names like .debug$S).
 *------------------------------------------------------------------------*/
static void resolve_section_name(const uint8_t *raw_name, const char *string_table,
                                 uint32_t string_table_size, char *out, size_t out_sz) {
  if (raw_name[0] == '/') {
    uint32_t offset = (uint32_t)strtoul((const char *)raw_name + 1, NULL, 10);
    if (string_table && offset < string_table_size) {
      size_t len = strlen(string_table + offset);
      if (len >= out_sz) len = out_sz - 1;
      memcpy(out, string_table + offset, len);
      out[len] = '\0';
      return;
    }
  }
  /* Standard 8-char name */
  size_t len = 0;
  while (len < 8 && raw_name[len]) len++;
  if (len >= out_sz) len = out_sz - 1;
  memcpy(out, raw_name, len);
  out[len] = '\0';
}

/*--------------------------------------------------------------------------
 * Read a COFF object file. Full pipeline:
 *   Step 1: Open file, read COFF header
 *   Step 2: Read section headers
 *   Step 3: Read string table (need it before resolving names)
 *   Step 4: Read symbol table, resolve names
 *   Step 5: Resolve section names (may reference string table)
 *   Step 6: Load section raw data
 *   Step 7: Load per-section relocations
 *------------------------------------------------------------------------*/
coff_file_t *coff_read(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "[coff_reader] cannot open: %s\n", path);
    return NULL;
  }

  coff_file_t *cf = (coff_file_t *)calloc(1, sizeof(coff_file_t));
  if (!cf) { fclose(f); return NULL; }
  strncpy(cf->source_path, path, sizeof(cf->source_path) - 1);

  /*======== Step 1: COFF File Header (20 bytes) ========
   * Offset  Size  Field
   * 0       2     Machine
   * 2       2     NumberOfSections
   * 4       4     TimeDateStamp
   * 8       4     PointerToSymbolTable
   * 12      4     NumberOfSymbols
   * 16      2     SizeOfOptionalHeader
   * 18      2     Characteristics
   *=====================================================*/
  uint8_t hdr[20];
  if (fread(hdr, 1, 20, f) != 20) {
    fprintf(stderr, "[coff_reader] %s: truncated header\n", path);
    free(cf); fclose(f); return NULL;
  }

  cf->machine          = rd16(hdr + 0);
  cf->num_sections     = rd16(hdr + 2);
  cf->timestamp        = rd32(hdr + 4);
  cf->sym_table_offset = rd32(hdr + 8);
  cf->num_symbols      = rd32(hdr + 12);
  cf->optional_hdr_size = rd16(hdr + 16);
  cf->characteristics  = rd16(hdr + 18);

  /* Validate machine type */
  if (cf->machine != COFF_MACHINE_AMD64 && cf->machine != COFF_MACHINE_I386 &&
      cf->machine != COFF_MACHINE_UNKNOWN) {
    fprintf(stderr, "[coff_reader] %s: unknown machine 0x%04X\n", path, cf->machine);
    free(cf); fclose(f); return NULL;
  }

  /* Skip optional header if present (shouldn't be for .obj, but handle it) */
  if (cf->optional_hdr_size > 0) {
    fseek(f, cf->optional_hdr_size, SEEK_CUR);
  }

  /*======== Step 2: Section Headers (40 bytes each) ========
   * Offset  Size  Field
   * 0       8     Name
   * 8       4     VirtualSize
   * 12      4     VirtualAddress
   * 16      4     SizeOfRawData
   * 20      4     PointerToRawData
   * 24      4     PointerToRelocations
   * 28      4     PointerToLinenumbers
   * 32      2     NumberOfRelocations
   * 34      2     NumberOfLinenumbers
   * 36      4     Characteristics
   *========================================================*/
  cf->sections = (coff_section_t *)calloc((size_t)cf->num_sections, sizeof(coff_section_t));
  if (!cf->sections && cf->num_sections > 0) {
    free(cf); fclose(f); return NULL;
  }

  uint8_t sec_raw[40];
  for (uint16_t i = 0; i < cf->num_sections; i++) {
    if (fread(sec_raw, 1, 40, f) != 40) {
      fprintf(stderr, "[coff_reader] %s: truncated section header %u\n", path, i);
      break;
    }
    /* Store raw name bytes temporarily; will resolve after loading string table */
    memcpy(cf->sections[i].name, sec_raw, 8);
    cf->sections[i].name[8] = '\0';

    cf->sections[i].virtual_size  = rd32(sec_raw + 8);
    cf->sections[i].virtual_addr  = rd32(sec_raw + 12);
    cf->sections[i].raw_size      = rd32(sec_raw + 16);
    cf->sections[i].raw_offset    = rd32(sec_raw + 20);
    cf->sections[i].reloc_offset  = rd32(sec_raw + 24);
    /* linenumbers at offset 28 (4 bytes) - skip */
    cf->sections[i].reloc_count   = rd16(sec_raw + 32);
    cf->sections[i].num_line_numbers = rd16(sec_raw + 34);
    cf->sections[i].characteristics = rd32(sec_raw + 36);

    /* Handle extended relocation count (IMAGE_SCN_LNK_NRELOC_OVFL) */
    if ((cf->sections[i].characteristics & SCN_LNK_NRELOC_OVFL) &&
        cf->sections[i].reloc_count == 0xFFFF) {
      /* First relocation entry holds the actual count in virtual_addr field */
      /* Will be resolved during relocation loading */
    }

    /* Decode alignment */
    cf->sections[i].alignment = coff_section_alignment(cf->sections[i].characteristics);
    cf->sections[i].data = NULL;
    cf->sections[i].relocs = NULL;
  }

  /*======== Step 3: String Table ========
   * Immediately follows the symbol table.
   * Format: uint32 total_size (including the 4-byte size field itself),
   *         then null-terminated strings.
   * Minimum size is 4 (just the size field, meaning empty table).
   *========================================*/
  if (cf->sym_table_offset > 0 && cf->num_symbols > 0) {
    uint32_t strtab_offset = cf->sym_table_offset + (cf->num_symbols * 18);
    long saved_pos = ftell(f);
    if (fseek(f, strtab_offset, SEEK_SET) == 0) {
      uint8_t sz_buf[4];
      if (fread(sz_buf, 1, 4, f) == 4) {
        cf->string_table_size = rd32(sz_buf);
        if (cf->string_table_size >= 4 && cf->string_table_size < 0x10000000) {
          cf->string_table = (char *)calloc(1, cf->string_table_size);
          if (cf->string_table) {
            /* Copy size field as first 4 bytes */
            memcpy(cf->string_table, sz_buf, 4);
            /* Read rest of string table */
            if (cf->string_table_size > 4) {
              size_t remaining = cf->string_table_size - 4;
              if (fread(cf->string_table + 4, 1, remaining, f) != remaining) {
                fprintf(stderr, "[coff_reader] %s: truncated string table\n", path);
              }
            }
          }
        }
      }
    }
    fseek(f, saved_pos, SEEK_SET);
  }

  /*======== Step 4: Symbol Table (18 bytes each) ========
   * Offset  Size  Field
   * 0       8     Name (or string table reference)
   * 8       4     Value
   * 12      2     SectionNumber (1-based, 0=UNDEF, -1=ABS, -2=DEBUG)
   * 14      2     Type (0x20 = function)
   * 16      1     StorageClass
   * 17      1     NumberOfAuxSymbols
   *====================================================*/
  if (cf->sym_table_offset > 0 && cf->num_symbols > 0) {
    cf->symbols = (coff_symbol_t *)calloc((size_t)cf->num_symbols, sizeof(coff_symbol_t));
    if (cf->symbols) {
      fseek(f, cf->sym_table_offset, SEEK_SET);
      uint8_t sym_raw[18];
      for (uint32_t i = 0; i < cf->num_symbols; i++) {
        if (fread(sym_raw, 1, 18, f) != 18) {
          fprintf(stderr, "[coff_reader] %s: truncated symbol %u\n", path, i);
          break;
        }
        resolve_symbol_name(sym_raw, cf->string_table, cf->string_table_size,
                            cf->symbols[i].name, sizeof(cf->symbols[i].name));
        cf->symbols[i].value         = rds32(sym_raw + 8);
        cf->symbols[i].section_num   = rds16(sym_raw + 12);
        cf->symbols[i].type          = rd16(sym_raw + 14);
        cf->symbols[i].storage_class = sym_raw[16];
        cf->symbols[i].num_aux       = sym_raw[17];
        cf->symbols[i].obj_index     = 0;
        cf->symbols[i].resolved_rva  = 0;

        /* Skip auxiliary symbol records (18 bytes each) */
        if (cf->symbols[i].num_aux > 0) {
          for (uint8_t a = 0; a < cf->symbols[i].num_aux && (i + 1) < cf->num_symbols; a++) {
            i++;
            if (fread(sym_raw, 1, 18, f) != 18) break;
            /* Mark aux records with empty name */
            cf->symbols[i].name[0] = '\0';
            cf->symbols[i].storage_class = 0xFF; /* sentinel: aux record */
            cf->symbols[i].num_aux = 0;
          }
        }
      }
    }
  }

  /*======== Step 5: Resolve section names (may use string table) ========*/
  for (uint16_t i = 0; i < cf->num_sections; i++) {
    char resolved[256];
    resolve_section_name((const uint8_t *)cf->sections[i].name,
                         cf->string_table, cf->string_table_size,
                         resolved, sizeof(resolved));
    /* Copy resolved name back (truncate to 8 for the struct field) */
    memset(cf->sections[i].name, 0, 9);
    strncpy(cf->sections[i].name, resolved, 8);
  }

  /*======== Step 6: Load section raw data ========*/
  for (uint16_t i = 0; i < cf->num_sections; i++) {
    if (cf->sections[i].raw_size == 0 || cf->sections[i].raw_offset == 0) continue;
    /* BSS sections have no raw data even if characteristics say initialized */
    if (cf->sections[i].characteristics & SCN_CNT_UNINITIALIZED) continue;

    cf->sections[i].data = (uint8_t *)calloc(1, cf->sections[i].raw_size);
    if (!cf->sections[i].data) continue;

    fseek(f, cf->sections[i].raw_offset, SEEK_SET);
    size_t rd = fread(cf->sections[i].data, 1, cf->sections[i].raw_size, f);
    if (rd != cf->sections[i].raw_size) {
      fprintf(stderr, "[coff_reader] %s: section %s: read %zu/%u bytes\n",
              path, cf->sections[i].name, rd, cf->sections[i].raw_size);
    }
  }

  /*======== Step 7: Load per-section relocations (10 bytes each) ========
   * Offset  Size  Field
   * 0       4     VirtualAddress (offset within section data)
   * 4       4     SymbolTableIndex
   * 8       2     Type (REL_AMD64_*)
   *================================================================*/
  for (uint16_t i = 0; i < cf->num_sections; i++) {
    uint32_t nrelocs = cf->sections[i].reloc_count;
    if (nrelocs == 0 || cf->sections[i].reloc_offset == 0) continue;

    fseek(f, cf->sections[i].reloc_offset, SEEK_SET);

    /* Handle extended relocation count */
    if ((cf->sections[i].characteristics & SCN_LNK_NRELOC_OVFL) && nrelocs == 0xFFFF) {
      uint8_t first_reloc[10];
      if (fread(first_reloc, 1, 10, f) == 10) {
        nrelocs = rd32(first_reloc);  /* actual count from first dummy reloc */
        /* First entry is consumed; remaining = nrelocs - 1 */
        if (nrelocs > 0) nrelocs--;
      }
    }

    cf->sections[i].relocs = (coff_reloc_t *)calloc((size_t)nrelocs, sizeof(coff_reloc_t));
    if (!cf->sections[i].relocs) { cf->sections[i].reloc_count = 0; continue; }
    cf->sections[i].reloc_count = nrelocs;

    uint8_t rel_raw[10];
    for (uint32_t r = 0; r < nrelocs; r++) {
      if (fread(rel_raw, 1, 10, f) != 10) break;
      cf->sections[i].relocs[r].virtual_addr  = rd32(rel_raw + 0);
      cf->sections[i].relocs[r].symbol_index  = rd32(rel_raw + 4);
      cf->sections[i].relocs[r].type          = rd16(rel_raw + 8);
    }
  }

  fclose(f);
  return cf;
}

/*--------------------------------------------------------------------------
 * Free all memory associated with a COFF file.
 *------------------------------------------------------------------------*/
void coff_free(coff_file_t *cf) {
  if (!cf) return;
  if (cf->sections) {
    for (uint16_t i = 0; i < cf->num_sections; i++) {
      free(cf->sections[i].data);
      free(cf->sections[i].relocs);
    }
    free(cf->sections);
  }
  free(cf->symbols);
  free(cf->string_table);
  free(cf);
}

/*--------------------------------------------------------------------------
 * Find a symbol by name (linear scan).
 *------------------------------------------------------------------------*/
coff_symbol_t *coff_find_symbol(coff_file_t *cf, const char *name) {
  if (!cf || !cf->symbols || !name) return NULL;
  for (uint32_t i = 0; i < cf->num_symbols; i++) {
    if (cf->symbols[i].name[0] && strcmp(cf->symbols[i].name, name) == 0)
      return &cf->symbols[i];
  }
  return NULL;
}

/*--------------------------------------------------------------------------
 * Find a section by name.
 *------------------------------------------------------------------------*/
coff_section_t *coff_find_section(coff_file_t *cf, const char *name) {
  if (!cf || !cf->sections || !name) return NULL;
  for (uint16_t i = 0; i < cf->num_sections; i++) {
    if (strcmp(cf->sections[i].name, name) == 0)
      return &cf->sections[i];
  }
  return NULL;
}

/*--------------------------------------------------------------------------
 * Human-readable relocation type name.
 *------------------------------------------------------------------------*/
const char *coff_reloc_type_name(uint16_t type) {
  switch (type) {
    case REL_AMD64_ABSOLUTE:  return "ABSOLUTE";
    case REL_AMD64_ADDR64:    return "ADDR64";
    case REL_AMD64_ADDR32:    return "ADDR32";
    case REL_AMD64_ADDR32NB:  return "ADDR32NB";
    case REL_AMD64_REL32:     return "REL32";
    case REL_AMD64_REL32_1:   return "REL32_1";
    case REL_AMD64_REL32_2:   return "REL32_2";
    case REL_AMD64_REL32_3:   return "REL32_3";
    case REL_AMD64_REL32_4:   return "REL32_4";
    case REL_AMD64_REL32_5:   return "REL32_5";
    case REL_AMD64_SECTION:   return "SECTION";
    case REL_AMD64_SECREL:    return "SECREL";
    case REL_AMD64_SECREL7:   return "SECREL7";
    case REL_AMD64_TOKEN:     return "TOKEN";
    case REL_AMD64_SREL32:    return "SREL32";
    case REL_AMD64_PAIR:      return "PAIR";
    case REL_AMD64_SSPAN32:   return "SSPAN32";
    default:                  return "UNKNOWN";
  }
}

/*--------------------------------------------------------------------------
 * Dump COFF file info for diagnostics.
 *------------------------------------------------------------------------*/
void coff_dump(coff_file_t *cf, FILE *out) {
  if (!cf || !out) return;
  fprintf(out, "=== COFF: %s ===\n", cf->source_path);
  fprintf(out, "  Machine: 0x%04X  Sections: %u  Symbols: %u\n",
          cf->machine, cf->num_sections, cf->num_symbols);
  fprintf(out, "  Timestamp: 0x%08X  SymTableOff: 0x%08X\n",
          cf->timestamp, cf->sym_table_offset);

  for (uint16_t i = 0; i < cf->num_sections; i++) {
    coff_section_t *s = &cf->sections[i];
    fprintf(out, "  Section[%u]: %-8s  RawSize=0x%X  RawOff=0x%X  Relocs=%u  Align=%u  Chars=0x%08X",
            i, s->name, s->raw_size, s->raw_offset, s->reloc_count, s->alignment, s->characteristics);
    if (s->characteristics & SCN_CNT_CODE)          fprintf(out, " CODE");
    if (s->characteristics & SCN_CNT_INITIALIZED)   fprintf(out, " IDATA");
    if (s->characteristics & SCN_CNT_UNINITIALIZED) fprintf(out, " UDATA");
    if (s->characteristics & SCN_MEM_EXECUTE)       fprintf(out, " EXEC");
    if (s->characteristics & SCN_MEM_READ)          fprintf(out, " READ");
    if (s->characteristics & SCN_MEM_WRITE)         fprintf(out, " WRITE");
    fprintf(out, "\n");

    /* Dump relocations */
    for (uint32_t r = 0; r < s->reloc_count && s->relocs; r++) {
      coff_reloc_t *rel = &s->relocs[r];
      const char *sym_name = "<unknown>";
      if (cf->symbols && rel->symbol_index < cf->num_symbols)
        sym_name = cf->symbols[rel->symbol_index].name;
      fprintf(out, "    Reloc[%u]: off=0x%04X  sym=%s  type=%s\n",
              r, rel->virtual_addr, sym_name, coff_reloc_type_name(rel->type));
    }
  }

  /* Dump symbols */
  fprintf(out, "  Symbols:\n");
  for (uint32_t i = 0; i < cf->num_symbols && cf->symbols; i++) {
    coff_symbol_t *sym = &cf->symbols[i];
    if (!sym->name[0]) continue; /* skip aux records */
    fprintf(out, "    [%u] %-30s  val=0x%08X  sec=%d  type=0x%04X  class=%u\n",
            i, sym->name, (uint32_t)sym->value, sym->section_num, sym->type, sym->storage_class);
  }
  fprintf(out, "\n");
}
