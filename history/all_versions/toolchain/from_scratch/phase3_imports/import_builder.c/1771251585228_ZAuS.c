/*==========================================================================
 * import_builder.c — PE Import Table Builder implementation
 *
 * Constructs the complete .idata section for a PE32+ executable.
 *
 * Build algorithm:
 *   1. Calculate sizes for all sub-structures
 *   2. Allocate single buffer for entire .idata section
 *   3. Write IDT (Import Directory Table) entries
 *   4. Write ILT (Import Lookup Table) per DLL
 *   5. Write Hint/Name entries
 *   6. Write DLL name strings
 *   7. Write IAT (Import Address Table) — mirrors ILT
 *
 * Layout within .idata:
 *   [IDT entries] [null IDT] [ILT arrays] [Hint/Names] [DLL names] [IAT arrays]
 *=========================================================================*/
#include "import_builder.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- Little-endian helpers ---- */
static void w16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
static void w32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static void w64(uint8_t *p, uint64_t v) {
    w32(p, (uint32_t)(v & 0xFFFFFFFF));
    w32(p + 4, (uint32_t)((v >> 32) & 0xFFFFFFFF));
}

/* ---- Builder management ---- */
import_builder_t *import_builder_new(void) {
    import_builder_t *ib = (import_builder_t *)calloc(1, sizeof(import_builder_t));
    ib->dll_capacity = 16;
    ib->dlls = (import_dll_t *)calloc(16, sizeof(import_dll_t));
    return ib;
}

void import_builder_free(import_builder_t *ib) {
    if (!ib) return;
    for (int i = 0; i < ib->dll_count; i++) {
        free(ib->dlls[i].functions);
    }
    free(ib->dlls);
    free(ib->idata);
    free(ib);
}

int import_builder_add_dll(import_builder_t *ib, const char *dll_name) {
    /* Check if already exists */
    int idx = import_builder_find_dll(ib, dll_name);
    if (idx >= 0) return idx;

    if (ib->dll_count >= ib->dll_capacity) {
        ib->dll_capacity *= 2;
        ib->dlls = (import_dll_t *)realloc(ib->dlls,
                    (size_t)ib->dll_capacity * sizeof(import_dll_t));
    }

    import_dll_t *dll = &ib->dlls[ib->dll_count];
    memset(dll, 0, sizeof(*dll));
    strncpy(dll->dll_name, dll_name, 255);
    dll->func_capacity = 32;
    dll->functions = (import_func_t *)calloc(32, sizeof(import_func_t));

    return ib->dll_count++;
}

int import_builder_find_dll(import_builder_t *ib, const char *dll_name) {
    for (int i = 0; i < ib->dll_count; i++) {
        /* Case-insensitive comparison for DLL names */
        const char *a = ib->dlls[i].dll_name;
        const char *b = dll_name;
        int match = 1;
        while (*a && *b) {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
                match = 0; break;
            }
            a++; b++;
        }
        if (match && *a == *b) return i;
    }
    return -1;
}

void import_builder_add_func(import_builder_t *ib, int dll_idx,
                             const char *func_name, uint16_t hint) {
    if (dll_idx < 0 || dll_idx >= ib->dll_count) return;
    import_dll_t *dll = &ib->dlls[dll_idx];

    /* Check duplicate */
    for (int i = 0; i < dll->func_count; i++) {
        if (strcmp(dll->functions[i].name, func_name) == 0) return;
    }

    if (dll->func_count >= dll->func_capacity) {
        dll->func_capacity *= 2;
        dll->functions = (import_func_t *)realloc(dll->functions,
                          (size_t)dll->func_capacity * sizeof(import_func_t));
    }

    import_func_t *func = &dll->functions[dll->func_count++];
    memset(func, 0, sizeof(*func));
    strncpy(func->name, func_name, 255);
    func->hint = hint;
    func->by_ordinal = 0;
}

void import_builder_add_func_ordinal(import_builder_t *ib, int dll_idx,
                                     uint16_t ordinal) {
    if (dll_idx < 0 || dll_idx >= ib->dll_count) return;
    import_dll_t *dll = &ib->dlls[dll_idx];

    if (dll->func_count >= dll->func_capacity) {
        dll->func_capacity *= 2;
        dll->functions = (import_func_t *)realloc(dll->functions,
                          (size_t)dll->func_capacity * sizeof(import_func_t));
    }

    import_func_t *func = &dll->functions[dll->func_count++];
    memset(func, 0, sizeof(*func));
    func->ordinal = ordinal;
    func->by_ordinal = 1;
}

