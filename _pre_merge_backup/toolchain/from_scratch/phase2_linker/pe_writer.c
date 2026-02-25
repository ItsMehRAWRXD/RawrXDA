/*==========================================================================
 * Phase 4: PE Executable Writer - Fully Reverse Engineered
 *
 * Builds a complete PE32+ executable from merged sections.
 *
 * File layout produced:
 *   [DOS Header 64B][DOS Stub 64B][PE Sig 4B][COFF Hdr 20B]
 *   [Optional Header 240B][Section Headers 40B * N]
 *   [Padding to FileAlign]
 *   [Section 1 raw data, padded to FileAlign]
 *   [Section 2 raw data, padded to FileAlign]
 *   ...
 *
 * Key constants:
 *   FileAlignment    = 0x200  (512 bytes)
 *   SectionAlignment = 0x1000 (4096 bytes)
 *   ImageBase        = 0x0000000140000000 (default x64)
 *   PE Signature offset (e_lfanew) = 0xC0
 *=========================================================================*/
#include "pe_writer.h"
#include "section_merge.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define PE_FILE_ALIGNMENT    0x200
#define PE_SECTION_ALIGNMENT 0x1000
#define PE_DEFAULT_IMAGE_BASE 0x0000000140000000ULL
#define PE_MAX_SECTIONS      96
#define PE_LFANEW_OFFSET     0xC0
#define PE_OPTIONAL_HDR_SIZE 0xF0   /* PE32+ optional header = 240 bytes */
#define PE_NUM_DATA_DIRS     16

/* ---- Internal section record ---- */
typedef struct {
  char     name[9];
  uint8_t *data;
  size_t   data_len;       /* actual data bytes */
  uint32_t virtual_size;   /* virtual size (>= data_len for BSS) */
  uint32_t rva;            /* virtual address relative to image base */
  uint32_t raw_offset;     /* file offset to raw data */
  uint32_t raw_size;       /* file-aligned raw data size */
  uint32_t characteristics;
  int      owns_data;      /* 1 if we malloc'd data (need to free) */
} pe_section_t;

/* ---- PE builder state ---- */
struct pe_builder {
  uint16_t machine;
  uint16_t subsystem;
  uint32_t entry_rva;
  uint64_t image_base;
  uint64_t stack_reserve;
  uint64_t stack_commit;
  uint64_t heap_reserve;
  uint64_t heap_commit;

  pe_section_t sections[PE_MAX_SECTIONS];
  int num_sections;

  uint32_t size_of_headers;  /* file-aligned headers size */
  uint32_t size_of_image;    /* section-aligned total image size */
  uint32_t timestamp;
};

/* ---- Alignment helpers ---- */
static uint32_t falign(uint32_t v) { return (v + PE_FILE_ALIGNMENT - 1) & ~(PE_FILE_ALIGNMENT - 1); }
static uint32_t salign(uint32_t v) { return (v + PE_SECTION_ALIGNMENT - 1) & ~(PE_SECTION_ALIGNMENT - 1); }

/* ---- Little-endian write helpers ---- */
static void w16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void w32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void w64(uint8_t *p, uint64_t v) { w32(p,(uint32_t)v); w32(p+4,(uint32_t)(v>>32)); }

/*--------------------------------------------------------------------------
 * Create a new PE builder.
 *------------------------------------------------------------------------*/
pe_builder_t *pe_builder_new(uint16_t machine) {
  pe_builder_t *pb = (pe_builder_t *)calloc(1, sizeof(pe_builder_t));
  if (!pb) return NULL;
  pb->machine       = machine;
  pb->subsystem     = PE_SUBSYS_CONSOLE;
  pb->image_base    = PE_DEFAULT_IMAGE_BASE;
  pb->stack_reserve = 0x100000;   /* 1 MB */
  pb->stack_commit  = 0x1000;     /* 4 KB */
  pb->heap_reserve  = 0x100000;
  pb->heap_commit   = 0x1000;
  pb->entry_rva     = 0;
  pb->timestamp     = (uint32_t)time(NULL);
  return pb;
}

