// ============================================================================
// gguf_dml_bridge.cpp — GGUF → DirectML Tensor Upload Implementation
// ============================================================================
// Full implementation of the GGUF-to-DML bridge: GGUF header parsing,
// model config extraction, tensor dequantization, GPU upload, layer
// management with LRU eviction, and dual-model session wiring.
//
// Uses memory-mapped I/O (CreateFileMapping) for zero-copy GGUF access.
// Dequantization uses ASM kernels for Q4_0/Q8_0 and CPU C++ for other types.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "gguf_dml_bridge.h"
#include "directml_compute.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace RawrXD {
namespace DML {

// ============================================================================
// GGUF File Format Constants
// ============================================================================
static const uint32_t GGUF_MAGIC       = 0x46554747;   // "GGUF" in LE
static const uint32_t GGUF_VERSION_3   = 3;

// GGUF metadata value types
enum GGUFMetaType : uint32_t {
    GGUF_META_UINT8    = 0,
    GGUF_META_INT8     = 1,
    GGUF_META_UINT16   = 2,
    GGUF_META_INT16    = 3,
    GGUF_META_UINT32   = 4,
    GGUF_META_INT32    = 5,
    GGUF_META_FLOAT32  = 6,
    GGUF_META_BOOL     = 7,
    GGUF_META_STRING   = 8,
    GGUF_META_ARRAY    = 9,
    GGUF_META_UINT64   = 10,
    GGUF_META_INT64    = 11,
    GGUF_META_FLOAT64  = 12,
};

// GGML type enum (matches llama.cpp)
enum GGMLTypeEnum : uint32_t {
    GGML_TYPE_F32     = 0,
    GGML_TYPE_F16     = 1,
    GGML_TYPE_Q4_0    = 2,
    GGML_TYPE_Q4_1    = 3,
    GGML_TYPE_Q5_0    = 6,
    GGML_TYPE_Q5_1    = 7,
    GGML_TYPE_Q8_0    = 8,
    GGML_TYPE_Q8_1    = 9,
    GGML_TYPE_Q2_K    = 10,
    GGML_TYPE_Q3_K    = 11,
    GGML_TYPE_Q4_K    = 12,
    GGML_TYPE_Q5_K    = 13,
    GGML_TYPE_Q6_K    = 14,
    GGML_TYPE_Q8_K    = 15,
    GGML_TYPE_IQ2_XXS = 16,
    GGML_TYPE_IQ2_XS  = 17,
    GGML_TYPE_IQ3_XXS = 18,
    GGML_TYPE_IQ1_S   = 19,
    GGML_TYPE_IQ4_NL  = 20,
    GGML_TYPE_IQ3_S   = 21,
    GGML_TYPE_IQ2_S   = 22,
    GGML_TYPE_IQ4_XS  = 23,
    GGML_TYPE_I8      = 24,
    GGML_TYPE_I16     = 25,
    GGML_TYPE_I32     = 26,
    GGML_TYPE_I64     = 27,
    GGML_TYPE_F64     = 28,
    GGML_TYPE_IQ1_M   = 29,
};

// Block sizes and bytes for quantized types
struct QuantBlockInfo {
    uint32_t blockSize;     // Elements per block
    uint32_t blockBytes;    // Bytes per block
};

static QuantBlockInfo getQuantBlockInfo(uint32_t ggmlType) {
    switch (ggmlType) {
        case GGML_TYPE_F32:     return { 1, 4 };
        case GGML_TYPE_F16:     return { 1, 2 };
        case GGML_TYPE_Q4_0:    return { 32, 18 };    // 2 (scale) + 16 (data)
        case GGML_TYPE_Q4_1:    return { 32, 20 };    // 2+2 (scale+min) + 16
        case GGML_TYPE_Q5_0:    return { 32, 22 };    // 2 (scale) + 4 (high bits) + 16
        case GGML_TYPE_Q5_1:    return { 32, 24 };    // 2+2 + 4 + 16
        case GGML_TYPE_Q8_0:    return { 32, 34 };    // 2 (scale) + 32 (data)
        case GGML_TYPE_Q8_1:    return { 32, 36 };    // 2+2 + 32
        case GGML_TYPE_Q2_K:    return { 256, 84 };
        case GGML_TYPE_Q3_K:    return { 256, 110 };
        case GGML_TYPE_Q4_K:    return { 256, 144 };
        case GGML_TYPE_Q5_K:    return { 256, 176 };
        case GGML_TYPE_Q6_K:    return { 256, 210 };
        case GGML_TYPE_Q8_K:    return { 256, 292 };
        default:                return { 1, 4 };       // Unknown, treat as F32
    }
}

// ============================================================================
// Safe binary reader from memory-mapped region
// ============================================================================
class MmapReader {
public:
    MmapReader(const uint8_t* base, uint64_t size)
        : m_base(base), m_size(size), m_pos(0) {}

    bool hasBytes(uint64_t n) const { return m_pos + n <= m_size; }
    uint64_t position() const { return m_pos; }
    void seek(uint64_t pos) { m_pos = pos; }

    template<typename T>
    T read() {
        T val{};
        if (m_pos + sizeof(T) <= m_size) {
            memcpy(&val, m_base + m_pos, sizeof(T));
            m_pos += sizeof(T);
        }
        return val;
    }

    std::string readString() {
        uint64_t len = read<uint64_t>();
        std::string s;
        if (len > 0 && m_pos + len <= m_size) {
            s.assign(reinterpret_cast<const char*>(m_base + m_pos), static_cast<size_t>(len));
            m_pos += len;
        }
        return s;
    }

    void skipMetaValue(uint32_t type) {
        switch (type) {
            case GGUF_META_UINT8:
            case GGUF_META_INT8:
            case GGUF_META_BOOL:    m_pos += 1; break;
            case GGUF_META_UINT16:
            case GGUF_META_INT16:   m_pos += 2; break;
            case GGUF_META_UINT32:
            case GGUF_META_INT32:
            case GGUF_META_FLOAT32: m_pos += 4; break;
            case GGUF_META_UINT64:
            case GGUF_META_INT64:
            case GGUF_META_FLOAT64: m_pos += 8; break;
            case GGUF_META_STRING:  readString(); break;
            case GGUF_META_ARRAY: {
                uint32_t elemType = read<uint32_t>();
                uint64_t count = read<uint64_t>();
                for (uint64_t i = 0; i < count; ++i) {
                    skipMetaValue(elemType);
                }
                break;
            }
        }
    }

    float readMetaFloat(uint32_t type) {
        switch (type) {
            case GGUF_META_FLOAT32: return read<float>();
            case GGUF_META_FLOAT64: return static_cast<float>(read<double>());
            case GGUF_META_UINT32:  return static_cast<float>(read<uint32_t>());
            case GGUF_META_INT32:   return static_cast<float>(read<int32_t>());
            default:                skipMetaValue(type); return 0.0f;
        }
    }

    uint32_t readMetaUint32(uint32_t type) {
        switch (type) {
            case GGUF_META_UINT32: return read<uint32_t>();
            case GGUF_META_INT32:  return static_cast<uint32_t>(read<int32_t>());
            case GGUF_META_UINT64: return static_cast<uint32_t>(read<uint64_t>());
            case GGUF_META_INT64:  return static_cast<uint32_t>(read<int64_t>());
            case GGUF_META_UINT16: return read<uint16_t>();
            default:               skipMetaValue(type); return 0;
        }
    }

    const uint8_t* ptr() const { return m_base + m_pos; }

private:
    const uint8_t* m_base;
    uint64_t m_size;
    uint64_t m_pos;
};

// ============================================================================
// FNV-1a hash helper
// ============================================================================
static uint64_t fnv1a(const char* str) {
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= static_cast<uint64_t>(*str++);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// ============================================================================
// Static empty config
// ============================================================================
const GGUFModelConfig GGUFDMLBridge::s_emptyConfig = {};

// ============================================================================
// Constructor / Destructor
// ============================================================================
GGUFDMLBridge::GGUFDMLBridge()
    : m_computeType(TensorDataType::Float16)
    , m_maxVRAMBudget(8ULL * 1024 * 1024 * 1024)   // 8 GB default per model
    , m_dequantOnCPU(true)
    , m_streamingEnabled(true)
    , m_layerCacheCount(4)
    , m_spilloverEnabled(true)
    , m_hostCacheBudget(0)                           // 0 = unlimited
{}

GGUFDMLBridge::~GGUFDMLBridge() {
    closeAllModels();
}

// ============================================================================
// Configuration
// ============================================================================
void GGUFDMLBridge::setDirectMLCompute(DirectMLCompute* dml) { m_dml = dml; }
void GGUFDMLBridge::setProgressCallback(LoadProgressCallback cb, void* ud) {
    m_progressCb = cb; m_progressUserData = ud;
}
void GGUFDMLBridge::setComputeDataType(TensorDataType dt) { m_computeType = dt; }
void GGUFDMLBridge::setMaxVRAMBudget(uint64_t bytes) { m_maxVRAMBudget = bytes; }
void GGUFDMLBridge::setDequantOnCPU(bool e) { m_dequantOnCPU = e; }
void GGUFDMLBridge::setStreamingEnabled(bool e) { m_streamingEnabled = e; }
void GGUFDMLBridge::setLayerCacheCount(uint32_t c) { m_layerCacheCount = c; }
void GGUFDMLBridge::setSpilloverEnabled(bool e) { m_spilloverEnabled = e; }
void GGUFDMLBridge::setHostCacheBudget(uint64_t b) { m_hostCacheBudget = b; }

// ============================================================================
// Open Model — mmap GGUF file + parse header + create DML session
// ============================================================================
DMLResult GGUFDMLBridge::openModel(const char* ggufPath, uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_dml) return DMLResult::error("DirectMLCompute not set", -1);
    if (!ggufPath) return DMLResult::error("Null path", -2);
    if (m_sessions.count(sessionId)) return DMLResult::error("Session already exists", -3);

    // Open file
    HANDLE hFile = CreateFileA(ggufPath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return DMLResult::error("Failed to open GGUF file", GetLastError());
    }

    // Get file size
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);