int import_builder_add_auto(import_builder_t *ib, const char *spec) {
    /* Parse "kernel32.dll!ExitProcess" or "kernel32!ExitProcess" */
    const char *bang = strchr(spec, '!');
    if (!bang) return -1;

    char dll_name[256];
    int dlen = (int)(bang - spec);
    if (dlen > 255) dlen = 255;
    memcpy(dll_name, spec, (size_t)dlen);
    dll_name[dlen] = '\0';

    /* Ensure .dll extension */
    if (!strstr(dll_name, ".dll") && !strstr(dll_name, ".DLL")) {
        strcat(dll_name, ".dll");
    }

    int dll_idx = import_builder_add_dll(ib, dll_name);
    import_builder_add_func(ib, dll_idx, bang + 1, 0);
    return dll_idx;
}

/* ============================================================
 * Build the .idata section
 *
 * Layout:
 *   Phase A: IDT (20 bytes per DLL + 20 bytes null terminator)
 *   Phase B: ILT arrays (8 bytes per func + 8 bytes null per DLL)
 *   Phase C: Hint/Name table (2 + strlen + 1 + padding per func)
 *   Phase D: DLL name strings (strlen + 1 per DLL)
 *   Phase E: IAT arrays (mirrors ILT)
 * ============================================================ */
int import_builder_build(import_builder_t *ib, uint32_t section_rva) {
    ib->section_rva = section_rva;
    int ndlls = ib->dll_count;
    if (ndlls == 0) return 0;

    /* ---- Calculate total function count ---- */
    int total_funcs = 0;
    for (int i = 0; i < ndlls; i++) {
        total_funcs += ib->dlls[i].func_count;
    }

    /* ---- Calculate sizes ---- */
    /* Phase A: IDT = (ndlls + 1) * 20 */
    uint32_t idt_size = (uint32_t)(ndlls + 1) * 20;

    /* Phase B: ILT = per DLL: (func_count + 1) * 8 */
    uint32_t ilt_total = 0;
    for (int i = 0; i < ndlls; i++) {
        ilt_total += (uint32_t)(ib->dlls[i].func_count + 1) * 8;
    }

    /* Phase C: Hint/Name = per func: 2 + strlen + 1, padded to even */
    uint32_t hn_total = 0;
    for (int i = 0; i < ndlls; i++) {
        for (int j = 0; j < ib->dlls[i].func_count; j++) {
            if (!ib->dlls[i].functions[j].by_ordinal) {
                uint32_t entry_size = 2 + (uint32_t)strlen(ib->dlls[i].functions[j].name) + 1;
                if (entry_size & 1) entry_size++; /* pad to even */
                hn_total += entry_size;
            }
        }
    }

    /* Phase D: DLL names */
    uint32_t names_total = 0;
    for (int i = 0; i < ndlls; i++) {
        names_total += (uint32_t)strlen(ib->dlls[i].dll_name) + 1;
    }

    /* Phase E: IAT = same size as ILT */
    uint32_t iat_total = ilt_total;

    /* Total .idata size */
    uint32_t total_size = idt_size + ilt_total + hn_total + names_total + iat_total;
    ib->idata = (uint8_t *)calloc(1, total_size);
    ib->idata_size = total_size;

    /* ---- Offset bases ---- */
    uint32_t idt_off = 0;
    uint32_t ilt_off = idt_size;
    uint32_t hn_off = idt_size + ilt_total;
    uint32_t names_off = idt_size + ilt_total + hn_total;
    uint32_t iat_off = idt_size + ilt_total + hn_total + names_total;

    /* ---- RVA bases ---- */
    ib->idt_rva = section_rva + idt_off;
    ib->idt_size = idt_size;
    ib->iat_rva = section_rva + iat_off;
    ib->iat_size = iat_total;

    /* ---- Phase B/C/D/E: Build per-DLL data ---- */
    uint32_t cur_ilt = ilt_off;
    uint32_t cur_hn = hn_off;
    uint32_t cur_names = names_off;
    uint32_t cur_iat = iat_off;

    for (int i = 0; i < ndlls; i++) {
        import_dll_t *dll = &ib->dlls[i];

        /* Record RVAs for this DLL */
        dll->ilt_rva = section_rva + cur_ilt;
        dll->iat_rva = section_rva + cur_iat;
        dll->name_rva = section_rva + cur_names;

        /* Write DLL name string */
        uint32_t namelen = (uint32_t)strlen(dll->dll_name);
        memcpy(ib->idata + cur_names, dll->dll_name, namelen + 1);
        cur_names += namelen + 1;

        /* Write ILT and IAT entries + Hint/Name entries */
        for (int j = 0; j < dll->func_count; j++) {
            import_func_t *func = &dll->functions[j];

            if (func->by_ordinal) {
                /* Import by ordinal: bit 63 set, ordinal in low 16 bits */
                uint64_t val = 0x8000000000000000ULL | (uint64_t)func->ordinal;
                w64(ib->idata + cur_ilt, val);
                w64(ib->idata + cur_iat, val);
            } else {
                /* Import by name: ILT entry = RVA of Hint/Name */
                uint32_t hn_rva = section_rva + cur_hn;
                w64(ib->idata + cur_ilt, (uint64_t)hn_rva);
                w64(ib->idata + cur_iat, (uint64_t)hn_rva);

                /* Write Hint/Name entry */
                w16(ib->idata + cur_hn, func->hint);
                cur_hn += 2;
                uint32_t fnamelen = (uint32_t)strlen(func->name);
                memcpy(ib->idata + cur_hn, func->name, fnamelen + 1);
                cur_hn += fnamelen + 1;
                if (cur_hn & 1) cur_hn++; /* pad to even */
            }

            cur_ilt += 8;
            cur_iat += 8;
        }

        /* Null terminator for ILT and IAT */
        w64(ib->idata + cur_ilt, 0);
        w64(ib->idata + cur_iat, 0);
        cur_ilt += 8;
        cur_iat += 8;

        /* ---- Write IDT entry (Phase A) ----
         * Offset: i * 20
         * Fields:
         *   [0..3] ImportLookupTableRVA (ILT)
         *   [4..7] TimeDateStamp (0)
         *   [8..11] ForwarderChain (-1 = no forwarder)
         *   [12..15] NameRVA
         *   [16..19] ImportAddressTableRVA (IAT)
         */
        uint32_t idt_entry = idt_off + (uint32_t)(i * 20);
        w32(ib->idata + idt_entry + 0, dll->ilt_rva);
        w32(ib->idata + idt_entry + 4, 0);
        w32(ib->idata + idt_entry + 8, 0);
        w32(ib->idata + idt_entry + 12, dll->name_rva);
        w32(ib->idata + idt_entry + 16, dll->iat_rva);
    }

    /* IDT null terminator (already zeroed by calloc) */

    return 0;
}

