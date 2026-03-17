// native/gguf_native_loader.cpp
#include "gguf_native_loader.hpp"
#include <cstring>
#include <algorithm>

namespace RawrXD::Native {

bool NativeGGUFLoader::load(const wchar_t* path) {
    // Native CreateFileW - no std::ifstream overhead
    hFile_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile_ == INVALID_HANDLE_VALUE) return false;
    
    // Get file size
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile_, &size)) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
        return false;
    }
    file_size_ = static_cast<size_t>(size.QuadPart);
    
    // Create file mapping
    hMapping_ = CreateFileMapping(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping_) {
        unload();
        return false;
    }
    
    // Map view
    mapped_view_ = MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0);
    if (!mapped_view_) {
        unload();
        return false;
    }
    
    return parseHeader() && parseMetadata() && parseTensors();
}

void NativeGGUFLoader::unload() {
    if (mapped_view_) {
        UnmapViewOfFile(mapped_view_);
        mapped_view_ = nullptr;
    }
    if (hMapping_) {
        CloseHandle(hMapping_);
        hMapping_ = nullptr;
    }
    if (hFile_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
    }
    metadata_.clear();
    tensors_.clear();
    file_size_ = 0;
}

bool NativeGGUFLoader::parseHeader() {
    size_t offset = 0;
    
    // Check magic
    uint32_t magic = readAt<uint32_t>(offset);
    if (magic != GGUF_MAGIC) return false;
    offset += 4;
    
    // Version
    uint32_t version = readAt<uint32_t>(offset);
    if (version != GGUF_VERSION) return false; // Only support v3
    offset += 4;
    
    // Tensor count
    uint64_t n_tensors = readAt<uint64_t>(offset);
    offset += 8;
    tensors_.reserve(static_cast<size_t>(n_tensors));
    
    // Metadata count
    uint64_t n_metadata = readAt<uint64_t>(offset);
    offset += 8;
    metadata_.reserve(static_cast<size_t>(n_metadata));
    
    // Alignment (usually 32)
    alignment_ = 32;
    
    return true;
}

bool NativeGGUFLoader::parseMetadata() {
    size_t offset = 24; // After header
    
    uint64_t n_metadata = readAt<uint64_t>(16);
    
    for (uint64_t i = 0; i < n_metadata; ++i) {
        GGUFMetadata meta;
        
        // Read key string
        uint64_t key_len = readAt<uint64_t>(offset);
        offset += 8;
        meta.key.length = key_len;
        meta.key.data.resize(static_cast<size_t>(key_len));
        std::memcpy(meta.key.data.data(), ptrAt(offset), key_len);
        offset += key_len;
        
        // Read type
        meta.type = static_cast<GGUFType>(readAt<uint32_t>(offset));
        offset += 4;
        
        // Read value based on type
        switch (meta.type) {
            case GGUFType::UINT8: meta.value.u8 = readAt<uint8_t>(offset); offset += 1; break;
            case GGUFType::INT8: meta.value.i8 = readAt<int8_t>(offset); offset += 1; break;
            case GGUFType::UINT16: meta.value.u16 = readAt<uint16_t>(offset); offset += 2; break;
            case GGUFType::INT16: meta.value.i16 = readAt<int16_t>(offset); offset += 2; break;
            case GGUFType::UINT32: meta.value.u32 = readAt<uint32_t>(offset); offset += 4; break;
            case GGUFType::INT32: meta.value.i32 = readAt<int32_t>(offset); offset += 4; break;
            case GGUFType::FLOAT32: meta.value.f32 = readAt<float>(offset); offset += 4; break;
            case GGUFType::BOOL: meta.value.b = readAt<uint8_t>(offset) != 0; offset += 1; break;
            case GGUFType::UINT64: meta.value.u64 = readAt<uint64_t>(offset); offset += 8; break;
            case GGUFType::INT64: meta.value.i64 = readAt<int64_t>(offset); offset += 8; break;
            case GGUFType::FLOAT64: meta.value.f64 = readAt<double>(offset); offset += 8; break;
            
            case GGUFType::STRING: {
                uint64_t len = readAt<uint64_t>(offset);
                offset += 8;
                meta.str.length = len;
                meta.str.data.resize(static_cast<size_t>(len));
                std::memcpy(meta.str.data.data(), ptrAt(offset), len);
                offset += len;
                break;
            }
            
            case GGUFType::ARRAY: {
                meta.arr.type = static_cast<GGUFType>(readAt<uint32_t>(offset));
                offset += 4;
                uint64_t size = readAt<uint64_t>(offset);
                offset += 8;
                meta.arr.size = size;
                // Skip array data for now (not needed for inference)
                size_t type_size = getTypeSize(meta.arr.type);
                offset += size * type_size;
                break;
            }
        }
        
        metadata_.push_back(std::move(meta));
    }
    
    return true;
}

