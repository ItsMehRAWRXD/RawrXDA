// native/gguf_native_loader.hpp
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>

namespace RawrXD::Native {

// GGUF spec v3 constants
constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF"
constexpr uint32_t GGUF_VERSION = 3;

enum class GGUFType : uint32_t {
    UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3,
    UINT32 = 4, INT32 = 5, FLOAT32 = 6, BOOL = 7,
    STRING = 8, ARRAY = 9, UINT64 = 10, INT64 = 11,
    FLOAT64 = 12
};

struct GGUFString {
    uint64_t length;
    std::vector<char> data;
    
    std::string_view view() const { 
        return std::string_view(data.data(), length); 
    }
};

struct GGUFArray {
    GGUFType type;
    uint64_t size;
    std::vector<uint8_t> data;
};

struct GGUFMetadata {
    GGUFString key;
    GGUFType type;
    union {
        uint8_t u8; int8_t i8; uint16_t u16; int16_t i16;
        uint32_t u32; int32_t i32; float f32; bool b;
        uint64_t u64; int64_t i64; double f64;
    } value;
    GGUFString str;
    GGUFArray arr;
};

struct GGUFTensorInfo {
    GGUFString name;
    uint32_t n_dims;
    std::vector<uint64_t> dims;
    GGUFType type;
    uint64_t offset;
    
    // Calculated
    size_t size_bytes;
    size_t element_size;
};

struct GGUFData {
    uint64_t offset;  // Where tensor data starts
    std::vector<uint8_t> raw_data; // If loaded into RAM
    HANDLE hMapping; // If memory-mapped
    HANDLE hFile;
    void* mapped_ptr;
    size_t mapped_size;
    
    GGUFData() : offset(0), hMapping(nullptr), hFile(INVALID_HANDLE_VALUE), 
                 mapped_ptr(nullptr), mapped_size(0) {}
    
    ~GGUFData() {
        if (mapped_ptr) UnmapViewOfFile(mapped_ptr);
        if (hMapping) CloseHandle(hMapping);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    }
};

class NativeGGUFLoader {
    std::vector<GGUFMetadata> metadata_;
    std::vector<GGUFTensorInfo> tensors_;
    GGUFData data_;
    uint64_t alignment_;
    
    // Native file I/O (no std::fstream for large files)
    HANDLE hFile_;
    HANDLE hMapping_;
    void* mapped_view_;
    size_t file_size_;
    
public:
    NativeGGUFLoader() : alignment_(32), hFile_(INVALID_HANDLE_VALUE), 
                         hMapping_(nullptr), mapped_view_(nullptr), file_size_(0) {}
    
    ~NativeGGUFLoader() { unload(); }
    
    // Load via memory mapping (zero-copy for tensors)
    bool load(const wchar_t* path);
    void unload();
    
    // Tensor access
    size_t tensorCount() const { return tensors_.size(); }
    const GGUFTensorInfo& getTensor(size_t idx) const { return tensors_[idx]; }
    
    // Find tensor by name
    const GGUFTensorInfo* findTensor(const char* name) const;
    
    // Get tensor data pointer (via memory map)
    const void* getTensorData(const GGUFTensorInfo& info) const;
    
    // Metadata access
    template<typename T>
    T getMetadata(const char* key, T default_val) const;
    
    // Quantization helpers
    size_t calcTensorSize(const GGUFTensorInfo& info) const;
    static size_t getTypeSize(GGUFType type);
    
private:
    bool parseHeader();
    bool parseMetadata();
    bool parseTensors();
    
    // Native byte reading from mapped memory
    template<typename T>
    T readAt(size_t offset) const {
        return *reinterpret_cast<const T*>(
            static_cast<const uint8_t*>(mapped_view_) + offset);
    }
    
    const uint8_t* ptrAt(size_t offset) const {
        return static_cast<const uint8_t*>(mapped_view_) + offset;
    }
};

} // namespace RawrXD::Native