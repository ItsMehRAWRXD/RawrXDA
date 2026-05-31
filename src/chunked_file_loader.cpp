// ============================================================================
// chunked_file_loader.cpp — Chunked File I/O Implementation
// Handles files >2GB using Windows file mapping
// ============================================================================

#include "chunked_file_loader.h"
#include <windows.h>
#include <memory>
#include <cstring>

namespace RawrXD {

// ============================================================================
// ChunkedFileLoader Implementation
// ============================================================================

ChunkedFileLoader::ChunkedFileLoader()
    : m_handle(INVALID_HANDLE_VALUE)
    , m_mapping(nullptr)
    , m_fileSize(0) {
}

ChunkedFileLoader::~ChunkedFileLoader() {
    Close();
}

bool ChunkedFileLoader::Open(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());
    return Open(wpath);
}

bool ChunkedFileLoader::Open(const std::wstring& path) {
    Close();
    
    m_path = path;
    
    // Open file with FILE_FLAG_SEQUENTIAL_SCAN for large files
    m_handle = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (m_handle == INVALID_HANDLE_VALUE) {
        m_lastError = "Failed to open file: " + std::to_string(GetLastError());
        return false;
    }
    
    // Get file size (handles >4GB)
    LARGE_INTEGER size;
    if (!GetFileSizeEx(m_handle, &size)) {
        m_lastError = "Failed to get file size: " + std::to_string(GetLastError());
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    m_fileSize = static_cast<uint64_t>(size.QuadPart);
    return true;
}

void ChunkedFileLoader::Close() {
    if (m_mapping) {
        CloseHandle(m_mapping);
        m_mapping = nullptr;
    }
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
    m_fileSize = 0;
}

bool ChunkedFileLoader::ReadChunk(uint64_t offset, uint64_t size, void* buffer) {
    if (m_handle == INVALID_HANDLE_VALUE) {
        m_lastError = "File not open";
        return false;
    }
    
    if (offset + size > m_fileSize) {
        m_lastError = "Read beyond file end";
        return false;
    }
    
    // Set file pointer (handles >4GB offsets)
    LARGE_INTEGER offsetLI;
    offsetLI.QuadPart = static_cast<LONGLONG>(offset);
    
    if (!SetFilePointerEx(m_handle, offsetLI, nullptr, FILE_BEGIN)) {
        m_lastError = "Failed to set file pointer: " + std::to_string(GetLastError());
        return false;
    }
    
    // Read in chunks (handles >4GB reads)
    uint8_t* ptr = static_cast<uint8_t*>(buffer);
    uint64_t remaining = size;
    
    while (remaining > 0) {
        DWORD toRead = static_cast<DWORD>(std::min(remaining, static_cast<uint64_t>(UINT32_MAX)));
        DWORD bytesRead = 0;
        
        if (!ReadFile(m_handle, ptr, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            m_lastError = "Failed to read file: " + std::to_string(GetLastError());
            return false;
        }
        
        ptr += bytesRead;
        remaining -= bytesRead;
    }
    
    return true;
}

bool ChunkedFileLoader::ReadAllChunks(ChunkCallback callback, uint64_t chunkSize) {
    if (m_handle == INVALID_HANDLE_VALUE) {
        m_lastError = "File not open";
        return false;
    }
    
    std::vector<uint8_t> buffer(chunkSize);
    uint64_t offset = 0;
    
    while (offset < m_fileSize) {
        uint64_t toRead = std::min(chunkSize, m_fileSize - offset);
        
        if (!ReadChunk(offset, toRead, buffer.data())) {
            return false;
        }
        
        if (!callback(buffer.data(), toRead, offset)) {
            m_lastError = "Callback returned false";
            return false;
        }
        
        offset += toRead;
    }
    
    return true;
}

void* ChunkedFileLoader::MapRegion(uint64_t offset, uint64_t size) {
    if (m_handle == INVALID_HANDLE_VALUE) {
        m_lastError = "File not open";
        return nullptr;
    }
    
    // Create file mapping if not exists
    if (!m_mapping) {
        m_mapping = CreateFileMappingW(
            m_handle,
            nullptr,
            PAGE_READONLY,
            static_cast<DWORD>(m_fileSize >> 32),
            static_cast<DWORD>(m_fileSize & 0xFFFFFFFF),
            nullptr
        );
        
        if (!m_mapping) {
            m_lastError = "Failed to create file mapping: " + std::to_string(GetLastError());
            return nullptr;
        }
    }
    
    // Map view of file
    void* view = MapViewOfFile(
        m_mapping,
        FILE_MAP_READ,
        static_cast<DWORD>(offset >> 32),
        static_cast<DWORD>(offset & 0xFFFFFFFF),
        static_cast<SIZE_T>(size)
    );
    
    if (!view) {
        m_lastError = "Failed to map view: " + std::to_string(GetLastError());
        return nullptr;
    }
    
    return view;
}

void ChunkedFileLoader::UnmapRegion(void* ptr) {
    if (ptr) {
        UnmapViewOfFile(ptr);
    }
}

bool ChunkedFileLoader::IsGGUF(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());
    return IsGGUF(wpath);
}

bool ChunkedFileLoader::IsGGUF(const std::wstring& path) {
    ChunkedFileLoader loader;
    if (!loader.Open(path)) {
        return false;
    }
    
    // Read magic
    uint8_t magic[4];
    if (!loader.ReadChunk(0, 4, magic)) {
        return false;
    }
    
    return magic[0] == 'G' && magic[1] == 'G' && magic[2] == 'U' && magic[3] == 'F';
}

uint64_t ChunkedFileLoader::GetFileSize(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());
    return GetFileSize(wpath);
}

