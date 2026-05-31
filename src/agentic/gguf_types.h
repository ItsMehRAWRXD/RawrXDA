// =============================================================================
// gguf_types.h — C-compatible GGUF structure definitions (packed)
// Must match offsets in gguf_parser.asm exactly
// =============================================================================
#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

// GGUF_Info (48 bytes) — offsets must match GINFO_* in gguf_parser.asm
struct GGUF_Info {
    uint32_t magic;           // +0
    uint32_t version;       // +4
    uint32_t tensor_count;  // +8
    uint32_t metadata_count;// +12
    uint64_t header_size;   // +16
    uint64_t tensor_offset; // +24
    uint64_t metadata_offset;// +32
    uint32_t alignment;     // +40
    uint32_t pad;           // +44
};

// GGUF_Tensor (80 bytes) — offsets must match GTENSOR_* in gguf_parser.asm
struct GGUF_Tensor {
    uint64_t name_len;      // +0
    uint64_t name_ptr;      // +8  (offset into mapped file)
    uint32_t n_dims;        // +16
    uint64_t dims[4];       // +20
    uint32_t type;          // +52
    uint64_t offset;        // +56
    uint64_t size;          // +64
    void*    data_ptr;      // +72
};

#pragma pack(pop)

// Exports from gguf_parser.asm
int32_t GGUF_ParseHeader(void* mappedBase, uint64_t fileSize, struct GGUF_Info* out);
int32_t GGUF_GetTensorInfo(void* mappedBase, uint32_t tensorIdx, struct GGUF_Info* info, struct GGUF_Tensor* out);

#ifdef __cplusplus
}
#endif
