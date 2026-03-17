/*==========================================================================
 * Phase 2: Section Merger - Fully Reverse Engineered
 *
 * Pipeline:
 *   Step 1: Iterate all objects, collect unique section names
 *   Step 2: For each unique name, create a merged_section_t
 *   Step 3: For each contributing section, align cursor, append data
 *   Step 4: Record contribution map entries
 *   Step 5: Assign RVAs with PE section alignment (0x1000 default)
 *   Step 6: Build global symbol table with resolved RVAs
 *   Step 7: Merge characteristics (OR flags, take max alignment)
 *
 * Section ordering follows PE convention:
 *   .text -> .rdata -> .data -> .pdata -> .xdata -> .bss -> others
 *=========================================================================*/
#include "section_merge.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_SECTION_ALIGN 0x1000
#define DEFAULT_FILE_ALIGN    0x200
#define INITIAL_DATA_CAPACITY 4096
#define INITIAL_CONTRIB_CAPACITY 16

/* Section ordering priority (lower = earlier in image) */
static int section_priority(const char *name) {
  if (strcmp(name, ".text")   == 0) return 0;
  if (strcmp(name, ".rdata")  == 0) return 1;
  if (strcmp(name, ".data")   == 0) return 2;
  if (strcmp(name, ".pdata")  == 0) return 3;
  if (strcmp(name, ".xdata")  == 0) return 4;
  if (strcmp(name, ".bss")    == 0) return 5;
  if (strcmp(name, ".rsrc")   == 0) return 6;
  if (strcmp(name, ".reloc")  == 0) return 7;
  return 100; /* unknown sections go last */
}

/* Round up to alignment boundary */
static uint32_t align_up(uint32_t val, uint32_t align) {
  if (align == 0) align = 1;
  return (val + align - 1) & ~(align - 1);
}

/* ---- Find or create a merged section by name ---- */
static merged_section_t *find_or_create(merged_section_t **head, const char *name) {
  /* Search existing */
  for (merged_section_t *m = *head; m; m = m->next) {
    if (strcmp(m->name, name) == 0) return m;
  }
  /* Create new */
  merged_section_t *m = (merged_section_t *)calloc(1, sizeof(merged_section_t));
  if (!m) return NULL;
  strncpy(m->name, name, 8);
  m->name[8] = '\0';
  m->alignment = 1;
  m->data = (uint8_t *)calloc(1, INITIAL_DATA_CAPACITY);
  m->data_capacity = m->data ? INITIAL_DATA_CAPACITY : 0;
  m->contributions = (section_contribution_t *)calloc(INITIAL_CONTRIB_CAPACITY,
                                                       sizeof(section_contribution_t));
  m->contributions_capacity = m->contributions ? INITIAL_CONTRIB_CAPACITY : 0;

  /* Insert in priority order */
  int prio = section_priority(name);
  merged_section_t **pp = head;
  while (*pp && section_priority((*pp)->name) <= prio) {
    pp = &(*pp)->next;
  }
  m->next = *pp;
  *pp = m;
  return m;
}

/* ---- Ensure data buffer has capacity ---- */
static int ensure_capacity(merged_section_t *m, size_t needed) {
  if (m->data_len + needed <= m->data_capacity) return 0;
  size_t new_cap = m->data_capacity * 2;
  while (new_cap < m->data_len + needed) new_cap *= 2;
  uint8_t *new_data = (uint8_t *)realloc(m->data, new_cap);
  if (!new_data) return -1;
  memset(new_data + m->data_capacity, 0, new_cap - m->data_capacity);
  m->data = new_data;
  m->data_capacity = new_cap;
  return 0;
}

/* ---- Add a contribution record ---- */
static void add_contribution(merged_section_t *m, int obj_idx, uint16_t sec_idx,
                              uint32_t src_off, uint32_t dst_off, uint32_t size, uint32_t rva) {
  if (m->num_contributions >= m->contributions_capacity) {
    int new_cap = m->contributions_capacity * 2;
    if (new_cap < 16) new_cap = 16;
    section_contribution_t *new_c = (section_contribution_t *)realloc(
        m->contributions, (size_t)new_cap * sizeof(section_contribution_t));
    if (!new_c) return;
    m->contributions = new_c;
    m->contributions_capacity = new_cap;
  }
  section_contribution_t *c = &m->contributions[m->num_contributions++];
  c->obj_index  = obj_idx;
  c->sec_index  = sec_idx;
  c->src_offset = src_off;
  c->dst_offset = dst_off;
  c->size       = size;
  c->rva        = rva;
}

/*--------------------------------------------------------------------------
 * Main merge function.
 *
 * Algorithm:
 *   For each object file (in order):
 *     For each section in that object:
 *       1. Skip debug/metadata sections (.debug$S, .debug$T, .drectve)
 *       2. Find or create merged section with same name
 *       3. Align current write cursor to section's required alignment
 *       4. Pad with zeros to alignment boundary
 *       5. Copy raw data (or just account for virtual size if BSS)
 *       6. Record contribution entry
 *       7. Merge characteristics flags (OR together)
 *       8. Track maximum alignment
 *
 *   After all data assembled:
 *     Assign RVAs starting at 0x1000 (after PE headers)
 *     Each section gets section_alignment (0x1000) boundary
 *------------------------------------------------------------------------*/