uint64_t ChunkedFileLoader::GetFileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    
    return (static_cast<uint64_t>(attrs.nFileSizeHigh) << 32) | attrs.nFileSizeLow;
}

// ============================================================================
// GGUFChunkedLoader Implementation
// ============================================================================

GGUFChunkedLoader::GGUFChunkedLoader() = default;
GGUFChunkedLoader::~GGUFChunkedLoader() = default;

bool GGUFChunkedLoader::Load(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());
    return Load(wpath);
}

bool GGUFChunkedLoader::Load(const std::wstring& path) {
    m_valid = false;
    m_metadata.clear();
    m_tensors.clear();
    
    if (!m_loader.Open(path)) {
        m_lastError = m_loader.GetLastError();
        return false;
    }
    
    m_fileSize = m_loader.GetSize();
    
    if (!ParseHeader()) {
        return false;
    }
    
    if (!ParseMetadata()) {
        return false;
    }
    
    if (!ParseTensorInfo()) {
        return false;
    }
    
    m_valid = true;
    return true;
}

bool GGUFChunkedLoader::ParseHeader() {
    // Read header (24 bytes: magic + version + tensor_count + metadata_kv_count)
    uint8_t headerBuf[24];
    if (!m_loader.ReadChunk(0, 24, headerBuf)) {
        m_lastError = "Failed to read GGUF header";
        return false;
    }
    
    // Verify magic
    if (headerBuf[0] != 'G' || headerBuf[1] != 'G' || headerBuf[2] != 'U' || headerBuf[3] != 'F') {
        m_lastError = "Invalid GGUF magic number";
        return false;
    }
    
    memcpy(&m_header.magic, headerBuf + 0, 4);
    memcpy(&m_header.version, headerBuf + 4, 4);
    memcpy(&m_header.tensor_count, headerBuf + 8, 8);
    memcpy(&m_header.metadata_kv_count, headerBuf + 16, 8);
    
    // Validate version (support v3 and v4)
    if (m_header.version < 3 || m_header.version > 4) {
        m_lastError = "Unsupported GGUF version: " + std::to_string(m_header.version);
        return false;
    }
    
    m_currentOffset = 24;
    return true;
}

bool GGUFChunkedLoader::ParseMetadata() {
    for (uint64_t i = 0; i < m_header.metadata_kv_count; i++) {
        // Read key length
        uint8_t keyLenBuf[4];
        if (!m_loader.ReadChunk(m_currentOffset, 4, keyLenBuf)) {
            m_lastError = "Failed to read metadata key length";
            return false;
        }
        uint32_t keyLen;
        memcpy(&keyLen, keyLenBuf, 4);
        m_currentOffset += 4;
        
        // Read key
        std::vector<char> keyBuf(keyLen + 1);
        if (!m_loader.ReadChunk(m_currentOffset, keyLen, keyBuf.data())) {
            m_lastError = "Failed to read metadata key";
            return false;
        }
        keyBuf[keyLen] = '\0';
        std::string key(keyBuf.data());
        m_currentOffset += keyLen;
        
        // Read value type
        uint8_t valueTypeBuf[4];
        if (!m_loader.ReadChunk(m_currentOffset, 4, valueTypeBuf)) {
            m_lastError = "Failed to read metadata value type";
            return false;
        }
        uint32_t valueType;
        memcpy(&valueType, valueTypeBuf, 4);
        m_currentOffset += 4;
        
        // Read value (simplified - just store as string)
        std::string value;
        if (valueType == 8) { // String
            uint8_t lenBuf[4];
            if (!m_loader.ReadChunk(m_currentOffset, 4, lenBuf)) {
                return false;
            }
            uint32_t len;
            memcpy(&len, lenBuf, 4);
            m_currentOffset += 4;
            
            std::vector<char> valBuf(len + 1);
            if (!m_loader.ReadChunk(m_currentOffset, len, valBuf.data())) {
                return false;
            }
            valBuf[len] = '\0';
            value = std::string(valBuf.data());
            m_currentOffset += len;
        } else if (valueType == 9) { // Array
            // Skip arrays for now
            uint8_t arrTypeBuf[4];
            if (!m_loader.ReadChunk(m_currentOffset, 4, arrTypeBuf)) {
                return false;
            }
            uint32_t arrType;
            memcpy(&arrType, arrTypeBuf, 4);
            m_currentOffset += 4;
            
            uint8_t arrLenBuf[8];
            if (!m_loader.ReadChunk(m_currentOffset, 8, arrLenBuf)) {
                return false;
            }
            uint64_t arrLen;
            memcpy(&arrLen, arrLenBuf, 8);
            m_currentOffset += 8;
            
            // Skip array elements
            for (uint64_t j = 0; j < arrLen; j++) {
                // Simplified - just advance offset
                m_currentOffset += 4; // Assume strings
            }
            value = "[array]";
        } else {
            // Numeric types - fixed size
            static const uint32_t sizes[] = {1, 1, 2, 2, 4, 4, 4, 8, 8, 8};
            if (valueType < 10) {
                m_currentOffset += sizes[valueType];
            }
            value = "[numeric]";
        }
        
        m_metadata[key] = value;
    }
    
    return true;
}

