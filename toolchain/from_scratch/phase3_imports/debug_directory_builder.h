/*==========================================================================
 * PE32+ IMAGE_DEBUG_DIRECTORY builder — CodeView RSDS (PDB path).
 * Feeds DataDirectory[6]; MSVC-style layout: [IMAGE_DEBUG_DIRECTORY][RSDS].
 *=========================================================================*/
#ifndef DEBUG_DIRECTORY_BUILDER_H
#define DEBUG_DIRECTORY_BUILDER_H

#include <stdint.h>
#include <stddef.h>

#define IMAGE_DEBUG_TYPE_UNKNOWN   0u
#define IMAGE_DEBUG_TYPE_COFF      1u
#define IMAGE_DEBUG_TYPE_CODEVIEW  2u
#define IMAGE_DEBUG_TYPE_FPO       3u
#define IMAGE_DEBUG_TYPE_MISC      4u
#define IMAGE_DEBUG_TYPE_EXCEPTION 5u
#define IMAGE_DEBUG_TYPE_FIXUP     6u
#define IMAGE_DEBUG_TYPE_BORLAND   9u
#define IMAGE_DEBUG_TYPE_COFFGRP   10u
#define IMAGE_DEBUG_TYPE_REPRO     16u

#define IMAGE_DEBUG_DIRECTORY_SIZE 28u

#pragma pack(push, 1)
typedef struct {
    uint32_t characteristics;
    uint32_t time_date_stamp;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t type;
    uint32_t size_of_data;
    uint32_t address_of_raw_data;
    uint32_t pointer_to_raw_data;
} ImageDebugDirectory;
#pragma pack(pop)

typedef struct {
    uint8_t* section_data;
    uint32_t section_size;
    uint32_t debug_dir_rva; /* RVA of first IMAGE_DEBUG_DIRECTORY */
    uint32_t debug_dir_size; /* bytes (n * 28) */
} debug_directory_result_t;

/* Build one IMAGE_DEBUG_TYPE_CODEVIEW (RSDS) entry + directory array. */
int debug_directory_build_rsdsp(const char* pdb_path_utf8,
                                uint32_t section_rva,
                                uint32_t section_file_offset,
                                uint32_t time_date_stamp,
                                debug_directory_result_t* out);

void debug_directory_result_free(debug_directory_result_t* r);

/* Single contiguous block [IMAGE_DEBUG_DIRECTORY][RSDS]; pointer_to_raw_data left 0 (emitter patches). */
int debug_directory_build_codeview_rsdS(const char* pdb_path,
                                       uint32_t debug_section_rva,
                                       uint32_t time_date_stamp,
                                       uint8_t** out,
                                       uint32_t* out_size);

#endif