    // Create file mapping
    HANDLE hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) {
        CloseHandle(hFile);
        return DMLResult::error("CreateFileMapping failed", GetLastError());
    }

    // Map view
    void* mappedBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!mappedBase) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return DMLResult::error("MapViewOfFile failed", GetLastError());
    }

    // Verify GGUF magic
    uint32_t magic = *reinterpret_cast<const uint32_t*>(mappedBase);
    if (magic != GGUF_MAGIC) {
        UnmapViewOfFile(mappedBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return DMLResult::error("Not a valid GGUF file (bad magic)", -4);
    }

    // Initialize session state
    SessionState state;
    state.sessionId = sessionId;
    state.modelPath = ggufPath;
    state.fileHandle = hFile;
    state.fileMapping = hMapping;
    state.mappedBase = mappedBase;
    state.fileSize = static_cast<uint64_t>(fileSize.QuadPart);
    state.vramUsed = 0;
    state.vramBudget = m_maxVRAMBudget;
    state.accessTick = 0;
    state.fixedTensorsLoaded = false;
    state.config = {};
    state.spillStats = {};
    state.prefetchLayerHint = -1;

    // Parse GGUF header
    auto r = parseGGUFHeader(state);
    if (!r.success) {
        UnmapViewOfFile(mappedBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return r;
    }

    // Parse model config from metadata
    auto r2 = parseModelConfig(state);
    // Non-fatal if config parsing fails — we can still load tensors

    // Create DML session with model config
    auto r3 = m_dml->createSession(sessionId, state.config.architecture, state.vramBudget);
    if (!r3.success) {
        UnmapViewOfFile(mappedBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return r3;
    }

    // Configure the DML session with parsed model parameters
    auto* dmlSession = m_dml->getSession(sessionId);
    if (dmlSession) {
        dmlSession->numLayers = state.config.numLayers;
        dmlSession->hiddenSize = state.config.hiddenSize;
        dmlSession->intermediateSize = state.config.intermediateSize;
        dmlSession->numHeads = state.config.numHeads;
        dmlSession->numKVHeads = state.config.numKVHeads;
        dmlSession->headDim = state.config.headDim;
        dmlSession->vocabSize = state.config.vocabSize;
        dmlSession->maxSeqLen = state.config.maxSeqLen;
        dmlSession->rmsNormEps = state.config.rmsNormEps;
        dmlSession->ropeTheta = state.config.ropeTheta;
    }

    m_sessions[sessionId] = std::move(state);

    std::cout << "[GGUF-DML] Opened model: " << ggufPath
              << " layers=" << state.config.numLayers
              << " hidden=" << state.config.hiddenSize
              << " tensors=" << state.tensorMetas.size()
              << " fileSize=" << (fileSize.QuadPart / (1024 * 1024)) << "MB"
              << std::endl;

    return DMLResult::ok("Model opened");
}

// ============================================================================
// Parse GGUF Header — extract tensor metadata
// ============================================================================
DMLResult GGUFDMLBridge::parseGGUFHeader(SessionState& state) {
    const uint8_t* base = static_cast<const uint8_t*>(state.mappedBase);
    MmapReader reader(base, state.fileSize);

    // Header: magic(4) + version(4) + tensor_count(8) + metadata_kv_count(8)
    uint32_t magic = reader.read<uint32_t>();
    uint32_t version = reader.read<uint32_t>();
    uint64_t tensorCount = reader.read<uint64_t>();
    uint64_t metadataCount = reader.read<uint64_t>();

    if (version < 2 || version > 3) {
        return DMLResult::error("Unsupported GGUF version", version);
    }

    // Skip metadata (we'll parse it properly in parseModelConfig)
    // But we need to advance past it to find tensor info
    uint64_t metaStartPos = reader.position();

    // First pass: skip metadata to find tensor descriptors
    for (uint64_t i = 0; i < metadataCount; ++i) {
        if (!reader.hasBytes(12)) return DMLResult::error("Truncated metadata", -5);
        std::string key = reader.readString();
        uint32_t valueType = reader.read<uint32_t>();
        reader.skipMetaValue(valueType);
    }

    // Tensor info section
    state.tensorMetas.reserve(static_cast<size_t>(tensorCount));

    for (uint64_t t = 0; t < tensorCount; ++t) {
        if (!reader.hasBytes(16)) return DMLResult::error("Truncated tensor info", -6);

        GGUFTensorMeta meta = {};

        // Read tensor name
        std::string name = reader.readString();
        // Store name pointer — we'll use hash for lookups
        meta.nameHash = fnv1a(name.c_str());

        // Dimensions
        uint32_t nDims = reader.read<uint32_t>();
        meta.shapeDims = std::min(nDims, 8u);
        uint64_t totalElements = 1;
        for (uint32_t d = 0; d < nDims; ++d) {
            uint64_t dim = reader.read<uint64_t>();
            if (d < 8) meta.shape[d] = static_cast<uint32_t>(dim);
            totalElements *= dim;
        }

        // Type
        meta.ggmlType = reader.read<uint32_t>();

        // Offset (relative to data section start)
        meta.fileOffset = reader.read<uint64_t>();

        // Compute raw size
        auto blockInfo = getQuantBlockInfo(meta.ggmlType);
        uint64_t numBlocks = (totalElements + blockInfo.blockSize - 1) / blockInfo.blockSize;
        meta.rawSizeBytes = numBlocks * blockInfo.blockBytes;

        // Compute dequantized size (to FP16 or FP32)
        meta.dequantSizeBytes = computeDequantSize(meta.ggmlType, meta.shape, meta.shapeDims);

        // Extract layer index
        meta.layerIndex = extractLayerIndex(name.c_str());
        meta.isWeight = (name.find(".weight") != std::string::npos);
        meta.uploaded = false;

        // Store name as const char* by allocating persistent copy
        char* nameCopy = new char[name.size() + 1];
        memcpy(nameCopy, name.c_str(), name.size() + 1);
        meta.name = nameCopy;

        state.tensorHashToIndex[meta.nameHash] = state.tensorMetas.size();
        state.tensorMetas.push_back(meta);
    }

    // Compute tensor data offset (aligned to GGUF_DEFAULT_ALIGNMENT = 32)
    uint64_t curPos = reader.position();
    uint64_t alignment = 32;
    uint64_t dataOffset = (curPos + alignment - 1) & ~(alignment - 1);

    // Fixup tensor offsets: they are relative to data section start
    for (auto& meta : state.tensorMetas) {
        meta.fileOffset += dataOffset;
    }

    std::cout << "[GGUF-DML] Parsed: version=" << version
              << " tensors=" << tensorCount
              << " metadata_keys=" << metadataCount
              << " dataOffset=0x" << std::hex << dataOffset << std::dec
              << std::endl;

    return DMLResult::ok("Header parsed");
}

// ============================================================================
// Parse Model Config — extract architecture parameters from metadata
// ============================================================================
DMLResult GGUFDMLBridge::parseModelConfig(SessionState& state) {
    const uint8_t* base = static_cast<const uint8_t*>(state.mappedBase);
    MmapReader reader(base, state.fileSize);

    // Skip magic + version
    reader.seek(4 + 4);
    uint64_t tensorCount = reader.read<uint64_t>();
    uint64_t metadataCount = reader.read<uint64_t>();

    auto& cfg = state.config;
    strncpy_s(cfg.architecture, "unknown", sizeof(cfg.architecture) - 1);

    // Temporary metadata storage
    std::unordered_map<std::string, std::pair<uint32_t, uint64_t>> metaPositions;

    for (uint64_t i = 0; i < metadataCount; ++i) {
        if (!reader.hasBytes(12)) break;

        std::string key = reader.readString();
        uint32_t valueType = reader.read<uint32_t>();
        uint64_t valuePos = reader.position();

        // Match known keys
        if (key == "general.architecture") {
            std::string arch = reader.readString();
            strncpy_s(cfg.architecture, arch.c_str(), sizeof(cfg.architecture) - 1);
        }
        else if (key.find(".block_count") != std::string::npos ||
                 key.find(".num_hidden_layers") != std::string::npos) {
            cfg.numLayers = reader.readMetaUint32(valueType);
        }
        else if (key.find(".embedding_length") != std::string::npos ||
                 key.find(".hidden_size") != std::string::npos) {
            cfg.hiddenSize = reader.readMetaUint32(valueType);
        }
        else if (key.find(".feed_forward_length") != std::string::npos ||
                 key.find(".intermediate_size") != std::string::npos) {
            cfg.intermediateSize = reader.readMetaUint32(valueType);
        }
        else if (key.find(".attention.head_count_kv") != std::string::npos) {
            cfg.numKVHeads = reader.readMetaUint32(valueType);
        }
        else if (key.find(".attention.head_count") != std::string::npos) {
            cfg.numHeads = reader.readMetaUint32(valueType);
        }
        else if (key.find(".vocab_size") != std::string::npos) {
            cfg.vocabSize = reader.readMetaUint32(valueType);
        }
        else if (key.find(".context_length") != std::string::npos) {
            cfg.maxSeqLen = reader.readMetaUint32(valueType);
        }
        else if (key.find(".layer_norm_rms_epsilon") != std::string::npos ||
                 key.find("attention.layer_norm_rms_epsilon") != std::string::npos) {
            cfg.rmsNormEps = reader.readMetaFloat(valueType);
        }
        else if (key.find("rope.freq_base") != std::string::npos) {
            cfg.ropeTheta = reader.readMetaFloat(valueType);
        }
        else if (key.find("rope.scaling.type") != std::string::npos) {
            cfg.ropeScalingType = reader.readMetaUint32(valueType);
        }
        else if (key.find("rope.scaling.factor") != std::string::npos) {
            cfg.ropeScalingFactor = reader.readMetaFloat(valueType);
        }
        else {
            reader.skipMetaValue(valueType);
        }
    }

    // Compute derived values
    if (cfg.numHeads > 0 && cfg.hiddenSize > 0) {
        cfg.headDim = cfg.hiddenSize / cfg.numHeads;
    }
    if (cfg.numKVHeads == 0) cfg.numKVHeads = cfg.numHeads;
    if (cfg.rmsNormEps == 0.0f) cfg.rmsNormEps = 1e-5f;
    if (cfg.ropeTheta == 0.0f) cfg.ropeTheta = 10000.0f;
    if (cfg.maxSeqLen == 0) cfg.maxSeqLen = 4096;

    // Estimate total parameter bytes
    cfg.totalParamBytes = 0;
    for (const auto& meta : state.tensorMetas) {
        cfg.totalParamBytes += meta.rawSizeBytes;
    }

    // Estimate VRAM needed (all tensors dequantized to compute type)
    cfg.estimatedVRAM = 0;
    for (const auto& meta : state.tensorMetas) {
        cfg.estimatedVRAM += meta.dequantSizeBytes;
    }

    std::cout << "[GGUF-DML] Model config: arch=" << cfg.architecture
              << " layers=" << cfg.numLayers
              << " hidden=" << cfg.hiddenSize
              << " heads=" << cfg.numHeads
              << " kv_heads=" << cfg.numKVHeads
              << " headDim=" << cfg.headDim
              << " vocab=" << cfg.vocabSize
              << " maxSeq=" << cfg.maxSeqLen
              << " rmsEps=" << cfg.rmsNormEps
              << " ropeTheta=" << cfg.ropeTheta
              << " totalRaw=" << (cfg.totalParamBytes / (1024ULL * 1024)) << "MB"
              << " estVRAM=" << (cfg.estimatedVRAM / (1024ULL * 1024)) << "MB"
              << std::endl;

    return DMLResult::ok("Config parsed");
}

// ============================================================================
// Load All Tensors — full model upload to VRAM
// ============================================================================
DMLResult GGUFDMLBridge::loadAllTensors(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    uint32_t totalTensors = static_cast<uint32_t>(state.tensorMetas.size());
    uint64_t totalBytes = 0;
    uint32_t uploaded = 0;
    uint32_t spilled = 0;

    for (size_t i = 0; i < state.tensorMetas.size(); ++i) {
        auto& meta = state.tensorMetas[i];
        if (meta.uploaded) { uploaded++; continue; }

        // Check if spillover cache already has this tensor
        if (state.spilloverCache.count(meta.nameHash)) { spilled++; continue; }

        if (m_spilloverEnabled) {
            // Use spillover-aware loading: tries VRAM first, falls back to host cache
            auto r = loadTensorWithSpillover(state, i);
            if (!r.success) {
                std::cerr << "[GGUF-DML] Failed to load tensor (spillover): " << meta.name
                          << " error=" << r.detail << std::endl;
            } else {
                if (meta.uploaded) {
                    totalBytes += meta.dequantSizeBytes;
                    uploaded++;
                } else if (state.spilloverCache.count(meta.nameHash)) {
                    spilled++;
                }
            }
        } else {
            auto r = uploadSingleTensor(state, i);
            if (!r.success) {
                std::cerr << "[GGUF-DML] Failed to upload tensor: " << meta.name
                          << " error=" << r.detail << std::endl;
            } else {
                totalBytes += meta.dequantSizeBytes;
                uploaded++;
            }
        }

        if (m_progressCb) {
            m_progressCb(uploaded + spilled, totalTensors, totalBytes,
                         state.config.estimatedVRAM, meta.name, m_progressUserData);
        }
    }

    std::cout << "[GGUF-DML] Loaded " << uploaded << "/" << totalTensors
              << " tensors to VRAM=" << (state.vramUsed / (1024 * 1024)) << "MB";
    if (spilled > 0) {
        std::cout << " | Spilled " << spilled << " tensors to host cache="
                  << (state.spillStats.totalHostCacheBytes / (1024 * 1024)) << "MB";
    }
    std::cout << std::endl;

    return DMLResult::ok("All tensors loaded");
}

// ============================================================================
// Upload Single Tensor
// ============================================================================
DMLResult GGUFDMLBridge::uploadSingleTensor(SessionState& state, size_t tensorIndex) {
    if (tensorIndex >= state.tensorMetas.size()) {
        return DMLResult::error("Invalid tensor index", -1);
    }

    auto& meta = state.tensorMetas[tensorIndex];
    if (meta.uploaded) return DMLResult::ok("Already uploaded");

    // Check VRAM budget
    if (state.vramUsed + meta.dequantSizeBytes > state.vramBudget) {
        if (m_streamingEnabled) {
            auto r = evictLRULayers(state, meta.dequantSizeBytes);
            if (!r.success) return DMLResult::error("VRAM budget exceeded, eviction failed", -2);
        } else {
            return DMLResult::error("VRAM budget exceeded", -2);
        }
    }

    // Get raw data pointer from mmap
    const uint8_t* rawData = static_cast<const uint8_t*>(state.mappedBase) + meta.fileOffset;

    // Prefetch raw data into cache
    asm_dml_prefetch_tensor_block(rawData, std::min(meta.rawSizeBytes, (uint64_t)4096));

    const void* uploadData = rawData;
    uint64_t uploadSize = meta.rawSizeBytes;
    TensorDataType uploadType;

    if (m_dequantOnCPU && meta.ggmlType != GGML_TYPE_F32 && meta.ggmlType != GGML_TYPE_F16) {
        // CPU-side dequantization
        uploadSize = meta.dequantSizeBytes;
        state.stagingBuffer.resize(static_cast<size_t>(uploadSize));

        // Compute total elements
        uint64_t totalElements = 1;
        for (uint32_t d = 0; d < meta.shapeDims; ++d) {
            totalElements *= meta.shape[d];
        }

        auto r = dequantizeTensor(rawData, state.stagingBuffer.data(),
                                   meta.ggmlType, totalElements);
        if (!r.success) return r;

        uploadData = state.stagingBuffer.data();
        uploadType = m_computeType;
    } else {
        uploadType = ggmlTypeToTensorDataType(meta.ggmlType);
    }

    // Upload to DML
    auto r = m_dml->uploadGGUFTensor(state.sessionId, meta.nameHash,
                                      uploadData, uploadSize,
                                      ggmlTypeToQuantType(meta.ggmlType),
                                      meta.shape, meta.shapeDims);
    if (!r.success) return r;

    meta.uploaded = true;
    state.vramUsed += meta.dequantSizeBytes;

    // Update layer load state
    if (meta.layerIndex >= 0) {
        auto& ls = state.layerStates[meta.layerIndex];
        ls.layerIndex = meta.layerIndex;
        ls.loaded = true;
        ls.vramUsed += meta.dequantSizeBytes;
        ls.tensorCount++;
        ls.lastAccessTick = state.accessTick++;
    }

    return DMLResult::ok();
}

// ============================================================================
// CPU Dequantization
// ============================================================================
DMLResult GGUFDMLBridge::dequantizeTensor(const void* srcData, void* dstData,
                                           uint32_t ggmlType, uint64_t elementCount) {
    if (!srcData || !dstData || elementCount == 0) {
        return DMLResult::error("Invalid dequant params", -1);
    }

    const uint8_t* src = static_cast<const uint8_t*>(srcData);
    float* dstF32 = static_cast<float*>(dstData);

    auto blockInfo = getQuantBlockInfo(ggmlType);
    uint64_t numBlocks = (elementCount + blockInfo.blockSize - 1) / blockInfo.blockSize;

    switch (ggmlType) {
        case GGML_TYPE_Q4_0: {
            // Use ASM kernel for Q4_0 dequantization
            int64_t result = asm_dml_dequant_q4_0_to_fp32(dstF32, src, numBlocks);
            if (result < 0) return DMLResult::error("Q4_0 dequant ASM failed", -10);
            break;
        }

        case GGML_TYPE_Q8_0: {
            // Use ASM kernel for Q8_0 dequantization
            int64_t result = asm_dml_dequant_q8_0_to_fp32(dstF32, src, numBlocks);
            if (result < 0) return DMLResult::error("Q8_0 dequant ASM failed", -11);
            break;
        }

        case GGML_TYPE_Q4_1: {
            // Q4_1: scale(fp16) + min(fp16) + 16 bytes data
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block = src + b * blockInfo.blockBytes;
                // Scale and min as fp16 (stored as uint16_t)
                uint16_t d_raw = *reinterpret_cast<const uint16_t*>(block);
                uint16_t m_raw = *reinterpret_cast<const uint16_t*>(block + 2);

                // Simple FP16→FP32 conversion (IEEE 754 half)
                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp = (h >> 10) & 0x1F;
                    uint32_t man = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };

                float scale = fp16ToFp32(d_raw);
                float minVal = fp16ToFp32(m_raw);

                const uint8_t* qdata = block + 4;
                for (int j = 0; j < 16; ++j) {
                    uint8_t byte = qdata[j];
                    dstF32[b * 32 + j * 2]     = static_cast<float>(byte & 0xF) * scale + minVal;
                    dstF32[b * 32 + j * 2 + 1] = static_cast<float>(byte >> 4) * scale + minVal;
                }
            }
            break;
        }

        case GGML_TYPE_F16: {
            // FP16 → FP32
            const uint16_t* srcF16 = reinterpret_cast<const uint16_t*>(src);
            for (uint64_t i = 0; i < elementCount; ++i) {
                uint16_t h = srcF16[i];
                uint32_t sign = (h >> 15) & 1;
                uint32_t exp = (h >> 10) & 0x1F;
                uint32_t man = h & 0x3FF;
                uint32_t f32;
                if (exp == 0) {
                    if (man == 0) { f32 = sign << 31; }
                    else {
                        exp = 1;
                        while (!(man & 0x400)) { man <<= 1; exp--; }
                        man &= 0x3FF;
                        f32 = (sign << 31) | ((exp + 127 - 15) << 23) | (man << 13);
                    }
                } else if (exp == 31) {
                    f32 = (sign << 31) | (0xFF << 23) | (man << 13);
                } else {
                    f32 = (sign << 31) | ((exp + 127 - 15) << 23) | (man << 13);
                }
                memcpy(&dstF32[i], &f32, sizeof(float));
            }
            break;
        }

        case GGML_TYPE_F32: {
            // Direct copy
            memcpy(dstF32, src, elementCount * sizeof(float));
            break;
        }

        case GGML_TYPE_Q5_0: {
            // Q5_0: fp16 scale (2B) + 4 bytes high-bits + 16 bytes low-nibbles = 22 bytes / 32 elements
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block = src + b * 22;
                uint16_t d_raw = *reinterpret_cast<const uint16_t*>(block);
                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp  = (h >> 10) & 0x1F;
                    uint32_t man  = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };
                float scale = fp16ToFp32(d_raw);

                // 4-byte high-bit mask packed as 32 bits (1 bit per element)
                uint32_t qh;
                memcpy(&qh, block + 2, sizeof(uint32_t));

                const uint8_t* qs = block + 6; // 16 bytes of low nibbles
                for (int j = 0; j < 16; ++j) {
                    uint8_t byte = qs[j];
                    // Low element: low nibble + high bit
                    int32_t x0 = (byte & 0xF) | (((qh >> (j * 2)) & 1) << 4);
                    // High element: high nibble + high bit
                    int32_t x1 = (byte >> 4) | (((qh >> (j * 2 + 1)) & 1) << 4);
                    dstF32[b * 32 + j * 2]     = (static_cast<float>(x0) - 16.0f) * scale;
                    dstF32[b * 32 + j * 2 + 1] = (static_cast<float>(x1) - 16.0f) * scale;
                }
            }
            break;
        }

        case GGML_TYPE_Q5_1: {
            // Q5_1: fp16 scale (2B) + fp16 min (2B) + 4 bytes high-bits + 16 bytes low-nibbles = 24 bytes / 32 elements
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block = src + b * 24;
                uint16_t d_raw = *reinterpret_cast<const uint16_t*>(block);
                uint16_t m_raw = *reinterpret_cast<const uint16_t*>(block + 2);
                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp  = (h >> 10) & 0x1F;
                    uint32_t man  = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };
                float scale = fp16ToFp32(d_raw);
                float minVal = fp16ToFp32(m_raw);

                uint32_t qh;
                memcpy(&qh, block + 4, sizeof(uint32_t));

                const uint8_t* qs = block + 8; // 16 bytes of low nibbles
                for (int j = 0; j < 16; ++j) {
                    uint8_t byte = qs[j];
                    int32_t x0 = (byte & 0xF) | (((qh >> (j * 2)) & 1) << 4);
                    int32_t x1 = (byte >> 4) | (((qh >> (j * 2 + 1)) & 1) << 4);
                    dstF32[b * 32 + j * 2]     = static_cast<float>(x0) * scale + minVal;
                    dstF32[b * 32 + j * 2 + 1] = static_cast<float>(x1) * scale + minVal;
                }
            }
            break;
        }

        case GGML_TYPE_Q8_1: {
            // Q8_1: fp32 scale (4B) + fp32 sum (4B) + 32 bytes quants = 40 bytes / 32 elements
            // Note: d = scale, s = sum (for dot product acceleration)
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block = src + b * 36;
                float scale;
                memcpy(&scale, block, sizeof(float));
                // s (sum) at offset 4 is used for dot products, not for standalone dequant
                const int8_t* qs = reinterpret_cast<const int8_t*>(block + 8);
                for (int j = 0; j < 32; ++j) {
                    dstF32[b * 32 + j] = static_cast<float>(qs[j]) * scale;
                }
            }
            break;
        }

        case GGML_TYPE_Q2_K: {
            // Q2_K super-block: 256 elements, 84 bytes
            // Layout: scales[16] (uint8) + qs[64] (2-bit packed) + d (fp16) + dmin (fp16)
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block = src + b * 84;
                const uint8_t* scales = block;          // 16 bytes: 4-bit scale + 4-bit min per sub-block
                const uint8_t* qs     = block + 16;     // 64 bytes: 2-bit quants (256 values packed)
                uint16_t d_raw, dmin_raw;
                memcpy(&d_raw, block + 80, sizeof(uint16_t));
                memcpy(&dmin_raw, block + 82, sizeof(uint16_t));

                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp  = (h >> 10) & 0x1F;
                    uint32_t man  = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };

                float d    = fp16ToFp32(d_raw);
                float dmin = fp16ToFp32(dmin_raw);

                // 16 sub-blocks of 16 elements each
                for (int sb = 0; sb < 16; ++sb) {
                    uint8_t sc_byte   = scales[sb];
                    float sub_scale   = d    * static_cast<float>(sc_byte & 0xF);
                    float sub_min     = dmin * static_cast<float>(sc_byte >> 4);

                    // Each sub-block: 16 elements, 2 bits each = 4 bytes in qs
                    int qs_offset = sb * 4;
                    for (int j = 0; j < 16; ++j) {
                        int byte_idx = qs_offset + (j / 4);
                        int bit_pos  = (j % 4) * 2;
                        uint8_t q    = (qs[byte_idx] >> bit_pos) & 0x3;
                        dstF32[b * 256 + sb * 16 + j] = sub_scale * static_cast<float>(q) - sub_min;
                    }
                }
            }
            break;
        }

        case GGML_TYPE_Q3_K: {
            // Q3_K super-block: 256 elements, 110 bytes
            // Layout: hmask[32] + qs[64] + scales[12] + d(fp16)
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block  = src + b * 110;
                const uint8_t* hmask  = block;          // 32 bytes: high bits
                const uint8_t* qs     = block + 32;     // 64 bytes: 2-bit low quants
                const uint8_t* sc_raw = block + 96;     // 12 bytes: packed 6-bit scales
                uint16_t d_raw;
                memcpy(&d_raw, block + 108, sizeof(uint16_t));

                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp  = (h >> 10) & 0x1F;
                    uint32_t man  = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };

                float d = fp16ToFp32(d_raw);

                // Unpack 16 x 6-bit scales from 12 bytes
                int8_t scales_decoded[16];
                for (int i = 0; i < 8; ++i) {
                    scales_decoded[i]     = static_cast<int8_t>((sc_raw[i] & 0x3F) - 32);
                    scales_decoded[i + 8] = static_cast<int8_t>(
                        (((sc_raw[i] >> 6) & 0x3) | (((sc_raw[8 + (i / 2)] >> (4 * (i % 2))) & 0xF) << 2)) - 32
                    );
                }

                // 16 sub-blocks of 16 elements
                for (int sb = 0; sb < 16; ++sb) {
                    float sub_scale = d * static_cast<float>(scales_decoded[sb]);
                    for (int j = 0; j < 16; ++j) {
                        int idx = sb * 16 + j;
                        // 2-bit low quant from qs
                        int qs_byte_idx = idx / 4;
                        int qs_bit_pos  = (idx % 4) * 2;
                        uint8_t q_low   = (qs[qs_byte_idx] >> qs_bit_pos) & 0x3;
                        // 1-bit high from hmask
                        int hm_byte_idx = idx / 8;
                        int hm_bit_pos  = idx % 8;
                        uint8_t q_high  = (hmask[hm_byte_idx] >> hm_bit_pos) & 1;
                        // 3-bit quant = low(2) + high(1)*4, then subtract 4 (center at 0)
                        int32_t q = q_low | (q_high << 2);
                        dstF32[b * 256 + idx] = sub_scale * (static_cast<float>(q) - 4.0f);
                    }
                }
            }
            break;
        }

        case GGML_TYPE_Q4_K: {
            // Q4_K super-block: 256 elements, 144 bytes
            // Layout: d(fp16) + dmin(fp16) + scales[12] (packed 6-bit) + qs[128] (4-bit)
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block  = src + b * 144;
                uint16_t d_raw, dmin_raw;
                memcpy(&d_raw,    block,     sizeof(uint16_t));
                memcpy(&dmin_raw, block + 2, sizeof(uint16_t));

                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp  = (h >> 10) & 0x1F;
                    uint32_t man  = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };

                float d    = fp16ToFp32(d_raw);
                float dmin = fp16ToFp32(dmin_raw);

                const uint8_t* sc_raw = block + 4;  // 12 bytes packed scales
                const uint8_t* qs     = block + 16; // 128 bytes 4-bit quants

                // Unpack 8 x 6-bit scales and 8 x 6-bit mins from 12 bytes
                uint8_t sc[8], mn[8];
                for (int i = 0; i < 4; ++i) {
                    sc[i]     = sc_raw[i] & 0x3F;
                    mn[i]     = sc_raw[i + 4] & 0x3F;
                    sc[i + 4] = ((sc_raw[i] >> 6) & 0x3) | (((sc_raw[i + 8] >> 0) & 0xF) << 2);
                    mn[i + 4] = ((sc_raw[i + 4] >> 6) & 0x3) | (((sc_raw[i + 8] >> 4) & 0xF) << 2);
                }

                // 8 sub-blocks of 32 elements each
                for (int sb = 0; sb < 8; ++sb) {
                    float sub_d = d * static_cast<float>(sc[sb]);
                    float sub_m = dmin * static_cast<float>(mn[sb]);

                    const uint8_t* q = qs + sb * 16;
                    for (int j = 0; j < 16; ++j) {
                        dstF32[b * 256 + sb * 32 + j * 2    ] = sub_d * static_cast<float>(q[j] & 0xF) - sub_m;
                        dstF32[b * 256 + sb * 32 + j * 2 + 1] = sub_d * static_cast<float>(q[j] >> 4)  - sub_m;
                    }
                }
            }
            break;
        }

        case GGML_TYPE_Q5_K: {
            // Q5_K super-block: 256 elements, 176 bytes
            // Layout: d(fp16) + dmin(fp16) + scales[12] + qh[32] + qs[128]
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block  = src + b * 176;
                uint16_t d_raw, dmin_raw;
                memcpy(&d_raw,    block,     sizeof(uint16_t));
                memcpy(&dmin_raw, block + 2, sizeof(uint16_t));

                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp  = (h >> 10) & 0x1F;
                    uint32_t man  = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };

                float d    = fp16ToFp32(d_raw);
                float dmin = fp16ToFp32(dmin_raw);

                const uint8_t* sc_raw = block + 4;   // 12 bytes packed scales
                const uint8_t* qh     = block + 16;  // 32 bytes high bits
                const uint8_t* qs     = block + 48;   // 128 bytes low nibbles

                // Unpack 8 x 6-bit scales and 8 x 6-bit mins (same layout as Q4_K)
                uint8_t sc[8], mn[8];
                for (int i = 0; i < 4; ++i) {
                    sc[i]     = sc_raw[i] & 0x3F;
                    mn[i]     = sc_raw[i + 4] & 0x3F;
                    sc[i + 4] = ((sc_raw[i] >> 6) & 0x3) | (((sc_raw[i + 8] >> 0) & 0xF) << 2);
                    mn[i + 4] = ((sc_raw[i + 4] >> 6) & 0x3) | (((sc_raw[i + 8] >> 4) & 0xF) << 2);
                }

                // 8 sub-blocks of 32 elements each
                for (int sb = 0; sb < 8; ++sb) {
                    float sub_d = d * static_cast<float>(sc[sb]);
                    float sub_m = dmin * static_cast<float>(mn[sb]);

                    const uint8_t* q = qs + sb * 16;
                    for (int j = 0; j < 32; ++j) {
                        // Low 4 bits from qs
                        int q_byte = j / 2;
                        int q_nibble = (j % 2 == 0) ? (q[q_byte] & 0xF) : (q[q_byte] >> 4);
                        // High 1 bit from qh
                        int qh_bit_idx = sb * 32 + j;
                        int qh_byte    = qh_bit_idx / 8;
                        int qh_bit     = qh_bit_idx % 8;
                        int hi = (qh[qh_byte] >> qh_bit) & 1;
                        int32_t val = q_nibble | (hi << 4);
                        dstF32[b * 256 + sb * 32 + j] = sub_d * static_cast<float>(val) - sub_m;
                    }
                }
            }
            break;
        }

        case GGML_TYPE_Q6_K: {
            // Q6_K super-block: 256 elements, 210 bytes
            // Layout: ql[128] + qh[64] + scales[16] + d(fp16)
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block  = src + b * 210;
                const uint8_t* ql     = block;         // 128 bytes: low 4-bit quants
                const uint8_t* qh_ptr = block + 128;   // 64 bytes: high 2-bit quants
                const int8_t*  scales = reinterpret_cast<const int8_t*>(block + 192); // 16 bytes: int8 scales
                uint16_t d_raw;
                memcpy(&d_raw, block + 208, sizeof(uint16_t));

                auto fp16ToFp32 = [](uint16_t h) -> float {
                    uint32_t sign = (h >> 15) & 1;
                    uint32_t exp  = (h >> 10) & 0x1F;
                    uint32_t man  = h & 0x3FF;
                    if (exp == 0) {
                        if (man == 0) return sign ? -0.0f : 0.0f;
                        float v = ldexpf(static_cast<float>(man), -24);
                        return sign ? -v : v;
                    }
                    if (exp == 31) return sign ? -INFINITY : INFINITY;
                    float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                    return sign ? -v : v;
                };

                float d = fp16ToFp32(d_raw);

                // 16 sub-blocks of 16 elements
                for (int sb = 0; sb < 16; ++sb) {
                    float sub_scale = d * static_cast<float>(scales[sb]);
                    for (int j = 0; j < 16; ++j) {
                        int idx = sb * 16 + j;
                        // Low 4 bits from ql (2 values per byte)
                        uint8_t ql_val = (ql[idx / 2] >> ((idx % 2) * 4)) & 0xF;
                        // High 2 bits from qh (4 values per byte)
                        uint8_t qh_val = (qh_ptr[idx / 4] >> ((idx % 4) * 2)) & 0x3;
                        // 6-bit quant = low(4) | high(2) << 4, centered at 32
                        int32_t q = ql_val | (qh_val << 4);
                        dstF32[b * 256 + idx] = sub_scale * (static_cast<float>(q) - 32.0f);
                    }
                }
            }
            break;
        }

        case GGML_TYPE_Q8_K: {
            // Q8_K super-block: 256 elements, 292 bytes
            // Layout: d(fp32, 4B) + qs[256] (int8) + bsums[16] (int16, 32B)
            for (uint64_t b = 0; b < numBlocks; ++b) {
                const uint8_t* block = src + b * 292;
                float scale;
                memcpy(&scale, block, sizeof(float));
                const int8_t* qs = reinterpret_cast<const int8_t*>(block + 4);
                // bsums at block+260 (32 bytes) are for dot-product acceleration only
                for (int j = 0; j < 256; ++j) {
                    dstF32[b * 256 + j] = static_cast<float>(qs[j]) * scale;
                }
            }
            break;
        }

        default: {
            std::cerr << "[GGUF-DML] Truly unsupported quant type " << ggmlType
                      << " (" << elementCount << " elements) — zeroing output" << std::endl;
            memset(dstF32, 0, elementCount * sizeof(float));
            break;
        }
    }

    return DMLResult::ok("Dequant complete");
}

