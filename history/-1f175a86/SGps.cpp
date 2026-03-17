// src/direct_io/gguf_burstzone_patcher.cpp
// ═══════════════════════════════════════════════════════════════════════════════
// RAWRXD v1.2.0 — GGUF BURSTZONE METADATA PATCHER
// ═══════════════════════════════════════════════════════════════════════════════
// PURPOSE: Inject "burstzone" key into GGUF metadata for zero-latency cold load
// USAGE:   burstzone_patch <model.gguf> <topology.json> [--output patched.gguf]
// ═══════════════════════════════════════════════════════════════════════════════

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <string>
#include <map>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// GGUF STRUCTURES (Minimal for Metadata Patching)
// ─────────────────────────────────────────────────────────────────────────────

constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF"

enum class GGUFValueType : uint32_t {
    UINT8   = 0,
    INT8    = 1,
    UINT16  = 2,
    INT16   = 3,
    UINT32  = 4,
    INT32   = 5,
    FLOAT32 = 6,
    BOOL    = 7,
    STRING  = 8,
    ARRAY   = 9,
    UINT64  = 10,
    INT64   = 11,
    FLOAT64 = 12,
};

#pragma pack(push, 1)
struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensorCount;
    uint64_t metadataKVCount;
};
#pragma pack(pop)

// ─────────────────────────────────────────────────────────────────────────────
// BURSTZONE METADATA STRUCTURE
// ─────────────────────────────────────────────────────────────────────────────
// Injected as: rawrxd.burstzone = "<base64-encoded BurstZoneHeader>"
//
// BurstZoneHeader (binary, then base64):
//   - Magic: "BZON" (4 bytes)
//   - Version: uint32 (0x00010200)
//   - DriveCount: uint8 (1-5)
//   - Flags: uint8 (bit 0: has_jitmap, bit 1: has_vcache_hints)
//   - Reserved: uint16
//   - TensorZoneCount: uint32
//   - TensorZones[]: array of TensorZone
//
// TensorZone (24 bytes):
//   - TensorIndex: uint32 (index in GGUF tensor table)
//   - DriveIndex: uint8
//   - Priority: uint8 (0=cold, 1=warm, 2=hot)
//   - Reserved: uint16
//   - PreferredLBA: uint64 (hint, or 0 for auto)
//   - SizeBytes: uint64

#pragma pack(push, 1)
struct BurstZoneHeader {
    uint32_t magic;          // 'BZON' = 0x4E4F5A42
    uint32_t version;
    uint8_t  driveCount;
    uint8_t  flags;
    uint16_t reserved;
    uint32_t tensorZoneCount;
};

struct TensorZone {
    uint32_t tensorIndex;
    uint8_t  driveIndex;
    uint8_t  priority;       // 0=cold, 1=warm, 2=hot (attention heads)
    uint16_t reserved;
    uint64_t preferredLBA;
    uint64_t sizeBytes;
};
#pragma pack(pop)

constexpr uint32_t BURSTZONE_MAGIC   = 0x4E4F5A42; // 'BZON'
constexpr uint32_t BURSTZONE_VERSION = 0x00010200;

// ─────────────────────────────────────────────────────────────────────────────
// BASE64 ENCODER (Minimal, for embedding binary in GGUF string)
// ─────────────────────────────────────────────────────────────────────────────

static const char* BASE64_CHARS = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve((len + 2) / 3 * 4);
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (data[i] << 16);
        if (i + 1 < len) n |= (data[i + 1] << 8);
        if (i + 2 < len) n |= data[i + 2];
        
        result += BASE64_CHARS[(n >> 18) & 0x3F];
        result += BASE64_CHARS[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? BASE64_CHARS[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? BASE64_CHARS[n & 0x3F] : '=';
    }
    return result;
}

std::vector<uint8_t> Base64Decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    // ... (decoding implementation for runtime)
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// GGUF READER (Minimal for Tensor Enumeration)
// ─────────────────────────────────────────────────────────────────────────────

