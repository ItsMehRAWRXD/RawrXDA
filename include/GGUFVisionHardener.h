#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace RawrXD {

/**
 * @brief GGUF Vision Tensor Metadata - Secure container for vision shards.
 * Prevents path traversal and memory corruption during multi-modal loading.
 */
struct VisionTensorMetadata {
    std::string name;
    uint64_t offset;
    uint64_t size;
    uint32_t dimensions[4];
    uint32_t num_dims;
    uint32_t type; // GGML_RXD_TYPE_F32, etc.

    void validate() const {
        if (size == 0 || size > 1024ULL * 1024 * 1024 * 4) { // 4GB max shard
            throw std::runtime_error("Invalid vision tensor size: " + std::to_string(size));
        }
        if (num_dims > 4) {
            throw std::runtime_error("Excessive vision tensor dimensions: " + std::to_string(num_dims));
        }
    }
};

class GGUFVisionHardener {
public:
    static bool verify_shard_integrity(const std::string& name, uint64_t size) {
        // Prevent known injection patterns in tensor naming
        if (name.find("..") != std::string::npos || name.find("/") != std::string::npos) {
            return false;
        }
        return (size > 0 && size < (1ULL << 32)); // Max 4GB per vision shard
    }
};

} // namespace RawrXD