void pe_builder_free(pe_builder_t *pb) {
  if (!pb) return;
  for (int i = 0; i < pb->num_sections; i++) {
    if (pb->sections[i].owns_data)
      free(pb->sections[i].data);
  }
  free(pb);
}

void pe_builder_set_entry(pe_builder_t *pb, uint32_t rva)         { if (pb) pb->entry_rva = rva; }
void pe_builder_set_image_base(pe_builder_t *pb, uint64_t base)   { if (pb) pb->image_base = base; }
void pe_builder_set_subsystem(pe_builder_t *pb, uint16_t subsys)  { if (pb) pb->subsystem = subsys; }
void pe_builder_set_stack(pe_builder_t *pb, uint64_t r, uint64_t c) { if (pb) { pb->stack_reserve=r; pb->stack_commit=c; } }
void pe_builder_set_heap(pe_builder_t *pb, uint64_t r, uint64_t c)  { if (pb) { pb->heap_reserve=r; pb->heap_commit=c; } }

/*--------------------------------------------------------------------------
 * Add a section manually.
 *------------------------------------------------------------------------*/
void pe_builder_add_section(pe_builder_t *pb, const char *name,
                            const uint8_t *data, size_t len, uint32_t characteristics) {
  if (!pb || pb->num_sections >= PE_MAX_SECTIONS) return;
  pe_section_t *s = &pb->sections[pb->num_sections];
  size_t nlen = strlen(name);
  if (nlen > 8) nlen = 8;
  memcpy(s->name, name, nlen);
  s->name[nlen] = '\0';
  if (data && len > 0) {
    s->data = (uint8_t *)malloc(len);
    if (s->data) memcpy(s->data, data, len);
    s->data_len = len;
    s->owns_data = 1;
  }
  s->virtual_size = (uint32_t)len;
  s->characteristics = characteristics;
  pb->num_sections++;
}

/*--------------------------------------------------------------------------
 * Import sections from merge context.
 *------------------------------------------------------------------------*/
int pe_builder_from_merge(pe_builder_t *pb, merge_context_t *ctx) {
  if (!pb || !ctx || !ctx->head) return -1;

  for (merged_section_t *m = ctx->head; m; m = m->next) {
    if (pb->num_sections >= PE_MAX_SECTIONS) break;
    pe_section_t *s = &pb->sections[pb->num_sections];
    strncpy(s->name, m->name, 8);
    s->name[8] = '\0';
    s->data = m->data;
    s->data_len = m->data_len;
    s->virtual_size = m->virtual_size;
    s->rva = m->rva;
    s->characteristics = m->characteristics;
    s->owns_data = 0; /* merge context owns the data */
    pb->num_sections++;
  }
  return 0;
}

/*--------------------------------------------------------------------------
 * Calculate layout and write the PE file.
 *
 * Layout calculation:
 *   1. Headers size = DOS(64) + stub(64) + PEsig(4) + COFF(20) +
 *                     OptHdr(240) + SecHdrs(40*N)
 *   2. Round headers to FileAlignment
 *   3. For each section:
 *      - raw_offset = current file position (file-aligned)
 *      - raw_size = file-aligned data length
 *      - rva = section-aligned virtual address
 *      - virtual_size = max(data_len, virtual_size)
 *   4. size_of_image = section-aligned end of last section
 *------------------------------------------------------------------------*/
