/*
 * PE32+ executable writer — DOS stub, PE signature, COFF, optional header, sections, IAT.
 */
#ifndef PHASE2_PE_WRITER_H
#define PHASE2_PE_WRITER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMAGE_FILE_MACHINE_AMD64  0x8664
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b
#define IMAGE_SUBSYSTEM_WINDOWS_CUI 3
#define IMAGE_SCN_CNT_CODE         0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_MEM_READ         0x40000000
#define IMAGE_SCN_MEM_EXEC         0x20000000
#define IMAGE_SCN_MEM_WRITE        0x80000000
#define IMAGE_DIRECTORY_ENTRY_IMPORT  1
#define IMAGE_DIRECTORY_ENTRY_IAT     12

typedef struct PeWriter PeWriter;

PeWriter* pe_writer_create(void);
void      pe_writer_destroy(PeWriter* pw);

/* Set image base and entry RVA (relative to .text start; 0 = first byte of .text). */
void pe_writer_set_entry(PeWriter* pw, uint32_t entry_rva);

/* Add .text section (code). data copied; can be NULL if size 0. */
void pe_writer_add_text(PeWriter* pw, const uint8_t* data, uint32_t size);

/* Set import: one DLL, one function. Builds IDT/IAT in .idata. */
void pe_writer_set_import(PeWriter* pw, const char* dll_name, const char* func_name);

/* Emit PE to buffer. Caller frees *out. Returns size or 0. */
uint32_t pe_writer_emit(PeWriter* pw, uint8_t** out);

#ifdef __cplusplus
}
#endif

#endif /* PHASE2_PE_WRITER_H */
