/*==========================================================================
 * rawrxd_check - COFF/PE Diagnostic Utility (Fully Reverse Engineered)
 *
 * Inspects and dumps information about .obj (COFF) and .exe (PE) files.
 * Uses the Phase 1 COFF reader for object files, and manually parses
 * PE headers for executables.
 *
 * Usage: rawrxd_check <file.obj|file.exe> [-sections] [-symbols] [-relocs] [-all]
 *=========================================================================*/
#include "coff_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24); }
static uint64_t rd64(const uint8_t *p) { return (uint64_t)rd32(p) | ((uint64_t)rd32(p+4)<<32); }

/*--------------------------------------------------------------------------
 * Dump PE executable headers.
 *------------------------------------------------------------------------*/
static int dump_pe(const char *path, FILE *out) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

  /* Read DOS header */
  uint8_t dos[64];
  if (fread(dos, 1, 64, f) != 64) { fclose(f); return 1; }
  if (dos[0] != 0x4D || dos[1] != 0x5A) {
    fprintf(out, "Not a PE file (no MZ signature)\n");
    fclose(f); return 1;
  }

  uint32_t pe_offset = rd32(dos + 0x3C);
  fprintf(out, "DOS Header:\n");
  fprintf(out, "  e_magic:  MZ\n");
  fprintf(out, "  e_lfanew: 0x%08X\n\n", pe_offset);

  /* Seek to PE signature */
  fseek(f, pe_offset, SEEK_SET);
  uint8_t pe_sig[4];
  if (fread(pe_sig, 1, 4, f) != 4) { fclose(f); return 1; }
  if (pe_sig[0] != 'P' || pe_sig[1] != 'E' || pe_sig[2] || pe_sig[3]) {
    fprintf(out, "Invalid PE signature\n");
    fclose(f); return 1;
  }

  /* COFF header */
  uint8_t coff[20];
  if (fread(coff, 1, 20, f) != 20) { fclose(f); return 1; }
  uint16_t machine = rd16(coff);
  uint16_t num_sec = rd16(coff + 2);
  uint32_t timestamp = rd32(coff + 4);
  uint16_t opt_size = rd16(coff + 16);
  uint16_t chars = rd16(coff + 18);

  fprintf(out, "COFF Header:\n");
  fprintf(out, "  Machine:           0x%04X (%s)\n", machine,
          machine == 0x8664 ? "AMD64" : machine == 0x14C ? "i386" : "unknown");
  fprintf(out, "  Sections:          %u\n", num_sec);
  fprintf(out, "  Timestamp:         0x%08X\n", timestamp);
  fprintf(out, "  OptionalHdrSize:   0x%X\n", opt_size);
  fprintf(out, "  Characteristics:   0x%04X", chars);
  if (chars & 0x0002) fprintf(out, " EXECUTABLE");
  if (chars & 0x0020) fprintf(out, " LARGE_ADDRESS_AWARE");
  if (chars & 0x2000) fprintf(out, " DLL");
  fprintf(out, "\n\n");

  /* Optional header */
  if (opt_size >= 24) {
    uint8_t opt[240];
    memset(opt, 0, sizeof(opt));
    size_t to_read = opt_size < 240 ? opt_size : 240;
    if (fread(opt, 1, to_read, f) != to_read) { fclose(f); return 1; }

    uint16_t magic = rd16(opt);
    int is_pe32plus = (magic == 0x020B);

    fprintf(out, "Optional Header (%s):\n", is_pe32plus ? "PE32+" : "PE32");
    fprintf(out, "  Magic:              0x%04X\n", magic);
    fprintf(out, "  LinkerVersion:      %u.%u\n", opt[2], opt[3]);
    fprintf(out, "  SizeOfCode:         0x%X\n", rd32(opt + 4));
    fprintf(out, "  SizeOfInitData:     0x%X\n", rd32(opt + 8));
    fprintf(out, "  SizeOfUninitData:   0x%X\n", rd32(opt + 12));
    fprintf(out, "  EntryPoint:         0x%08X\n", rd32(opt + 16));
    fprintf(out, "  BaseOfCode:         0x%08X\n", rd32(opt + 20));

    if (is_pe32plus) {
      fprintf(out, "  ImageBase:          0x%016llX\n", (unsigned long long)rd64(opt + 24));
      fprintf(out, "  SectionAlignment:   0x%X\n", rd32(opt + 32));
      fprintf(out, "  FileAlignment:      0x%X\n", rd32(opt + 36));
      fprintf(out, "  OSVersion:          %u.%u\n", rd16(opt + 40), rd16(opt + 42));
      fprintf(out, "  SizeOfImage:        0x%X\n", rd32(opt + 56));
      fprintf(out, "  SizeOfHeaders:      0x%X\n", rd32(opt + 60));
      fprintf(out, "  Subsystem:          %u (%s)\n", rd16(opt + 68),
              rd16(opt+68)==2 ? "GUI" : rd16(opt+68)==3 ? "Console" : "other");
      fprintf(out, "  DllCharacteristics: 0x%04X\n", rd16(opt + 70));
      fprintf(out, "  StackReserve:       0x%llX\n", (unsigned long long)rd64(opt + 72));
      fprintf(out, "  StackCommit:        0x%llX\n", (unsigned long long)rd64(opt + 80));
      fprintf(out, "  HeapReserve:        0x%llX\n", (unsigned long long)rd64(opt + 88));
      fprintf(out, "  HeapCommit:         0x%llX\n", (unsigned long long)rd64(opt + 96));
      fprintf(out, "  DataDirectories:    %u\n", rd32(opt + 108));
    }
    fprintf(out, "\n");
  }

  /* Section headers */
  fprintf(out, "Section Headers:\n");
  fprintf(out, "  %-8s  %10s  %10s  %10s  %10s  %s\n",
          "Name", "VirtSize", "VirtAddr", "RawSize", "RawAddr", "Flags");
  for (uint16_t i = 0; i < num_sec; i++) {
    uint8_t sh[40];
    if (fread(sh, 1, 40, f) != 40) break;
    char name[9] = {0};
    memcpy(name, sh, 8);
    fprintf(out, "  %-8s  0x%08X  0x%08X  0x%08X  0x%08X  0x%08X",
            name, rd32(sh+8), rd32(sh+12), rd32(sh+16), rd32(sh+20), rd32(sh+36));
    uint32_t ch = rd32(sh + 36);
    if (ch & 0x00000020) fprintf(out, " CODE");
    if (ch & 0x00000040) fprintf(out, " IDATA");
    if (ch & 0x00000080) fprintf(out, " UDATA");
    if (ch & 0x20000000) fprintf(out, " EXEC");
    if (ch & 0x40000000) fprintf(out, " READ");
    if (ch & 0x80000000) fprintf(out, " WRITE");
    fprintf(out, "\n");
  }

  fclose(f);
  return 0;
}