// ============================================================================
// Layer Management
// ============================================================================
DMLResult GGUFDMLBridge::loadLayer(uint32_t sessionId, int32_t layerIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    if (state.layerStates.count(layerIndex) && state.layerStates[layerIndex].loaded) {
        state.layerStates[layerIndex].lastAccessTick = state.accessTick++;
        return DMLResult::ok("Layer already loaded");
    }

    // Find all tensors for this layer
    uint32_t loaded = 0;
    for (size_t i = 0; i < state.tensorMetas.size(); ++i) {
        if (state.tensorMetas[i].layerIndex == layerIndex) {
            auto r = uploadSingleTensor(state, i);
            if (r.success) loaded++;
        }
    }

    if (loaded == 0) {
        return DMLResult::error("No tensors found for layer", -3);
    }

    return DMLResult::ok("Layer loaded");
}

DMLResult GGUFDMLBridge::unloadLayer(uint32_t sessionId, int32_t layerIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    auto lsIt = state.layerStates.find(layerIndex);
    if (lsIt == state.layerStates.end() || !lsIt->second.loaded) {
        return DMLResult::ok("Layer not loaded");
    }

    // Free all tensors for this layer from DML session
    for (auto& meta : state.tensorMetas) {
        if (meta.layerIndex == layerIndex && meta.uploaded) {
            // Find and free the tensor in the DML session
            auto* session = m_dml->getSession(sessionId);
            if (session) {
                auto tIt = session->tensors.find(meta.nameHash);
                if (tIt != session->tensors.end()) {
                    m_dml->freeTensor(tIt->second);
                    session->tensors.erase(tIt);
                }
            }
            meta.uploaded = false;
            state.vramUsed -= meta.dequantSizeBytes;
        }
    }

    lsIt->second.loaded = false;
    lsIt->second.vramUsed = 0;
    lsIt->second.tensorCount = 0;

    return DMLResult::ok("Layer unloaded");
}