class GGUFReader {
public:
    bool Open(const std::string& path) {
        m_file.open(path, std::ios::binary);
        if (!m_file) return false;
        
        m_file.read(reinterpret_cast<char*>(&m_header), sizeof(m_header));
        if (m_header.magic != GGUF_MAGIC) {
            std::cerr << "Invalid GGUF magic" << std::endl;
            return false;
        }
        
        std::cout << "GGUF Version: " << m_header.version << std::endl;
        std::cout << "Tensor Count: " << m_header.tensorCount << std::endl;
        std::cout << "Metadata KV: " << m_header.metadataKVCount << std::endl;
        
        return true;
    }
    
    // Skip through metadata to find tensor info offset
    bool ScanMetadata() {
        for (uint64_t i = 0; i < m_header.metadataKVCount; i++) {
            // Read key
            uint64_t keyLen;
            m_file.read(reinterpret_cast<char*>(&keyLen), 8);
            std::string key(keyLen, '\0');
            m_file.read(&key[0], keyLen);
            
            // Read value type
            uint32_t valueType;
            m_file.read(reinterpret_cast<char*>(&valueType), 4);
            
            // Skip value based on type
            SkipValue(static_cast<GGUFValueType>(valueType));
            
            m_metadataKeys.push_back(key);
        }
        
        m_tensorInfoOffset = m_file.tellg();
        return true;
    }
    
    // Get tensor info for burst planning
    struct TensorInfo {
        std::string name;
        uint64_t offset;
        uint64_t size;
        uint32_t index;
    };
    
    std::vector<TensorInfo> GetTensorInfos() {
        std::vector<TensorInfo> tensors;
        m_file.seekg(m_tensorInfoOffset);
        
        for (uint64_t i = 0; i < m_header.tensorCount; i++) {
            TensorInfo info;
            info.index = static_cast<uint32_t>(i);
            
            // Read name
            uint64_t nameLen;
            m_file.read(reinterpret_cast<char*>(&nameLen), 8);
            info.name.resize(nameLen);
            m_file.read(&info.name[0], nameLen);
            
            // Read dimensions
            uint32_t nDims;
            m_file.read(reinterpret_cast<char*>(&nDims), 4);
            
            uint64_t totalElements = 1;
            for (uint32_t d = 0; d < nDims; d++) {
                uint64_t dim;
                m_file.read(reinterpret_cast<char*>(&dim), 8);
                totalElements *= dim;
            }
            
            // Read type
            uint32_t type;
            m_file.read(reinterpret_cast<char*>(&type), 4);
            
            // Read offset
            m_file.read(reinterpret_cast<char*>(&info.offset), 8);
            
            // Calculate size based on type
            info.size = totalElements * GetTypeSize(type);
            
            tensors.push_back(info);
        }
        
        return tensors;
    }
    
    uint64_t GetDataOffset() const {
        return m_tensorInfoOffset; // Approximate
    }
    
    const GGUFHeader& GetHeader() const { return m_header; }
    
private:
    std::ifstream m_file;
    GGUFHeader m_header;
    std::vector<std::string> m_metadataKeys;
    std::streampos m_tensorInfoOffset;
    
    void SkipValue(GGUFValueType type) {
        switch (type) {
            case GGUFValueType::UINT8:
            case GGUFValueType::INT8:
            case GGUFValueType::BOOL:
                m_file.seekg(1, std::ios::cur);
                break;
            case GGUFValueType::UINT16:
            case GGUFValueType::INT16:
                m_file.seekg(2, std::ios::cur);
                break;
            case GGUFValueType::UINT32:
            case GGUFValueType::INT32:
            case GGUFValueType::FLOAT32:
                m_file.seekg(4, std::ios::cur);
                break;
            case GGUFValueType::UINT64:
            case GGUFValueType::INT64:
            case GGUFValueType::FLOAT64:
                m_file.seekg(8, std::ios::cur);
                break;
            case GGUFValueType::STRING: {
                uint64_t len;
                m_file.read(reinterpret_cast<char*>(&len), 8);
                m_file.seekg(len, std::ios::cur);
                break;
            }
            case GGUFValueType::ARRAY: {
                uint32_t elemType;
                uint64_t count;
                m_file.read(reinterpret_cast<char*>(&elemType), 4);
                m_file.read(reinterpret_cast<char*>(&count), 8);
                for (uint64_t i = 0; i < count; i++) {
                    SkipValue(static_cast<GGUFValueType>(elemType));
                }
                break;
            }
        }
    }
    
