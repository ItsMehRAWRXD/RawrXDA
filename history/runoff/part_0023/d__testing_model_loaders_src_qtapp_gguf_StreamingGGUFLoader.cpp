// StreamingGGUFLoader.cpp — Full GGUF parser with memory-mapped zone loading
// Converted from Qt (QFile, QFile::map, QHash, QByteArray, QDebug, signals) to pure C++17
// Preserves ALL original logic: GGUF magic/version parsing, tensor count/metadata,
// tensor index building, zone assignment, memory-mapped loading with page alignment,
// tensor data retrieval from mapped zones

#include "StreamingGGUFLoader.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

// GGUF constants
static constexpr uint32_t GGUF_MAGIC = 0x46475547; // "GGUF" in little-endian

// GGUF metadata value types
enum GGUFValueType : uint32_t {
    GGUF_TYPE_UINT8    = 0,
    GGUF_TYPE_INT8     = 1,
    GGUF_TYPE_UINT16   = 2,
    GGUF_TYPE_INT16    = 3,
    GGUF_TYPE_UINT32   = 4,
    GGUF_TYPE_INT32    = 5,
    GGUF_TYPE_FLOAT32  = 6,
    GGUF_TYPE_BOOL     = 7,
    GGUF_TYPE_STRING   = 8,
    GGUF_TYPE_ARRAY    = 9,
    GGUF_TYPE_UINT64   = 10,
    GGUF_TYPE_INT64    = 11,
    GGUF_TYPE_FLOAT64  = 12,
};

// ======================== Constructor / Destructor ========================

StreamingGGUFLoader::StreamingGGUFLoader() {
}

StreamingGGUFLoader::~StreamingGGUFLoader() {
    close();
}

// ======================== Open / Close ========================