merged_section_t *section_merge_all(coff_file_t **files, int num_files) {
  if (!files || num_files <= 0) return NULL;

  merged_section_t *head = NULL;

  /*==== Pass 1: Collect and concatenate section data ====*/
  for (int obj = 0; obj < num_files; obj++) {
    coff_file_t *cf = files[obj];
    if (!cf) continue;

    for (uint16_t si = 0; si < cf->num_sections; si++) {
      coff_section_t *sec = &cf->sections[si];

      /* Skip linker directive sections */
      if (strcmp(sec->name, ".drectve") == 0) continue;
      /* Skip debug sections (but could be kept for debug builds) */
      if (strncmp(sec->name, ".debug$", 7) == 0) continue;

      merged_section_t *m = find_or_create(&head, sec->name);
      if (!m) continue;

      /* Align write cursor */
      uint32_t sec_align = sec->alignment;
      if (sec_align < 1) sec_align = 1;
      if (sec_align > m->alignment) m->alignment = sec_align;

      uint32_t aligned_offset = align_up((uint32_t)m->data_len, sec_align);
      uint32_t padding = aligned_offset - (uint32_t)m->data_len;

      /* Handle BSS (uninitialized data) */
      if (sec->characteristics & SCN_CNT_UNINITIALIZED) {
        uint32_t bss_size = sec->virtual_size > 0 ? sec->virtual_size : sec->raw_size;
        uint32_t bss_offset = aligned_offset;
        m->virtual_size = align_up(aligned_offset + bss_size, sec_align);

        add_contribution(m, obj, si, 0, bss_offset, bss_size, 0);
        /* Don't actually write data for BSS */
        if (m->data_len < aligned_offset) m->data_len = aligned_offset;
        continue;
      }

      /* Ensure we have space for padding + data */
      uint32_t raw = sec->raw_size;
      if (ensure_capacity(m, padding + raw) != 0) continue;

      /* Write padding zeros */
      if (padding > 0) {
        memset(m->data + m->data_len, 0, padding);
        m->data_len += padding;
      }

      /* Copy raw data */
      if (sec->data && raw > 0) {
        memcpy(m->data + m->data_len, sec->data, raw);
      }

      add_contribution(m, obj, si, 0, (uint32_t)m->data_len, raw, 0);
      m->data_len += raw;

      /* Merge characteristics */
      m->characteristics |= sec->characteristics;
    }
  }

  /*==== Pass 2: Assign RVAs ====*/
  uint32_t rva = DEFAULT_SECTION_ALIGN; /* start after PE headers */
  for (merged_section_t *m = head; m; m = m->next) {
    rva = align_up(rva, DEFAULT_SECTION_ALIGN);
    m->rva = rva;

    /* Update contribution RVAs */
    for (int c = 0; c < m->num_contributions; c++) {
      m->contributions[c].rva = m->rva + m->contributions[c].dst_offset;
    }

    /* Virtual size = max of data_len and virtual_size (for BSS) */
    if (m->virtual_size < (uint32_t)m->data_len)
      m->virtual_size = (uint32_t)m->data_len;

    rva += align_up(m->virtual_size, DEFAULT_SECTION_ALIGN);
  }

  /*==== Pass 3: Strip alignment bits from merged characteristics ====*/
  for (merged_section_t *m = head; m; m = m->next) {
    m->characteristics &= ~SCN_ALIGN_MASK; /* alignment is separate in PE headers */
  }

  return head;
}

/*--------------------------------------------------------------------------
 * Full merge context (used by reloc resolver and PE writer).
 * Includes global symbol table with resolved RVAs.
 *------------------------------------------------------------------------*/
