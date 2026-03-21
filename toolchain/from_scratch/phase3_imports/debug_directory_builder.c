/*==========================================================================
 * debug_directory_builder.c — RSDS + IMAGE_DEBUG_DIRECTORY for PE DD[6]
 * Layout matches MSVC / link.exe: [IMAGE_DEBUG_DIRECTORY][RSDS payload], 4-byte padded.
 *=========================================================================*/
#include "debug_directory_builder.h"
#include <stdlib.h>
#include <string.h>

#define ALIGN_UP(v, a) (((v) + (a)-1u) & ~((a)-1u))

static void w32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)(v >> 24);
}

static void w16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

/* RSDS: 'RSDS' + GUID(16) + Age + null-terminated PDB path (UTF-8) */
static int build_rsds(const char* pdb_path_utf8, uint8_t** out, uint32_t* out_len) {
    if (!pdb_path_utf8 || !out || !out_len)
        return -1;
    size_t plen = strlen(pdb_path_utf8);
    if (plen > 2040u)
        return -1;
    uint32_t total = 4u + 16u + 4u + (uint32_t)plen + 1u;
    uint8_t* b = (uint8_t*)malloc((size_t)total);
    if (!b)
        return -1;
    memcpy(b, "RSDS", 4);
    memset(b + 4, 0, 16);
    w32(b + 20, 1u);
    memcpy(b + 24, pdb_path_utf8, plen + 1);
    *out = b;
    *out_len = total;
    return 0;
}

int debug_directory_build_rsdsp(const char* pdb_path_utf8,
                                uint32_t section_rva,
                                uint32_t section_file_offset,
                                uint32_t time_date_stamp,
                                debug_directory_result_t* out) {
    if (!out)
        return -1;
    memset(out, 0, sizeof(*out));

    uint8_t* rsds = NULL;
    uint32_t rsds_len = 0;
    if (build_rsds(pdb_path_utf8, &rsds, &rsds_len) != 0)
        return -1;

    uint32_t rsds_padded = ALIGN_UP(rsds_len, 4u);
    uint32_t sec_size = IMAGE_DEBUG_DIRECTORY_SIZE + rsds_padded;

    uint8_t* sec = (uint8_t*)calloc(1, (size_t)sec_size);
    if (!sec) {
        free(rsds);
        return -1;
    }

    /* Directory first (DataDirectory[6] points here), then RSDS */
    uint8_t* ent = sec;
    w32(ent + 0, 0u);
    w32(ent + 4, time_date_stamp);
    w16(ent + 8, 0u);
    w16(ent + 10, 0u);
    w32(ent + 12, IMAGE_DEBUG_TYPE_CODEVIEW);
    w32(ent + 16, rsds_padded);
    w32(ent + 20, section_rva + IMAGE_DEBUG_DIRECTORY_SIZE);
    w32(ent + 24, section_file_offset + IMAGE_DEBUG_DIRECTORY_SIZE);

    memcpy(sec + IMAGE_DEBUG_DIRECTORY_SIZE, rsds, (size_t)rsds_len);
    /* calloc zero-filled padding after rsds */
    free(rsds);

    out->section_data = sec;
    out->section_size = sec_size;
    out->debug_dir_rva = section_rva;
    out->debug_dir_size = IMAGE_DEBUG_DIRECTORY_SIZE;
    return 0;
}

void debug_directory_result_free(debug_directory_result_t* r) {
    if (!r)
        return;
    free(r->section_data);
    memset(r, 0, sizeof(*r));
}

#define RSDS_SIG_LE 0x53445352u /* 'RSDS' */

int debug_directory_build_codeview_rsdS(const char* pdb_path,
                                       uint32_t debug_section_rva,
                                       uint32_t time_date_stamp,
                                       uint8_t** out,
                                       uint32_t* out_size) {
    if (!pdb_path || !*pdb_path || !out || !out_size)
        return -1;

    size_t path_len = strlen(pdb_path);
    if (path_len > 4090u)
        return -1;

    uint32_t cv_body = 4u + 16u + 4u + (uint32_t)(path_len + 1u);
    uint32_t cv_size = ALIGN_UP(cv_body, 4u);
    uint32_t dir_size = IMAGE_DEBUG_DIRECTORY_SIZE;
    uint32_t total = dir_size + cv_size;

    uint8_t* buf = (uint8_t*)calloc(1, (size_t)total);
    if (!buf)
        return -1;

    ImageDebugDirectory* dir = (ImageDebugDirectory*)buf;
    dir->characteristics = 0;
    dir->time_date_stamp = time_date_stamp;
    dir->major_version = 0;
    dir->minor_version = 0;
    dir->type = IMAGE_DEBUG_TYPE_CODEVIEW;
    dir->size_of_data = cv_size;
    dir->address_of_raw_data = debug_section_rva + dir_size;
    dir->pointer_to_raw_data = 0;

    uint8_t* cv = buf + dir_size;
    w32(cv, RSDS_SIG_LE);
    memset(cv + 4, 0, 16);
    w32(cv + 20, 1u);
    memcpy(cv + 24, pdb_path, path_len + 1);

    *out = buf;
    *out_size = total;
    return 0;
}