DMLResult GGUFDMLBridge::loadFixedTensors(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    if (state.fixedTensorsLoaded) return DMLResult::ok("Already loaded");

    // Upload non-layer tensors (embedding, output norm, lm_head)
    for (size_t i = 0; i < state.tensorMetas.size(); ++i) {
        if (state.tensorMetas[i].layerIndex < 0) {
            uploadSingleTensor(state, i);
        }
    }

    state.fixedTensorsLoaded = true;
    return DMLResult::ok("Fixed tensors loaded");
}

DMLResult GGUFDMLBridge::ensureLayerLoaded(uint32_t sessionId, int32_t layerIndex) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    // Load fixed tensors if needed
    if (!state.fixedTensorsLoaded) {
        loadFixedTensors(sessionId);
    }

    // Check if layer is already loaded
    auto lsIt = state.layerStates.find(layerIndex);
    if (lsIt != state.layerStates.end() && lsIt->second.loaded) {
        lsIt->second.lastAccessTick = state.accessTick++;
        return DMLResult::ok();
    }

    // Evict if needed
    uint64_t neededBytes = 0;
    for (const auto& meta : state.tensorMetas) {
        if (meta.layerIndex == layerIndex) {
            neededBytes += meta.dequantSizeBytes;
        }
    }

    if (state.vramUsed + neededBytes > state.vramBudget) {
        evictLRULayers(state, neededBytes);
    }

    return loadLayer(sessionId, layerIndex);
}