int pe_builder_write(pe_builder_t *pb, const char *out_path) {
  if (!pb || !out_path) return -1;

  /*==== Step 1: Calculate header sizes ====*/
  uint32_t dos_hdr_size    = 64;
  uint32_t dos_stub_size   = PE_LFANEW_OFFSET - dos_hdr_size;
  uint32_t pe_sig_size     = 4;
  uint32_t coff_hdr_size   = 20;
  uint32_t opt_hdr_size    = PE_OPTIONAL_HDR_SIZE;
  uint32_t sec_hdrs_size   = (uint32_t)pb->num_sections * 40;
  uint32_t raw_headers     = PE_LFANEW_OFFSET + pe_sig_size + coff_hdr_size +
                             opt_hdr_size + sec_hdrs_size;
  pb->size_of_headers = falign(raw_headers);

  /*==== Step 2: Calculate section layout ====*/
  uint32_t file_offset = pb->size_of_headers;
  uint32_t virt_offset = PE_SECTION_ALIGNMENT; /* first section at 0x1000 */

  for (int i = 0; i < pb->num_sections; i++) {
    pe_section_t *s = &pb->sections[i];

    /* Virtual layout */
    if (s->rva == 0) s->rva = virt_offset;
    if (s->virtual_size < (uint32_t)s->data_len)
      s->virtual_size = (uint32_t)s->data_len;

    /* File layout */
    s->raw_offset = file_offset;
    s->raw_size = (s->data_len > 0) ? falign((uint32_t)s->data_len) : 0;

    file_offset += s->raw_size;
    virt_offset = salign(s->rva + s->virtual_size);
  }

  pb->size_of_image = virt_offset;

  /*==== Step 3: Build the file in memory ====*/
  uint32_t total_file_size = file_offset;
  uint8_t *image = (uint8_t *)calloc(1, total_file_size);
  if (!image) return -1;

  /*---- DOS Header (64 bytes) ----
   * Only critical fields: e_magic (MZ) and e_lfanew (offset to PE sig)
   */
  image[0] = 0x4D; image[1] = 0x5A; /* "MZ" */
  w32(image + 0x3C, PE_LFANEW_OFFSET); /* e_lfanew */

  /*---- DOS Stub (PE_LFANEW_OFFSET - 64 bytes) ----
   * Minimal stub: "This program cannot be run in DOS mode.\r\n$"
   */
  static const char dos_msg[] = "This program cannot be run in DOS mode.\r\n$";
  if (dos_stub_size > sizeof(dos_msg))
    memcpy(image + dos_hdr_size, dos_msg, sizeof(dos_msg));

  uint32_t pe_off = PE_LFANEW_OFFSET;

  /*---- PE Signature (4 bytes) ----*/
  image[pe_off] = 'P'; image[pe_off+1] = 'E';
  image[pe_off+2] = 0;  image[pe_off+3] = 0;
  pe_off += 4;

  /*---- COFF File Header (20 bytes) ----
   * Offset  Size  Field
   * 0       2     Machine
   * 2       2     NumberOfSections
   * 4       4     TimeDateStamp
   * 8       4     PointerToSymbolTable (0)
   * 12      4     NumberOfSymbols (0)
   * 16      2     SizeOfOptionalHeader
   * 18      2     Characteristics
   */
  w16(image + pe_off + 0, pb->machine);
  w16(image + pe_off + 2, (uint16_t)pb->num_sections);
  w32(image + pe_off + 4, pb->timestamp);
  w32(image + pe_off + 8, 0);  /* no symbol table in exe */
  w32(image + pe_off + 12, 0);
  w16(image + pe_off + 16, PE_OPTIONAL_HDR_SIZE);
  /* Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE */
  w16(image + pe_off + 18, 0x0022);
  pe_off += 20;

  /*---- Optional Header PE32+ (240 bytes) ----
   * Standard fields (24 bytes):
   *   0     2   Magic (0x020B = PE32+)
   *   2     1   MajorLinkerVersion
   *   3     1   MinorLinkerVersion
   *   4     4   SizeOfCode
   *   8     4   SizeOfInitializedData
   *   12    4   SizeOfUninitializedData
   *   16    4   AddressOfEntryPoint
   *   20    4   BaseOfCode
   *
   * Windows-specific fields (88 bytes):
   *   24    8   ImageBase
   *   32    4   SectionAlignment
   *   36    4   FileAlignment
   *   40    2   MajorOSVersion
   *   42    2   MinorOSVersion
   *   44    2   MajorImageVersion
   *   46    2   MinorImageVersion
   *   48    2   MajorSubsystemVersion
   *   50    2   MinorSubsystemVersion
   *   52    4   Win32VersionValue (0)
   *   56    4   SizeOfImage
   *   60    4   SizeOfHeaders
   *   64    4   CheckSum (0, filled by tools)
   *   68    2   Subsystem
   *   70    2   DllCharacteristics
   *   72    8   SizeOfStackReserve
   *   80    8   SizeOfStackCommit
   *   88    8   SizeOfHeapReserve
   *   96    8   SizeOfHeapCommit
   *   104   4   LoaderFlags (0)
   *   108   4   NumberOfRvaAndSizes
   *
   * Data directories (128 bytes = 16 x 8):
   *   112   8   Export Table RVA + Size
   *   120   8   Import Table
   *   128   8   Resource Table
   *   136   8   Exception Table
   *   144   8   Certificate Table
   *   152   8   Base Relocation Table
   *   160   8   Debug
   *   168   8   Architecture (0)
   *   176   8   Global Ptr (0)
   *   184   8   TLS Table
   *   192   8   Load Config Table
   *   200   8   Bound Import
   *   208   8   IAT
   *   216   8   Delay Import Descriptor
   *   224   8   CLR Runtime Header
   *   232   8   Reserved (0)
   */
  uint8_t *opt = image + pe_off;

  /* Standard fields */
  w16(opt + 0, 0x020B);   /* PE32+ magic */
  opt[2] = 1;              /* Major linker version */
  opt[3] = 0;              /* Minor linker version */

  /* Calculate code/data sizes */
  uint32_t code_size = 0, idata_size = 0, udata_size = 0;
  uint32_t base_of_code = 0;
  for (int i = 0; i < pb->num_sections; i++) {
    pe_section_t *s = &pb->sections[i];
    if (s->characteristics & 0x00000020) { /* CODE */
      code_size += s->raw_size;
      if (base_of_code == 0) base_of_code = s->rva;
    }
    if (s->characteristics & 0x00000040) idata_size += s->raw_size;
    if (s->characteristics & 0x00000080) udata_size += s->virtual_size;
  }

  w32(opt + 4, code_size);
  w32(opt + 8, idata_size);
  w32(opt + 12, udata_size);
  w32(opt + 16, pb->entry_rva);
  w32(opt + 20, base_of_code);

  /* Windows-specific fields */
  w64(opt + 24, pb->image_base);
  w32(opt + 32, PE_SECTION_ALIGNMENT);
  w32(opt + 36, PE_FILE_ALIGNMENT);
  w16(opt + 40, 6);   /* MajorOSVersion (Windows Vista+) */
  w16(opt + 42, 0);
  w16(opt + 44, 0);   /* Image version */
  w16(opt + 46, 0);
  w16(opt + 48, 6);   /* MajorSubsystemVersion */
  w16(opt + 50, 0);
  w32(opt + 52, 0);   /* Win32VersionValue */
  w32(opt + 56, pb->size_of_image);
  w32(opt + 60, pb->size_of_headers);
  w32(opt + 64, 0);   /* CheckSum */
  w16(opt + 68, pb->subsystem);
  w16(opt + 70, PE_DLL_DYNAMIC_BASE | PE_DLL_NX_COMPAT | PE_DLL_HIGH_ENTROPY_VA | PE_DLL_TERMINAL_SERVER);
  w64(opt + 72, pb->stack_reserve);
  w64(opt + 80, pb->stack_commit);
  w64(opt + 88, pb->heap_reserve);
  w64(opt + 96, pb->heap_commit);
  w32(opt + 104, 0);  /* LoaderFlags */
  w32(opt + 108, PE_NUM_DATA_DIRS);

  /* Data directories (112..239) - all zero for minimal exe */
  /* Exception table (.pdata) - if present */
  for (int i = 0; i < pb->num_sections; i++) {
    if (strcmp(pb->sections[i].name, ".pdata") == 0) {
      w32(opt + 112 + 3*8 + 0, pb->sections[i].rva);       /* Exception RVA */
      w32(opt + 112 + 3*8 + 4, pb->sections[i].virtual_size); /* Exception Size */
      break;
    }
  }

  pe_off += PE_OPTIONAL_HDR_SIZE;

  /*---- Section Headers (40 bytes each) ----
   * Offset  Size  Field
   * 0       8     Name
   * 8       4     VirtualSize
   * 12      4     VirtualAddress
   * 16      4     SizeOfRawData
   * 20      4     PointerToRawData
   * 24      4     PointerToRelocations (0 in exe)
   * 28      4     PointerToLinenumbers (0)
   * 32      2     NumberOfRelocations (0)
   * 34      2     NumberOfLinenumbers (0)
   * 36      4     Characteristics
   */
  for (int i = 0; i < pb->num_sections; i++) {
    pe_section_t *s = &pb->sections[i];
    uint8_t *sh = image + pe_off;
    memcpy(sh, s->name, 8);
    w32(sh + 8,  s->virtual_size);
    w32(sh + 12, s->rva);
    w32(sh + 16, s->raw_size);
    w32(sh + 20, s->raw_offset);
    w32(sh + 24, 0); /* no relocs in exe */
    w32(sh + 28, 0);
    w16(sh + 32, 0);
    w16(sh + 34, 0);
    w32(sh + 36, s->characteristics);
    pe_off += 40;
  }

  /*---- Section Raw Data ----*/
  for (int i = 0; i < pb->num_sections; i++) {
    pe_section_t *s = &pb->sections[i];
    if (s->data && s->data_len > 0 && s->raw_offset + s->data_len <= total_file_size) {
      memcpy(image + s->raw_offset, s->data, s->data_len);
    }
  }

  /*==== Step 4: Write to disk ====*/
  FILE *f = fopen(out_path, "wb");
  if (!f) {
    fprintf(stderr, "[pe_writer] cannot create: %s\n", out_path);
    free(image);
    return -1;
  }

  size_t written = fwrite(image, 1, total_file_size, f);
  fclose(f);
  free(image);

  if (written != total_file_size) {
    fprintf(stderr, "[pe_writer] write error: %zu/%u bytes\n", written, total_file_size);
    return -1;
  }

  fprintf(stderr, "[pe_writer] %s: %u bytes, %d sections, entry=0x%X, base=0x%llX\n",
          out_path, total_file_size, pb->num_sections, pb->entry_rva,
          (unsigned long long)pb->image_base);
  return 0;
}