/*--------------------------------------------------------------------------
 * Main
 *------------------------------------------------------------------------*/
int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr,
      "rawrxd_check - COFF/PE Diagnostic Utility\n"
      "Usage: rawrxd_check <file.obj|file.exe> [-all]\n");
    return 1;
  }

  const char *path = argv[1];
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", path);
    return 1;
  }

  unsigned char magic[2];
  if (fread(magic, 1, 2, f) != 2) { fclose(f); return 1; }
  fclose(f);

  if (magic[0] == 0x4D && magic[1] == 0x5A) {
    printf("Format: PE executable (MZ)\n\n");
    return dump_pe(path, stdout);
  }

  if (magic[0] == 0x64 && magic[1] == 0x86) {
    printf("Format: COFF object (x64)\n\n");
    coff_file_t *cf = coff_read(path);
    if (!cf) { printf("Failed to parse\n"); return 1; }
    coff_dump(cf, stdout);
    coff_free(cf);
    return 0;
  }

  if (magic[0] == 0x4C && magic[1] == 0x01) {
    printf("Format: COFF object (x86)\n\n");
    coff_file_t *cf = coff_read(path);
    if (!cf) { printf("Failed to parse\n"); return 1; }
    coff_dump(cf, stdout);
    coff_free(cf);
    return 0;
  }

  printf("Unknown format: magic bytes 0x%02X 0x%02X\n", magic[0], magic[1]);
  return 0;
}
