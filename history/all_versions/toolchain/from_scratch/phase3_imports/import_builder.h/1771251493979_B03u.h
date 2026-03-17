/*==========================================================================
 * Phase 3: PE Import Table Builder — Fully Reverse Engineered
 *
 * Generates the complete PE import directory structure from a list of
 * DLL imports. This plugs into phase2's PE writer to produce executables
 * that call Windows API functions.
 *
 * PE Import Structure Layout:
 *
 * .idata section contains (in order):
 *   1. Import Directory Table (IDT)
 *      - Array of IMAGE_IMPORT_DESCRIPTOR (20 bytes each)
 *      - Terminated by a null entry (20 zero bytes)
 *      - Fields: ImportLookupTableRVA, TimeDateStamp, ForwarderChain,
 *                NameRVA, ImportAddressTableRVA
 *
 *   2. Import Lookup Table (ILT) — per DLL
 *      - Array of 8-byte entries (PE32+): bit63=0 means hint/name
 *      - Terminated by 8 zero bytes
 *
 *   3. Hint/Name Table
 *      - Each entry: uint16_t Hint, char[] Name (null-terminated, padded to even)
 *
 *   4. DLL Name strings (null-terminated)
 *
 *   5. Import Address Table (IAT) — per DLL
 *      - Identical to ILT at link time; loader overwrites with actual addresses
 *      - Array of 8-byte entries, null-terminated
 *
 * Data Directory Entries (set in PE optional header):
 *   [1] = Import Directory Table RVA + size
 *   [12] = IAT RVA + size
 *=========================================================================*/
#ifndef IMPORT_BUILDER_H
#define IMPORT_BUILDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ---- Import function entry ---- */
typedef struct {
    char     name[256];    /* function name */
    uint16_t hint;         /* ordinal hint (0 if unknown) */
    uint16_t ordinal;      /* import by ordinal if > 0 */
    int      by_ordinal;   /* 1 = import by ordinal, 0 = by name */
} import_func_t;

/* ---- DLL import entry ---- */
typedef struct {
    char           dll_name[256];
    import_func_t *functions;
    int            func_count;
    int            func_capacity;

    /* Resolved RVAs (filled during build) */
    uint32_t       ilt_rva;      /* Import Lookup Table RVA */
    uint32_t       iat_rva;      /* Import Address Table RVA */
    uint32_t       name_rva;     /* DLL name string RVA */
} import_dll_t;

/* ---- Import builder ---- */
typedef struct {
    import_dll_t *dlls;
    int           dll_count;
    int           dll_capacity;

    /* Generated section data */
    uint8_t      *idata;         /* .idata section content */
    uint32_t      idata_size;

    /* Key RVAs for PE data directories */
    uint32_t      idt_rva;       /* Import Directory Table RVA */
    uint32_t      idt_size;      /* IDT size (including null terminator) */
    uint32_t      iat_rva;       /* IAT start RVA */
    uint32_t      iat_size;      /* Total IAT size */
    uint32_t      section_rva;   /* Base RVA of .idata section */
} import_builder_t;

/* ---- API ---- */
import_builder_t *import_builder_new(void);
void import_builder_free(import_builder_t *ib);

/* Add DLL import */
int import_builder_add_dll(import_builder_t *ib, const char *dll_name);
int import_builder_find_dll(import_builder_t *ib, const char *dll_name);

/* Add function to a DLL */
void import_builder_add_func(import_builder_t *ib, int dll_idx,
                             const char *func_name, uint16_t hint);
void import_builder_add_func_ordinal(import_builder_t *ib, int dll_idx,
                                     uint16_t ordinal);

/* Auto-detect DLL from decorated name (e.g., "kernel32.dll!ExitProcess") */
int import_builder_add_auto(import_builder_t *ib, const char *spec);

/* Build the .idata section
 * section_rva: the RVA where .idata will be placed in the PE */
int import_builder_build(import_builder_t *ib, uint32_t section_rva);

/* Dump import table for diagnostics */
void import_builder_dump(const import_builder_t *ib);

/* Get the IAT entry RVA for a specific function
 * (used by the linker to patch CALL instructions) */
uint32_t import_builder_get_iat_rva(import_builder_t *ib,
                                    const char *dll_name,
                                    const char *func_name);

#endif
