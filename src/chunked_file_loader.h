// ============================================================================
// chunked_file_loader.h — Chunked File I/O for Files >2GB
// Solves the PowerShell/.NET 2GB ReadAllBytes limit
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace RawrXD {

// ============================================================================
// ChunkedFileLoader — Handles files of any size (no 2GB limit)
// Uses Windows file mapping for zero-copy access
// ============================================================================

class ChunkedFileLoader {
public:
    // Callback for chunk processing
    using ChunkCallback = std::function<bool(const void* data, uint64_t size, uint64_t offset)>;
    
    ChunkedFileLoader();
    ~ChunkedFileLoader();
    
    // Open file (handles files >2GB)
    bool Open(const std::string& path);
    bool Open(const std::wstring& path);
    
    // Close file
    void Close();
    
    // Get file size (handles >4GB)
    uint64_t GetSize() const { return m_fileSize; }
    
    // Read chunk into buffer (no size limit)
    bool ReadChunk(uint64_t offset, uint64_t size, void* buffer);
    
    // Read entire file in chunks (processes files >2GB)
    bool ReadAllChunks(ChunkCallback callback, uint64_t chunkSize = 64 * 1024 * 1024);
    
    // Memory-map a region (zero-copy)
    void* MapRegion(uint64_t offset, uint64_t size);
    void UnmapRegion(void* ptr);
    
    // Get last error
    std::string GetLastError() const { return m_lastError; }
    
    // Check if file is open
    bool IsOpen() const { return m_handle != INVALID_HANDLE_VALUE; }
    
    // Static helper: Check if file is GGUF
    static bool IsGGUF(const std::string& path);
    static bool IsGGUF(const std::wstring& path);
    
    // Static helper: Get file size without opening
    static uint64_t GetFileSize(const std::string& path);
    static uint64_t GetFileSize(const std::wstring& path);

private:
    void* m_handle;         // HANDLE (void* for header compatibility)
    void* m_mapping;        // HANDLE for file mapping
    uint64_t m_fileSize;
    std::string m_lastError;
    std::wstring m_path;
};

// ============================================================================
// GGUFChunkedLoader — GGUF-specific chunked loader
// Parses GGUF header and metadata without loading entire file
// ============================================================================

struct GGUFHeader {
    uint32_t magic;           // "GGUF"
    uint32_t version;         // 3 or 4
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
};

struct GGUFTensorInfo {
    std::string name;
    std::vector<uint64_t> shape;
    uint32_t type;
    uint64_t offset;
    uint64_t size;
};

class GGUFChunkedLoader {
public:
    GGUFChunkedLoader();
    ~GGUFChunkedLoader();
    
    // Load GGUF file (handles files >2GB)
    bool Load(const std::string& path);
    bool Load(const std::wstring& path);
    
    // Get header info
    const GGUFHeader& GetHeader() const { return m_header; }
    
    // Get metadata
    std::string GetMetadata(const std::string& key) const;
    std::map<std::string, std::string> GetAllMetadata() const { return m_metadata; }
    
    // Get tensor info
    std::vector<std::string> GetTensorNames() const;
    const GGUFTensorInfo* GetTensorInfo(const std::string& name) const;
    
    // Load tensor data (memory-mapped, zero-copy)
    bool LoadTensor(const std::string& name, void** data, uint64_t* size);
    void UnloadTensor(void* data);
    
    // Get tensor data offset in file
    uint64_t GetTensorDataOffset() const { return m_tensorDataOffset; }
    
    // Get file size
    uint64_t GetFileSize() const { return m_fileSize; }
    
    // Get last error
    std::string GetLastError() const { return m_lastError; }
    
    // Validate GGUF file
    bool IsValid() const { return m_valid; }

private:
    bool ParseHeader();
    bool ParseMetadata();
    bool ParseTensorInfo();
    uint64_t AlignOffset(uint64_t offset, uint64_t alignment);
    uint32_t GetTypeSize(uint32_t type);
    
    ChunkedFileLoader m_loader;
    GGUFHeader m_header = {};
    std::map<std::string, std::string> m_metadata;
    std::map<std::string, GGUFTensorInfo> m_tensors;
    uint64_t m_tensorDataOffset = 0;
    uint64_t m_fileSize = 0;
    uint64_t m_currentOffset = 0;
    bool m_valid = false;
    std::string m_lastError;
};

// ============================================================================
// InferenceChunkedLoader — Loads model for inference (handles >2GB)
// ============================================================================

class InferenceChunkedLoader {
public:
    InferenceChunkedLoader();
    ~InferenceChunkedLoader();
    
    // Load model for inference
    bool LoadModel(const std::string& path);
    bool LoadModel(const std::wstring& path);
    
    // Get model info
    std::string GetModelName() const;
    std::string GetModelType() const;
    uint64_t GetParameterCount() const;
    uint64_t GetContextLength() const;
    uint32_t GetLayerCount() const;
    uint32_t GetHeadCount() const;
    uint32_t GetEmbeddingDim() const;
    
    // Get tensor for inference
    bool GetTensor(const std::string& name, void** data, uint64_t* size);
    
    // Validate model is loaded
    bool IsLoaded() const { return m_loaded; }
    
    // Get last error
    std::string GetLastError() const { return m_lastError; }

private:
    std::unique_ptr<GGUFChunkedLoader> m_loader;
    bool m_loaded = false;
    std::string m_lastError;
};

} // namespace RawrXD
