/*==========================================================================
 * Phase 3: Relocation Resolver - Fully Reverse Engineered
 *
 * Applies COFF relocations to produce a position-dependent PE image.
 * This is the most critical phase — incorrect relocations = crash.
 *
 * Relocation types handled (IMAGE_REL_AMD64_*):
 *
 *   ADDR64   (0x01): 64-bit absolute address
 *     patch = symbol_rva + image_base + addend
 *     Writes 8 bytes at reloc offset
 *
 *   ADDR32   (0x02): 32-bit absolute address (rare in x64)
 *     patch = symbol_rva + image_base + addend
 *     Writes 4 bytes; error if > 32 bits
 *
 *   ADDR32NB (0x03): 32-bit RVA (no base, used for exception tables/.pdata)
 *     patch = symbol_rva + addend
 *     Writes 4 bytes
 *
 *   REL32    (0x04): 32-bit PC-relative (most common: call, jmp, lea)
 *     patch = (symbol_rva - (reloc_rva + 4)) + addend
 *     The +4 accounts for the 4 bytes of the relocation field itself
 *     Writes 4 bytes as signed 32-bit
 *
 *   REL32_1..5 (0x05-0x09): Same as REL32 but adds 1..5 to displacement
 *     patch = (symbol_rva - (reloc_rva + 4 + N))
 *     Used when reloc field isn't at end of instruction
 *
 *   SECTION  (0x0A): 16-bit section index of the symbol
 *     Writes 2 bytes
 *
 *   SECREL   (0x0B): 32-bit offset from start of symbol's section
 *     patch = symbol.value (offset within section)
 *     Writes 4 bytes
 *
 *   SECREL7  (0x0C): 7-bit section-relative offset (rare)
 *
 * Pipeline:
 *   1. For each merged section, iterate its contributions
 *   2. For each contribution, find the original COFF file and section
 *   3. For each relocation in that COFF section, resolve the target symbol
 *   4. Calculate the patch value based on relocation type
 *   5. Write the patch into the merged section's data buffer
 *=========================================================================*/
#include "coff_reader.h"
#include "section_merge.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* ---- Write helpers (little-endian) ---- */
static void wr16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void wr32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void wr64(uint8_t *p, uint64_t v) { wr32(p,(uint32_t)v); wr32(p+4,(uint32_t)(v>>32)); }
static int32_t rd32s(const uint8_t *p) { return (int32_t)(p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24)); }
static int64_t rd64s(const uint8_t *p) { return (int64_t)((uint64_t)rd32s(p) | ((uint64_t)rd32s(p+4) << 32)); }

/*--------------------------------------------------------------------------
 * Resolve a single symbol's RVA.
 * Searches the merge context's global symbol table.
 * Returns 0 and sets *found=0 if unresolved.
 *------------------------------------------------------------------------*/
static uint32_t resolve_symbol_rva(merge_context_t *ctx, coff_file_t *cf,
                                    uint32_t sym_index, int *found) {
  *found = 0;
  if (!cf->symbols || sym_index >= cf->num_symbols) return 0;

  coff_symbol_t *sym = &cf->symbols[sym_index];
  if (!sym->name[0]) return 0;

  /* Search global symbol table for matching name */
  for (uint32_t g = 0; g < ctx->num_global_symbols; g++) {
    if (strcmp(ctx->global_symbols[g].name, sym->name) == 0) {
      *found = 1;
      return ctx->global_symbols[g].resolved_rva;
    }
  }

  /* If the symbol is defined locally (non-external), use its section + value */
  if (sym->section_num > 0 && sym->section_num <= cf->num_sections) {
    uint16_t sec_idx = (uint16_t)(sym->section_num - 1);
    coff_section_t *sec = &cf->sections[sec_idx];

    /* Find the merged section and contribution */
    for (merged_section_t *m = ctx->head; m; m = m->next) {
      if (strcmp(m->name, sec->name) != 0) continue;
      for (int c = 0; c < m->num_contributions; c++) {
        if (m->contributions[c].obj_index == sym->obj_index &&
            m->contributions[c].sec_index == sec_idx) {
          *found = 1;
          return m->contributions[c].rva + (uint32_t)sym->value;
        }
      }
    }
  }

  return 0;
}

/*--------------------------------------------------------------------------
 * Find the merged section index (1-based) for a symbol's section.
 *------------------------------------------------------------------------*/
static uint16_t resolve_symbol_section_index(merge_context_t *ctx, coff_file_t *cf,
                                              uint32_t sym_index) {
  if (!cf->symbols || sym_index >= cf->num_symbols) return 0;
  coff_symbol_t *sym = &cf->symbols[sym_index];
  if (sym->section_num <= 0 || sym->section_num > cf->num_sections) return 0;

  coff_section_t *sec = &cf->sections[sym->section_num - 1];
  uint16_t idx = 1;
  for (merged_section_t *m = ctx->head; m; m = m->next, idx++) {
    if (strcmp(m->name, sec->name) == 0) return idx;
  }
  return 0;
}

/*--------------------------------------------------------------------------
 * Apply all relocations across all merged sections.
 *
 * This is the main relocation pass. For each contribution in each merged
 * section, we look up the original COFF section's relocations and apply
 * them to the merged data buffer.
 *------------------------------------------------------------------------*/
