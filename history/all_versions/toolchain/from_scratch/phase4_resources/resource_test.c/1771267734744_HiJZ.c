#include "resource_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("=== Resource Builder Test ===\n\n");

    ResourceBuilder rb;
    if (!resource_builder_init(&rb, 4096)) {
        printf("Failed to initialize resource builder\n");
        return 1;
    }

    // Add some test resources
    const char* test_data1 = "Hello World Resource 1";
    const char* test_data2 = "Test Resource Data 2";
    const char* test_data3 = "Another Resource Entry";

    resource_builder_add_resource(&rb, RT_RCDATA, 1, 1033, test_data1, strlen(test_data1) + 1);
    resource_builder_add_resource(&rb, RT_RCDATA, 2, 1033, test_data2, strlen(test_data2) + 1);
    resource_builder_add_resource(&rb, RT_RCDATA, 3, 1033, test_data3, strlen(test_data3) + 1);

    size_t rsrc_size;
    uint8_t* rsrc_data = resource_builder_finalize(&rb, &rsrc_size);

    if (!rsrc_data) {
        printf("Failed to finalize resource builder\n");
        resource_builder_free(&rb);
        return 1;
    }

    printf("Built .rsrc section at RVA 0x4000...\n\n");
    printf("=== Resource Section (size: %zu bytes) ===\n", rsrc_size);

    // Parse and display the structure
    IMAGE_RESOURCE_DIRECTORY* root = (IMAGE_RESOURCE_DIRECTORY*)rsrc_data;
    printf("Root Directory:\n");
    printf("  Characteristics: 0x%08X\n", root->characteristics);
    printf("  TimeDateStamp: 0x%08X\n", root->time_date_stamp);
    printf("  MajorVersion: %d\n", root->major_version);
    printf("  MinorVersion: %d\n", root->minor_version);
    printf("  NumberOfNamedEntries: %d\n", root->number_of_named_entries);
    printf("  NumberOfIdEntries: %d\n", root->number_of_id_entries);

    if (root->number_of_id_entries > 0) {
        IMAGE_RESOURCE_DIRECTORY_ENTRY* entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(root + 1);
        printf("\nType Entry[0]:\n");
        printf("  Name/ID: %d\n", entry->name_or_id);
        printf("  Offset: 0x%08X\n", entry->offset_to_data);

        IMAGE_RESOURCE_DIRECTORY* type_dir = (IMAGE_RESOURCE_DIRECTORY*)(rsrc_data + entry->offset_to_data);
        printf("\nType Directory:\n");
        printf("  NumberOfNamedEntries: %d\n", type_dir->number_of_named_entries);
        printf("  NumberOfIdEntries: %d\n", type_dir->number_of_id_entries);

        for (int i = 0; i < type_dir->number_of_id_entries; i++) {
            IMAGE_RESOURCE_DIRECTORY_ENTRY* name_entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(type_dir + 1) + i;
            printf("\n  Name Entry[%d]:\n", i);
            printf("    Name/ID: %d\n", name_entry->name_or_id);
            printf("    Offset: 0x%08X\n", name_entry->offset_to_data);

            IMAGE_RESOURCE_DATA_ENTRY* data_entry = (IMAGE_RESOURCE_DATA_ENTRY*)(rsrc_data + name_entry->offset_to_data);
            printf("    Data Entry:\n");
            printf("      OffsetToData: 0x%08X\n", data_entry->offset_to_data);
            printf("      Size: %d\n", data_entry->size);
            printf("      CodePage: %d\n", data_entry->code_page);

            char* data = (char*)(rsrc_data + data_entry->offset_to_data);
            printf("      Data: \"%s\"\n", data);
        }
    }

    // Write to file
    FILE* f = fopen("test_rsrc.bin", "wb");
    if (f) {
        fwrite(rsrc_data, 1, rsrc_size, f);
        fclose(f);
        printf("\nWrote test_rsrc.bin (%zu bytes)\n", rsrc_size);
    }

    resource_builder_free(&rb);

    printf("\nSUCCESS\n");
    return 0;
}