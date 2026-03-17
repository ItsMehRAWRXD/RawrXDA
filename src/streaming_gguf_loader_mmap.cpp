// =============================================================================
// streaming_gguf_loader_mmap.cpp — Implementation
// =============================================================================

#include "streaming_gguf_loader_mmap.h"
#include "../model_loader/GGUFConstants.hpp"
#include "../utils/Diagnostics.hpp"
#if defined(RAWR_HAS_ZLIB) && RAWR_HAS_ZLIB
#include <zlib.h>
#endif
#include <algorithm>
#include <iostream>

namespace RawrXD {

StreamingGGUFLoaderMMap::StreamingGGUFLoaderMMap()
    : file_handle_(INVALID_HANDLE_VALUE)
    , mapping_handle_(nullptr)
    , mapped_base_(nullptr)
    , mapped_size_(0)
    , loading_(false)
    , bytes_loaded_(0)
    , total_bytes_(0)
{
}

StreamingGGUFLoaderMMap::~StreamingGGUFLoaderMMap() {
    closeMemoryMapping();
}

// =============================================================================
// Async Model Loading
// =============================================================================

std::future<bool> StreamingGGUFLoaderMMap::loadModelStreamingAsync(
    const std::string& filepath,
    ProgressCallback progress)
{
    return std::async(std::launch::async, [this, filepath, progress]() {
        return loadModelStreaming(filepath, progress);
    });
}

bool StreamingGGUFLoaderMMap::loadModelStreaming(
    const std::string& filepath,
    ProgressCallback progress)
{
    if (loading_.exchange(true)) {
        std::cerr << "❌ Already loading a model" << std::endl;
        return false;
    }

    filepath_ = filepath;
    
    // Phase 1: Open file and create memory mapping
    if (progress) progress(0, 100, "Opening file");
    
    file_handle_ = CreateFileA(
        filepath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (file_handle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "❌ Failed to open file: " << filepath << " (Error: " << GetLastError() << ")" << std::endl;
        loading_.store(false);
        return false;
    }
    
    // Get file size
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle_, &file_size)) {
        std::cerr << "❌ Failed to get file size" << std::endl;
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
        loading_.store(false);
        return false;
    }
    
    mapped_size_ = file_size.QuadPart;
    total_bytes_.store(mapped_size_);
    
    if (progress) progress(10, 100, "Creating memory mapping");
    
    // Create file mapping
    mapping_handle_ = CreateFileMappingA(
        file_handle_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );
    
    if (!mapping_handle_) {
        std::cerr << "❌ Failed to create file mapping (Error: " << GetLastError() << ")" << std::endl;
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
        loading_.store(false);
        return false;
    }
    
    // Map view of file
    mapped_base_ = static_cast<uint8_t*>(MapViewOfFile(
        mapping_handle_,
        FILE_MAP_READ,
        0,
        0,
        0
    ));
    
    if (!mapped_base_) {
        std::cerr << "❌ Failed to map view of file (Error: " << GetLastError() << ")" << std::endl;
        CloseHandle(mapping_handle_);
        CloseHandle(file_handle_);
        mapping_handle_ = nullptr;
        file_handle_ = INVALID_HANDLE_VALUE;
        loading_.store(false);
        return false;
    }
    
    is_open_ = true;
    bytes_loaded_.store(mapped_size_);
    
    if (progress) progress(30, 100, "Parsing GGUF header");
    
    // Phase 2: Parse header using mmap
    if (!parseHeaderMMap()) {
        closeMemoryMapping();
        loading_.store(false);
        return false;
    }
    
    if (progress) progress(50, 100, "Parsing metadata");
    
    // Phase 3: Parse metadata
    if (!ParseMetadata()) {
        closeMemoryMapping();
        loading_.store(false);
        return false;
    }
    
    if (progress) progress(70, 100, "Building tensor index");
    
    // Phase 4: Build tensor index (no data loading, just metadata)
    if (!parseTensorsMMap()) {
        closeMemoryMapping();
        loading_.store(false);
        return false;
    }
    
    if (progress) progress(90, 100, "Assigning zones");
    
    // Phase 5: Assign tensors to zones
    AssignTensorsToZones();
    
    if (progress) progress(100, 100, "Complete");
    
    loading_.store(false);
    
