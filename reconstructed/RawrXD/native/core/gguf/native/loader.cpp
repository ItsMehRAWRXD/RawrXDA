// native_core/gguf_native_loader.cpp
#include "gguf_native_loader.hpp"
#include <cstring>
#include <algorithm>

using namespace RawrXD::Native;

// Native Windows file ops—zero std::filesystem
HANDLE GGUFNativeLoader::OpenFileNative(const wchar_t* path) {
    return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
}

bool GGUFNativeLoader::Load(const wchar_t* filePath) {
    hFile_ = OpenFileNative(filePath);
    if (hFile_ == INVALID_HANDLE_VALUE) return false;

    // Memory-map for zero-copy parsing
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile_, &size)) return false;
    fileSize_ = size.QuadPart;

    hMapping_ = CreateFileMapping(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping_) return false;

    pView_ = MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0);
    if (!pView_) return false;

    return ParseHeader() && ParseTensors();
}

bool GGUFNativeLoader::ParseHeader() {
    uint8_t* ptr = static_cast<uint8_t*>(pView_);
    uint8_t* end = ptr + fileSize_;

    // Magic: "GGUF" = 0x46554747
    if (ptr + 4 > end) return false;
    std::memcpy(&header_.magic, ptr, 4);
    if (header_.magic != 0x46554747) return false;
    ptr += 4;

    // Version
    if (ptr + 4 > end) return false;
    std::memcpy(&header_.version, ptr, 4);
    if (header_.version > 3) return false; // Max supported version
    ptr += 4;

    // Tensor count (u64)
    if (ptr + 8 > end) return false;
    std::memcpy(&header_.tensorCount, ptr, 8);
    ptr += 8;

    // KV count (u64)
    if (ptr + 8 > end) return false;
    std::memcpy(&header_.kvCount, ptr, 8);
    ptr += 8;

    // Parse KV pairs (metadata)
    for (uint64_t i = 0; i < header_.kvCount && ptr < end; ++i) {
        // Key: string (u64 len + bytes)
        uint64_t keyLen = 0;
        std::memcpy(&keyLen, ptr, 8);
        ptr += 8;
        if (ptr + keyLen > end) return false;
        std::string key(reinterpret_cast<char*>(ptr), keyLen);
        ptr += keyLen;

        // Value type
        uint32_t type = 0;
        std::memcpy(&type, ptr, 4);
        ptr += 4;

        // Value (simplified—just store strings for now)
        if (type == static_cast<uint32_t>(GGUFType::STRING)) {
            uint64_t valLen = 0;
            std::memcpy(&valLen, ptr, 8);
            ptr += 8;
            if (ptr + valLen > end) return false;
            std::string val(reinterpret_cast<char*>(ptr), valLen);
            ptr += valLen;
            metadata_[key] = val;
        } else {
            // Skip other types for now (extend as needed)
            uint64_t skip = 8; // Default skip
            if (type <= 12) {
                static const uint8_t typeSizes[] = {1,1,2,2,4,4,4,1,0,0,8,8,8};
                skip = typeSizes[type];
            }
            ptr += skip;
        }
    }

    return true;
}

bool GGUFNativeLoader::ParseTensors() {
    uint8_t* ptr = static_cast<uint8_t*>(pView_);
    uint8_t* end = ptr + fileSize_;

    // Skip to tensor info (after KV pairs)
    // (Already advanced in ParseHeader)

    tensors_.reserve(header_.tensorCount);

    for (uint64_t i = 0; i < header_.tensorCount; ++i) {
        GGUFTensor tensor;

        // Name
        uint64_t nameLen = 0;
        std::memcpy(&nameLen, ptr, 8);
        ptr += 8;
        tensor.name.assign(reinterpret_cast<char*>(ptr), nameLen);
        ptr += nameLen;

        // NDims
        uint32_t nDims = 0;
        std::memcpy(&nDims, ptr, 4);
        ptr += 4;

        // Dimensions
        tensor.dims.resize(nDims);
        for (uint32_t d = 0; d < nDims; ++d) {
            std::memcpy(&tensor.dims[d], ptr, 8);
            ptr += 8;
        }

        // Type
        std::memcpy(&tensor.type, ptr, 4);
        ptr += 4;

        // Offset (into data blob)
        std::memcpy(&tensor.offset, ptr, 8);
        ptr += 8;

        // Calculate size from dims (simplified)
        tensor.size = 1;
        for (auto d : tensor.dims) tensor.size *= d;

        // Point to data (mapped file remains valid)
        if (tensor.offset < fileSize_) {
            // Defer actual data copy to GetTensor for lazy loading
        }

        tensors_.push_back(std::move(tensor));
    }

    return true;
}

void GGUFNativeLoader::Unload() {
    if (pView_) UnmapViewOfFile(pView_);
    if (hMapping_) CloseHandle(hMapping_);
    if (hFile_ != INVALID_HANDLE_VALUE) CloseHandle(hFile_);
    pView_ = nullptr;
    hMapping_ = nullptr;
    hFile_ = INVALID_HANDLE_VALUE;
}

const GGUFTensor* GGUFNativeLoader::GetTensor(const char* name) const {
    for (const auto& t : tensors_) {
        if (t.name == name) return &t;
    }
    return nullptr;
}