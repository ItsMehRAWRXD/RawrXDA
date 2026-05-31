// src/streaming_gguf_loader_enhanced_v1_1.h
#pragma once
#include "streaming_gguf_loader_enhanced.h"
#include "io/backend_interface.hpp"

namespace RawrXD {

class EnhancedLoaderV1_1 : public EnhancedStreamingGGUFLoader {
public:
    EnhancedLoaderV1_1();
    virtual ~EnhancedLoaderV1_1();

    // v1.1.0 Initializer: Sets up the Direct I/O Bypass
    bool InitializeIORing(const std::string& filepath);
    
    // Override: Use IORing for zone loads (DMA Direct)
    std::span<const std::byte> GetTensorViewIORing(const std::string& tensor_name);
    
    // Batch prefetch for sequential patterns
    void PrefetchZoneBatch(const std::vector<uint32_t>& zone_ids);

    // Pump the I/O completion queue
    void PollIO();

private:
    IDirectIOBackend* io_backend_ = nullptr;
    bool use_ioring_ = false;
    void* ring_buffer_ptr_ = nullptr;
    size_t ring_buffer_size_ = 0;
};

} // namespace RawrXD