int reloc_resolve_all(merge_context_t *ctx, coff_file_t **files, int num_files,
                      uint64_t image_base) {
  int errors = 0;
  int applied = 0;

  for (merged_section_t *m = ctx->head; m; m = m->next) {
    if (!m->data) continue;

    for (int ci = 0; ci < m->num_contributions; ci++) {
      section_contribution_t *contrib = &m->contributions[ci];
      int obj_idx = contrib->obj_index;
      if (obj_idx < 0 || obj_idx >= num_files) continue;

      coff_file_t *cf = files[obj_idx];
      if (!cf || contrib->sec_index >= cf->num_sections) continue;

      coff_section_t *sec = &cf->sections[contrib->sec_index];
      if (!sec->relocs || sec->reloc_count == 0) continue;

      for (uint32_t ri = 0; ri < sec->reloc_count; ri++) {
        coff_reloc_t *rel = &sec->relocs[ri];

        /* Calculate offset into merged data buffer */
        uint32_t patch_offset = contrib->dst_offset + rel->virtual_addr;
        if (patch_offset + 4 > (uint32_t)m->data_len &&
            rel->type != REL_AMD64_ADDR64) {
          fprintf(stderr, "[reloc] %-8s: reloc at 0x%X overflows section (len=0x%zX)\n",
                  m->name, patch_offset, m->data_len);
          errors++;
          continue;
        }

        /* RVA of the relocation site in final image */
        uint32_t reloc_rva = m->rva + patch_offset;

        /* Resolve target symbol */
        int found = 0;
        uint32_t sym_rva = resolve_symbol_rva(ctx, cf, rel->symbol_index, &found);
        if (!found && rel->type != REL_AMD64_ABSOLUTE) {
          const char *sym_name = "<unknown>";
          if (cf->symbols && rel->symbol_index < cf->num_symbols)
            sym_name = cf->symbols[rel->symbol_index].name;
          fprintf(stderr, "[reloc] %-8s: unresolved symbol '%s' at offset 0x%X\n",
                  m->name, sym_name, patch_offset);
          errors++;
          continue;
        }

        uint8_t *patch = m->data + patch_offset;

        /* ---- Apply relocation based on type ---- */
        switch (rel->type) {

          case REL_AMD64_ABSOLUTE:
            /* No-op (padding/alignment) */
            break;

          case REL_AMD64_ADDR64: {
            /* 64-bit absolute virtual address */
            if (patch_offset + 8 > (uint32_t)m->data_len) {
              errors++; break;
            }
            int64_t addend = rd64s(patch);
            uint64_t result = (uint64_t)sym_rva + image_base + addend;
            wr64(patch, result);
            applied++;
            break;
          }

          case REL_AMD64_ADDR32: {
            /* 32-bit absolute virtual address */
            int32_t addend = rd32s(patch);
            uint64_t result = (uint64_t)sym_rva + image_base + addend;
            if (result > 0xFFFFFFFF) {
              fprintf(stderr, "[reloc] %-8s: ADDR32 overflow at 0x%X (result=0x%llX)\n",
                      m->name, patch_offset, (unsigned long long)result);
              errors++;
            }
            wr32(patch, (uint32_t)result);
            applied++;
            break;
          }

          case REL_AMD64_ADDR32NB: {
            /* 32-bit RVA (relative to image base, no base added) */
            int32_t addend = rd32s(patch);
            uint32_t result = sym_rva + addend;
            wr32(patch, result);
            applied++;
            break;
          }

          case REL_AMD64_REL32:
          case REL_AMD64_REL32_1:
          case REL_AMD64_REL32_2:
          case REL_AMD64_REL32_3:
          case REL_AMD64_REL32_4:
          case REL_AMD64_REL32_5: {
            /* 32-bit PC-relative */
            int extra = 0;
            if (rel->type >= REL_AMD64_REL32_1 && rel->type <= REL_AMD64_REL32_5)
              extra = rel->type - REL_AMD64_REL32;

            int32_t addend = rd32s(patch);
            /* PC-relative: target - (site + 4 + extra) */
            int32_t result = (int32_t)sym_rva - (int32_t)(reloc_rva + 4 + extra) + addend;
            wr32(patch, (uint32_t)result);
            applied++;
            break;
          }

          case REL_AMD64_SECTION: {
            /* 16-bit section index */
            uint16_t sec_idx = resolve_symbol_section_index(ctx, cf, rel->symbol_index);
            wr16(patch, sec_idx);
            applied++;
            break;
          }

          case REL_AMD64_SECREL: {
            /* 32-bit section-relative offset */
            /* Value is symbol's offset within its section */
            int32_t addend = rd32s(patch);
            int32_t sym_value = 0;
            if (cf->symbols && rel->symbol_index < cf->num_symbols)
              sym_value = cf->symbols[rel->symbol_index].value;
            wr32(patch, (uint32_t)(sym_value + addend));
            applied++;
            break;
          }

          case REL_AMD64_SECREL7: {
            /* 7-bit section-relative (masked) */
            int32_t sym_value = 0;
            if (cf->symbols && rel->symbol_index < cf->num_symbols)
              sym_value = cf->symbols[rel->symbol_index].value;
            patch[0] = (patch[0] & 0x80) | (uint8_t)(sym_value & 0x7F);
            applied++;
            break;
          }

          default:
            fprintf(stderr, "[reloc] %-8s: unsupported reloc type 0x%04X at 0x%X\n",
                    m->name, rel->type, patch_offset);
            errors++;
            break;
        }
      }
    }
  }

  fprintf(stderr, "[reloc] Applied %d relocations, %d errors\n", applied, errors);
  return errors;
}

/*--------------------------------------------------------------------------
 * Legacy API compatibility.
 *------------------------------------------------------------------------*/
void reloc_resolve(coff_file_t *cf, uint32_t image_base) {
  (void)cf;
  (void)image_base;
  /* Use reloc_resolve_all() with merge_context instead */
}

void reloc_apply(coff_file_t **files, int num_files, uint8_t *image, uint32_t image_base) {
  (void)files;
  (void)num_files;
  (void)image;
  (void)image_base;
  /* Use reloc_resolve_all() with merge_context instead */
}