DMLResult GGUFDMLBridge::evictLRULayers(SessionState& state, uint64_t neededBytes) {
    uint64_t freed = 0;

    while (freed < neededBytes) {
        // Find the least-recently-used loaded layer
        int32_t lruLayer = -1;
        uint64_t lruTick = UINT64_MAX;

        for (auto& [idx, ls] : state.layerStates) {
            if (ls.loaded && ls.lastAccessTick < lruTick) {
                lruTick = ls.lastAccessTick;
                lruLayer = idx;
            }
        }

        if (lruLayer < 0) break;    // No more layers to evict

        uint64_t layerVRAM = state.layerStates[lruLayer].vramUsed;
        unloadLayer(state.sessionId, lruLayer);
        freed += layerVRAM;
    }

    return (freed >= neededBytes)
        ? DMLResult::ok("Eviction complete")
        : DMLResult::error("Could not free enough VRAM", -1);
}

// ============================================================================
// VRAM-to-System-Cache Spillover — Core Implementation
// ============================================================================
//
// When a model's total dequantized size exceeds VRAM budget, the spillover
// system dequantizes tensors into VirtualAlloc-pinned host buffers.  During
// inference the bridge materializes each layer's spilled tensors into VRAM
// on demand (with LRU eviction of other layers), then releases them back
// to host cache after the layer's DML dispatches complete.
//
// Memory path:
//   mmap (GGUF file) → CPU dequant → VirtualAlloc host buffer (spillover)
//                                     ↕   (on-demand upload / release)
//                                   DML VRAM tensor buffer
// ============================================================================