    std::cout << "✅ GGUF Model loaded with memory mapping" << std::endl;
    std::cout << "   File: " << filepath << std::endl;
    std::cout << "   Size: " << (mapped_size_ / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "   Tensors: " << tensor_index_.size() << std::endl;
    std::cout << "   Zones: " << zones_.size() << std::endl;
    std::cout << "   Memory-mapped: " << (mapped_base_ ? "Yes" : "No") << std::endl;
    
    return true;
}

// =============================================================================
// Memory Mapping Management
// =============================================================================

bool StreamingGGUFLoaderMMap::createMemoryMapping() {
    // Already created in loadModelStreaming()
    return mapped_base_ != nullptr;
}

void StreamingGGUFLoaderMMap::closeMemoryMapping() {
    if (mapped_base_) {
        UnmapViewOfFile(mapped_base_);
        mapped_base_ = nullptr;
    }
    if (mapping_handle_) {
        CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
    if (file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
    is_open_ = false;
    mapped_size_ = 0;
}

// =============================================================================
// Memory-Mapped Header Parsing
// =============================================================================

bool StreamingGGUFLoaderMMap::parseHeaderMMap() {
    if (!mapped_base_ || mapped_size_ < sizeof(GGUFHeader)) {
        std::cerr << "❌ Invalid memory mapping for header parsing" << std::endl;
        return false;
    }
    
    uint64_t offset = 0;
    
    // Read magic
    if (offset + 4 > mapped_size_) return false;
    memcpy(&header_.magic, mapped_base_ + offset, 4);
    offset += 4;
    
    if (header_.magic != GGUFConstants::GGUF_MAGIC) {
        std::cerr << "❌ Invalid GGUF magic: 0x" << std::hex << header_.magic << std::dec << std::endl;
        Diagnostics::error("Invalid GGUF magic number", "StreamingGGUFLoaderMMap");
        return false;
    }
    
    // Read version
    if (offset + 4 > mapped_size_) return false;
    memcpy(&header_.version, mapped_base_ + offset, 4);
    offset += 4;
    
    if (header_.version != GGUFConstants::GGUF_VERSION && header_.version != 2) {
        std::cerr << "❌ Unsupported GGUF version: " << header_.version << std::endl;
        Diagnostics::error("Unsupported GGUF version: " + std::to_string(header_.version), "StreamingGGUFLoaderMMap");
        return false;
    }
    
    // Read tensor count
    if (offset + 8 > mapped_size_) return false;
    memcpy(&header_.tensor_count, mapped_base_ + offset, 8);
    offset += 8;
    
    // Read metadata KV count
    if (offset + 8 > mapped_size_) return false;
    memcpy(&header_.metadata_kv_count, mapped_base_ + offset, 8);
    offset += 8;
    
    std::cout << "✅ GGUF Header parsed:" << std::endl;
    std::cout << "   Magic: 0x" << std::hex << header_.magic << std::dec << std::endl;
    std::cout << "   Version: " << header_.version << std::endl;
    std::cout << "   Tensors: " << header_.tensor_count << std::endl;
    std::cout << "   Metadata KVs: " << header_.metadata_kv_count << std::endl;
    
    return true;
}

// =============================================================================
// Memory-Mapped Tensor Index Building
// =============================================================================

bool StreamingGGUFLoaderMMap::parseTensorsMMap() {
    // Start after header + metadata
    // For now, delegate to the base class method which uses file I/O
    // Could be optimized to use mmap directly, but this is safer
    return BuildTensorIndex();
}

// =============================================================================
// Zero-Copy Tensor Access
// =============================================================================

const uint8_t* StreamingGGUFLoaderMMap::getTensorMappedPtr(
    const std::string& tensor_name,
    uint64_t* out_size)
{
    auto it = tensor_index_.find(tensor_name);
    if (it == tensor_index_.end()) {
        std::cerr << "❌ Tensor not found: " << tensor_name << std::endl;
        return nullptr;
    }
    
    const TensorRef& ref = it->second;
    
    if (ref.offset >= mapped_size_) {
        std::cerr << "❌ Tensor offset out of bounds" << std::endl;
        return nullptr;
    }
    
    if (ref.offset + ref.size > mapped_size_) {
        std::cerr << "❌ Tensor size exceeds mapped region" << std::endl;
        return nullptr;
    }
    
    if (out_size) {
        *out_size = ref.size;
    }
    
    return mapped_base_ + ref.offset;
}

// =============================================================================
// Compression Detection and Decompression
// =============================================================================

bool StreamingGGUFLoaderMMap::isTensorCompressed(const TensorRef& ref) {
    // Check if tensor has DEFLATE header (0x78 0x9C for default compression)
    const uint8_t* data = getTensorMappedPtr(ref.name);
    if (!data || ref.size < 2) return false;
    
    return (data[0] == 0x78) && ((data[1] == 0x9C) || (data[1] == 0xDA) || (data[1] == 0x01));
}

bool StreamingGGUFLoaderMMap::decompressDeflate(
    const uint8_t* compressed,
    uint64_t comp_size,
    std::vector<uint8_t>& out_data,
    uint64_t expected_size)
{
    // Allocate output buffer
    out_data.resize(expected_size);
    
    // Setup zlib stream
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(compressed);
    stream.avail_in = static_cast<uInt>(comp_size);
    stream.next_out = out_data.data();
    stream.avail_out = static_cast<uInt>(expected_size);
    
    // Initialize inflate
    if (inflateInit(&stream) != Z_OK) {
        std::cerr << "❌ Failed to initialize zlib inflate" << std::endl;
        return false;
    }
    
    // Decompress
    int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    
    if (ret != Z_STREAM_END) {
        std::cerr << "❌ DEFLATE decompression failed: " << ret << std::endl;
        return false;
    }
    
    // Resize to actual decompressed size
    out_data.resize(stream.total_out);
    
    return true;
}

bool StreamingGGUFLoaderMMap::decompressTensor(
    const std::string& tensor_name,
    std::vector<uint8_t>& out_data)
{
    auto it = tensor_index_.find(tensor_name);
    if (it == tensor_index_.end()) {
        return false;
    }
    
    const TensorRef& ref = it->second;
    
    // Check if compressed
    if (!isTensorCompressed(ref)) {
        // Not compressed, just copy
        uint64_t size = 0;
        const uint8_t* data = getTensorMappedPtr(tensor_name, &size);
        if (!data) return false;
        
        out_data.assign(data, data + size);
        return true;
    }
    
    // Decompress
    const uint8_t* compressed = getTensorMappedPtr(tensor_name);
    if (!compressed) return false;
    
    // Expected size is stored in tensor metadata (approximation)
    uint64_t expected_size = ref.size * 2;  // Assume 2x compression ratio
    
    return decompressDeflate(compressed, ref.size, out_data, expected_size);
}

// =============================================================================
// Async Zone Streaming
// =============================================================================

bool StreamingGGUFLoaderMMap::streamZoneAsync(
    const std::string& zone_name,
    std::function<void(float)> progress)
{
    auto zone_it = zones_.find(zone_name);
    if (zone_it == zones_.end()) {
        std::cerr << "❌ Zone not found: " << zone_name << std::endl;
        return false;
    }
    
    TensorZoneInfo& zone = zone_it->second;
    
    if (zone.is_loaded) {
        if (progress) progress(1.0f);
        return true;
    }
    
    // Stream using memory-mapped access (zero-copy)
    zone.data.clear();
    zone.data.reserve(zone.total_bytes);
    
    size_t loaded = 0;
    
    for (size_t i = 0; i < zone.tensors.size(); ++i) {
        const auto& tensor_name = zone.tensors[i];
        
        uint64_t size = 0;
        const uint8_t* data = getTensorMappedPtr(tensor_name, &size);
        
        if (!data) {
            std::cerr << "❌ Failed to get tensor: " << tensor_name << std::endl;
            return false;
        }
        
        // Copy from mmap to zone buffer
        size_t prev_size = zone.data.size();
        zone.data.resize(prev_size + size);
        memcpy(zone.data.data() + prev_size, data, size);
        
        loaded += size;
        
        if (progress) {
            progress(static_cast<float>(loaded) / static_cast<float>(zone.total_bytes));
        }
    }
    
    zone.is_loaded = true;
    active_zones_[zone_name] = true;
    
    std::cout << "✅ Zone streamed: " << zone_name 
              << " (" << (loaded / (1024.0 * 1024.0)) << " MB)" << std::endl;
    
    return true;
}

} // namespace RawrXD