    uint32_t GetTypeSize(uint32_t type) {
        // GGML types: 0=F32, 1=F16, 2=Q4_0, etc.
        switch (type) {
            case 0: return 4;   // F32
            case 1: return 2;   // F16
            case 2: return 18;  // Q4_0 (block of 32 elements)
            case 3: return 20;  // Q4_1
            case 6: return 18;  // Q5_0
            case 7: return 22;  // Q5_1
            case 8: return 34;  // Q8_0
            default: return 2;  // Default to F16 size
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BURSTZONE PLANNER
// ─────────────────────────────────────────────────────────────────────────────

class BurstZonePlanner {
public:
    BurstZonePlanner(uint8_t driveCount = 6) : m_driveCount(driveCount) {}
    
    // Assign tensors to drives based on access patterns and drive tiers
    std::vector<TensorZone> PlanZones(const std::vector<GGUFReader::TensorInfo>& tensors) {
        std::vector<TensorZone> zones;
        zones.reserve(tensors.size());
        
        for (const auto& tensor : tensors) {
            TensorZone zone;
            zone.tensorIndex = tensor.index;
            zone.sizeBytes = tensor.size;
            zone.preferredLBA = 0; // Auto-assign
            
            // Heuristic: Assign based on tensor name pattern AND size
            zone.driveIndex = AssignDrive(tensor.name, tensor.size);
            zone.priority = AssignPriority(tensor.name);
            zone.reserved = 0;
            
            zones.push_back(zone);
        }
        
        return zones;
    }
    
    // Generate binary BurstZone data
    std::vector<uint8_t> GenerateBurstZoneData(const std::vector<TensorZone>& zones) {
        std::vector<uint8_t> data;
        
        BurstZoneHeader header;
        header.magic = BURSTZONE_MAGIC;
        header.version = BURSTZONE_VERSION;
        header.driveCount = m_driveCount;
        header.flags = 0x03; // has_jitmap + has_tiered_storage
        header.reserved = 0;
        header.tensorZoneCount = static_cast<uint32_t>(zones.size());
        
        // Append header
        const uint8_t* hdr = reinterpret_cast<const uint8_t*>(&header);
        data.insert(data.end(), hdr, hdr + sizeof(header));
        
        // Append zones
        for (const auto& zone : zones) {
            const uint8_t* z = reinterpret_cast<const uint8_t*>(&zone);
            data.insert(data.end(), z, z + sizeof(zone));
        }
        
        return data;
    }
    
private:
    uint8_t m_driveCount;
    
    // Tiered drive assignment for 6-drive topology:
    // TIER 0 (NVMe - lowest latency):
    //   Drive 0: KV Cache, embeddings (random access)
    //   Drive 1: Attention Q/K projections (hot path)
    //   Drive 2: FFN/MLP layers (hot path)
    // TIER 1 (External SSD - high throughput):
    //   Drive 4: V projections, large weight matrices
    //   Drive 5: Output heads, lm_head, large embeddings
    // TIER 2 (USB HDD - cold storage):
    //   Drive 3: Checkpoints, overflow, rarely-accessed tensors
    
    uint8_t AssignDrive(const std::string& name, uint64_t size) {
        // HOT PATH: Attention heads → NVMe (drives 1-2)
        if (name.find(".q_proj") != std::string::npos ||
            name.find("_q.") != std::string::npos ||
            name.find(".k_proj") != std::string::npos ||
            name.find("_k.") != std::string::npos) {
            return 1;  // NVMe drive 1 (attention Q/K)
        }
        
        // HOT PATH: FFN/MLP → NVMe drive 2
        if (name.find("ffn") != std::string::npos ||
            name.find("mlp") != std::string::npos ||
            name.find("feed_forward") != std::string::npos) {
            return 2;  // NVMe drive 2 (FFN)
        }
        
        // WARM PATH: V projections → External SSD drive 4
        if (name.find(".v_proj") != std::string::npos ||
            name.find("_v.") != std::string::npos) {
            return 4;  // Micron X10 Pro (large capacity)
        }
        
        // WARM PATH: Output/embeddings → External SSD drive 5
        if (name.find("output") != std::string::npos ||
            name.find("lm_head") != std::string::npos ||
            size > 500 * 1024 * 1024) {  // > 500MB tensors
            return 5;  // Micron X10 Pro
        }
        
        // EMBEDDINGS: token_embd goes to KV cache drive
        if (name.find("embed") != std::string::npos ||
            name.find("token_embd") != std::string::npos) {
            return 0;  // NVMe drive 0 (KV cache, random access)
        }
        
        // COLD PATH: Everything else → balanced across NVMe
        // Use size-based heuristic: small → NVMe, large → External SSD
        if (size < 100 * 1024 * 1024) {
            return (name.length() % 3);  // Distribute across NVMe 0-2
        } else {
            return 4 + (name.length() % 2);  // Distribute across External SSD 4-5
        }
    }
    
    // Priority: 0=cold (loaded once), 1=warm (reused), 2=hot (attention)
    uint8_t AssignPriority(const std::string& name) {
        if (name.find("attn") != std::string::npos ||
            name.find("attention") != std::string::npos) {
            return 2; // Hot
        }
        if (name.find("ffn") != std::string::npos ||
            name.find("mlp") != std::string::npos) {
            return 1; // Warm
        }
        return 0; // Cold
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// GGUF METADATA INJECTOR
// ─────────────────────────────────────────────────────────────────────────────

class GGUFMetadataInjector {
public:
    // Inject burstzone metadata into GGUF
    // Strategy: Create new GGUF with additional metadata key
    bool InjectBurstZone(const std::string& inputPath,
                          const std::string& outputPath,
                          const std::vector<uint8_t>& burstZoneData) {
        
        std::ifstream input(inputPath, std::ios::binary);
        std::ofstream output(outputPath, std::ios::binary);
        
        if (!input || !output) {
            std::cerr << "Failed to open files" << std::endl;
            return false;
        }
        
        // Read original header
        GGUFHeader header;
        input.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        // Increment metadata count
        header.metadataKVCount++;
        
        // Write modified header
        output.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Copy existing metadata
        // (In production, we'd parse and preserve all metadata properly)
        // For now, we'll append our key at the end of metadata section
        
        // ... (Full implementation would stream-copy metadata here)
        
        // Write our new key-value pair
        const std::string key = "rawrxd.burstzone";
        uint64_t keyLen = key.size();
        output.write(reinterpret_cast<const char*>(&keyLen), 8);
        output.write(key.c_str(), keyLen);
        
        // Value type: STRING
        uint32_t valueType = static_cast<uint32_t>(GGUFValueType::STRING);
        output.write(reinterpret_cast<const char*>(&valueType), 4);
        
        // Encode burstzone data as base64 string
        std::string encoded = Base64Encode(burstZoneData.data(), burstZoneData.size());
        uint64_t strLen = encoded.size();
        output.write(reinterpret_cast<const char*>(&strLen), 8);
        output.write(encoded.c_str(), strLen);
        
        // Copy rest of file (tensor info + data)
        char buffer[65536];
        while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
            output.write(buffer, input.gcount());
        }
        
        std::cout << "✓ BurstZone metadata injected: " << encoded.size() << " bytes (base64)" << std::endl;
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// CLI TOOL
// ─────────────────────────────────────────────────────────────────────────────

void PrintUsage(const char* prog) {
    std::cout << "RAWRXD v1.2.0 BurstZone Patcher" << std::endl;
    std::cout << "Usage: " << prog << " <input.gguf> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --output <path>    Output patched GGUF (default: input_burstzone.gguf)" << std::endl;
    std::cout << "  --drives <N>       Number of drives in topology (default: 5)" << std::endl;
    std::cout << "  --analyze          Analyze only, don't patch" << std::endl;
    std::cout << "  --dump-zones       Print zone assignments" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    std::string inputPath = argv[1];
    std::string outputPath;
    uint8_t driveCount = 5;
    bool analyzeOnly = false;
    bool dumpZones = false;
    
    // Parse arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "--drives" && i + 1 < argc) {
            driveCount = static_cast<uint8_t>(std::stoi(argv[++i]));
        } else if (arg == "--analyze") {
            analyzeOnly = true;
        } else if (arg == "--dump-zones") {
            dumpZones = true;
        }
    }
    
    if (outputPath.empty()) {
        size_t dot = inputPath.rfind('.');
        outputPath = inputPath.substr(0, dot) + "_burstzone.gguf";
    }
    
    // Read GGUF
    GGUFReader reader;
    if (!reader.Open(inputPath)) {
        std::cerr << "Failed to open GGUF: " << inputPath << std::endl;
        return 1;
    }
    
    if (!reader.ScanMetadata()) {
        std::cerr << "Failed to scan metadata" << std::endl;
        return 1;
    }
    
    auto tensors = reader.GetTensorInfos();
    std::cout << "Found " << tensors.size() << " tensors" << std::endl;
    
    // Plan zones
    BurstZonePlanner planner(driveCount);
    auto zones = planner.PlanZones(tensors);
    
    if (dumpZones) {
        std::cout << "\n--- Zone Assignments ---" << std::endl;
        for (size_t i = 0; i < std::min(zones.size(), size_t(50)); i++) {
            std::cout << "  [" << zones[i].tensorIndex << "] "
                      << tensors[i].name
                      << " -> Drive " << (int)zones[i].driveIndex
                      << " (Priority " << (int)zones[i].priority << ")"
                      << " [" << (zones[i].sizeBytes / 1024 / 1024) << " MB]"
                      << std::endl;
        }
        if (zones.size() > 50) {
            std::cout << "  ... (" << (zones.size() - 50) << " more tensors)" << std::endl;
        }
    }
    
    // Compute statistics
    uint64_t totalSize = 0;
    uint64_t driveSize[5] = {0};
    for (const auto& zone : zones) {
        totalSize += zone.sizeBytes;
        if (zone.driveIndex < 5) {
            driveSize[zone.driveIndex] += zone.sizeBytes;
        }
    }
    
    std::cout << "\n--- Drive Distribution ---" << std::endl;
    for (int i = 0; i < driveCount; i++) {
        double pct = 100.0 * driveSize[i] / totalSize;
        std::cout << "  Drive " << i << ": " 
                  << (driveSize[i] / 1024 / 1024 / 1024) << " GB ("
                  << pct << "%)" << std::endl;
    }
    std::cout << "  Total: " << (totalSize / 1024 / 1024 / 1024) << " GB" << std::endl;
    
    if (analyzeOnly) {
        std::cout << "\n[Analyze-only mode, not patching]" << std::endl;
        return 0;
    }
    
    // Generate burstzone data
    auto burstZoneData = planner.GenerateBurstZoneData(zones);
    std::cout << "\nBurstZone data size: " << burstZoneData.size() << " bytes" << std::endl;
    
    // Inject into GGUF
    GGUFMetadataInjector injector;
    if (!injector.InjectBurstZone(inputPath, outputPath, burstZoneData)) {
        std::cerr << "Failed to inject burstzone" << std::endl;
        return 1;
    }
    
    std::cout << "\n✓ Patched GGUF written to: " << outputPath << std::endl;
    std::cout << "✓ BurstZone ready for Tier-7 vectorized loading" << std::endl;
    
    return 0;
}
