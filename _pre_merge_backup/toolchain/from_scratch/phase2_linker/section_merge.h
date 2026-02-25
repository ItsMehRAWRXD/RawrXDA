#ifndef SECTION_MERGE_H
#define SECTION_MERGE_H

/*==========================================================================
 * Phase 2: Section Merger - Fully Reverse Engineered
 *
 * Merges same-named sections from multiple COFF objects into a flat image.
 * This is the core layout engine of the linker:
 *
 *   1. Collect all sections across all input .obj files
 *   2. Group by canonical name (.text, .data, .rdata, .bss, .pdata, .xdata)
 *   3. Within each group, respect per-section alignment requirements
 *   4. Assign monotonically increasing RVAs starting at 0x1000
 *   5. Concatenate raw data with alignment padding
 *   6. Build a contribution map (which bytes came from which obj+section)
 *   7. Track total virtual size including BSS
 *
 * The merged output feeds directly into the PE writer (Phase 4).
 *=========================================================================*/

#include "coff_reader.h"
#include <stdio.h>

/* ---- Contribution record: tracks where each piece of merged data came from ---- */
typedef struct section_contribution {
  int      obj_index;         /* which object file (0-based) */
  uint16_t sec_index;         /* section index within that object */
  uint32_t src_offset;        /* offset within original section's raw data */
  uint32_t dst_offset;        /* offset within merged section's data buffer */
  uint32_t size;              /* size of this contribution */
  uint32_t rva;               /* absolute RVA in final image */
} section_contribution_t;

/* ---- A single merged output section ---- */
typedef struct merged_section {
  char     name[9];           /* canonical section name */
  uint8_t *data;              /* concatenated raw data (malloc'd) */
  size_t   data_capacity;     /* allocated capacity */
  size_t   data_len;          /* used bytes of raw data */
  uint32_t virtual_size;      /* total virtual size (may exceed data_len for BSS) */
  uint32_t rva;               /* base RVA assigned during layout */
  uint32_t characteristics;   /* merged characteristics (OR of all contributing sections) */
  uint32_t alignment;         /* maximum alignment required */

  /* Contribution tracking */
  section_contribution_t *contributions;
  int num_contributions;
  int contributions_capacity;

  struct merged_section *next; /* linked list */
} merged_section_t;

/* ---- Merge context: holds the full merge result ---- */
typedef struct merge_context {
  merged_section_t *head;     /* linked list of merged sections */
  int num_sections;           /* count of merged sections */
  uint32_t next_rva;          /* next available RVA for layout */
  uint32_t section_alignment; /* PE section alignment (default 0x1000) */
  uint32_t file_alignment;    /* PE file alignment (default 0x200) */

  /* Global symbol table (collected from all objects) */
  coff_symbol_t *global_symbols;
  uint32_t num_global_symbols;
  uint32_t global_symbols_capacity;
} merge_context_t;

/* ---- API ---- */

/* Main merge function: takes array of COFF files, returns linked list */
merged_section_t *section_merge_all(coff_file_t **files, int num_files);

/* Free all merged sections */
void merged_section_free(merged_section_t *ms);

/* Query helpers */
uint8_t *merged_section_data(merged_section_t *ms, const char *name, size_t *out_len);
uint32_t merged_section_rva(merged_section_t *ms, const char *name);
merged_section_t *merged_section_find(merged_section_t *ms, const char *name);
int merged_section_count(merged_section_t *ms);

/* Full merge context (for advanced use by PE writer and reloc resolver) */
merge_context_t *section_merge_context(coff_file_t **files, int num_files);
void merge_context_free(merge_context_t *ctx);

/* Dump merge map for diagnostics */
void merged_section_dump(merged_section_t *ms, FILE *out);

#endif