bool GGUFChunkedLoader::ParseTensorInfo() {
    for (uint64_t i = 0; i < m_header.tensor_count; i++) {
        // Read name length
        uint8_t nameLenBuf[4];
        if (!m_loader.ReadChunk(m_currentOffset, 4, nameLenBuf)) {
            m_lastError = "Failed to read tensor name length";
            return false;
        }
        uint32_t nameLen;
        memcpy(&nameLen, nameLenBuf, 4);
        m_currentOffset += 4;
        
        // Read name
        std::vector<char> nameBuf(nameLen + 1);
        if (!m_loader.ReadChunk(m_currentOffset, nameLen, nameBuf.data())) {
            m_lastError = "Failed to read tensor name";
            return false;
        }
        nameBuf[nameLen] = '\0';
        std::string name(nameBuf.data());
        m_currentOffset += nameLen;
        
        // Read dimensions
        uint8_t dimsBuf[4];
        if (!m_loader.ReadChunk(m_currentOffset, 4, dimsBuf)) {
            m_lastError = "Failed to read tensor dimensions";
            return false;
        }
        uint32_t dims;
        memcpy(&dims, dimsBuf, 4);
        m_currentOffset += 4;
        
        // Read shape
        std::vector<uint64_t> shape(dims);
        if (!m_loader.ReadChunk(m_currentOffset, dims * 8, shape.data())) {
            m_lastError = "Failed to read tensor shape";
            return false;
        }
        m_currentOffset += dims * 8;
        
        // Read type
        uint8_t typeBuf[4];
        if (!m_loader.ReadChunk(m_currentOffset, 4, typeBuf)) {
            m_lastError = "Failed to read tensor type";
            return false;
        }
        uint32_t type;
        memcpy(&type, typeBuf, 4);
        m_currentOffset += 4;
        
        // Read offset
        uint8_t offsetBuf[8];
        if (!m_loader.ReadChunk(m_currentOffset, 8, offsetBuf)) {
            m_lastError = "Failed to read tensor offset";
            return false;
        }
        uint64_t tensorOffset;
        memcpy(&tensorOffset, offsetBuf, 8);
        m_currentOffset += 8;
        
        // Calculate tensor size
        uint64_t size = 1;
        for (uint64_t dim : shape) {
            size *= dim;
        }
        size *= GetTypeSize(type);
        
        // Store tensor info
        m_tensors[name] = {name, shape, type, tensorOffset, size};
    }
    
    // Calculate tensor data offset (aligned to 32 bytes)
    m_tensorDataOffset = AlignOffset(m_currentOffset, 32);
    
    return true;
}