bool StreamingGGUFLoader::open(const std::string& filePath) {
    if (m_fileOpen) close();

    m_filePath = filePath;
    emitProgress(0.0f, "Opening file");

#ifdef _WIN32
    // Open file with Win32 API for memory mapping
    m_fileHandle = CreateFileA(filePath.c_str(), GENERIC_READ,
                               FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_fileHandle == INVALID_HANDLE_VALUE) {
        emitError("Failed to open file: " + filePath);
        return false;
    }

    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(m_fileHandle, &fileSize)) {
        emitError("Failed to get file size");
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    m_fileSize = static_cast<uint64_t>(fileSize.QuadPart);

    // Create file mapping
    m_fileMappingHandle = CreateFileMappingA(m_fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!m_fileMappingHandle) {
        emitError("Failed to create file mapping");
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
#else
    m_fd = ::open(filePath.c_str(), O_RDONLY);
    if (m_fd < 0) {
        emitError("Failed to open file: " + filePath);
        return false;
    }
    struct stat st;
    if (fstat(m_fd, &st) != 0) {
        emitError("Failed to stat file");
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    m_fileSize = static_cast<uint64_t>(st.st_size);
#endif

    m_fileOpen = true;
    std::cout << "[StreamingGGUFLoader] File opened: " << filePath
              << " (" << (m_fileSize / (1024 * 1024)) << " MB)" << std::endl;

    // Parse GGUF structure
    emitProgress(0.1f, "Parsing header");
    if (!parseHeader()) {
        close();
        return false;
    }

    emitProgress(0.3f, "Parsing metadata");
    if (!parseMetadata()) {
        close();
        return false;
    }

    emitProgress(0.5f, "Building tensor index");
    if (!parseTensorIndex()) {
        close();
        return false;
    }

    emitProgress(0.8f, "Building zone map");
    buildZoneMap();

    emitProgress(1.0f, "Ready");
    std::cout << "[StreamingGGUFLoader] Loaded: version=" << m_version
              << ", tensors=" << m_tensorCount
              << ", metadata_kv=" << m_metadataKVCount
              << ", zones=" << m_zones.size() << std::endl;

    return true;
}

void StreamingGGUFLoader::close() {
    // Unload all zones
    for (size_t i = 0; i < m_zones.size(); i++) {
        unloadZone(static_cast<int>(i));
    }
    m_zones.clear();

#ifdef _WIN32
    if (m_fileMappingHandle) {
        CloseHandle(m_fileMappingHandle);
        m_fileMappingHandle = NULL;
    }
    if (m_fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif

    m_tensors.clear();
    m_metadataStrings.clear();
    m_metadataInts.clear();
    m_metadataFloats.clear();
    m_fileOpen = false;
    m_fileSize = 0;
}

// ======================== GGUF Header Parsing ========================

bool StreamingGGUFLoader::parseHeader() {
    uint64_t offset = 0;

    // Read magic number
    uint32_t magic = readU32(offset);
    if (magic != GGUF_MAGIC) {
        emitError("Invalid GGUF magic number: " + std::to_string(magic));
        return false;
    }

    // Read version
    m_version = readU32(offset);
    if (m_version < 2 || m_version > 3) {
        emitError("Unsupported GGUF version: " + std::to_string(m_version));
        return false;
    }

    // Read tensor count and metadata KV count
    m_tensorCount = readU64(offset);
    m_metadataKVCount = readU64(offset);

    std::cout << "[StreamingGGUFLoader] Header: version=" << m_version
              << ", tensors=" << m_tensorCount
              << ", metadata_kv=" << m_metadataKVCount << std::endl;

    return true;
}

// ======================== Metadata Parsing ========================

bool StreamingGGUFLoader::parseMetadata() {
    // Metadata starts after the header (4 + 4 + 8 + 8 = 24 bytes)
    uint64_t offset = 24;

    for (uint64_t i = 0; i < m_metadataKVCount; i++) {
        emitProgress(0.3f + 0.2f * static_cast<float>(i) / m_metadataKVCount, "Reading metadata");

        // Read key
        std::string key = readString(offset);
        if (key.empty()) {
            emitError("Failed to read metadata key at offset " + std::to_string(offset));
            return false;
        }

        // Read value type
        uint32_t valueType = readU32(offset);

        // Read value based on type
        switch (valueType) {
            case GGUF_TYPE_UINT8:
            case GGUF_TYPE_INT8: {
                uint8_t val;
                readBytes(offset, &val, 1);
                offset += 1;
                m_metadataInts[key] = val;
                break;
            }
            case GGUF_TYPE_UINT16:
            case GGUF_TYPE_INT16: {
                uint16_t val;
                readBytes(offset, &val, 2);
                offset += 2;
                m_metadataInts[key] = val;
                break;
            }
            case GGUF_TYPE_UINT32:
            case GGUF_TYPE_INT32: {
                uint32_t val = readU32(offset);
                m_metadataInts[key] = static_cast<int64_t>(val);
                break;
            }
            case GGUF_TYPE_UINT64:
            case GGUF_TYPE_INT64: {
                uint64_t val = readU64(offset);
                m_metadataInts[key] = static_cast<int64_t>(val);
                break;
            }
            case GGUF_TYPE_FLOAT32: {
                float val = readF32(offset);
                m_metadataFloats[key] = val;
                break;
            }
            case GGUF_TYPE_FLOAT64: {
                double val;
                readBytes(offset, &val, 8);
                offset += 8;
                m_metadataFloats[key] = static_cast<float>(val);
                break;
            }
            case GGUF_TYPE_BOOL: {
                uint8_t val;
                readBytes(offset, &val, 1);
                offset += 1;
                m_metadataInts[key] = val ? 1 : 0;
                break;
            }
            case GGUF_TYPE_STRING: {
                std::string val = readString(offset);
                m_metadataStrings[key] = val;
                break;
            }
            case GGUF_TYPE_ARRAY: {
                // Read array type and count
                uint32_t arrType = readU32(offset);
                uint64_t arrLen  = readU64(offset);
                // Skip array data based on element type
                size_t elemSize = 0;
                switch (arrType) {
                    case GGUF_TYPE_UINT8:  case GGUF_TYPE_INT8:  case GGUF_TYPE_BOOL: elemSize = 1; break;
                    case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: elemSize = 2; break;
                    case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: elemSize = 4; break;
                    case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: elemSize = 8; break;
                    case GGUF_TYPE_STRING: {
                        // Skip strings one by one
                        for (uint64_t j = 0; j < arrLen; j++) readString(offset);
                        elemSize = 0; arrLen = 0; // Already handled
                        break;
                    }
                    default: elemSize = 4; break;
                }
                offset += elemSize * arrLen;
                break;
            }
            default: {
                std::cerr << "[StreamingGGUFLoader] Unknown metadata type " << valueType
                          << " for key '" << key << "'" << std::endl;
                // Can't reliably skip unknown types
                break;
            }
        }
    }

    m_dataOffset = offset; // Mark where tensor info starts
    return true;
}

// ======================== Tensor Index Parsing ========================

bool StreamingGGUFLoader::parseTensorIndex() {
    uint64_t offset = m_dataOffset;

    for (uint64_t i = 0; i < m_tensorCount; i++) {
        TensorMetadata meta;

        // Read tensor name
        meta.name = readString(offset);

        // Read dimensions
        uint32_t nDims = readU32(offset);
        meta.nDims = static_cast<int>(nDims);
        for (uint32_t d = 0; d < nDims && d < 4; d++) {
            meta.dims[d] = static_cast<int64_t>(readU64(offset));
        }

        // Read type
        meta.type = readU32(offset);

        // Read offset within data section
        meta.offset = readU64(offset);

        // Calculate size in bytes
        size_t elemSize = 4; // default F32
        switch (meta.type) {
            case 0:  elemSize = 4; break;  // F32
            case 1:  elemSize = 2; break;  // F16
            case 2:  elemSize = 18; break; // Q4_0 (block size)
            case 8:  elemSize = 34; break; // Q8_0 (block size)
            default: elemSize = 4; break;
        }

        uint64_t numElements = 1;
        for (int d = 0; d < meta.nDims; d++) {
            if (meta.dims[d] > 0) numElements *= meta.dims[d];
        }

        // For quantized types, adjust size
        if (meta.type >= 2) {
            // Block-quantized: 32 elements per block typically
            uint64_t numBlocks = (numElements + 31) / 32;
            meta.sizeBytes = numBlocks * elemSize;
        } else {
            meta.sizeBytes = numElements * elemSize;
        }

        m_tensors[meta.name] = meta;
    }

    // Data section starts after tensor info, aligned to page boundary
    m_dataOffset = offset;
    // Align to 32 bytes (GGUF alignment requirement)
    m_dataOffset = (m_dataOffset + 31) & ~31ULL;

    // Update tensor offsets relative to data section
    for (auto& [name, meta] : m_tensors) {
        meta.offset += m_dataOffset;
    }

    std::cout << "[StreamingGGUFLoader] Indexed " << m_tensors.size() << " tensors, "
              << "data starts at offset " << m_dataOffset << std::endl;

    return true;
}

// ======================== Zone Map ========================

void StreamingGGUFLoader::buildZoneMap() {
    if (m_tensors.empty()) return;

    // Calculate number of zones
    uint64_t dataSize = (m_fileSize > m_dataOffset) ? (m_fileSize - m_dataOffset) : 0;
    int numZones = static_cast<int>((dataSize + m_zoneSize - 1) / m_zoneSize);
    if (numZones < 1) numZones = 1;

    m_zones.resize(numZones);
    for (int z = 0; z < numZones; z++) {
        m_zones[z].zoneIndex = z;
        m_zones[z].fileOffset = m_dataOffset + static_cast<uint64_t>(z) * m_zoneSize;
        m_zones[z].zoneSize = std::min(m_zoneSize, m_fileSize - m_zones[z].fileOffset);
    }

    // Assign tensors to zones
    for (auto& [name, meta] : m_tensors) {
        uint64_t tensorStart = meta.offset;
        // Find which zone this tensor belongs to
        for (int z = 0; z < numZones; z++) {
            uint64_t zoneStart = m_zones[z].fileOffset;
            uint64_t zoneEnd   = zoneStart + m_zones[z].zoneSize;
            if (tensorStart >= zoneStart && tensorStart < zoneEnd) {
                meta.zoneIndex = z;
                break;
            }
        }
    }

    std::cout << "[StreamingGGUFLoader] Built " << numZones << " zones, "
              << "zone size = " << (m_zoneSize / (1024 * 1024)) << " MB" << std::endl;
}

// ======================== Zone Loading ========================

bool StreamingGGUFLoader::loadZone(int zoneIndex) {
    std::lock_guard<std::mutex> lock(m_zoneMutex);

    if (zoneIndex < 0 || zoneIndex >= static_cast<int>(m_zones.size())) {
        emitError("Invalid zone index: " + std::to_string(zoneIndex));
        return false;
    }

    ZoneMemory& zone = m_zones[zoneIndex];
    if (zone.loaded) {
        zone.refCount++;
        return true;
    }

    emitProgress(0.0f, "Loading zone " + std::to_string(zoneIndex));

    // Memory-map the zone with page alignment
    zone.mappedPtr = mapFileRegion(zone.fileOffset, zone.zoneSize);
    if (!zone.mappedPtr) {
        emitError("Failed to map zone " + std::to_string(zoneIndex));
        return false;
    }

    zone.loaded = true;
    zone.refCount = 1;

    std::cout << "[StreamingGGUFLoader] Zone " << zoneIndex << " loaded: "
              << (zone.zoneSize / (1024 * 1024)) << " MB at offset "
              << zone.fileOffset << std::endl;

    return true;
}

void StreamingGGUFLoader::unloadZone(int zoneIndex) {
    std::lock_guard<std::mutex> lock(m_zoneMutex);

    if (zoneIndex < 0 || zoneIndex >= static_cast<int>(m_zones.size())) return;

    ZoneMemory& zone = m_zones[zoneIndex];
    if (!zone.loaded) return;

    zone.refCount--;
    if (zone.refCount <= 0) {
        if (zone.mappedPtr) {
            unmapFileRegion(zone.mappedPtr, zone.zoneSize);
            zone.mappedPtr = nullptr;
        }
        zone.loaded = false;
        zone.refCount = 0;
    }
}

bool StreamingGGUFLoader::isZoneLoaded(int zoneIndex) const {
    std::lock_guard<std::mutex> lock(m_zoneMutex);
    if (zoneIndex < 0 || zoneIndex >= static_cast<int>(m_zones.size())) return false;
    return m_zones[zoneIndex].loaded;
}

// ======================== Tensor Access ========================

std::vector<std::string> StreamingGGUFLoader::tensorNames() const {
    std::vector<std::string> names;
    names.reserve(m_tensors.size());
    for (const auto& [name, _] : m_tensors) names.push_back(name);
    return names;
}

bool StreamingGGUFLoader::hasTensor(const std::string& name) const {
    return m_tensors.find(name) != m_tensors.end();
}

TensorMetadata StreamingGGUFLoader::getTensorMetadata(const std::string& name) const {
    auto it = m_tensors.find(name);
    return (it != m_tensors.end()) ? it->second : TensorMetadata{};
}

std::vector<uint8_t> StreamingGGUFLoader::getTensorData(const std::string& name) {
    auto it = m_tensors.find(name);
    if (it == m_tensors.end()) {
        emitError("Tensor not found: " + name);
        return {};
    }

    const TensorMetadata& meta = it->second;

    // Ensure zone is loaded
    if (meta.zoneIndex >= 0) {
        if (!isZoneLoaded(meta.zoneIndex)) {
            if (!loadZone(meta.zoneIndex)) {
                emitError("Failed to load zone for tensor: " + name);
                return {};
            }
        }

        // Get data from mapped zone
        std::lock_guard<std::mutex> lock(m_zoneMutex);
        const ZoneMemory& zone = m_zones[meta.zoneIndex];
        if (zone.mappedPtr && meta.offset >= zone.fileOffset) {
            uint64_t localOffset = meta.offset - zone.fileOffset;
            if (localOffset + meta.sizeBytes <= zone.zoneSize) {
                std::vector<uint8_t> data(meta.sizeBytes);
                memcpy(data.data(), zone.mappedPtr + localOffset, meta.sizeBytes);
                return data;
            }
        }
    }

    // Fallback: direct file read
    std::vector<uint8_t> data(meta.sizeBytes);
    if (readBytes(meta.offset, data.data(), meta.sizeBytes)) {
        return data;
    }

    emitError("Failed to read tensor data: " + name);
    return {};
}

// ======================== Metadata Access ========================

std::string StreamingGGUFLoader::getMetadataString(const std::string& key) const {
    auto it = m_metadataStrings.find(key);
    return (it != m_metadataStrings.end()) ? it->second : "";
}

int64_t StreamingGGUFLoader::getMetadataInt(const std::string& key) const {
    auto it = m_metadataInts.find(key);
    return (it != m_metadataInts.end()) ? it->second : 0;
}

float StreamingGGUFLoader::getMetadataFloat(const std::string& key) const {
    auto it = m_metadataFloats.find(key);
    return (it != m_metadataFloats.end()) ? it->second : 0.0f;
}

// ======================== File I/O ========================

bool StreamingGGUFLoader::readBytes(uint64_t offset, void* buffer, size_t count) {
    if (!m_fileOpen || !buffer || count == 0) return false;
    if (offset + count > m_fileSize) return false;

#ifdef _WIN32
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);
    OVERLAPPED ov{};
    ov.Offset = li.LowPart;
    ov.OffsetHigh = li.HighPart;

    DWORD bytesRead = 0;
    if (!ReadFile(m_fileHandle, buffer, static_cast<DWORD>(count), &bytesRead, &ov)) {
        return false;
    }
    return bytesRead == static_cast<DWORD>(count);
#else
    ssize_t result = pread(m_fd, buffer, count, static_cast<off_t>(offset));
    return result == static_cast<ssize_t>(count);
#endif
}

std::string StreamingGGUFLoader::readString(uint64_t& offset) {
    uint64_t len = readU64(offset);
    if (len == 0 || len > 65536) return ""; // Sanity check

    std::string str(len, '\0');
    readBytes(offset, str.data(), len);
    offset += len;
    return str;
}

uint32_t StreamingGGUFLoader::readU32(uint64_t& offset) {
    uint32_t val = 0;
    readBytes(offset, &val, 4);
    offset += 4;
    return val;
}

uint64_t StreamingGGUFLoader::readU64(uint64_t& offset) {
    uint64_t val = 0;
    readBytes(offset, &val, 8);
    offset += 8;
    return val;
}

float StreamingGGUFLoader::readF32(uint64_t& offset) {
    float val = 0;
    readBytes(offset, &val, 4);
    offset += 4;
    return val;
}

// ======================== Memory Mapping ========================

uint8_t* StreamingGGUFLoader::mapFileRegion(uint64_t offset, uint64_t size) {
#ifdef _WIN32
    if (!m_fileMappingHandle) return nullptr;

    // Align offset to allocation granularity
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uint64_t alignedOffset = (offset / si.dwAllocationGranularity) * si.dwAllocationGranularity;
    uint64_t delta = offset - alignedOffset;
    uint64_t mapSize = size + delta;

    DWORD offsetHigh = static_cast<DWORD>(alignedOffset >> 32);
    DWORD offsetLow  = static_cast<DWORD>(alignedOffset & 0xFFFFFFFF);

    void* ptr = MapViewOfFile(m_fileMappingHandle, FILE_MAP_READ,
                              offsetHigh, offsetLow, static_cast<SIZE_T>(mapSize));
    if (!ptr) return nullptr;

    return static_cast<uint8_t*>(ptr) + delta;
#else
    // Align to page size
    long pageSize = sysconf(_SC_PAGESIZE);
    uint64_t alignedOffset = (offset / pageSize) * pageSize;
    uint64_t delta = offset - alignedOffset;
    uint64_t mapSize = size + delta;

    void* ptr = mmap(nullptr, mapSize, PROT_READ, MAP_PRIVATE, m_fd, alignedOffset);
    if (ptr == MAP_FAILED) return nullptr;

    return static_cast<uint8_t*>(ptr) + delta;
#endif
}

void StreamingGGUFLoader::unmapFileRegion(uint8_t* ptr, uint64_t size) {
    if (!ptr) return;

#ifdef _WIN32
    // Need to find the original base address (before delta adjustment)
    // For simplicity, unmap from the pointer directly
    // MapViewOfFile base must be used — we store the adjusted pointer
    // so we need to reverse the alignment delta
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr / si.dwAllocationGranularity) * si.dwAllocationGranularity;
    UnmapViewOfFile(reinterpret_cast<void*>(aligned));
#else
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr / pageSize) * pageSize;
    uint64_t delta = addr - aligned;
    munmap(reinterpret_cast<void*>(aligned), size + delta);
#endif
}

// ======================== Callbacks ========================

void StreamingGGUFLoader::emitError(const std::string& msg) {
    std::cerr << "[StreamingGGUFLoader] Error: " << msg << std::endl;
    if (m_errorCb) m_errorCb(msg);
}

void StreamingGGUFLoader::emitProgress(float progress, const std::string& stage) {
    if (m_progressCb) m_progressCb(progress, stage);
}