merge_context_t *section_merge_context(coff_file_t **files, int num_files) {
  merge_context_t *ctx = (merge_context_t *)calloc(1, sizeof(merge_context_t));
  if (!ctx) return NULL;

  ctx->section_alignment = DEFAULT_SECTION_ALIGN;
  ctx->file_alignment    = DEFAULT_FILE_ALIGN;

  /* Merge sections */
  ctx->head = section_merge_all(files, num_files);
  if (!ctx->head) { free(ctx); return NULL; }

  /* Count sections */
  for (merged_section_t *m = ctx->head; m; m = m->next) ctx->num_sections++;

  /* Determine next_rva from last section */
  for (merged_section_t *m = ctx->head; m; m = m->next) {
    uint32_t end = m->rva + align_up(m->virtual_size, DEFAULT_SECTION_ALIGN);
    if (end > ctx->next_rva) ctx->next_rva = end;
  }

  /*==== Build global symbol table ====*/
  uint32_t total_syms = 0;
  for (int i = 0; i < num_files; i++) {
    if (files[i]) total_syms += files[i]->num_symbols;
  }

  ctx->global_symbols = (coff_symbol_t *)calloc(total_syms + 1, sizeof(coff_symbol_t));
  ctx->global_symbols_capacity = total_syms + 1;

  for (int obj = 0; obj < num_files; obj++) {
    coff_file_t *cf = files[obj];
    if (!cf || !cf->symbols) continue;

    for (uint32_t si = 0; si < cf->num_symbols; si++) {
      coff_symbol_t *sym = &cf->symbols[si];
      if (!sym->name[0]) continue;        /* skip aux records */
      if (sym->storage_class == 0xFF) continue; /* aux sentinel */

      /* Copy symbol */
      coff_symbol_t *gsym = &ctx->global_symbols[ctx->num_global_symbols];
      *gsym = *sym;
      gsym->obj_index = obj;

      /* Resolve RVA: find the contribution that covers this symbol's section + offset */
      if (sym->section_num > 0 && sym->section_num <= cf->num_sections) {
        uint16_t sec_idx = (uint16_t)(sym->section_num - 1);
        coff_section_t *sec = &cf->sections[sec_idx];

        /* Find the merged section and contribution for this obj+section */
        for (merged_section_t *m = ctx->head; m; m = m->next) {
          if (strcmp(m->name, sec->name) != 0) continue;
          for (int c = 0; c < m->num_contributions; c++) {
            if (m->contributions[c].obj_index == obj &&
                m->contributions[c].sec_index == sec_idx) {
              gsym->resolved_rva = m->contributions[c].rva + (uint32_t)sym->value;
              break;
            }
          }
          break;
        }
      } else if (sym->section_num == -1) {
        /* Absolute symbol */
        gsym->resolved_rva = (uint32_t)sym->value;
      }

      ctx->num_global_symbols++;
    }
  }

  return ctx;
}

/*--------------------------------------------------------------------------
 * Free merged sections (linked list).
 *------------------------------------------------------------------------*/
void merged_section_free(merged_section_t *ms) {
  while (ms) {
    merged_section_t *n = ms->next;
    free(ms->data);
    free(ms->contributions);
    free(ms);
    ms = n;
  }
}

/*--------------------------------------------------------------------------
 * Free full merge context.
 *------------------------------------------------------------------------*/
void merge_context_free(merge_context_t *ctx) {
  if (!ctx) return;
  merged_section_free(ctx->head);
  free(ctx->global_symbols);
  free(ctx);
}

/*--------------------------------------------------------------------------
 * Query: get merged data for a named section.
 *------------------------------------------------------------------------*/
uint8_t *merged_section_data(merged_section_t *ms, const char *name, size_t *out_len) {
  for (merged_section_t *m = ms; m; m = m->next) {
    if (strcmp(m->name, name) == 0) {
      if (out_len) *out_len = m->data_len;
      return m->data;
    }
  }
  if (out_len) *out_len = 0;
  return NULL;
}

/*--------------------------------------------------------------------------
 * Query: get RVA for a named section.
 *------------------------------------------------------------------------*/
uint32_t merged_section_rva(merged_section_t *ms, const char *name) {
  for (merged_section_t *m = ms; m; m = m->next) {
    if (strcmp(m->name, name) == 0) return m->rva;
  }
  return 0;
}

/*--------------------------------------------------------------------------
 * Query: find merged section by name.
 *------------------------------------------------------------------------*/
merged_section_t *merged_section_find(merged_section_t *ms, const char *name) {
  for (merged_section_t *m = ms; m; m = m->next) {
    if (strcmp(m->name, name) == 0) return m;
  }
  return NULL;
}

/*--------------------------------------------------------------------------
 * Count merged sections.
 *------------------------------------------------------------------------*/
int merged_section_count(merged_section_t *ms) {
  int count = 0;
  for (merged_section_t *m = ms; m; m = m->next) count++;
  return count;
}

/*--------------------------------------------------------------------------
 * Dump merged section map for diagnostics.
 *------------------------------------------------------------------------*/
void merged_section_dump(merged_section_t *ms, FILE *out) {
  if (!out) return;
  fprintf(out, "=== Merged Section Map ===\n");
  int idx = 0;
  for (merged_section_t *m = ms; m; m = m->next, idx++) {
    fprintf(out, "  [%d] %-8s  RVA=0x%08X  VirtSize=0x%X  RawSize=0x%zX  Align=%u  Chars=0x%08X\n",
            idx, m->name, m->rva, m->virtual_size, m->data_len, m->alignment, m->characteristics);
    fprintf(out, "       Contributions: %d\n", m->num_contributions);
    for (int c = 0; c < m->num_contributions; c++) {
      section_contribution_t *cont = &m->contributions[c];
      fprintf(out, "         obj[%d] sec[%u]: src=0x%X -> dst=0x%X  size=0x%X  rva=0x%08X\n",
              cont->obj_index, cont->sec_index, cont->src_offset,
              cont->dst_offset, cont->size, cont->rva);
    }
  }
  fprintf(out, "\n");
}
