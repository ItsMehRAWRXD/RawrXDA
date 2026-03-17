#ifndef RESOURCE_BUILDER_H
#define RESOURCE_BUILDER_H

#include <stdint.h>
#include <stddef.h>

// Resource types
#define RT_CURSOR       1
#define RT_BITMAP       2
#define RT_ICON         3
#define RT_MENU         4
#define RT_DIALOG       5
#define RT_STRING       6
#define RT_FONTDIR      7
#define RT_FONT         8
#define RT_ACCELERATOR  9
#define RT_RCDATA       10
#define RT_MESSAGETABLE 11
#define RT_GROUP_CURSOR 12
#define RT_GROUP_ICON   14
#define RT_VERSION      16
#define RT_DLGINCLUDE   17
#define RT_PLUGPLAY     19
#define RT_VXD          20
#define RT_ANICURSOR    21
#define RT_ANIICON      22
#define RT_HTML         23
#define RT_MANIFEST     24

// Resource directory entry
typedef struct {
    uint32_t name_or_id;
    uint32_t offset_to_data;
} IMAGE_RESOURCE_DIRECTORY_ENTRY;

// Resource directory
typedef struct {
    uint32_t characteristics;
    uint32_t time_date_stamp;
    uint16_t major_version;
    uint16_t minor_version;
    uint16_t number_of_named_entries;
    uint16_t number_of_id_entries;
    IMAGE_RESOURCE_DIRECTORY_ENTRY entries[1]; // Variable size
} IMAGE_RESOURCE_DIRECTORY;

// Resource data entry
typedef struct {
    uint32_t offset_to_data;
    uint32_t size;
    uint32_t code_page;
    uint32_t reserved;
} IMAGE_RESOURCE_DATA_ENTRY;

// Resource builder context
typedef struct {
    uint8_t* rsrc_section;
    size_t rsrc_size;
    size_t current_offset;
    IMAGE_RESOURCE_DIRECTORY* root_dir;
} ResourceBuilder;

// Initialize resource builder
int resource_builder_init(ResourceBuilder* rb, size_t initial_size);

// Add a resource
int resource_builder_add_resource(ResourceBuilder* rb,
                                  uint32_t type,
                                  uint32_t name,
                                  uint16_t language,
                                  const void* data,
                                  size_t data_size);

// Finalize and get the .rsrc section
uint8_t* resource_builder_finalize(ResourceBuilder* rb, size_t* out_size);

// Cleanup
void resource_builder_free(ResourceBuilder* rb);

#endif // RESOURCE_BUILDER_H