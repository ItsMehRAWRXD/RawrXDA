/*==========================================================================
 * import_test.c — Import table builder test & verification
 *
 * Builds a sample import table for kernel32.dll + user32.dll
 * and dumps the generated .idata section.
 *=========================================================================*/
#include "import_builder.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Import Table Builder Test ===\n\n");

    import_builder_t *ib = import_builder_new();

    /* Add kernel32.dll imports */
    int k32 = import_builder_add_dll(ib, "kernel32.dll");
    import_builder_add_func(ib, k32, "ExitProcess", 0);
    import_builder_add_func(ib, k32, "GetProcessHeap", 0);
    import_builder_add_func(ib, k32, "HeapAlloc", 0);
    import_builder_add_func(ib, k32, "HeapFree", 0);
    import_builder_add_func(ib, k32, "GetStdHandle", 0);
    import_builder_add_func(ib, k32, "WriteConsoleA", 0);

    /* Add user32.dll imports */
    int u32 = import_builder_add_dll(ib, "user32.dll");
    import_builder_add_func(ib, u32, "MessageBoxA", 0);
    import_builder_add_func(ib, u32, "MessageBoxW", 0);

    /* Test auto-detect format */
    import_builder_add_auto(ib, "ntdll!RtlInitUnicodeString");

    /* Build at section RVA 0x3000 */
    printf("Building .idata at RVA 0x3000...\n\n");
    import_builder_build(ib, 0x3000);

    /* Dump structure */
    import_builder_dump(ib);

    /* Verify IAT lookups */
    printf("\n--- IAT RVA Lookups ---\n");
    uint32_t rva;

    rva = import_builder_get_iat_rva(ib, "kernel32.dll", "ExitProcess");
    printf("  ExitProcess IAT RVA: 0x%08X %s\n", rva, rva ? "OK" : "FAIL");

    rva = import_builder_get_iat_rva(ib, "kernel32.dll", "HeapAlloc");
    printf("  HeapAlloc IAT RVA:   0x%08X %s\n", rva, rva ? "OK" : "FAIL");

    rva = import_builder_get_iat_rva(ib, "user32.dll", "MessageBoxA");
    printf("  MessageBoxA IAT RVA: 0x%08X %s\n", rva, rva ? "OK" : "FAIL");

    rva = import_builder_get_iat_rva(ib, "ntdll.dll", "RtlInitUnicodeString");
    printf("  RtlInitUnicodeString IAT RVA: 0x%08X %s\n", rva, rva ? "OK" : "FAIL");

    /* Verify raw bytes at IDT */
    printf("\n--- IDT Raw Bytes (first 60 bytes) ---\n  ");
    for (uint32_t i = 0; i < 60 && i < ib->idata_size; i++) {
        printf("%02X ", ib->idata[i]);
        if ((i + 1) % 20 == 0) printf("\n  ");
    }
    printf("\n");

    /* Write .idata to file for inspection */
    FILE *f = fopen("test_idata.bin", "wb");
    if (f) {
        fwrite(ib->idata, 1, ib->idata_size, f);
        fclose(f);
        printf("\nWrote test_idata.bin (%u bytes)\n", ib->idata_size);
    }

    import_builder_free(ib);
    printf("\nSUCCESS\n");
    return 0;
}