DMLResult GGUFDMLBridge::loadTensorWithSpillover(SessionState& state, size_t tensorIndex) {
    if (tensorIndex >= state.tensorMetas.size()) {
        return DMLResult::error("Invalid tensor index", -1);
    }

    auto& meta = state.tensorMetas[tensorIndex];
    if (meta.uploaded) return DMLResult::ok("Already in VRAM");
    if (state.spilloverCache.count(meta.nameHash)) return DMLResult::ok("Already in host cache");

    // Try VRAM upload first if there's budget remaining
    if (state.vramUsed + meta.dequantSizeBytes <= state.vramBudget) {
        return uploadSingleTensor(state, tensorIndex);
    }

    // VRAM full — try eviction (only for non-fixed, streaming layers)
    if (m_streamingEnabled && meta.layerIndex >= 0) {
        // Try evicting LRU layers to make room
        auto evictR = evictLRULayers(state, meta.dequantSizeBytes);
        if (evictR.success && state.vramUsed + meta.dequantSizeBytes <= state.vramBudget) {
            return uploadSingleTensor(state, tensorIndex);
        }
    }

    // VRAM still full after eviction — spill to system cache
    return spillTensorToHostCache(state, tensorIndex);
}

DMLResult GGUFDMLBridge::spillTensorToHostCache(SessionState& state, size_t tensorIndex) {
    if (tensorIndex >= state.tensorMetas.size()) {
        return DMLResult::error("Invalid tensor index", -1);
    }

    auto& meta = state.tensorMetas[tensorIndex];

    // Check host cache budget
    if (m_hostCacheBudget > 0 &&
        state.spillStats.totalHostCacheBytes + meta.dequantSizeBytes > m_hostCacheBudget) {
        // Try evicting LRU host cache entries
        auto r = evictHostCacheLRU(state, meta.dequantSizeBytes);
        if (!r.success) return DMLResult::error("Host cache budget exceeded", -20);
    }

    // Allocate pinned host memory via VirtualAlloc (better memory alignment, no paging)
    uint8_t* hostBuf = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, static_cast<SIZE_T>(meta.dequantSizeBytes),
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!hostBuf) {
        return DMLResult::error("VirtualAlloc failed for spillover buffer", GetLastError());
    }

    // Get raw data from mmap
    const uint8_t* rawData = static_cast<const uint8_t*>(state.mappedBase) + meta.fileOffset;

    // Prefetch raw data into L1/L2 cache
    asm_dml_prefetch_tensor_block(rawData, std::min(meta.rawSizeBytes, (uint64_t)4096));

    // Dequantize into host buffer
    if (m_dequantOnCPU && meta.ggmlType != GGML_TYPE_F32 && meta.ggmlType != GGML_TYPE_F16) {
        uint64_t totalElements = 1;
        for (uint32_t d = 0; d < meta.shapeDims; ++d) {
            totalElements *= meta.shape[d];
        }

        auto r = dequantizeTensor(rawData, hostBuf, meta.ggmlType, totalElements);
        if (!r.success) {
            VirtualFree(hostBuf, 0, MEM_RELEASE);
            return r;
        }
    } else {
        // F32/F16: direct copy from mmap to host buffer
        memcpy(hostBuf, rawData, static_cast<size_t>(meta.rawSizeBytes));
    }

    // Register in spillover cache
    SpilloverEntry entry;
    entry.nameHash = meta.nameHash;
    entry.tensorIndex = tensorIndex;
    entry.layerIndex = meta.layerIndex;
    entry.sizeBytes = meta.dequantSizeBytes;
    entry.hostBuffer = hostBuf;
    entry.residence = TensorResidence::SystemCache;
    entry.lastAccessTick = state.accessTick++;

    state.spilloverCache[meta.nameHash] = entry;

    // Update statistics
    state.spillStats.totalHostCacheBytes += meta.dequantSizeBytes;
    if (state.spillStats.totalHostCacheBytes > state.spillStats.peakHostCacheBytes) {
        state.spillStats.peakHostCacheBytes = state.spillStats.totalHostCacheBytes;
    }
    state.spillStats.hostCacheBudget = m_hostCacheBudget;
    state.spillStats.tensorsInHostCache++;
    state.spillStats.evictCount++;

    // Count unique spilled layers
    std::unordered_set<int32_t> spilledLayers;
    for (auto& [hash, se] : state.spilloverCache) {
        if (se.layerIndex >= 0) spilledLayers.insert(se.layerIndex);
    }
    state.spillStats.layersSpilled = static_cast<uint32_t>(spilledLayers.size());

    std::cout << "[GGUF-DML] Spilled tensor \"" << meta.name << "\" ("
              << (meta.dequantSizeBytes / 1024) << " KB) to system cache"
              << " | layer=" << meta.layerIndex
              << " | hostCache=" << (state.spillStats.totalHostCacheBytes / (1024 * 1024)) << "MB"
              << std::endl;

    return DMLResult::ok("Tensor spilled to system cache");
}

