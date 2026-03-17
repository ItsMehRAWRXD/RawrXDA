// native_core/gguf_native_loader.hpp
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

// Zero external deps—pure C++20 + Win32
namespace RawrXD::Native {

enum class GGUFType : uint32_t {
    UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3, UINT32 = 4,
    INT32 = 5, FLOAT32 = 6, BOOL = 7, STRING = 8,
    ARRAY = 9, UINT64 = 10, INT64 = 11, FLOAT64 = 12
};

struct GGUFTensor {
    std::string name;
    std::vector<uint64_t> dims;
    uint64_t offset;           // Offset in file
    uint64_t size;             // Raw size on disk
    uint32_t type;             // Quantization type (e.g., 2=Q4_0, 3=Q8_0)
    std::vector<uint8_t> data; // Dequantized or raw
};

class GGUFNativeLoader {
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = nullptr;
    void* pView_ = nullptr;
    size_t fileSize_ = 0;

    struct Header {
        uint32_t magic = 0;
        uint32_t version = 0;
        uint64_t tensorCount = 0;
        uint64_t kvCount = 0;
    } header_;

    std::unordered_map<std::string, std::string> metadata_;
    std::vector<GGUFTensor> tensors_;

    // Native file I/O—no std::filesystem
    static HANDLE OpenFileNative(const wchar_t* path);
    bool ParseHeader();
    bool ParseTensors();

public:
    GGUFNativeLoader() = default;
    ~GGUFNativeLoader() { Unload(); }

    // Delete copy—RAII only
    GGUFNativeLoader(const GGUFNativeLoader&) = delete;
    GGUFNativeLoader& operator=(const GGUFNativeLoader&) = delete;

    bool Load(const wchar_t* filePath);
    void Unload();

    const GGUFTensor* GetTensor(const char* name) const;
    size_t GetTensorCount() const { return tensors_.size(); }
    const auto& GetMetadata() const { return metadata_; }
};

} // namespace RawrXD::Native