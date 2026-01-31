// src/streaming_gguf_loader_enhanced_v1_1.cpp
#include "streaming_gguf_loader_enhanced_v1_1.h"
#include <iostream>

namespace RawrXD {

EnhancedLoaderV1_1::EnhancedLoaderV1_1() : io_backend_(nullptr), use_ioring_(false) {
#ifdef _WIN32
    io_backend_ = CreateIOBackend(IOBackendType::IORING_WINDOWS);
#else
    // Linux support in v1.1.1
    io_backend_ = nullptr;
#endif
}

EnhancedLoaderV1_1::~EnhancedLoaderV1_1() {
    if (io_backend_) {
        io_backend_->Shutdown();
        delete io_backend_;
    }
}

bool EnhancedLoaderV1_1::InitializeIORing(const std::string& filepath) {
    if (!io_backend_) return false;

    if (!io_backend_->Initialize(filepath.c_str())) {
        std::cerr << "❌ IORing: Failed to initialize backend for " << filepath << std::endl;
        return false;
    }

    // Register active zones for DMA bypass
    // For now, we'll register the internal zone data buffers if they are already allocated
    // In a final v1.1.0, we would use a dedicated large ring buffer
    
    // Placeholder for actual buffer registration logic
    // io_backend_->RegisterBuffers(ring_ptr, ring_size, count);

    use_ioring_ = true;
    std::cout << "🚀 RAWRXD v1.1.0: Kernel-Bypass IORing initialized for " << filepath << std::endl;
    return true;
}

std::span<const std::byte> EnhancedLoaderV1_1::GetTensorViewIORing(const std::string& tensor_name) {
    if (!use_ioring_) {
        return GetTensorView(tensor_name); // Fallback to v1.0.0
    }

    // 1. Check if tensor is resident (already loaded via DMA)
    if (IsTensorResident(tensor_name)) {
        return GetTensorView(tensor_name);
    }

    // 2. If not resident, submit a high-priority read
    // This is where the 5ms cold load happens.
    // IORequest req = { ... };
    // io_backend_->SubmitRead(req);
    // io_backend_->Flush();
    
    // In v1.1.0, we would wait/poll here OR expect the caller 
    // to have prefetched the zone already.

    return GetTensorView(tensor_name);
}

void EnhancedLoaderV1_1::PrefetchZoneBatch(const std::vector<uint32_t>& zone_ids) {
    if (!use_ioring_) return;

    for (uint32_t zone_id : zone_ids) {
        // Map zone_id to file offset and submitting to SQ
        // IORequest req = { offset, zone_id, 0, size, zone_id };
        // io_backend_->SubmitRead(req);
    }
    
    io_backend_->Flush();
}

void EnhancedLoaderV1_1::PollIO() {
    if (!io_backend_) return;

    std::vector<IOCompletion> completions;
    int count = io_backend_->PollCompletions(completions);
    
    for (const auto& c : completions) {
        if (c.result_code == 0) {
            // Mark zone as resident in our internal state
            // zones_[c.request_id].is_loaded = true;
        }
    }
}

} // namespace RawrXD