uint64_t GGUFChunkedLoader::AlignOffset(uint64_t offset, uint64_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

uint32_t GGUFChunkedLoader::GetTypeSize(uint32_t type) {
    // GGUF type sizes (simplified)
    static const uint32_t sizes[] = {
        4,  // F32
        2,  // F16
        1,  // Q4_0
        1,  // Q4_1
        1,  // Q4_2 (removed)
        1,  // Q4_3 (removed)
        1,  // Q5_0
        1,  // Q5_1
        1,  // Q8_0
        1,  // Q8_1
        1,  // Q2_K
        1,  // Q3_K
        1,  // Q4_K
        1,  // Q5_K
        1,  // Q6_K
        1,  // Q8_K
        1,  // IQ2_XXS
        1,  // IQ2_XS
        1,  // IQ3_XXS
        1,  // IQ1_S
        1,  // IQ4_NL
        1,  // IQ3_S
        1,  // IQ2_S
        1,  // IQ4_XS
        1,  // I8
        2,  // I16
        4,  // I32
        8,  // I64
        8,  // F64
        1,  // IQ4_NL_Q4
    };
    
    if (type < sizeof(sizes) / sizeof(sizes[0])) {
        return sizes[type];
    }
    return 1;
}

std::string GGUFChunkedLoader::GetMetadata(const std::string& key) const {
    auto it = m_metadata.find(key);
    if (it != m_metadata.end()) {
        return it->second;
    }
    return "";
}

std::vector<std::string> GGUFChunkedLoader::GetTensorNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : m_tensors) {
        names.push_back(name);
    }
    return names;
}

const GGUFTensorInfo* GGUFChunkedLoader::GetTensorInfo(const std::string& name) const {
    auto it = m_tensors.find(name);
    if (it != m_tensors.end()) {
        return &it->second;
    }
    return nullptr;
}

bool GGUFChunkedLoader::LoadTensor(const std::string& name, void** data, uint64_t* size) {
    auto it = m_tensors.find(name);
    if (it == m_tensors.end()) {
        m_lastError = "Tensor not found: " + name;
        return false;
    }
    
    const GGUFTensorInfo& info = it->second;
    uint64_t tensorOffset = m_tensorDataOffset + info.offset;
    
    *data = m_loader.MapRegion(tensorOffset, info.size);
    if (!*data) {
        m_lastError = m_loader.GetLastError();
        return false;
    }
    
    *size = info.size;
    return true;
}

void GGUFChunkedLoader::UnloadTensor(void* data) {
    m_loader.UnmapRegion(data);
}

// ============================================================================
// InferenceChunkedLoader Implementation
// ============================================================================

InferenceChunkedLoader::InferenceChunkedLoader() = default;
InferenceChunkedLoader::~InferenceChunkedLoader() = default;

bool InferenceChunkedLoader::LoadModel(const std::string& path) {
    m_loader = std::make_unique<GGUFChunkedLoader>();
    
    if (!m_loader->Load(path)) {
        m_lastError = m_loader->GetLastError();
        return false;
    }
    
    m_loaded = true;
    return true;
}

bool InferenceChunkedLoader::LoadModel(const std::wstring& path) {
    m_loader = std::make_unique<GGUFChunkedLoader>();
    
    if (!m_loader->Load(path)) {
        m_lastError = m_loader->GetLastError();
        return false;
    }
    
    m_loaded = true;
    return true;
}

std::string InferenceChunkedLoader::GetModelName() const {
    if (!m_loaded) return "";
    return m_loader->GetMetadata("general.name");
}

std::string InferenceChunkedLoader::GetModelType() const {
    if (!m_loaded) return "";
    return m_loader->GetMetadata("general.architecture");
}

uint64_t InferenceChunkedLoader::GetParameterCount() const {
    if (!m_loaded) return 0;
    // Parse from metadata
    std::string params = m_loader->GetMetadata("general.parameter_count");
    if (!params.empty()) {
        return std::stoull(params);
    }
    return 0;
}

uint64_t InferenceChunkedLoader::GetContextLength() const {
    if (!m_loaded) return 0;
    std::string ctx = m_loader->GetMetadata("llama.context_length");
    if (!ctx.empty()) {
        return std::stoull(ctx);
    }
    return 4096; // Default
}

uint32_t InferenceChunkedLoader::GetLayerCount() const {
    if (!m_loaded) return 0;
    std::string layers = m_loader->GetMetadata("llama.block_count");
    if (!layers.empty()) {
        return std::stoul(layers);
    }
    return 32; // Default
}

uint32_t InferenceChunkedLoader::GetHeadCount() const {
    if (!m_loaded) return 0;
    std::string heads = m_loader->GetMetadata("llama.head_count");
    if (!heads.empty()) {
        return std::stoul(heads);
    }
    return 32; // Default
}

uint32_t InferenceChunkedLoader::GetEmbeddingDim() const {
    if (!m_loaded) return 0;
    std::string dim = m_loader->GetMetadata("llama.embedding_length");
    if (!dim.empty()) {
        return std::stoul(dim);
    }
    return 4096; // Default
}

bool InferenceChunkedLoader::GetTensor(const std::string& name, void** data, uint64_t* size) {
    if (!m_loaded) {
        m_lastError = "Model not loaded";
        return false;
    }
    return m_loader->LoadTensor(name, data, size);
}

} // namespace RawrXD
