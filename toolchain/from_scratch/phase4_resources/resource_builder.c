#include "resource_builder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper to align to 4 bytes
#define ALIGN4(x) (((x) + 3) & ~3)

// Internal resource entry
typedef struct ResourceEntry {
    uint32_t type;
    uint32_t name;
    uint16_t language;
    const void* data;
    size_t data_size;
    struct ResourceEntry* next;
} ResourceEntry;

// Resource directory node
typedef struct DirNode {
    IMAGE_RESOURCE_DIRECTORY dir;
    struct DirNode* subdirs;
    IMAGE_RESOURCE_DATA_ENTRY* data_entries;
    int num_entries;
} DirNode;

struct ResourceBuilder {
    ResourceEntry* entries;
    size_t data_offset;
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_used;
};

int resource_builder_init(ResourceBuilder* rb, size_t initial_size) {
    rb->entries = NULL;
    rb->data_offset = 0;
    rb->buffer_size = initial_size;
    rb->buffer_used = 0;
    rb->buffer = (uint8_t*)malloc(initial_size);
    if (!rb->buffer) return 0;
    return 1;
}

static int ensure_space(ResourceBuilder* rb, size_t needed) {
    if (rb->buffer_used + needed > rb->buffer_size) {
        size_t new_size = rb->buffer_size * 2;
        while (new_size < rb->buffer_used + needed) new_size *= 2;
        uint8_t* new_buf = (uint8_t*)realloc(rb->buffer, new_size);
        if (!new_buf) return 0;
        rb->buffer = new_buf;
        rb->buffer_size = new_size;
    }
    return 1;
}

int resource_builder_add_resource(ResourceBuilder* rb,
                                  uint32_t type,
                                  uint32_t name,
                                  uint16_t language,
                                  const void* data,
                                  size_t data_size) {
    ResourceEntry* entry = (ResourceEntry*)malloc(sizeof(ResourceEntry));
    if (!entry) return 0;

    entry->type = type;
    entry->name = name;
    entry->language = language;
    entry->data = data;
    entry->data_size = data_size;
    entry->next = rb->entries;
    rb->entries = entry;

    // Reserve space for data (aligned)
    rb->data_offset = ALIGN4(rb->data_offset + data_size);

    return 1;
}

static int compare_entries(const void* a, const void* b) {
    const ResourceEntry* ea = *(const ResourceEntry**)a;
    const ResourceEntry* eb = *(const ResourceEntry**)b;

    if (ea->type != eb->type) return ea->type - eb->type;
    if (ea->name != eb->name) return ea->name - eb->name;
    return ea->language - eb->language;
}

uint8_t* resource_builder_finalize(ResourceBuilder* rb, size_t* out_size) {
    // Count entries and sort them
    int num_entries = 0;
    for (ResourceEntry* e = rb->entries; e; e = e->next) num_entries++;

    ResourceEntry** entry_array = (ResourceEntry**)malloc(num_entries * sizeof(ResourceEntry*));
    if (!entry_array) return NULL;

    int i = 0;
    for (ResourceEntry* e = rb->entries; e; e = e->next) {
        entry_array[i++] = e;
    }
    qsort(entry_array, num_entries, sizeof(ResourceEntry*), compare_entries);

    // Build directory structure
    // This is a simplified implementation - in practice, we'd build the full tree
    // For now, just create a basic structure

    size_t total_size = sizeof(IMAGE_RESOURCE_DIRECTORY) +
                       num_entries * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY) +
                       num_entries * sizeof(IMAGE_RESOURCE_DATA_ENTRY);

    // Add space for data
    total_size += rb->data_offset;

    if (!ensure_space(rb, total_size)) {
        free(entry_array);
        return NULL;
    }

    uint8_t* output = rb->buffer;
    size_t offset = 0;

    // Root directory
    IMAGE_RESOURCE_DIRECTORY* root = (IMAGE_RESOURCE_DIRECTORY*)(output + offset);
    root->characteristics = 0;
    root->time_date_stamp = 0;
    root->major_version = 0;
    root->minor_version = 0;
    root->number_of_named_entries = 0;
    root->number_of_id_entries = 1; // One type directory
    offset += sizeof(IMAGE_RESOURCE_DIRECTORY);

    // Type directory entry
    IMAGE_RESOURCE_DIRECTORY_ENTRY* type_entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(output + offset);
    type_entry->name_or_id = RT_RCDATA; // Raw data
    type_entry->offset_to_data = sizeof(IMAGE_RESOURCE_DIRECTORY) + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
    offset += sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);

    // Type directory
    IMAGE_RESOURCE_DIRECTORY* type_dir = (IMAGE_RESOURCE_DIRECTORY*)(output + offset);
    type_dir->characteristics = 0;
    type_dir->time_date_stamp = 0;
    type_dir->major_version = 0;
    type_dir->minor_version = 0;
    type_dir->number_of_named_entries = 0;
    type_dir->number_of_id_entries = num_entries;
    offset += sizeof(IMAGE_RESOURCE_DIRECTORY);

    // Name entries and data entries
    for (i = 0; i < num_entries; i++) {
        ResourceEntry* e = entry_array[i];

        // Name directory entry
        IMAGE_RESOURCE_DIRECTORY_ENTRY* name_entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(output + offset);
        name_entry->name_or_id = e->name;
        name_entry->offset_to_data = offset + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY) +
                                   (num_entries - i - 1) * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY) +
                                   num_entries * sizeof(IMAGE_RESOURCE_DATA_ENTRY);
        offset += sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
    }

    // Data entries
    size_t data_start = offset + num_entries * sizeof(IMAGE_RESOURCE_DATA_ENTRY);
    for (i = 0; i < num_entries; i++) {
        ResourceEntry* e = entry_array[i];

        IMAGE_RESOURCE_DATA_ENTRY* data_entry = (IMAGE_RESOURCE_DATA_ENTRY*)(output + offset);
        data_entry->offset_to_data = data_start;
        data_entry->size = (uint32_t)e->data_size;
        data_entry->code_page = 0;
        data_entry->reserved = 0;
        offset += sizeof(IMAGE_RESOURCE_DATA_ENTRY);

        // Copy data
        memcpy(output + data_start, e->data, e->data_size);
        data_start += ALIGN4(e->data_size);
    }

    free(entry_array);

    // Free entries
    ResourceEntry* e = rb->entries;
    while (e) {
        ResourceEntry* next = e->next;
        free(e);
        e = next;
    }
    rb->entries = NULL;

    *out_size = total_size;
    return output;
}

void resource_builder_free(ResourceBuilder* rb) {
    if (rb->buffer) free(rb->buffer);
    rb->buffer = NULL;

    ResourceEntry* e = rb->entries;
    while (e) {
        ResourceEntry* next = e->next;
        free(e);
        e = next;
    }
    rb->entries = NULL;
}