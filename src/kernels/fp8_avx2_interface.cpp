#include "kernels/fp8_avx2_interface.h"
#include <immintrin.h>
#include <intrin.h>  // For __cpuid, __cpuidex
#include <cstring>
#include <new>

namespace RawrXD {
namespace Kernels {

// Check AVX2 availability using CPUID
bool FP8AVX2Quantizer::IsAVX2Available() {
    int cpuInfo[4] = {0};
    
    // Check CPUID leaf 1 for OSXSAVE (bit 27 of ECX)
    __cpuid(cpuInfo, 1);
    const bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    if (!osxsave) return false;
    
    // Check XCR0 for AVX state support
    const uint64_t xcr0 = _xgetbv(0);
    const bool avxState = (xcr0 & 0x6) == 0x6;  // XMM and YMM state
    if (!avxState) return false;
    
    // Check CPUID leaf 7 for AVX2 (bit 5 of EBX)
    __cpuidex(cpuInfo, 7, 0);
    const bool avx2 = (cpuInfo[1] & (1 << 5)) != 0;
    
    return avx2;
}

// FP8BatchProcessor implementation

FP8BatchProcessor::FP8BatchProcessor(size_t max_batches)
    : max_batches_(max_batches)
    , pending_count_(0)
    , write_index_(0)
    , read_index_(0) {
    
    // Allocate aligned memory for float batches (256-byte alignment for AVX2)
    const size_t total_floats = max_batches * kFloatsPerBatch;
    const size_t buffer_size = total_floats * sizeof(float);
    
    aligned_buffer_ = static_cast<float*>(
        _aligned_malloc(buffer_size, FP8AVX2Quantizer::kAlign256)
    );
    
    if (!aligned_buffer_) {
        throw std::bad_alloc();
    }
    
    // Allocate completion flags
    completion_flags_ = new uint8_t[max_batches]();
}

FP8BatchProcessor::~FP8BatchProcessor() {
    _aligned_free(aligned_buffer_);
    delete[] completion_flags_;
}

float* FP8BatchProcessor::AcquireInputBuffer() {
    if (IsFull()) {
        return nullptr;
    }
    
    // Calculate offset for current write position
    const size_t offset = write_index_ * kFloatsPerBatch;
    float* buffer = &aligned_buffer_[offset];
    
    // Mark as pending (not yet ready for quantization)
    completion_flags_[write_index_] = 0;
    
    // Advance write index
    write_index_ = (write_index_ + 1) % max_batches_;
    pending_count_++;
    
    return buffer;
}

bool FP8BatchProcessor::CommitAndQuantize(uint8_t* output_destination, float scale) {
    if (IsEmpty()) {
        return false;
    }
    
    // Mark current read position as ready
    completion_flags_[read_index_] = 1;
    
    // Get the input buffer
    const size_t offset = read_index_ * kFloatsPerBatch;
    float* input = &aligned_buffer_[offset];
    
    // Quantize using AVX2 batch kernel
    FP8AVX2Quantizer::QuantizeBatch64(input, output_destination, scale);
    
    // Advance read index
    read_index_ = (read_index_ + 1) % max_batches_;
    pending_count_--;
    
    return true;
}

size_t FP8BatchProcessor::FlushAll(uint8_t* output_base, float scale) {
    size_t processed = 0;
    
    while (!IsEmpty()) {
        // Calculate output offset
        uint8_t* output = output_base + (processed * kFloatsPerBatch);
        
        if (!CommitAndQuantize(output, scale)) {
            break;
        }
        
        processed++;
    }
    
    return processed;
}

} // namespace Kernels
} // namespace RawrXD