bool NativeGGUFLoader::parseTensors() {
    // Calculate offset to tensor data
    size_t offset = 24; // Header
    
    // Skip metadata
    uint64_t n_metadata = readAt<uint64_t>(16);
    for (uint64_t i = 0; i < n_metadata; ++i) {
        uint64_t key_len = readAt<uint64_t>(offset);
        offset += 8 + key_len;
        GGUFType type = static_cast<GGUFType>(readAt<uint32_t>(offset));
        offset += 4;
        
        // Skip value
        switch (type) {
            case GGUFType::UINT8: case GGUFType::INT8: offset += 1; break;
            case GGUFType::UINT16: case GGUFType::INT16: offset += 2; break;
            case GGUFType::UINT32: case GGUFType::INT32: case GGUFType::FLOAT32: offset += 4; break;
            case GGUFType::UINT64: case GGUFType::INT64: case GGUFType::FLOAT64: offset += 8; break;
            case GGUFType::BOOL: offset += 1; break;
            case GGUFType::STRING: {
                uint64_t len = readAt<uint64_t>(offset);
                offset += 8 + len;
                break;
            }
            case GGUFType::ARRAY: {
                offset += 4; // type
                uint64_t size = readAt<uint64_t>(offset);
                offset += 8;
                GGUFType arr_type = static_cast<GGUFType>(readAt<uint32_t>(offset - 12));
                offset += size * getTypeSize(arr_type);
                break;
            }
        }
    }
    
    // Parse tensor infos
    uint64_t n_tensors = readAt<uint64_t>(8);
    for (uint64_t i = 0; i < n_tensors; ++i) {
        GGUFTensorInfo info;
        
        // Name
        uint64_t name_len = readAt<uint64_t>(offset);
        offset += 8;
        info.name.length = name_len;
        info.name.data.resize(static_cast<size_t>(name_len));
        std::memcpy(info.name.data.data(), ptrAt(offset), name_len);
        offset += name_len;
        
        // Dimensions
        uint32_t n_dims = readAt<uint32_t>(offset);
        offset += 4;
        info.n_dims = n_dims;
        info.dims.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; ++d) {
            info.dims[d] = readAt<uint64_t>(offset);
            offset += 8;
        }
        
        // Type
        info.type = static_cast<GGUFType>(readAt<uint32_t>(offset));
        offset += 4;
        
        // Offset
        info.offset = readAt<uint64_t>(offset);
        offset += 8;
        
        // Calculate sizes
        info.element_size = getTypeSize(info.type);
        info.size_bytes = calcTensorSize(info);
        
        tensors_.push_back(std::move(info));
    }
    
    // Tensor data starts at aligned offset
    data_.offset = (offset + alignment_ - 1) & ~(alignment_ - 1);
    
    return true;
}

const GGUFTensorInfo* NativeGGUFLoader::findTensor(const char* name) const {
    for (const auto& t : tensors_) {
        if (t.name.view() == name) return &t;
    }
    return nullptr;
}

const void* NativeGGUFLoader::getTensorData(const GGUFTensorInfo& info) const {
    if (!mapped_view_) return nullptr;
    return static_cast<const uint8_t*>(mapped_view_) + data_.offset + info.offset;
}

size_t NativeGGUFLoader::calcTensorSize(const GGUFTensorInfo& info) const {
    size_t n_elements = 1;
    for (auto d : info.dims) n_elements *= d;
    return n_elements * info.element_size;
}

size_t NativeGGUFLoader::getTypeSize(GGUFType type) {
    switch (type) {
        case GGUFType::UINT8: case GGUFType::INT8: return 1;
        case GGUFType::UINT16: case GGUFType::INT16: return 2;
        case GGUFType::UINT32: case GGUFType::INT32: case GGUFType::FLOAT32: return 4;
        case GGUFType::UINT64: case GGUFType::INT64: case GGUFType::FLOAT64: return 8;
        case GGUFType::BOOL: return 1;
        default: return 0;
    }
}

template<>
uint32_t NativeGGUFLoader::getMetadata<uint32_t>(const char* key, uint32_t default_val) const {
    for (const auto& m : metadata_) {
        if (m.key.view() == key && m.type == GGUFType::UINT32) {
            return m.value.u32;
        }
    }
    return default_val;
}

template<>
std::string NativeGGUFLoader::getMetadata<std::string>(const char* key, std::string default_val) const {
    for (const auto& m : metadata_) {
        if (m.key.view() == key && m.type == GGUFType::STRING) {
            return std::string(m.str.view());
        }
    }
    return default_val;
}

} // namespace RawrXD::Native