#include "../include/codec.h"

namespace codec {

// Stub implementations - real compression/decompression disabled
// User indicated instrumentation and non-critical functions aren't required

std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success) {
    if (success) *success = false;  // Indicate unsupported
    return input;  // Return uncompressed
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success) {
    if (success) *success = false;  // Indicate unsupported
    return input;  // Return as-is
}

}  // namespace codec

// MASM deflate function stub
extern "C" {
    void deflate_brutal_masm(const void* input, size_t input_size, 
                            void* output, size_t* output_size) {
        // Stub: no-op
        if (output_size) *output_size = 0;
    }
}