/*--------------------------------------------------------------------------
 * Dump PE builder state for diagnostics.
 *------------------------------------------------------------------------*/
void pe_builder_dump(pe_builder_t *pb, FILE *out) {
  if (!pb || !out) return;
  fprintf(out, "=== PE Builder ===\n");
  fprintf(out, "  Machine: 0x%04X  Subsystem: %u\n", pb->machine, pb->subsystem);
  fprintf(out, "  ImageBase: 0x%llX  Entry: 0x%08X\n",
          (unsigned long long)pb->image_base, pb->entry_rva);
  fprintf(out, "  SizeOfHeaders: 0x%X  SizeOfImage: 0x%X\n",
          pb->size_of_headers, pb->size_of_image);
  fprintf(out, "  Stack: reserve=0x%llX commit=0x%llX\n",
          (unsigned long long)pb->stack_reserve, (unsigned long long)pb->stack_commit);
  fprintf(out, "  Heap:  reserve=0x%llX commit=0x%llX\n",
          (unsigned long long)pb->heap_reserve, (unsigned long long)pb->heap_commit);
  fprintf(out, "  Sections: %d\n", pb->num_sections);
  for (int i = 0; i < pb->num_sections; i++) {
    pe_section_t *s = &pb->sections[i];
    fprintf(out, "    [%d] %-8s  RVA=0x%08X  VSize=0x%X  RawOff=0x%X  RawSz=0x%X  Chars=0x%08X\n",
            i, s->name, s->rva, s->virtual_size, s->raw_offset, s->raw_size, s->characteristics);
  }
  fprintf(out, "\n");
}
