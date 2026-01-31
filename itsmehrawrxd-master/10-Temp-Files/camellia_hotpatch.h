#ifndef CAMELLIA_HOTPATCH_H
#define CAMELLIA_HOTPATCH_H

#include <stdint.h>

// Camellia context structure
struct camellia_ctx {
    uint32_t key_table[68];
};

// Camellia function declarations
void camellia_setup(struct camellia_ctx *ctx, const uint8_t *key, int key_len);
void camellia_encrypt(const struct camellia_ctx *ctx, const uint8_t *src, uint8_t *dst);
void camellia_decrypt(const struct camellia_ctx *ctx, const uint8_t *src, uint8_t *dst);

// Hot-patch function
void camellia_hotpatch(const uint8_t *blob, size_t blob_len, const uint8_t *shared_secret);

// Hardware ID spoofing functions
void spoof_volume_serial(const char *serial);
void spoof_mac_address(const char *mac);
void spoof_smbios_uuid(const char *uuid);
void spoof_hdd_firmware(const char *firmware);

// Patch application functions
void patch_memory(void *dst, const void *src, size_t size);
void apply_hardware_breakpoint(void *address);
void apply_fpb_patch(void *flash_addr, void *sram_addr);

// Utility functions
void hex_dump(const uint8_t *data, size_t len);
int verify_poly1305_tag(const uint8_t *data, size_t len, const uint8_t *key, const uint8_t *tag);

// Constants
#define CAMELLIA_BLOCK_SIZE 16
#define CAMELLIA_KEY_SIZE_128 16
#define CAMELLIA_KEY_SIZE_192 24
#define CAMELLIA_KEY_SIZE_256 32
#define POLY1305_TAG_SIZE 16
#define HOTPATCH_HEADER_SIZE 12

// Hot-patch header structure
typedef struct {
    uint8_t  magic[2];     // "CP"
    uint8_t  version;      // Version number
    uint8_t  flags;        // Feature flags
    uint32_t payload_len;  // Encrypted payload length
    uint32_t nonce;        // Nonce for CTR mode
} __attribute__((packed)) hotpatch_header_t;

// Patch descriptor structure
typedef struct {
    uint32_t rva;          // Relative virtual address
    uint32_t size;         // Patch size
    uint8_t  data[];       // Patch data
} __attribute__((packed)) patch_descriptor_t;

// Hardware spoof descriptor structure
typedef struct {
    uint8_t type;          // Spoof type
    uint8_t len;           // Data length
    uint8_t data[];        // Spoof data
} __attribute__((packed)) spoof_descriptor_t;

// Spoof types
#define SPOOF_VOLUME_SERIAL 0x01
#define SPOOF_MAC_ADDRESS   0x02
#define SPOOF_SMBIOS_UUID   0x03
#define SPOOF_HDD_FIRMWARE  0x04
#define SPOOF_CPU_ID        0x05
#define SPOOF_MOTHERBOARD   0x06

// Feature flags
#define FLAG_HARDWARE_SPOOF 0x01
#define FLAG_ANTI_DEBUG     0x02
#define SPOOF_ANTI_VM       0x04
#define FLAG_PERSISTENT     0x08

#endif // CAMELLIA_HOTPATCH_H