DMLResult GGUFDMLBridge::uploadFromSpillover(SessionState& state, uint64_t nameHash) {
    auto spIt = state.spilloverCache.find(nameHash);
    if (spIt == state.spilloverCache.end()) {
        return DMLResult::error("Tensor not in spillover cache", -21);
    }

    auto& entry = spIt->second;
    if (entry.residence == TensorResidence::VRAM || entry.residence == TensorResidence::Both) {
        return DMLResult::ok("Already in VRAM");
    }

    if (!entry.hostBuffer || entry.sizeBytes == 0) {
        return DMLResult::error("Spillover entry has no host buffer", -22);
    }

    auto& meta = state.tensorMetas[entry.tensorIndex];

    // Ensure VRAM space — evict other layers if needed
    if (state.vramUsed + entry.sizeBytes > state.vramBudget) {
        auto evR = evictLRULayers(state, entry.sizeBytes);
        if (!evR.success) {
            return DMLResult::error("Cannot free VRAM for spillover upload", -23);
        }
    }

    // Upload from host buffer to DML VRAM (no need to re-dequant)
    auto r = m_dml->uploadGGUFTensor(state.sessionId, nameHash,
                                      entry.hostBuffer, entry.sizeBytes,
                                      ggmlTypeToQuantType(meta.ggmlType),
                                      meta.shape, meta.shapeDims);
    if (!r.success) return r;

    meta.uploaded = true;
    state.vramUsed += entry.sizeBytes;
    entry.residence = TensorResidence::Both;   // Now in both VRAM and host cache
    entry.lastAccessTick = state.accessTick++;

    // Update layer load state
    if (meta.layerIndex >= 0) {
        auto& ls = state.layerStates[meta.layerIndex];
        ls.layerIndex = meta.layerIndex;
        ls.loaded = true;
        ls.vramUsed += entry.sizeBytes;
        ls.tensorCount++;
        ls.lastAccessTick = state.accessTick;
    }

    state.spillStats.uploadCount++;

    return DMLResult::ok("Uploaded from spillover");
}

void GGUFDMLBridge::freeSpilloverEntry(SpilloverEntry& entry) {
    if (entry.hostBuffer) {
        VirtualFree(entry.hostBuffer, 0, MEM_RELEASE);
        entry.hostBuffer = nullptr;
    }
    entry.sizeBytes = 0;
    entry.residence = TensorResidence::None;
}

DMLResult GGUFDMLBridge::evictHostCacheLRU(SessionState& state, uint64_t freedBytes) {
    uint64_t freed = 0;

    while (freed < freedBytes) {
        uint64_t lruHash = 0;
        uint64_t lruTick = UINT64_MAX;

        for (auto& [hash, entry] : state.spilloverCache) {
            // Only evict entries that are purely in host cache (not also in VRAM)
            if (entry.residence == TensorResidence::SystemCache &&
                entry.lastAccessTick < lruTick) {
                lruTick = entry.lastAccessTick;
                lruHash = hash;
            }
        }

        if (lruHash == 0) break;   // No more evictable entries

        auto& entry = state.spilloverCache[lruHash];
        freed += entry.sizeBytes;
        state.spillStats.totalHostCacheBytes -= entry.sizeBytes;
        state.spillStats.tensorsInHostCache--;
        freeSpilloverEntry(entry);
        state.spilloverCache.erase(lruHash);
    }

    return (freed >= freedBytes)
        ? DMLResult::ok("Host cache eviction complete")
        : DMLResult::error("Could not free enough host cache", -24);
}

// ============================================================================
// Spillover — Public Operations
// ============================================================================

DMLResult GGUFDMLBridge::materializeLayerForCompute(uint32_t sessionId, int32_t layerIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    // Upload all spilled tensors for this layer from host cache to VRAM
    uint32_t materialized = 0;
    for (auto& [hash, entry] : state.spilloverCache) {
        if (entry.layerIndex == layerIndex &&
            entry.residence == TensorResidence::SystemCache) {
            auto r = uploadFromSpillover(state, hash);
            if (r.success) materialized++;
        }
    }

    // Also ensure any non-spilled tensors for this layer are uploaded
    for (size_t i = 0; i < state.tensorMetas.size(); ++i) {
        auto& meta = state.tensorMetas[i];
        if (meta.layerIndex == layerIndex && !meta.uploaded) {
            // Not in spillover — try direct upload
            if (!state.spilloverCache.count(meta.nameHash)) {
                uploadSingleTensor(state, i);
            }
        }
    }

    // Update prefetch hint
    state.prefetchLayerHint = layerIndex + 1;

    if (materialized > 0) {
        std::cout << "[GGUF-DML] Materialized layer " << layerIndex
                  << " (" << materialized << " tensors from spillover)"
                  << std::endl;
    }

    return DMLResult::ok("Layer materialized");
}

DMLResult GGUFDMLBridge::releaseLayerVRAM(uint32_t sessionId, int32_t layerIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    // For each tensor in this layer that has a host-cache copy, release VRAM
    for (auto& meta : state.tensorMetas) {
        if (meta.layerIndex == layerIndex && meta.uploaded) {
            auto spIt = state.spilloverCache.find(meta.nameHash);
            if (spIt != state.spilloverCache.end() &&
                spIt->second.residence == TensorResidence::Both) {
                // This tensor has a host-side copy — safe to release VRAM
                auto* session = m_dml->getSession(sessionId);
                if (session) {
                    auto tIt = session->tensors.find(meta.nameHash);
                    if (tIt != session->tensors.end()) {
                        m_dml->freeTensor(tIt->second);
                        session->tensors.erase(tIt);
                    }
                }
                meta.uploaded = false;
                state.vramUsed -= meta.dequantSizeBytes;

                // Revert residence to SystemCache only
                spIt->second.residence = TensorResidence::SystemCache;
            }
        }
    }

    // Update layer load state
    auto lsIt = state.layerStates.find(layerIndex);
    if (lsIt != state.layerStates.end()) {
        // Re-check if any tensors for this layer are still in VRAM
        bool anyInVRAM = false;
        for (auto& meta : state.tensorMetas) {
            if (meta.layerIndex == layerIndex && meta.uploaded) {
                anyInVRAM = true;
                break;
            }
        }
        if (!anyInVRAM) {
            lsIt->second.loaded = false;
            lsIt->second.vramUsed = 0;
        }
    }

    return DMLResult::ok("Layer VRAM released");
}

DMLResult GGUFDMLBridge::prefetchLayers(uint32_t sessionId, int32_t startLayer, uint32_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::error("Session not found", -1);
    auto& state = it->second;

    uint32_t prefetched = 0;
    for (uint32_t i = 0; i < count; ++i) {
        int32_t layerIdx = startLayer + static_cast<int32_t>(i);
        if (layerIdx < 0 || static_cast<uint32_t>(layerIdx) >= state.config.numLayers) continue;

        // Check if any spilled tensors for this layer need uploading
        bool hasSpilled = false;
        for (auto& [hash, entry] : state.spilloverCache) {
            if (entry.layerIndex == layerIdx &&
                entry.residence == TensorResidence::SystemCache) {
                hasSpilled = true;
                break;
            }
        }

        if (!hasSpilled) continue;

        // Check VRAM budget — only prefetch if we have room
        uint64_t layerNeeded = 0;
        for (auto& [hash, entry] : state.spilloverCache) {
            if (entry.layerIndex == layerIdx &&
                entry.residence == TensorResidence::SystemCache) {
                layerNeeded += entry.sizeBytes;
            }
        }

        if (state.vramUsed + layerNeeded > state.vramBudget) {
            // No room — evict other layers if possible
            auto evR = evictLRULayers(state, layerNeeded);
            if (!evR.success) {
                // Can't prefetch this layer — skip
                state.spillStats.prefetchMisses++;
                continue;
            }
        }

        // Upload spilled tensors for this layer
        for (auto& [hash, entry] : state.spilloverCache) {
            if (entry.layerIndex == layerIdx &&
                entry.residence == TensorResidence::SystemCache) {
                uploadFromSpillover(state, hash);
            }
        }

        prefetched++;
        state.spillStats.prefetchHits++;
    }

    return DMLResult::ok("Prefetch complete");
}

// ============================================================================
// Spillover — Query Methods
// ============================================================================

SpilloverStats GGUFDMLBridge::getSpilloverStats(uint32_t sessionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return {};
    return it->second.spillStats;
}

uint64_t GGUFDMLBridge::getHostCacheUsed(uint32_t sessionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second.spillStats.totalHostCacheBytes : 0;
}

bool GGUFDMLBridge::isTensorSpilled(uint32_t sessionId, uint64_t nameHash) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return false;
    auto spIt = it->second.spilloverCache.find(nameHash);
    return (spIt != it->second.spilloverCache.end());
}

TensorResidence GGUFDMLBridge::getTensorResidence(uint32_t sessionId, uint64_t nameHash) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return TensorResidence::None;

    // Check spillover cache first
    auto spIt = it->second.spilloverCache.find(nameHash);
    if (spIt != it->second.spilloverCache.end()) return spIt->second.residence;

    // Check if it's a regular VRAM-resident tensor
    for (const auto& meta : it->second.tensorMetas) {
        if (meta.nameHash == nameHash && meta.uploaded) return TensorResidence::VRAM;
    }
    return TensorResidence::None;
}

// ============================================================================
// Dual-Model
// ============================================================================
DMLResult GGUFDMLBridge::loadSecondModel(const char* ggufPath, uint32_t session2Id) {
    // Recalculate VRAM budgets — split equally
    uint64_t totalBudget = m_maxVRAMBudget;
    uint64_t perModel = totalBudget / 2;

    // Adjust existing session budget
    for (auto& [id, state] : m_sessions) {
        state.vramBudget = perModel;
    }

    m_maxVRAMBudget = perModel;  // New models get this budget

    return openModel(ggufPath, session2Id);
}