/* ---- Get IAT RVA for a specific function ---- */
uint32_t import_builder_get_iat_rva(import_builder_t *ib,
                                    const char *dll_name,
                                    const char *func_name) {
    int dll_idx = import_builder_find_dll(ib, dll_name);
    if (dll_idx < 0) return 0;

    import_dll_t *dll = &ib->dlls[dll_idx];
    uint32_t iat_entry = dll->iat_rva;
    for (int j = 0; j < dll->func_count; j++) {
        if (strcmp(dll->functions[j].name, func_name) == 0) {
            return iat_entry;
        }
        iat_entry += 8; /* each IAT entry is 8 bytes (PE32+) */
    }
    return 0;
}

/* ---- Diagnostic dump ---- */
void import_builder_dump(const import_builder_t *ib) {
    printf("=== Import Table (%d DLLs) ===\n", ib->dll_count);
    printf("  IDT RVA: 0x%08X  size: %u\n", ib->idt_rva, ib->idt_size);
    printf("  IAT RVA: 0x%08X  size: %u\n", ib->iat_rva, ib->iat_size);
    printf("  .idata size: %u bytes\n\n", ib->idata_size);

    for (int i = 0; i < ib->dll_count; i++) {
        const import_dll_t *dll = &ib->dlls[i];
        printf("  DLL[%d]: %s\n", i, dll->dll_name);
        printf("    Name RVA: 0x%08X\n", dll->name_rva);
        printf("    ILT RVA:  0x%08X\n", dll->ilt_rva);
        printf("    IAT RVA:  0x%08X\n", dll->iat_rva);
        printf("    Functions (%d):\n", dll->func_count);
        for (int j = 0; j < dll->func_count; j++) {
            const import_func_t *f = &dll->functions[j];
            if (f->by_ordinal) {
                printf("      [%d] ordinal #%u\n", j, f->ordinal);
            } else {
                printf("      [%d] %s (hint=%u)  IAT_entry=0x%08X\n",
                       j, f->name, f->hint,
                       dll->iat_rva + (uint32_t)(j * 8));
            }
        }
    }
}
