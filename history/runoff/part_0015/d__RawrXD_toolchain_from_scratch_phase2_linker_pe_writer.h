#ifndef PE_WRITER_H
#define PE_WRITER_H

/*==========================================================================
 * Phase 4: PE Executable Writer - Fully Reverse Engineered
 *
 * Writes a complete PE32+ (64-bit) executable with proper layout:
 *
 *   Offset 0x000: DOS Header (64 bytes)
 *     - MZ signature at [0..1]
 *     - e_lfanew pointer at [0x3C..0x3F] -> PE signature offset
 *
 *   Offset 0x080: DOS Stub (optional, ~64 bytes)
 *     - "This program cannot be run in DOS mode" message
 *
 *   Offset 0x0C0: PE Signature "PE\0\0" (4 bytes)
 *
 *   Offset 0x0C4: COFF File Header (20 bytes)
 *     - Machine, NumberOfSections, TimeDateStamp
 *     - PointerToSymbolTable (0 for executables)
 *     - NumberOfSymbols (0 for executables)
 *     - SizeOfOptionalHeader (0xF0 for PE32+)
 *     - Characteristics
 *
 *   Offset 0x0D8: Optional Header PE32+ (240 bytes = 0xF0)
 *     - Magic 0x020B (PE32+)
 *     - AddressOfEntryPoint, ImageBase, SectionAlignment, FileAlignment
 *     - SizeOfImage, SizeOfHeaders
 *     - Subsystem, DLL characteristics
 *     - Stack/Heap sizes
 *     - NumberOfRvaAndSizes (16)
 *     - Data Directories (16 x 8 bytes)
 *
 *   After Optional Header: Section Headers (40 bytes each)
 *
 *   File-aligned: Raw section data
 *
 * Total header size = DOS(64) + stub(64) + PE_sig(4) + COFF(20) + OptHdr(240)
 *                   + SecHdrs(40*N), rounded up to FileAlignment (0x200)
 *=========================================================================*/

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Forward declare merge types */
typedef struct merged_section merged_section_t;
typedef struct merge_context merge_context_t;

typedef struct pe_builder pe_builder_t;

/* ---- PE subsystem types ---- */
#define PE_SUBSYS_CONSOLE  3
#define PE_SUBSYS_WINDOWS  2

/* ---- PE DLL characteristics ---- */
#define PE_DLL_DYNAMIC_BASE     0x0040
#define PE_DLL_NX_COMPAT        0x0100
#define PE_DLL_NO_SEH           0x0400
#define PE_DLL_TERMINAL_SERVER  0x8000
#define PE_DLL_HIGH_ENTROPY_VA  0x0020

/* ---- Create/destroy ---- */
pe_builder_t *pe_builder_new(uint16_t machine);
void pe_builder_free(pe_builder_t *pb);

/* ---- Configuration ---- */
void pe_builder_set_entry(pe_builder_t *pb, uint32_t entry_rva);
void pe_builder_set_image_base(pe_builder_t *pb, uint64_t base);
void pe_builder_set_subsystem(pe_builder_t *pb, uint16_t subsys);
void pe_builder_set_stack(pe_builder_t *pb, uint64_t reserve, uint64_t commit);
void pe_builder_set_heap(pe_builder_t *pb, uint64_t reserve, uint64_t commit);

/* ---- Section management ---- */
void pe_builder_add_section(pe_builder_t *pb, const char *name,
                            const uint8_t *data, size_t len, uint32_t characteristics);

/* ---- Build from merge context (preferred) ---- */
int pe_builder_from_merge(pe_builder_t *pb, merge_context_t *ctx);

/* ---- Write to file ---- */
int pe_builder_write(pe_builder_t *pb, const char *out_path);

/* ---- Diagnostics ---- */
void pe_builder_dump(pe_builder_t *pb, FILE *out);

#endif