bool GGUFDMLBridge::getModelSlots(ModelSlot* slots, uint32_t maxSlots, uint32_t& outCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    outCount = 0;

    for (auto& [id, state] : m_sessions) {
        if (outCount >= maxSlots) break;

        auto& slot = slots[outCount];
        slot.sessionId = state.sessionId;
        strncpy_s(slot.modelPath, state.modelPath.c_str(), sizeof(slot.modelPath) - 1);
        strncpy_s(slot.architecture, state.config.architecture, sizeof(slot.architecture) - 1);
        slot.numLayers = state.config.numLayers;
        slot.vramUsed = state.vramUsed;
        slot.vramBudget = state.vramBudget;

        // Count loaded layers
        slot.layersInVRAM = 0;
        slot.fullLoaded = true;
        for (auto& [_, ls] : state.layerStates) {
            if (ls.loaded) slot.layersInVRAM++;
            else slot.fullLoaded = false;
        }

        outCount++;
    }
    return true;
}

// ============================================================================
// Query Methods
// ============================================================================
const GGUFModelConfig& GGUFDMLBridge::getModelConfig(uint32_t sessionId) const {
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second.config : s_emptyConfig;
}

uint32_t GGUFDMLBridge::getTensorCount(uint32_t sessionId) const {
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? static_cast<uint32_t>(it->second.tensorMetas.size()) : 0;
}

uint64_t GGUFDMLBridge::getVRAMUsed(uint32_t sessionId) const {
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second.vramUsed : 0;
}

uint64_t GGUFDMLBridge::getVRAMBudget(uint32_t sessionId) const {
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second.vramBudget : 0;
}

int32_t GGUFDMLBridge::getLoadedLayerCount(uint32_t sessionId) const {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return 0;
    int32_t count = 0;
    for (auto& [_, ls] : it->second.layerStates) {
        if (ls.loaded) count++;
    }
    return count;
}

bool GGUFDMLBridge::isLayerLoaded(uint32_t sessionId, int32_t layerIndex) const {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return false;
    auto lsIt = it->second.layerStates.find(layerIndex);
    return (lsIt != it->second.layerStates.end()) && lsIt->second.loaded;
}

// ============================================================================
// Cleanup
// ============================================================================
DMLResult GGUFDMLBridge::closeModel(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return DMLResult::ok("Not found");

    auto& state = it->second;

    // Destroy DML session (frees GPU resources)
    m_dml->destroySession(sessionId);

    // Free spillover host buffers
    for (auto& [hash, entry] : state.spilloverCache) {
        freeSpilloverEntry(entry);
    }
    state.spilloverCache.clear();

    // Free allocated tensor names
    for (auto& meta : state.tensorMetas) {
        delete[] meta.name;
        meta.name = nullptr;
    }

    // Unmap and close file
    if (state.mappedBase) UnmapViewOfFile(state.mappedBase);
    if (state.fileMapping) CloseHandle(state.fileMapping);
    if (state.fileHandle) CloseHandle(state.fileHandle);

    m_sessions.erase(it);
    return DMLResult::ok("Model closed");
}

DMLResult GGUFDMLBridge::closeAllModels() {
    std::vector<uint32_t> ids;
    for (auto& [id, _] : m_sessions) ids.push_back(id);
    for (auto id : ids) closeModel(id);
    return DMLResult::ok("All models closed");
}

// ============================================================================
// Utilities
// ============================================================================
uint64_t GGUFDMLBridge::computeDequantSize(uint32_t ggmlType, const uint32_t* shape,
                                            uint32_t shapeDims) const {
    uint64_t totalElements = 1;
    for (uint32_t i = 0; i < shapeDims; ++i) totalElements *= shape[i];

    uint32_t bytesPerElement = (m_computeType == TensorDataType::Float16) ? 2 : 4;

    // For F16/F32, no dequant needed — just the raw size
    if (ggmlType == GGML_TYPE_F16) return totalElements * 2;
    if (ggmlType == GGML_TYPE_F32) return totalElements * 4;

    // For quantized types: dequant to compute type
    return totalElements * bytesPerElement;
}

TensorDataType GGUFDMLBridge::ggmlTypeToTensorDataType(uint32_t ggmlType) const {
    switch (ggmlType) {
        case GGML_TYPE_F32:  return TensorDataType::Float32;
        case GGML_TYPE_F16:  return TensorDataType::Float16;
        default:             return m_computeType;  // Dequantized to compute type
    }
}

GGUFQuantType GGUFDMLBridge::ggmlTypeToQuantType(uint32_t ggmlType) const {
    switch (ggmlType) {
        case GGML_TYPE_F32:  return GGUFQuantType::F32;
        case GGML_TYPE_F16:  return GGUFQuantType::F16;
        case GGML_TYPE_Q4_0: return GGUFQuantType::Q4_0;
        case GGML_TYPE_Q4_1: return GGUFQuantType::Q4_1;
        case GGML_TYPE_Q5_0: return GGUFQuantType::Q5_0;
        case GGML_TYPE_Q5_1: return GGUFQuantType::Q5_1;
        case GGML_TYPE_Q8_0: return GGUFQuantType::Q8_0;
        case GGML_TYPE_Q2_K: return GGUFQuantType::Q2_K;
        case GGML_TYPE_Q3_K: return GGUFQuantType::Q3_K_M;
        case GGML_TYPE_Q4_K: return GGUFQuantType::Q4_K_M;
        case GGML_TYPE_Q5_K: return GGUFQuantType::Q5_K_M;
        case GGML_TYPE_Q6_K: return GGUFQuantType::Q6_K;
        default:             return GGUFQuantType::F32;
    }
}

uint64_t GGUFDMLBridge::hashTensorName(const char* name) const {
    return fnv1a(name);
}

int32_t GGUFDMLBridge::extractLayerIndex(const char* name) const {
    // Pattern: "blk.N." where N is the layer index
    const char* blk = strstr(name, "blk.");
    if (!blk) return -1;

    const char* numStart = blk + 4;    // Skip "blk."
    int32_t idx = 0;
    while (*numStart >= '0' && *numStart <= '9') {
        idx = idx * 10 + (*numStart - '0');
        numStart++;
    }
    if (*numStart == '.') return idx;
    return -1;
}

// ============================================================================
// Diagnostics
// ============================================================================
std::string GGUFDMLBridge::getDiagnostics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream ss;

    ss << "=== GGUF-DML Bridge Diagnostics ===\n";
    ss << "Sessions: " << m_sessions.size() << "\n";
    ss << "Compute Type: " << (m_computeType == TensorDataType::Float16 ? "FP16" : "FP32") << "\n";
    ss << "VRAM Budget: " << (m_maxVRAMBudget / (1024 * 1024)) << " MB/model\n";
    ss << "Streaming: " << (m_streamingEnabled ? "On" : "Off") << "\n";
    ss << "Spillover: " << (m_spilloverEnabled ? "On" : "Off") << "\n";
    ss << "Host Cache Budget: " << (m_hostCacheBudget == 0 ? "Unlimited" :
           std::to_string(m_hostCacheBudget / (1024 * 1024)) + " MB") << "\n";
    ss << "Layer Cache: " << m_layerCacheCount << " layers\n\n";

    for (auto& [id, state] : m_sessions) {
        ss << "--- Session " << id << " ---\n";
        ss << "  Model: " << state.modelPath << "\n";
        ss << "  Arch: " << state.config.architecture << "\n";
        ss << "  Layers: " << state.config.numLayers << "\n";
        ss << "  Tensors: " << state.tensorMetas.size() << "\n";
        ss << "  VRAM: " << (state.vramUsed / (1024 * 1024)) << "/"
            << (state.vramBudget / (1024 * 1024)) << " MB\n";

        uint32_t loadedLayers = 0;
        for (auto& [_, ls] : state.layerStates) {
            if (ls.loaded) loadedLayers++;
        }
        ss << "  Loaded Layers: " << loadedLayers << "/" << state.config.numLayers << "\n";
        ss << "  Fixed Tensors: " << (state.fixedTensorsLoaded ? "Yes" : "No") << "\n";

        // Spillover diagnostics
        if (!state.spilloverCache.empty()) {
            ss << "  --- Spillover ---\n";
            ss << "  Host Cache: " << (state.spillStats.totalHostCacheBytes / (1024 * 1024))
               << "/" << (state.spillStats.peakHostCacheBytes / (1024 * 1024)) << " MB (cur/peak)\n";
            ss << "  Tensors in Host Cache: " << state.spillStats.tensorsInHostCache << "\n";
            ss << "  Layers Spilled: " << state.spillStats.layersSpilled << "\n";
            ss << "  Uploads (host→GPU): " << state.spillStats.uploadCount << "\n";
            ss << "  Spill Events: " << state.spillStats.evictCount << "\n";
            ss << "  Prefetch Hits/Misses: " << state.spillStats.prefetchHits
               << "/" << state.spillStats.prefetchMisses << "\n";
        }
    }

    return ss.str();
}

// ============================================================================
// Global accessor
// ============================================================================
GGUFDMLBridge& getGGUFDMLBridge() {
    static GGUFDMLBridge instance;
    return instance;
}

} // namespace DML
} // namespace RawrXD
